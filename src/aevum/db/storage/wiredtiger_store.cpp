// Copyright (c) 2026 Ananda Firmansyah.
// Licensed under the AEVUMDB COMMUNITY LICENSE, Version 1.0. See LICENSE file in the root
// directory.

/**
 * @file wiredtiger_store.cpp
 * @brief Implements the `WiredTigerStore` class, providing a high-level C++ interface to the
 * WiredTiger embedded database engine.
 * @details This file contains the concrete logic for all persistent storage operations. It
 * handles the initialization and shutdown of the WiredTiger connection, management of sessions
 * and cursors, and the serialization/deserialization of BSON documents for storage as key-value
 * pairs.
 */
#include "aevum/db/storage/wiredtiger_store.hpp"

#include <filesystem>
#include <string>

#include "aevum/util/defer.hpp"
#include "aevum/util/log/logger.hpp"

#ifdef HAVE_WIREDTIGER
#include <bson/bson.h>
#endif

namespace fs = std::filesystem;

namespace aevum::db::storage {

/**
 * @brief Constructs a `WiredTigerStore` instance, setting the physical location for the database.
 * @param base_path The filesystem directory where the WiredTiger database files will be stored.
 *        This path is moved into the class member.
 */
WiredTigerStore::WiredTigerStore(std::string base_path) : base_path_(std::move(base_path)) {}

/**
 * @brief Destructor for the `WiredTigerStore`.
 * @details Ensures a graceful shutdown of the database by explicitly closing the WiredTiger
 * connection if it is active. This is a critical step to prevent data corruption by ensuring
 * all cached data is flushed to disk.
 */
WiredTigerStore::~WiredTigerStore() {
#ifdef HAVE_WIREDTIGER
    if (conn_) {
        aevum::util::log::Logger::info("WiredTiger: Closing database connection.");
        conn_->close(conn_, nullptr);
        conn_ = nullptr;
    }
#endif
}

/**
 * @brief Initializes the WiredTiger storage engine.
 * @details This is the primary setup method. It first verifies that the specified `base_path_`
 * directory exists, creating it if necessary. It then invokes `wiredtiger_open` to either create
 * a new database or open an existing one at that location. Essential configuration parameters,
 * such as cache size and statistics, are provided during this call.
 *
 * @return `aevum::util::Status::OK()` if the directory is created/verified and the WiredTiger
 *         connection is successfully opened.
 * @return `aevum::util::Status::IOError` with a descriptive message if directory creation fails or
 *         if `wiredtiger_open` returns an error.
 */
aevum::util::Status WiredTigerStore::init() {
#ifdef HAVE_WIREDTIGER
    aevum::util::log::Logger::debug("WiredTiger: Initializing storage at path: " + base_path_);
    if (!fs::exists(base_path_)) {
        aevum::util::log::Logger::info(
            "WiredTiger: Database directory not found. Creating new directory at " + base_path_);
        std::error_code ec;
        if (!fs::create_directories(base_path_, ec)) {
            return aevum::util::Status::IOError("Failed to create db directory: " + ec.message());
        }
    }

    // Attempt to open the WiredTiger database with a specific configuration.
    // 'create' allows the database to be created if it doesn't exist.
    int ret = wiredtiger_open(base_path_.c_str(), nullptr,
                              "create,cache_size=128M,statistics=(fast)", &conn_);
    if (ret != 0) {
        return aevum::util::Status::IOError(std::string("WT Open failed: ") +
                                            wiredtiger_strerror(ret));
    }

    aevum::util::log::Logger::info("WiredTiger: Storage engine initialized successfully.");
    return aevum::util::Status::OK();
#else
    aevum::util::log::Logger::warn("WiredTiger support is disabled in this build.");
    return aevum::util::Status::OK();  // Proceed without persistence
#endif
}

/**
 * @brief Constructs the standard WiredTiger URI for accessing a table.
 * @param collection The logical name of the collection.
 * @return A `std::string` formatted as "table:collection_name".
 */
std::string WiredTigerStore::make_uri(std::string_view collection) const {
    return std::string("table:") + std::string(collection);
}

/**
 * @brief Ensures a table with the specified name exists within the database.
 * @details This is an idempotent operation. It attempts to create a table with the given name
 * using a default schema (`key_format=S` for string keys, `value_format=u` for raw byte values).
 * If the table already exists, WiredTiger returns `EEXIST`, which is handled as a success case.
 *
 * @param session An active `WT_SESSION` pointer.
 * @param collection The name of the collection/table to ensure exists.
 * @return `aevum::util::Status::OK()` if the table exists or was successfully created.
 * @return `aevum::util::Status::IOError` if the creation call fails with an error other than
 * `EEXIST`.
 */
aevum::util::Status WiredTigerStore::ensure_table([[maybe_unused]] WT_SESSION *session,
                                                  [[maybe_unused]] std::string_view collection) {
#ifdef HAVE_WIREDTIGER
    if (!session) return aevum::util::Status::Corruption("WT Session is null");

    std::string uri = make_uri(collection);
    // Create the table with a string key format and a raw byte array value format (for BSON).
    int ret = session->create(session, uri.c_str(), "key_format=S,value_format=u");

    // EEXIST is not an error; it simply means the table was already there.
    if (ret != 0 && ret != EEXIST) {
        aevum::util::log::Logger::error("WiredTiger: Failed to create table '" + uri +
                                        "'. Error: " + wiredtiger_strerror(ret));
        return aevum::util::Status::IOError(std::string("WT Create Table failed: ") +
                                            wiredtiger_strerror(ret));
    }

    return aevum::util::Status::OK();
#else
    return aevum::util::Status::NotSupported("WiredTiger support is disabled.");
#endif
}

/**
 * @brief Scans database metadata to retrieve a list of all user-defined collection names.
 * @details This function opens a cursor on the special `metadata:` URI in WiredTiger, which lists
 * all internal and user-defined tables. It iterates through the metadata, filtering for entries
 * that are prefixed with "table:" and extracting the collection name.
 *
 * @return A `std::vector<std::string>` containing the names of all collections.
 */
std::vector<std::string> WiredTigerStore::list_collections() {
    std::vector<std::string> collections;
#ifdef HAVE_WIREDTIGER
    if (!conn_) return collections;

    WT_SESSION *session = nullptr;
    conn_->open_session(conn_, nullptr, nullptr, &session);
    AEVUM_DEFER([&]() {
        if (session) session->close(session, nullptr);
    });

    WT_CURSOR *cursor = nullptr;
    int ret = session->open_cursor(session, "metadata:", nullptr, nullptr, &cursor);
    if (ret != 0) {
        aevum::util::log::Logger::error(
            "WiredTiger: Failed to open metadata cursor to list collections.");
        return collections;
    }

    AEVUM_DEFER([&]() {
        if (cursor) cursor->close(cursor);
    });

    const char *key;
    while ((ret = cursor->next(cursor)) == 0) {
        cursor->get_key(cursor, &key);
        std::string_view s_key(key);

        // Isolate user table URIs from other metadata entries.
        // Check length before substr to prevent out_of_range exception
        if (s_key.size() >= 6 && s_key.substr(0, 6) == "table:") {
            collections.emplace_back(s_key.substr(6));
        }
    }
#endif
    return collections;
}

/**
 * @brief Reads all key-value pairs from a collection's table and deserializes them into BSON
 * documents.
 * @details This function performs a full table scan. It opens a cursor on the specified
 * collection's table and iterates through every record. For each record, it retrieves the raw value
 * (a byte array) and uses `bson_new_from_data` to reconstruct it into a `bson_t`, which is then
 * wrapped in a `aevum::bson::doc::Document`.
 *
 * @param collection The name of the collection to load.
 * @return A `std::vector` of `aevum::bson::doc::Document`s. If the collection does not exist or is
 * empty, an empty vector is returned.
 */
std::vector<aevum::bson::doc::Document> WiredTigerStore::load_collection(
    std::string_view collection) {
    std::vector<aevum::bson::doc::Document> documents;
#ifdef HAVE_WIREDTIGER
    if (!conn_) return documents;

    WT_SESSION *session = nullptr;
    conn_->open_session(conn_, nullptr, nullptr, &session);
    AEVUM_DEFER([&]() {
        if (session) session->close(session, nullptr);
    });

    if (auto status = ensure_table(session, collection); !status.ok()) {
        aevum::util::log::Logger::error("WiredTiger: Cannot load collection '" +
                                        std::string(collection) + "'. " + status.to_string());
        return documents;
    }

    std::string uri = make_uri(collection);
    WT_CURSOR *cursor = nullptr;
    int ret = session->open_cursor(session, uri.c_str(), nullptr, nullptr, &cursor);
    if (ret != 0) {
        aevum::util::log::Logger::error("WiredTiger: Failed to open read cursor on table '" + uri +
                                        "'.");
        return documents;
    }

    AEVUM_DEFER([&]() {
        if (cursor) cursor->close(cursor);
    });

    const char *key;
    WT_ITEM value_item;
    while ((ret = cursor->next(cursor)) == 0) {
        cursor->get_key(cursor, &key);
        cursor->get_value(cursor, &value_item);

        // Reconstruct the BSON document from the raw data stored in WiredTiger.
        bson_t *b =
            bson_new_from_data(static_cast<const uint8_t *>(value_item.data), value_item.size);
        if (b) {
            documents.emplace_back(b);
        } else {
            aevum::util::log::Logger::warn(
                "WiredTiger: Failed to deserialize BSON document from collection '" +
                std::string(collection) + "'.");
        }
    }
#endif
    return documents;
}

/**
 * @brief Inserts a new record or updates an existing one (upsert) in a specified collection.
 * @details The function maps the document's `_id` to the table's key and the document's binary
 * BSON data to the table's value. It uses `cursor->insert()`, which in WiredTiger's default
 * configuration performs an upsert.
 *
 * @param collection The name of the target collection.
 * @param id The unique identifier for the document, used as the primary key.
 * @param doc The `aevum::bson::doc::Document` to be written to storage.
 * @return `aevum::util::Status::OK()` on successful write, `IOError` on failure.
 */
aevum::util::Status WiredTigerStore::put([[maybe_unused]] std::string_view collection,
                                         [[maybe_unused]] std::string_view id,
                                         [[maybe_unused]] const aevum::bson::doc::Document &doc) {
#ifdef HAVE_WIREDTIGER
    if (!conn_) return aevum::util::Status::Corruption("WT Connection is null");
    if (doc.empty() || !doc.get())
        return aevum::util::Status::InvalidArgument("Empty BSON document");

    WT_SESSION *session = nullptr;
    conn_->open_session(conn_, nullptr, nullptr, &session);
    AEVUM_DEFER([&]() {
        if (session) session->close(session, nullptr);
    });

    if (auto status = ensure_table(session, collection); !status.ok()) return status;

    std::string uri = make_uri(collection);
    WT_CURSOR *cursor = nullptr;
    int ret = session->open_cursor(session, uri.c_str(), nullptr, nullptr, &cursor);
    if (ret != 0) return aevum::util::Status::IOError("WT Open Cursor failed for '" + uri + "'.");

    AEVUM_DEFER([&]() {
        if (cursor) cursor->close(cursor);
    });

    const uint8_t *bson_data = bson_get_data(doc.get());
    WT_ITEM value_item;
    value_item.data = bson_data;
    value_item.size = doc.length();

    std::string id_str(id);
    cursor->set_key(cursor, id_str.c_str());
    cursor->set_value(cursor, &value_item);

    ret = cursor->insert(cursor);
    if (ret != 0) {
        aevum::util::log::Logger::error("WiredTiger: Insert/update failed for key '" + id_str +
                                        "' in table '" + uri +
                                        "'. Error: " + wiredtiger_strerror(ret));
        return aevum::util::Status::IOError(std::string("WT Insert failed: ") +
                                            wiredtiger_strerror(ret));
    }

    return aevum::util::Status::OK();
#else
    return aevum::util::Status::OK();  // No-op
#endif
}

/**
 * @brief Removes a record from a collection table, identified by its key (`_id`).
 * @param collection The name of the target collection.
 * @param id The unique ID of the document to be removed.
 * @return `aevum::util::Status::OK()` on success. `WT_NOTFOUND` is treated as success. Returns
 *         `IOError` if the underlying WiredTiger operation fails for other reasons.
 */
aevum::util::Status WiredTigerStore::remove([[maybe_unused]] std::string_view collection,
                                            [[maybe_unused]] std::string_view id) {
#ifdef HAVE_WIREDTIGER
    if (!conn_) return aevum::util::Status::Corruption("WT Connection is null");

    WT_SESSION *session = nullptr;
    conn_->open_session(conn_, nullptr, nullptr, &session);
    AEVUM_DEFER([&]() {
        if (session) session->close(session, nullptr);
    });

    if (auto status = ensure_table(session, collection); !status.ok()) return status;

    std::string uri = make_uri(collection);
    WT_CURSOR *cursor = nullptr;
    int ret = session->open_cursor(session, uri.c_str(), nullptr, nullptr, &cursor);
    if (ret != 0) return aevum::util::Status::IOError("WT Open Cursor failed for '" + uri + "'.");

    AEVUM_DEFER([&]() {
        if (cursor) cursor->close(cursor);
    });

    std::string id_str(id);
    cursor->set_key(cursor, id_str.c_str());

    ret = cursor->remove(cursor);
    if (ret != 0 && ret != WT_NOTFOUND) {
        aevum::util::log::Logger::error("WiredTiger: Remove failed for key '" + id_str +
                                        "' in table '" + uri +
                                        "'. Error: " + wiredtiger_strerror(ret));
        return aevum::util::Status::IOError(std::string("WT Remove failed: ") +
                                            wiredtiger_strerror(ret));
    }

    return aevum::util::Status::OK();
#else
    return aevum::util::Status::OK();  // No-op
#endif
}

/**
 * @brief Atomically replaces the entire content of a collection with a new set of documents.
 * @details This highly destructive but efficient method is used to synchronize the state of a
 * collection with an entirely new dataset (e.g., after an in-memory update operation from the
 * Rust FFI). It first forcibly drops the existing table, ignoring "not found" errors, then
 * re-creates it, and finally performs a bulk insert of all the new documents.
 *
 * @param collection The name of the collection to be synchronized.
 * @param documents The complete new dataset for the collection.
 * @return `aevum::util::Status::OK()` on success, `IOError` on failure.
 */
aevum::util::Status WiredTigerStore::sync_collection(
    [[maybe_unused]] std::string_view collection,
    [[maybe_unused]] const std::vector<std::pair<std::string, aevum::bson::doc::Document>>
        &documents) {
#ifdef HAVE_WIREDTIGER
    if (!conn_) return aevum::util::Status::Corruption("WT Connection is null");

    std::string coll_str(collection);
    aevum::util::log::Logger::debug("WiredTiger: Beginning full sync for collection '" + coll_str +
                                    "'. " + std::to_string(documents.size()) +
                                    " documents to be written.");

    WT_SESSION *session = nullptr;
    conn_->open_session(conn_, nullptr, nullptr, &session);
    AEVUM_DEFER([&]() {
        if (session) session->close(session, nullptr);
    });

    std::string uri = make_uri(collection);

    int ret = session->drop(session, uri.c_str(), "force");
    if (ret != 0 && ret != WT_NOTFOUND) {
        aevum::util::log::Logger::error("WiredTiger: Drop table failed during sync for '" + uri +
                                        "'. Error: " + wiredtiger_strerror(ret));
        return aevum::util::Status::IOError(std::string("WT Drop failed: ") +
                                            wiredtiger_strerror(ret));
    }

    if (auto status = ensure_table(session, collection); !status.ok()) return status;

    WT_CURSOR *cursor = nullptr;
    ret = session->open_cursor(session, uri.c_str(), nullptr, nullptr, &cursor);
    if (ret != 0) return aevum::util::Status::IOError("WT Open Cursor failed for '" + uri + "'.");

    AEVUM_DEFER([&]() {
        if (cursor) cursor->close(cursor);
    });

    for (const auto &[id_str, doc] : documents) {
        if (doc.empty() || !doc.get()) continue;

        WT_ITEM value_item;
        value_item.data = bson_get_data(doc.get());
        value_item.size = doc.length();

        cursor->set_key(cursor, id_str.c_str());
        cursor->set_value(cursor, &value_item);

        ret = cursor->insert(cursor);
        if (ret != 0) {
            aevum::util::log::Logger::warn("WiredTiger: Insert failed during sync for ID '" +
                                           id_str + "' in '" + uri + "'.");
        }
    }

    aevum::util::log::Logger::debug("WiredTiger: Sync completed for collection '" + coll_str +
                                    "'.");
    return aevum::util::Status::OK();
#else
    return aevum::util::Status::OK();  // No-op
#endif
}

}  // namespace aevum::db::storage
