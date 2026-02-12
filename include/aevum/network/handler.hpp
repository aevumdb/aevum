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
 * @file handler.hpp
 * @brief Protocol adapter and command dispatcher for the server.
 *
 * @details
 * This header declares the `Handler` class, which functions as the **Application Layer**
 * gateway for the AevumDB server. It bridges the gap between the raw Network Transport Layer
 * (TCP/IP) and the Storage Engine by deserializing JSON payloads, validating command
 * schemas, and routing execution flow.
 */

#pragma once

#include "aevum/storage/db.hpp"

#include <string>

namespace aevum::network {

/**
 * @class Handler
 * @brief A static controller for interpreting requests and marshaling responses.
 *
 * @details
 * The Handler enforces the Separation of Concerns principle by decoupling the
 * networking logic from the database logic. Its primary responsibilities are:
 * 1. **Ingest:** Parsing raw JSON strings from the socket buffer.
 * 2. **Decode:** Extracting the operation opcode (`op`) and arguments.
 * 3. **Dispatch:** Invoking the appropriate method on the `aevum::storage::Db` instance.
 * 4. **Emit:** Serializing the execution result (or exception) into a standardized JSON response.
 */
class Handler {
  public:
    /**
     * @brief Processes a raw client request and triggers database execution.
     *
     * This function acts as the central entry point for all client interactions.
     * It expects a valid JSON object containing an `"op"` field (opcode) and
     * a `"collection"` identifier.
     *
     * @param db A reference to the active `Db` storage engine instance.
     * @param raw_json The raw request payload received from the network socket.
     *
     * @return std::string A serialized JSON string representing the operation result.
     *
     * **Response Formats:**
     * - **Success:** `{"status": "ok", "data": <result_object>}`
     * - **Error:** `{"status": "error", "message": "<error_description>"}`
     *
     * @code
     * // Example Request Payload:
     * {
     * "op": "find",
     * "collection": "users",
     * "query": { "age": { "$gt": 18 } },
     * "limit": 10
     * }
     * @endcode
     */
    static std::string process(aevum::storage::Db& db, const std::string& raw_json);
};

} // namespace aevum::network
