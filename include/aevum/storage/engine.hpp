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
 * @file engine.hpp
 * @brief Low-level disk persistence layer.
 *
 * @details
 * This header declares the `Engine` class, which abstracts the physical filesystem interactions.
 * It implements an Append-Only Log (AOL) storage strategy, prioritizing write throughput
 * and crash durability over random access read performance.
 */

#pragma once

#include <string>
#include <vector>

namespace aevum::storage {

/**
 * @class Engine
 * @brief Manages physical data durability using an Append-Only Log (AOL) strategy.
 *
 * @details
 * Instead of modifying data files in-place (which requires random I/O and complex locking),
 * the Engine appends every write operation to the end of a log file.
 *
 * **Storage Characteristics:**
 * 1. **Sequential Write Throughput:** appends are O(1) and leverage filesystem buffering
 * efficiently.
 * 2. **Durability:** Data is persisted to disk before the in-memory state is updated.
 * 3. **Crash Recovery:** The database state is reconstructed deterministically by replaying the log
 * from the beginning.
 */
class Engine {
  public:
    /**
     * @brief Configures the storage subsystem.
     *
     * @param base_path The root filesystem directory where collection log files (`.aev`)
     * will be persisted.
     */
    explicit Engine(std::string base_path);

    /**
     * @brief Bootstraps the storage environment.
     *
     * Checks for the existence of the base storage directory. If absent,
     * attempts to create it with the appropriate read/write permissions.
     * Throws a runtime error if the filesystem is inaccessible.
     */
    void init();

    /**
     * @brief Replays the historical log for a specific collection.
     *
     * Used during the startup phase ("Warm-up") to reconstruct the in-memory
     * state (indexes and data stores) from the disk artifacts.
     *
     * @param collection The identifier of the collection to load.
     * @return std::vector<std::string> A sequential list of raw JSON documents representing the
     * transaction history.
     */
    std::vector<std::string> load_log(const std::string& collection);

    /**
     * @brief Persists a document by appending it to the collection's log.
     *
     * This is the primary durability mechanism. A successful return guarantees
     * that the data has been handed off to the OS filesystem layer.
     *
     * @param collection The target collection name.
     * @param raw_json The serialized JSON string to persist.
     * @return true If the append operation succeeded.
     * @return false If a filesystem error occurred (e.g., Disk Full, Permission Denied).
     */
    bool append(const std::string& collection, const std::string& raw_json);

    /**
     * @brief Performs Log Compaction (Garbage Collection).
     *
     * Because the engine uses an append-only strategy, updated and deleted records
     * remain in the file as "tombstones" or obsolete versions. This function:
     * 1. Creates a temporary file.
     * 2. Writes only the *currently active* snapshot of documents.
     * 3. Atomically renames the temporary file to overwrite the old log.
     *
     * @param collection The name of the collection to compact.
     * @param active_docs A snapshot of valid documents currently in memory.
     * @return true If compaction completed and the file was swapped.
     *
     * @warning This is an I/O intensive operation.
     */
    bool compact(const std::string& collection, const std::vector<std::string>& active_docs);

    /**
     * @brief Discovers existing database files on the disk.
     *
     * Scans the `base_path` for files with the `.aev` extension.
     *
     * @return std::vector<std::string> A list of available collection names.
     */
    std::vector<std::string> list_collections();

  private:
    /// @brief The configured root directory for persistence.
    std::string base_path_;

    /**
     * @brief Resolves a collection name to its absolute filesystem path.
     *
     * Example: `get_path("users")` -> `/var/lib/aevum/users.aev`
     *
     * @param collection The logical collection name.
     * @return std::string The full file path.
     */
    std::string get_path(const std::string& collection);
};

} // namespace aevum::storage
