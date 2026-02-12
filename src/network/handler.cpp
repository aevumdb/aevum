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
 * @file handler.cpp
 * @brief Implementation of the central command processing pipeline.
 *
 * @details
 * This file implements the `Handler::process` method, acting as the primary
 * dispatcher for the database server. It orchestrates the entire request lifecycle:
 * 1. **Ingest**: Parsing raw JSON from the socket.
 * 2. **Secure**: Authenticating credentials and authorizing RBAC policies.
 * 3. **Execute**: Routing commands to the storage engine.
 * 4. **Respond**: Formatting execution results into standardized JSON responses.
 */

#include "aevum/network/handler.hpp"

#include <cJSON.h>
#include <cstring>
#include <iostream>

namespace aevum::network {

/**
 * @brief Processes a raw client request and generates a JSON response.
 *
 * This method implements a strict Request-Response pipeline:
 *
 * @param db A reference to the active `Db` storage engine instance.
 * @param raw_json The raw JSON payload received from the network layer.
 * @return std::string A serialized JSON string representing the operation result.
 */
std::string Handler::process(aevum::storage::Db& db, const std::string& raw_json)
{
    // [Safety Check] Short-circuit empty payloads to prevent parser crashes.
    if (raw_json.empty()) {
        return "{\"status\":\"error\",\"message\":\"Empty request payload\"}";
    }

    // 1. INGEST PHASE: Parse JSON
    cJSON* req = cJSON_Parse(raw_json.c_str());
    if (!req) {
        return "{\"status\":\"error\",\"message\":\"Invalid JSON syntax\"}";
    }

    // Extract opcode (action)
    cJSON* act = cJSON_GetObjectItem(req, "action");
    std::string action = (act && act->valuestring) ? act->valuestring : "";

    // 2. AUTHENTICATION PHASE
    cJSON* auth = cJSON_GetObjectItem(req, "auth");
    std::string api_key = (auth && auth->valuestring) ? auth->valuestring : "";

    // Verify API key and resolve User Role
    aevum::storage::UserRole role = db.authenticate(api_key);

    // [Security] Reject unauthorized requests immediately
    if (role == aevum::storage::UserRole::NONE) {
        cJSON_Delete(req);
        return "{\"status\":\"error\",\"message\":\"Unauthorized: Invalid or missing API Key\"}";
    }

    // 3. AUTHORIZATION PHASE
    // Special handling for 'create_user' which requires specific ADMIN checks later
    if (action != "create_user" && !db.has_permission(role, action)) {
        cJSON_Delete(req);
        return "{\"status\":\"error\",\"message\":\"Forbidden: Insufficient RBAC privileges\"}";
    }

    // Special Command: Session Termination
    if (action == "exit") {
        cJSON_Delete(req);
        return "{\"status\":\"goodbye\",\"message\":\"Closing connection\"}";
    }

    // Extract common arguments
    cJSON* col = cJSON_GetObjectItem(req, "collection");
    std::string collection = (col && col->valuestring) ? col->valuestring : "";

    // Prepare Response Object
    cJSON* resp_root = cJSON_CreateObject();
    bool success = false;
    std::string msg = "";

    // 4. COMMAND DISPATCH PHASE
    if (action == "create_user") {
        // Enforce Granular Security: Only ADMIN can provision users
        if (role != aevum::storage::UserRole::ADMIN) {
            msg = "Forbidden: User provisioning requires ADMIN role";
        } else {
            cJSON* u_key = cJSON_GetObjectItem(req, "key");
            cJSON* u_role = cJSON_GetObjectItem(req, "role");
            if (u_key && u_role) {
                if (db.create_user(u_key->valuestring, u_role->valuestring)) {
                    success = true;
                    msg = "User created successfully";
                } else {
                    msg = "Failed to persist user";
                }
            } else {
                msg = "Missing required arguments: 'key' or 'role'";
            }
        }
    } else if (action == "insert") {
        cJSON* data = cJSON_GetObjectItem(req, "data");
        if (data) {
            if (db.insert(collection, data)) {
                success = true;
                msg = "Document inserted";
            } else {
                msg = "Insert failed (Schema violation or I/O error)";
            }
        } else {
            msg = "Missing payload: 'data'";
        }
    } else if (action == "upsert") {
        cJSON* q = cJSON_GetObjectItem(req, "query");
        cJSON* data = cJSON_GetObjectItem(req, "data");
        if (q && data) {
            if (db.upsert(collection, q, data)) {
                success = true;
                msg = "Document upserted";
            } else {
                msg = "Upsert failed";
            }
        } else {
            msg = "Missing arguments: 'query' or 'data'";
        }
    } else if (action == "find") {
        cJSON* q = cJSON_GetObjectItem(req, "query");
        cJSON* sort = cJSON_GetObjectItem(req, "sort");
        cJSON* proj = cJSON_GetObjectItem(req, "projection");
        cJSON* limit = cJSON_GetObjectItem(req, "limit");
        cJSON* skip = cJSON_GetObjectItem(req, "skip");

        int l = limit ? limit->valueint : 0;
        int s = skip ? skip->valueint : 0;

        // Execute Query
        cJSON* res_array = db.find(collection, q, sort, proj, l, s);

        // Ownership Transfer: 'res_array' becomes child of 'resp_root'
        cJSON_AddItemToObject(resp_root, "data", res_array);
        success = true;
    } else if (action == "count") {
        cJSON* q = cJSON_GetObjectItem(req, "query");
        int count = db.count(collection, q);
        cJSON_AddNumberToObject(resp_root, "count", count);
        success = true;
    } else if (action == "update") {
        cJSON* q = cJSON_GetObjectItem(req, "query");
        cJSON* u = cJSON_GetObjectItem(req, "update");
        if (q && u) {
            if (db.update(collection, q, u)) {
                success = true;
                msg = "Update committed";
            } else {
                msg = "Update failed (Collection not found or I/O error)";
            }
        } else {
            msg = "Missing arguments: 'query' or 'update'";
        }
    } else if (action == "delete") {
        cJSON* q = cJSON_GetObjectItem(req, "query");
        if (q) {
            if (db.remove(collection, q)) {
                success = true;
                msg = "Documents deleted";
            } else {
                msg = "No documents matched or collection not found";
            }
        } else {
            msg = "Missing argument: 'query'";
        }
    } else if (action == "set_schema") {
        if (role == aevum::storage::UserRole::ADMIN) {
            cJSON* schema = cJSON_GetObjectItem(req, "schema");
            if (schema) {
                if (db.set_schema(collection, schema)) {
                    success = true;
                    msg = "Schema applied";
                } else {
                    msg = "Failed to persist schema";
                }
            } else {
                msg = "Missing argument: 'schema'";
            }
        } else {
            msg = "Forbidden: Only ADMIN can modify schemas";
        }
    } else if (action == "create_index") {
        if (role == aevum::storage::UserRole::ADMIN) {
            cJSON* field = cJSON_GetObjectItem(req, "field");
            if (field && field->valuestring) {
                if (db.create_index(collection, field->valuestring)) {
                    success = true;
                    msg = "Index created and backfilled";
                } else {
                    msg = "Index creation failed";
                }
            } else {
                msg = "Missing argument: 'field'";
            }
        } else {
            msg = "Forbidden: Only ADMIN can manage indexes";
        }
    } else if (action == "compact") {
        if (role == aevum::storage::UserRole::ADMIN) {
            if (db.trigger_compaction(collection)) {
                success = true;
                msg = "Compaction completed";
            } else {
                msg = "Compaction failed";
            }
        } else {
            msg = "Forbidden: Maintenance commands are ADMIN-only";
        }
    } else {
        msg = "Unknown action opcode: " + action;
    }

    // 5. RESPONSE CONSTRUCTION PHASE
    cJSON_AddStringToObject(resp_root, "status", success ? "ok" : "error");
    if (!msg.empty()) {
        cJSON_AddStringToObject(resp_root, "message", msg.c_str());
    }

    // Serialize to Compact JSON String
    char* raw_output = cJSON_PrintUnformatted(resp_root);
    std::string final_response = std::string(raw_output);

    // Memory Cleanup
    free(raw_output);
    cJSON_Delete(resp_root);
    cJSON_Delete(req);

    return final_response;
}

} // namespace aevum::network
