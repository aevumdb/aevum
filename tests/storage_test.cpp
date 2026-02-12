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
 * @file storage_test.cpp
 * @brief Unit tests for storage durability, persistence, and CRUD orchestration.
 *
 * @details
 * This file validates the core durability guarantees of the AevumDB engine.
 * It ensures that the storage subsystem correctly handles:
 * 1. Physical serialization of binary logs upon document insertion.
 * 2. Deterministic state recovery through Write-Ahead Log (WAL) replay.
 * 3. Lifecycle management of volatile test environments.
 */

#include "aevum/storage/db.hpp"
#include "framework.hpp"

#include <cJSON.h>
#include <chrono>
#include <filesystem>
#include <thread>

namespace fs = std::filesystem;

/**
 * @class StorageTestManager
 * @brief RAII infrastructure for managing isolated test database environments.
 *
 * @details
 * Implements a strict "Clean Room" policy for storage integration tests:
 * - **Setup**: Purges the target directory before database bootstrap.
 * - **Teardown**: Sanitizes the filesystem upon test suite termination.
 */
class StorageTestManager {
  public:
    /// @brief Path to the temporary, volatile test directory.
    const std::string path = "./test_db_unit";

    /**
     * @brief Constructor: Ensures a fresh state before any test execution.
     */
    StorageTestManager()
    {
        if (fs::exists(path)) {
            fs::remove_all(path);
        }
    }

    /**
     * @brief Destructor: Performs automatic post-test environment cleanup.
     */
    ~StorageTestManager()
    {
        if (fs::exists(path)) {
            fs::remove_all(path);
        }
    }

    /**
     * @brief Explicitly resets the test workspace to prevent cross-test leakage.
     */
    void reset()
    {
        if (fs::exists(path)) {
            fs::remove_all(path);
        }
    }
};

// Global instance to handle automated cleanup upon process exit.
static StorageTestManager g_test_manager;

/**
 * @brief Verifies the atomicity and physical commit of the insert operation.
 *
 * Validation Criteria:
 * 1. The `insert` API returns a successful commit signal.
 * 2. The underlying binary log (`.aev`) is physically materialized on the filesystem.
 * 3. The file contains a non-zero length payload.
 */
void test_db_insert()
{
    g_test_manager.reset();

    // Block-scope used to trigger the Db destructor and ensure file flush.
    {
        aevum::storage::Db db(g_test_manager.path);

        cJSON* item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, "name", "aevum_test_unit");
        cJSON_AddNumberToObject(item, "value", 1337);

        bool res = db.insert("items", item);
        ASSERT_TRUE(res);

        cJSON_Delete(item);
    }

    // Verification: Assert physical artifact existence and binary integrity.
    ASSERT_TRUE(fs::exists(g_test_manager.path + "/items.aev"));
    ASSERT_TRUE(fs::file_size(g_test_manager.path + "/items.aev") > 0);
}

/**
 * @brief Validates the Log Replay and Recovery mechanism.
 *
 *
 *
 * Test Scenario:
 * 1. **Phase 1 (Commit)**: Persist a document to the AOL and shutdown the engine.
 * 2. **Phase 2 (Recover)**: Re-instantiate the engine and verify the in-memory state
 * is reconstructed correctly from disk logs.
 */
void test_db_persistence()
{
    g_test_manager.reset();

    // Phase 1: Persistence Simulation
    {
        aevum::storage::Db db(g_test_manager.path);
        cJSON* item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, "persistence_key", "verified_durable");

        db.insert("durability_test", item);
        cJSON_Delete(item);
    }

    // Phase 2: Warm-start and WAL Replay
    {
        aevum::storage::Db db(g_test_manager.path);

        cJSON* query = cJSON_CreateObject();
        cJSON_AddStringToObject(query, "persistence_key", "verified_durable");

        // Verify that the reconstructed memory store contains the persisted document.
        int count = db.count("durability_test", query);
        ASSERT_EQ(count, 1);

        cJSON_Delete(query);
    }
}
