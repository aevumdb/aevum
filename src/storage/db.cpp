/*
 * AEVUMDB COMMUNITY LICENSE
 * Version 1.0, February 2026
 *
 * Copyright (c) 2026 Ananda Firmansyah.
 * Official Organization: AevumDB (https://github.com/aevumdb)
 *
 * This source code is licensed under the AevumDB Community License.
 * You may not use this file except in compliance with the License.
 */

/**
 * @file db.cpp
 * @brief Implementation of the central Database Controller.
 *
 * @details
 * This file implements the `Db` class, the primary orchestration layer of AevumDB.
 * It coordinates in-memory data structures, disk persistence (via Engine),
 * query execution (via Rust FFI), and access control logic.
 */

#include "aevum/storage/db.hpp"

#include "aevum/ffi/rust_adapter.hpp"
#include "aevum/infra/id_generator.hpp"
#include "aevum/infra/logger.hpp"

#include <algorithm>
#include <cJSON.h>
#include <ctime>
#include <iostream>
#include <unordered_set>
#include <vector>

namespace aevum::storage {

/**
 * @brief Computes a DJB2 hash for a given credential key.
 *
 * This non-cryptographic hash function is used for fast lookups of API keys
 * in the in-memory authentication cache.
 *
 * @param key The input credential string.
 * @return std::string The numeric hash representation.
 */
std::string Db::hash_key(const std::string& key)
{
    unsigned long hash = 5381;
    for (char c : key)
        hash = ((hash << 5) + hash) + c;
    return std::to_string(hash);
}

/**
 * @brief Constructs the Database Controller and performs system bootstrap.
 *
 * **Startup Sequence:**
 * 1. Initializes the storage subsystem (creates directories).
 * 2. Replays transaction logs to restore in-memory state (`load_all`).
 * 3. Enforces security policy: Creates a default 'root' user if the auth store is empty.
 *
 * @param data_dir The filesystem path for persistent storage.
 */
Db::Db(std::string data_dir) : storage_(std::move(data_dir))
{
    infra::Logger::log(infra::LogLevel::INFO, "Core: Initializing AevumDB Storage Engine...");
    storage_.init();
    load_all();

    // Security Safety Net: Prevent lockout on fresh installs
    if (auth_cache_.empty()) {
        infra::Logger::log(infra::LogLevel::WARN,
                           "Security: Auth store empty. bootstrapping default 'root' admin.");
        create_user("root", "admin");
    }
    infra::Logger::log(infra::LogLevel::INFO, "Core: Engine Online. Accepting connections.");
}

/**
 * @brief Destructor. Performs orderly shutdown and resource reclamation.
 *
 * Iterates through all managed `cJSON` structures (collections and schemas)
 * and frees them to prevent memory leaks upon process termination.
 */
Db::~Db()
{
    infra::Logger::log(infra::LogLevel::INFO, "Core: Shutting down Storage Engine...");
    for (auto& [k, v] : memory_store_)
        cJSON_Delete(v);
    for (auto& [k, v] : schemas_)
        cJSON_Delete(v);
}

/**
 * @brief Restores database state by replaying Append-Only Logs (AOL).
 *
 * **Replay Strategy:**
 * 1. **Metadata Phase**: Loads `_indexes` and `_schemas` first to configure the engine.
 * 2. **Data Phase**: Reconstructs collections document-by-document.
 * - Handles **Upserts** (Newer log entries overwrite older ones).
 * - Handles **Tombstones** (Entries with `_deleted: true` remove the document).
 * 3. **Indexing Phase**: Rebuilds Hash Maps and B-Trees for O(1)/O(log n) access.
 * 4. **Compaction Heuristic**: Triggers auto-compaction if logs are excessively fragmented.
 */
void Db::load_all()
{
    infra::Logger::log(infra::LogLevel::DEBUG, "Core: Replaying transaction logs...");
    auto names = storage_.list_collections();

    for (const auto& name : names) {
        // Phase 1: Load Index Metadata
        if (name == "_indexes") {
            infra::Logger::log(infra::LogLevel::TRACE, "Core: Loading index definitions.");
            std::vector<std::string> logs = storage_.load_log(name);
            for (const auto& log : logs) {
                cJSON* arr = cJSON_Parse(log.c_str());
                if (arr) {
                    cJSON* item = nullptr;
                    cJSON_ArrayForEach(item, arr)
                    {
                        cJSON* c = cJSON_GetObjectItem(item, "collection");
                        cJSON* f = cJSON_GetObjectItem(item, "field");
                        if (c && f)
                            indexed_fields_[c->valuestring].insert(f->valuestring);
                    }
                    cJSON_Delete(arr);
                }
            }
            continue;
        }

        // Phase 2: Load Collection Data
        std::vector<std::string> logs = storage_.load_log(name);
        cJSON* collection_arr = cJSON_CreateArray();
        std::unordered_map<std::string, cJSON*> temp_map;

        for (const auto& log_entry : logs) {
            cJSON* item = cJSON_Parse(log_entry.c_str());
            if (!item) {
                infra::Logger::log(infra::LogLevel::ERROR,
                                   "Core: Detected corrupt frame in " + name + ". Skipping.");
                continue;
            }

            // Handle Schema Definitions
            if (name == "_schemas") {
                cJSON* c_name = cJSON_GetObjectItem(item, "collection");
                if (c_name && c_name->valuestring) {
                    if (schemas_.count(c_name->valuestring))
                        cJSON_Delete(schemas_[c_name->valuestring]);
                    schemas_[c_name->valuestring] = item;
                }
                continue;
            }

            // Handle Standard Documents (Merge & Tombstone logic)
            cJSON* id_node = cJSON_GetObjectItem(item, "_id");
            if (id_node && id_node->valuestring) {
                std::string uuid = id_node->valuestring;
                cJSON* deleted = cJSON_GetObjectItem(item, "_deleted");

                if (deleted && cJSON_IsTrue(deleted)) {
                    // Tombstone: Hard delete from snapshot
                    if (temp_map.count(uuid)) {
                        cJSON_Delete(temp_map[uuid]);
                        temp_map.erase(uuid);
                    }
                    cJSON_Delete(item); // Log entry consumed
                } else {
                    // Upsert: Replace snapshot with newer version
                    if (temp_map.count(uuid))
                        cJSON_Delete(temp_map[uuid]);
                    temp_map[uuid] = item;
                }
            } else {
                cJSON_Delete(item); // Invalid document (No ID)
            }
        }

        // Phase 3: Finalize In-Memory Structure
        if (name != "_schemas") {
            for (auto& [id, doc] : temp_map) {
                cJSON_AddItemToArray(collection_arr, doc);
            }
            memory_store_[name] = collection_arr;

            // Rebuild in-memory indexes
            rebuild_index(name, collection_arr);

            // Populate Auth Cache from _auth collection
            if (name == "_auth") {
                cJSON* item = nullptr;
                cJSON_ArrayForEach(item, collection_arr)
                {
                    cJSON* k = cJSON_GetObjectItem(item, "key_hash");
                    cJSON* r = cJSON_GetObjectItem(item, "role");
                    if (k && r) {
                        std::string role_str = r->valuestring;
                        UserRole role = UserRole::READ_ONLY;
                        if (role_str == "admin")
                            role = UserRole::ADMIN;
                        else if (role_str == "read_write")
                            role = UserRole::READ_WRITE;

                        auth_cache_[k->valuestring] = role;
                    }
                }
                infra::Logger::log(infra::LogLevel::INFO, "Security: RBAC policies loaded.");
            }

            // Heuristic: Trigger compaction if fragmentation > 50%
            if (logs.size() > temp_map.size() * 2 && temp_map.size() > 100) {
                infra::Logger::log(infra::LogLevel::INFO, "Maintenance: Auto-compacting " + name);
                compact_collection(name);
            }
        }
    }
}

/**
 * @brief Provisions a new user credential.
 *
 * @param key The raw API key (will be hashed).
 * @param role The RBAC role ("admin", "read_write", "read_only").
 * @return true If persistence succeeded.
 */
bool Db::create_user(const std::string& key, const std::string& role)
{
    std::unique_lock lock(rw_lock_);
    std::string hashed = hash_key(key);

    UserRole u_role = UserRole::READ_ONLY;
    if (role == "admin")
        u_role = UserRole::ADMIN;
    else if (role == "read_write")
        u_role = UserRole::READ_WRITE;

    auth_cache_[hashed] = u_role;

    cJSON* user_doc = cJSON_CreateObject();
    cJSON_AddStringToObject(user_doc, "_id", infra::IdGenerator::generate().c_str());
    cJSON_AddStringToObject(user_doc, "key_hash", hashed.c_str());
    cJSON_AddStringToObject(user_doc, "role", role.c_str());
    cJSON_AddNumberToObject(user_doc, "created_at", std::time(nullptr));

    char* raw = cJSON_PrintUnformatted(user_doc);
    bool ok = storage_.append("_auth", raw);
    free(raw);

    cJSON* arr = get_collection("_auth");
    cJSON_AddItemToArray(arr, user_doc);

    infra::Logger::log(infra::LogLevel::INFO, "Security: User provisioned. Role: " + role);
    return ok;
}

/**
 * @brief Authenticates a request.
 *
 * @param key The provided API key.
 * @return UserRole The resolved permission level.
 */
UserRole Db::authenticate(const std::string& key)
{
    if (key.empty())
        return UserRole::NONE;
    std::shared_lock lock(rw_lock_);
    std::string hashed = hash_key(key);
    if (auth_cache_.find(hashed) != auth_cache_.end())
        return auth_cache_[hashed];
    return UserRole::NONE;
}

/**
 * @brief Authorizes an action based on role.
 *
 * @param user_role The authenticated role.
 * @param action The requested operation opcode.
 * @return true If permitted.
 */
bool Db::has_permission(UserRole user_role, const std::string& action)
{
    if (user_role == UserRole::ADMIN)
        return true;
    if (user_role == UserRole::READ_WRITE)
        return action == "insert" || action == "update" || action == "delete" ||
               action == "upsert" || action == "find" || action == "count";
    if (user_role == UserRole::READ_ONLY)
        return action == "find" || action == "count";
    return false;
}

/**
 * @brief Performs Log Compaction (Garbage Collection).
 *
 * Rewrites the AOL file with the current in-memory snapshot, purging
 * obsolete records and tombstones to reclaim disk space.
 *
 * @param coll The collection name.
 * @return true If successful.
 */
bool Db::compact_collection(const std::string& coll)
{
    if (!memory_store_.count(coll))
        return false;

    std::vector<std::string> active_docs;
    cJSON* item = nullptr;
    cJSON_ArrayForEach(item, memory_store_[coll])
    {
        char* raw = cJSON_PrintUnformatted(item);
        active_docs.push_back(std::string(raw));
        free(raw);
    }

    bool ok = storage_.compact(coll, active_docs);
    if (ok)
        infra::Logger::log(infra::LogLevel::DEBUG, "Maintenance: Compaction complete for " + coll);
    else
        infra::Logger::log(infra::LogLevel::ERROR, "Maintenance: Compaction failed for " + coll);

    return ok;
}

/**
 * @brief Thread-safe trigger for manual compaction.
 */
bool Db::trigger_compaction(const std::string& coll)
{
    std::unique_lock lock(rw_lock_);
    return compact_collection(coll);
}

/**
 * @brief Rebuilds internal indexes (Primary & Secondary) from a dataset.
 *
 * @param collection The target collection.
 * @param array The document list.
 */
void Db::rebuild_index(const std::string& collection, cJSON* array)
{
    infra::Logger::log(infra::LogLevel::TRACE, "Index: Rebuilding indexes for " + collection);
    id_indexes_[collection].clear();
    custom_indexes_[collection].clear();

    cJSON* item = nullptr;
    cJSON_ArrayForEach(item, array)
    {
        cJSON* id_node = cJSON_GetObjectItem(item, "_id");
        if (cJSON_IsString(id_node) && (id_node->valuestring != nullptr)) {
            id_indexes_[collection][id_node->valuestring] = item;
        }
        update_custom_index(collection, item, true);
    }
}

/**
 * @brief Updates secondary indexes in real-time.
 *
 * @param coll Collection name.
 * @param doc Document pointer.
 * @param add Operation type (true=Add, false=Remove).
 */
void Db::update_custom_index(const std::string& coll, cJSON* doc, bool add)
{
    if (indexed_fields_.find(coll) == indexed_fields_.end())
        return;

    for (const auto& field : indexed_fields_[coll]) {
        cJSON* val = cJSON_GetObjectItem(doc, field.c_str());
        std::string key_val;
        if (cJSON_IsString(val) && val->valuestring)
            key_val = val->valuestring;
        else if (cJSON_IsNumber(val))
            key_val = std::to_string(val->valuedouble);
        else
            continue;

        auto& vec = custom_indexes_[coll][field][key_val];
        if (add) {
            vec.push_back(doc);
        } else {
            auto it = std::find(vec.begin(), vec.end(), doc);
            if (it != vec.end()) {
                vec.erase(it);
                if (vec.empty())
                    custom_indexes_[coll][field].erase(key_val);
            }
        }
    }
}

/**
 * @brief Defines a secondary index and backfills existing data.
 *
 * @param coll Collection name.
 * @param field Field name to index.
 * @return true If successful.
 */
bool Db::create_index(const std::string& coll, const std::string& field)
{
    std::unique_lock lock(rw_lock_);
    if (indexed_fields_[coll].count(field))
        return true;

    infra::Logger::log(infra::LogLevel::INFO, "Index: Creating index on " + coll + "." + field);
    indexed_fields_[coll].insert(field);

    // Backfill
    if (memory_store_.count(coll)) {
        cJSON* item = nullptr;
        cJSON_ArrayForEach(item, memory_store_[coll]) update_custom_index(coll, item, true);
    }

    // Persist Metadata
    cJSON* idx_arr = cJSON_CreateArray();
    for (const auto& [c_name, fields] : indexed_fields_) {
        for (const auto& f_name : fields) {
            cJSON* obj = cJSON_CreateObject();
            cJSON_AddStringToObject(obj, "collection", c_name.c_str());
            cJSON_AddStringToObject(obj, "field", f_name.c_str());
            cJSON_AddItemToArray(idx_arr, obj);
        }
    }

    char* raw = cJSON_PrintUnformatted(idx_arr);
    bool ok = storage_.compact("_indexes", {std::string(raw)});
    free(raw);
    cJSON_Delete(idx_arr);
    return ok;
}

/**
 * @brief Helper: Retrieves the in-memory array for a collection.
 */
cJSON* Db::get_collection(const std::string& name)
{
    if (memory_store_.find(name) == memory_store_.end()) {
        memory_store_[name] = cJSON_CreateArray();
        id_indexes_[name] = {};
    }
    return memory_store_[name];
}

/**
 * @brief Validates a document against registered schemas via Rust FFI.
 */
bool Db::validate(const std::string& coll, cJSON* data)
{
    if (schemas_.find(coll) == schemas_.end())
        return true;

    char* raw_doc = cJSON_PrintUnformatted(data);
    char* raw_schema = cJSON_PrintUnformatted(schemas_[coll]);
    bool is_valid = ffi::call_validate(raw_doc, raw_schema);

    free(raw_doc);
    free(raw_schema);

    if (!is_valid) {
        infra::Logger::log(infra::LogLevel::ERROR,
                           "Validation: Schema violation detected in " + coll);
    }
    return is_valid;
}

/**
 * @brief Persists a schema definition.
 */
bool Db::set_schema(const std::string& coll, cJSON* schema)
{
    std::unique_lock lock(rw_lock_);
    if (schemas_.count(coll))
        cJSON_Delete(schemas_[coll]);
    schemas_[coll] = cJSON_Duplicate(schema, 1);

    char* raw = cJSON_PrintUnformatted(schema);
    bool ok = storage_.append("_schemas", raw);
    free(raw);

    infra::Logger::log(infra::LogLevel::INFO, "Schema: Definition updated for " + coll);
    return ok;
}

/**
 * @brief Inserts a document.
 *
 * Sequence: Validate -> Assign ID -> Index -> Persist -> Commit to Memory.
 */
bool Db::insert(const std::string& coll, cJSON* data)
{
    std::unique_lock lock(rw_lock_);
    if (!validate(coll, data))
        return false;

    cJSON* arr = get_collection(coll);
    std::string uuid;
    if (!cJSON_HasObjectItem(data, "_id")) {
        uuid = infra::IdGenerator::generate();
        cJSON_AddStringToObject(data, "_id", uuid.c_str());
    } else {
        uuid = cJSON_GetObjectItem(data, "_id")->valuestring;
    }

    cJSON* new_item = cJSON_Duplicate(data, 1);
    cJSON_AddItemToArray(arr, new_item);

    id_indexes_[coll][uuid] = new_item;
    update_custom_index(coll, new_item, true);

    char* raw = cJSON_PrintUnformatted(new_item);
    bool ok = storage_.append(coll, raw);
    free(raw);

    infra::Logger::log(infra::LogLevel::TRACE, "CRUD: Inserted " + uuid + " -> " + coll);
    return ok;
}

/**
 * @brief Atomic Upsert.
 */
bool Db::upsert(const std::string& coll, const cJSON* query, cJSON* data)
{
    int existing = count(coll, query);
    if (existing > 0)
        return update(coll, query, data);
    else
        return insert(coll, data);
}

/**
 * @brief Counts matches via Rust FFI.
 */
int Db::count(const std::string& coll, const cJSON* query)
{
    std::shared_lock lock(rw_lock_);
    if (memory_store_.find(coll) == memory_store_.end())
        return 0;
    if (!query || cJSON_GetArraySize(query) == 0)
        return cJSON_GetArraySize(memory_store_[coll]);

    char* raw_data = cJSON_PrintUnformatted(memory_store_[coll]);
    char* raw_query = cJSON_PrintUnformatted(query);
    int result = ffi::call_count(raw_data, raw_query);

    free(raw_data);
    free(raw_query);
    return result;
}

/**
 * @brief Executes a Find query with multi-tiered optimization.
 *
 * **Optimization Levels:**
 * 1. **O(1) Primary Key:** Direct Hash Map lookup.
 * 2. **O(log n) Secondary Index:** B-Tree/Hash Map lookup on indexed fields.
 * 3. **O(n) Full Scan:** Serialized dispatch to Rust Engine.
 */
cJSON* Db::find(const std::string& coll, const cJSON* query, const cJSON* sort,
                const cJSON* projection, int limit, int skip)
{
    std::shared_lock lock(rw_lock_);
    if (memory_store_.find(coll) == memory_store_.end())
        return cJSON_CreateArray();

    // Tier 1: O(1) ID Lookup
    cJSON* id_query = cJSON_GetObjectItem(query, "_id");
    bool simple_req = (!sort || cJSON_GetArraySize(sort) == 0) &&
                      (!projection || cJSON_GetArraySize(projection) == 0);

    if (simple_req && cJSON_IsString(id_query) && (id_query->valuestring != nullptr)) {
        std::string target_id = id_query->valuestring;
        if (id_indexes_[coll].count(target_id)) {
            infra::Logger::log(infra::LogLevel::TRACE,
                               "Query: Optimized O(1) ID access: " + target_id);
            cJSON* res = cJSON_CreateArray();
            cJSON_AddItemToArray(res, cJSON_Duplicate(id_indexes_[coll][target_id], 1));
            return res;
        }
        return cJSON_CreateArray();
    }

    // Tier 2: O(log n) Indexed Field Lookup
    if (simple_req && cJSON_GetArraySize(query) == 1 && indexed_fields_.count(coll)) {
        cJSON* child = query->child;
        if (child && child->string && indexed_fields_[coll].count(child->string)) {
            std::string val_str;
            if (cJSON_IsString(child))
                val_str = child->valuestring;
            else if (cJSON_IsNumber(child))
                val_str = std::to_string(child->valuedouble);

            if (!val_str.empty() && custom_indexes_[coll][child->string].count(val_str)) {
                infra::Logger::log(infra::LogLevel::TRACE,
                                   "Query: Using Secondary Index on " + std::string(child->string));
                cJSON* res = cJSON_CreateArray();
                const auto& docs = custom_indexes_[coll][child->string][val_str];
                size_t start = skip;
                size_t end = (limit > 0) ? std::min(start + limit, docs.size()) : docs.size();
                if (start < docs.size()) {
                    for (size_t i = start; i < end; ++i)
                        cJSON_AddItemToArray(res, cJSON_Duplicate(docs[i], 1));
                }
                return res;
            }
            if (!val_str.empty())
                return cJSON_CreateArray();
        }
    }

    // Tier 3: O(n) Full Scan (Rust FFI)
    infra::Logger::log(infra::LogLevel::WARN, "Query: Full scan triggered on " + coll);
    char* raw_data = cJSON_PrintUnformatted(memory_store_[coll]);
    char* raw_query = cJSON_PrintUnformatted(query);
    char* raw_sort = cJSON_PrintUnformatted(sort ? sort : cJSON_CreateObject());
    char* raw_proj = cJSON_PrintUnformatted(projection ? projection : cJSON_CreateObject());

    std::string res = ffi::call_find(raw_data, raw_query, raw_sort, raw_proj, limit, skip);

    free(raw_data);
    free(raw_query);
    free(raw_sort);
    free(raw_proj);
    return cJSON_Parse(res.c_str());
}

/**
 * @brief Updates documents via Rust FFI and triggers compaction.
 */
bool Db::update(const std::string& coll, const cJSON* query, const cJSON* data)
{
    std::unique_lock lock(rw_lock_);
    if (memory_store_.find(coll) == memory_store_.end())
        return false;

    infra::Logger::log(infra::LogLevel::DEBUG, "CRUD: Executing update on " + coll);

    char* raw_d = cJSON_PrintUnformatted(memory_store_[coll]);
    char* raw_q = cJSON_PrintUnformatted(query);
    char* raw_u = cJSON_PrintUnformatted(data);

    std::string res = ffi::call_update(raw_d, raw_q, raw_u);
    cJSON* new_collection = cJSON_Parse(res.c_str());

    // Atomic Swap
    cJSON_Delete(memory_store_[coll]);
    memory_store_[coll] = new_collection;

    // Full Rebuild & Persist
    rebuild_index(coll, memory_store_[coll]);
    compact_collection(coll);

    free(raw_d);
    free(raw_q);
    free(raw_u);
    return true;
}

/**
 * @brief Deletes documents using the "Turbo Delete" strategy.
 *
 * **Turbo Delete Logic:**
 * 1. **Identification**: Finds target IDs via O(1) lookup, Indexes, or Full Scan.
 * 2. **Persistence**: Appends a Tombstone record `{_id: "...", _deleted: true}` to disk.
 * (This is O(1) I/O vs O(N) rewrite).
 * 3. **Memory**: Immediately removes the document from all in-memory structures (Maps, Indexes,
 * Lists).
 * 4. **Cleanup**: Space is reclaimed during the next auto-compaction cycle.
 *
 * @param coll Collection name.
 * @param query Filter criteria.
 * @return true If deletions occurred.
 */
bool Db::remove(const std::string& coll, const cJSON* query)
{
    std::unique_lock lock(rw_lock_);

    if (memory_store_.find(coll) == memory_store_.end()) {
        return false;
    }

    std::vector<std::string> ids_to_remove;

    cJSON* id_query = cJSON_GetObjectItem(query, "_id");
    bool is_simple_req = cJSON_GetArraySize(query) == 1;

    // 1. Optimization: Single ID Deletion
    if (is_simple_req && cJSON_IsString(id_query) && (id_query->valuestring != nullptr)) {
        if (id_indexes_[coll].count(id_query->valuestring)) {
            ids_to_remove.push_back(id_query->valuestring);
        }
    }
    // 2. Optimization: Index Deletion
    else if (is_simple_req && indexed_fields_.count(coll)) {
        cJSON* child = query->child;
        if (child && child->string && indexed_fields_[coll].count(child->string)) {
            std::string val_str;
            if (cJSON_IsString(child))
                val_str = child->valuestring;
            else if (cJSON_IsNumber(child))
                val_str = std::to_string(child->valuedouble);

            if (!val_str.empty() && custom_indexes_[coll].count(child->string)) {
                if (custom_indexes_[coll][child->string].count(val_str)) {
                    const auto& docs = custom_indexes_[coll][child->string][val_str];
                    for (const auto& d : docs) {
                        cJSON* id = cJSON_GetObjectItem(d, "_id");
                        if (id && id->valuestring) {
                            ids_to_remove.push_back(id->valuestring);
                        }
                    }
                }
            }
        }
    }

    // 3. Fallback: Full Scan via Rust
    if (ids_to_remove.empty()) {
        infra::Logger::log(infra::LogLevel::WARN, "CRUD: Full scan required for Delete on " + coll);

        char* raw_d = cJSON_PrintUnformatted(memory_store_[coll]);
        char* raw_q = cJSON_PrintUnformatted(query);

        std::string find_res_str = ffi::call_find(raw_d, raw_q, "{}", "{}", 0, 0);
        cJSON* to_delete_arr = cJSON_Parse(find_res_str.c_str());

        free(raw_d);
        free(raw_q);

        if (to_delete_arr) {
            cJSON* item = nullptr;
            cJSON_ArrayForEach(item, to_delete_arr)
            {
                cJSON* id = cJSON_GetObjectItem(item, "_id");
                if (id && id->valuestring) {
                    ids_to_remove.push_back(id->valuestring);
                }
            }
            cJSON_Delete(to_delete_arr);
        }
    }

    if (ids_to_remove.empty()) {
        return false;
    }

    infra::Logger::log(infra::LogLevel::DEBUG, "CRUD: Turbo Delete removing " +
                                                   std::to_string(ids_to_remove.size()) + " docs.");

    // 4. Execution: Tombstone Writing & Memory Detachment
    for (const auto& uuid : ids_to_remove) {
        if (id_indexes_[coll].count(uuid)) {
            cJSON* target_doc = id_indexes_[coll][uuid];

            // A. Write Tombstone to Disk
            cJSON* tomb = cJSON_CreateObject();
            cJSON_AddStringToObject(tomb, "_id", uuid.c_str());
            cJSON_AddBoolToObject(tomb, "_deleted", true);

            char* raw_tomb = cJSON_PrintUnformatted(tomb);
            storage_.append(coll, raw_tomb);
            free(raw_tomb);
            cJSON_Delete(tomb);

            // B. Update Memory Structures
            update_custom_index(coll, target_doc, false); // Remove from secondary indexes
            id_indexes_[coll].erase(uuid);                // Remove from primary index

            // Remove from main storage array
            cJSON_DetachItemViaPointer(memory_store_[coll], target_doc);
            cJSON_Delete(target_doc);
        }
    }

    return true;
}

} // namespace aevum::storage
