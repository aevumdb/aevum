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
 * @file db.hpp
 * @brief High-level storage controller and query interface.
 *
 * @details
 * This header defines the `Db` class, the primary orchestration layer of the
 * AevumDB storage engine. It manages in-memory data structures (Hash Maps/B-Trees),
 * coordinates disk persistence via the `Engine` class (WAL), enforces ACID
 * properties (Atomicity, Consistency, Isolation, Durability), and handles access control.
 */

#pragma once

#include "aevum/storage/engine.hpp"

#include <cJSON.h>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace aevum::storage {

/**
 * @enum UserRole
 * @brief Role-Based Access Control (RBAC) levels.
 */
enum class UserRole {
    NONE,       ///< Access Denied.
    READ_ONLY,  ///< Permission to execute non-mutating queries (FIND, COUNT).
    READ_WRITE, ///< Full permission for data manipulation (CRUD).
    ADMIN       ///< Superuser privileges (Schema management, Indexing, User Admin).
};

/**
 * @class Db
 * @brief The central database controller managing memory, persistence, and querying.
 *
 * @details
 * The Db class encapsulates the entire state of the database instance.
 *
 * **Core Responsibilities:**
 * - **Concurrency Control:** Utilizes `std::shared_mutex` to implement Reader-Writer locks.
 * - **Memory Management:** Maintains the primary data store and secondary indexes in RAM.
 * - **Persistence:** Delegates log-structured storage operations to the `Engine`.
 * - **Query Execution:** Parsers and executes JSON-based query plans.
 */
class Db {
  public:
    /**
     * @brief Initializes the database instance and performs crash recovery.
     *
     * The constructor initializes the underlying storage engine and replays the
     * Write-Ahead Logs (WAL) to restore the in-memory state to the last consistent checkpoint.
     *
     * @param data_dir The root directory path where `.aev` log files are persisted.
     */
    explicit Db(std::string data_dir);

    /**
     * @brief Destructor.
     *
     * Ensures a graceful shutdown by flushing pending writes, closing file handles,
     * and releasing allocated memory resources.
     */
    ~Db();

    // ========================================================================
    //  CRUD OPERATIONS (Thread-Safe)
    // ========================================================================

    /**
     * @brief Inserts a new document into a collection.
     *
     * @param coll The target collection name.
     * @param data The JSON document object to insert.
     * @return true If the operation committed successfully.
     * @return false If schema validation failed or an I/O error occurred.
     *
     * @note Acquires a **Writer Lock** (Exclusive).
     */
    bool insert(const std::string& coll, cJSON* data);

    /**
     * @brief Atomic Upsert (Update or Insert).
     *
     * If a document matching the query exists, it is updated. Otherwise, a new
     * document is created.
     *
     * @param coll The target collection name.
     * @param query The criteria to identify the document.
     * @param data The payload to save.
     * @return true If the operation committed successfully.
     *
     * @note Acquires a **Writer Lock** (Exclusive).
     */
    bool upsert(const std::string& coll, const cJSON* query, cJSON* data);

    /**
     * @brief Counts documents matching a filter criteria.
     *
     * @param coll The collection name.
     * @param query The filter object (pass `nullptr` or `{}` for total count).
     * @return int The number of matching records.
     *
     * @note Acquires a **Reader Lock** (Shared).
     */
    int count(const std::string& coll, const cJSON* query);

    /**
     * @brief Retrieves documents based on query parameters.
     *
     * Executes a full query pipeline including filtering, sorting, projection,
     * and pagination.
     *
     * @param coll The collection name.
     * @param query The filter criteria.
     * @param sort (Optional) Sorting instructions.
     * @param projection (Optional) Field selection mask.
     * @param limit Maximum records to return (0 = unlimited).
     * @param skip Number of records to bypass.
     * @return cJSON* A generic JSON Array containing the result set.
     *
     * @warning The caller assumes ownership of the returned `cJSON*` pointer
     * and is responsible for freeing it.
     *
     * @note Acquires a **Reader Lock** (Shared).
     */
    cJSON* find(const std::string& coll, const cJSON* query, const cJSON* sort = nullptr,
                const cJSON* projection = nullptr, int limit = 0, int skip = 0);

    /**
     * @brief Modifies existing documents matching a query.
     *
     * @param coll The collection name.
     * @param query The selection criteria.
     * @param update_data The update operators (e.g., `{"$set": ...}`).
     * @return true If the update committed successfully.
     *
     * @note Acquires a **Writer Lock** (Exclusive).
     */
    bool update(const std::string& coll, const cJSON* query, const cJSON* update_data);

    /**
     * @brief Deletes documents matching a query.
     *
     * @param coll The collection name.
     * @param query The selection criteria.
     * @return true If the deletion committed successfully.
     *
     * @note Acquires a **Writer Lock** (Exclusive).
     */
    bool remove(const std::string& coll, const cJSON* query);

    // ========================================================================
    //  ADMINISTRATIVE OPERATIONS
    // ========================================================================

    /**
     * @brief Enforces a validation schema on a collection.
     *
     * @param coll The collection name.
     * @param schema The JSON schema definition.
     * @return true If the schema was applied successfully.
     */
    bool set_schema(const std::string& coll, cJSON* schema);

    /**
     * @brief Creates a secondary index to optimize query performance.
     *
     * @param coll The collection name.
     * @param field The field name to index.
     * @return true If the index was built successfully.
     */
    bool create_index(const std::string& coll, const std::string& field);

    /**
     * @brief Initiates manual log compaction (Garbage Collection).
     *
     * Rewrites the append-only log file to purge obsolete (updated/deleted) records,
     * reclaiming disk space and improving startup time.
     *
     * @param coll The collection name.
     * @return true If compaction completed successfully.
     */
    bool trigger_compaction(const std::string& coll);

    // ========================================================================
    //  SECURITY & AUTHENTICATION
    // ========================================================================

    /**
     * @brief Provisions a new database user.
     *
     * @param key The API key or username credential.
     * @param role The assigned role (e.g., "admin", "read_write").
     * @return true If the user was created successfully.
     */
    bool create_user(const std::string& key, const std::string& role);

    /**
     * @brief Authenticates a credential against the user store.
     *
     * @param key The API key to verify.
     * @return UserRole The privileges associated with the key, or `UserRole::NONE`.
     */
    UserRole authenticate(const std::string& key);

    /**
     * @brief Authorizes an action based on the user's role.
     *
     * @param user_role The authenticated user's role.
     * @param action The requested operation (e.g., "insert", "find").
     * @return true If the action is permitted.
     */
    bool has_permission(UserRole user_role, const std::string& action);

  private:
    /// @brief The persistence layer responsible for physical disk I/O.
    Engine storage_;

    /// @brief Concurrency primitive for thread-safe memory access.
    mutable std::shared_mutex rw_lock_;

    /// @brief Primary In-Memory Store (Collection Name -> JSON Document Array).
    std::unordered_map<std::string, cJSON*> memory_store_;

    /// @brief Primary Key Index (Collection -> _id -> Document Pointer).
    /// Enables O(1) lookups by ID.
    std::unordered_map<std::string, std::unordered_map<std::string, cJSON*>> id_indexes_;

    /// @brief Schema Registry (Collection Name -> JSON Schema).
    std::unordered_map<std::string, cJSON*> schemas_;

    /// @brief Secondary Indexes (Collection -> Field -> Value -> List of Document Pointers).
    /// Supports optimized range and equality queries on non-ID fields.
    std::unordered_map<
        std::string,
        std::unordered_map<std::string, std::unordered_map<std::string, std::vector<cJSON*>>>>
        custom_indexes_;

    /// @brief Metadata tracking active indexes per collection.
    std::unordered_map<std::string, std::unordered_set<std::string>> indexed_fields_;

    /// @brief In-memory cache for user credentials and roles.
    std::unordered_map<std::string, UserRole> auth_cache_;

    // --- Internal Logic ---

    std::string hash_key(const std::string& key);
    void load_all();
    void rebuild_index(const std::string& collection, cJSON* array);
    void update_custom_index(const std::string& coll, cJSON* doc, bool add);
    cJSON* get_collection(const std::string& name);
    bool validate(const std::string& coll, cJSON* data);
    bool compact_collection(const std::string& coll);
};

} // namespace aevum::storage
