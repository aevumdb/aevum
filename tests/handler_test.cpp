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
 * @file handler_test.cpp
 * @brief Integration tests for the AevumDB Network Handler and Command Dispatcher.
 *
 * @details
 * This file verifies the critical path between the Application Layer (Network Handler)
 * and the Storage Layer (Db Engine). It utilizes a specialized RAII-based directory
 * manager to ensure total environment isolation and deterministic test results.
 */

#include "aevum/network/handler.hpp"
#include "aevum/storage/db.hpp"
#include "framework.hpp"

#include <cJSON.h>
#include <cstdio>
#include <filesystem>
#include <string>

namespace fs = std::filesystem;

/**
 * @class TestDirManager
 * @brief RAII infrastructure for automated test workspace lifecycle.
 *
 * @details
 * Implements a "Clean Slate" policy for database integration tests:
 * - **Construction**: Purges any legacy test artifacts from the filesystem.
 * - **Destruction**: Sanitizes the environment after the test suite execution.
 */
class TestDirManager {
  public:
    /// @brief Target directory path for the volatile test database.
    const std::string db_path = "./handler_test_db";

    /**
     * @brief Constructor: Ensures the test directory is cleared before engine bootstrap.
     */
    TestDirManager()
    {
        if (fs::exists(db_path)) {
            fs::remove_all(db_path);
        }
    }

    /**
     * @brief Destructor: Reclaims disk space and removes temporary test artifacts.
     */
    ~TestDirManager()
    {
        if (fs::exists(db_path)) {
            fs::remove_all(db_path);
        }
    }
};

/**
 * @brief Singleton provider for the integration test database.
 *
 * @details
 * Orchestrates the creation of a static Db instance while ensuring the
 * `TestDirManager` handles the disk lifecycle correctly.
 *
 * @return aevum::storage::Db& A thread-safe reference to the test database kernel.
 */
aevum::storage::Db& get_handler_db()
{
    // Static order guarantees manager is constructed first and destroyed last.
    static TestDirManager manager;
    static aevum::storage::Db db(manager.db_path);
    return db;
}

/**
 * @brief Validates the "Happy Path" for a document insertion request.
 *
 * **Scenario Execution:**
 * 1. Assemble a valid JSON request with the default administrative token ("root").
 * 2. Dispatch the "insert" action targeting "test_col".
 * 3. Verify that the Application Layer returns a success status ("ok").
 */
void test_handle_insert_request()
{
    auto& db = get_handler_db();

    // Construct a protocol-compliant JSON request
    std::string req_str = "{\"auth\":\"root\", \"action\": \"insert\", \"collection\": "
                          "\"test_col\", \"data\": {\"name\": \"unit_test_entry\"}}";

    // Process request via the Handler dispatcher
    std::string resp_str = aevum::network::Handler::process(db, req_str);

    cJSON* resp = cJSON_Parse(resp_str.c_str());
    ASSERT_NE(resp, (cJSON*)nullptr);

    cJSON* status = cJSON_GetObjectItem(resp, "status");
    ASSERT_NE(status, (cJSON*)nullptr);

    // Diagnostic logging if assertion fails
    if (std::string(status->valuestring) != "ok") {
        cJSON* msg = cJSON_GetObjectItem(resp, "message");
        if (msg)
            printf("    [DEBUG] Handler Rejection: %s\n", msg->valuestring);
    }

    ASSERT_EQ(std::string(status->valuestring), std::string("ok"));
    cJSON_Delete(resp);
}

/**
 * @brief Validates robust error handling for syntactically invalid JSON.
 *
 * **Scenario Execution:**
 * 1. Dispatch a malformed JSON string (unbalanced braces/missing keys).
 * 2. Verify that the Application Layer catches the parse error gracefully.
 * 3. Ensure a standardized error response is returned to the simulated client.
 */
void test_handle_invalid_json()
{
    auto& db = get_handler_db();

    // Malformed JSON payload
    std::string req_str = "{ action : \"insert\", collection : ... ";

    std::string resp_str = aevum::network::Handler::process(db, req_str);

    cJSON* resp = cJSON_Parse(resp_str.c_str());
    ASSERT_NE(resp, (cJSON*)nullptr);

    cJSON* status = cJSON_GetObjectItem(resp, "status");
    ASSERT_NE(status, (cJSON*)nullptr);

    // Expecting "error" status for bad syntax
    ASSERT_EQ(std::string(status->valuestring), std::string("error"));
    cJSON_Delete(resp);
}
