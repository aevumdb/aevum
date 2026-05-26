// Copyright (c) 2026 Ananda Firmansyah.
// Licensed under the AEVUMDB COMMUNITY LICENSE, Version 1.0. See LICENSE file in the root
// directory.

/**
 * @file request_cache.hpp
 * @brief Request deduplication cache for idempotent request handling.
 * @details Implements a simple LRU-style cache to deduplicate identical requests received within
 * a short time window. This prevents duplicate operations from network retries.
 */
#pragma once

#include <chrono>
#include <map>
#include <mutex>
#include <string>
#include <unordered_map>

namespace aevum::util::deduplication {

/**
 * @class RequestCache
 * @brief Simple request deduplication cache for handling retried requests.
 * @details Maintains a cache of recent request hashes with their responses. If an identical
 * request is received within the cache window, the cached response is returned instead of
 * re-executing the operation.
 */
class RequestCache {
  public:
    /**
     * @brief Constructs the request cache with a specified cache window.
     * @param cache_window_ms The time window (in milliseconds) during which duplicates are cached.
     *        Default is 1000ms.
     */
    explicit RequestCache(int cache_window_ms = 1000) : cache_window_ms_(cache_window_ms) {}

    /**
     * @brief Checks if a request has been seen before and returns the cached response.
     * @param request_hash A hash or unique identifier for the request.
     * @param cached_response Output parameter to receive the cached response.
     * @return true if a valid cached entry exists, false otherwise.
     */
    bool get_cached_response(const std::string &request_hash, std::string &cached_response) {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        auto now = std::chrono::system_clock::now();

        auto it = cache_.find(request_hash);
        if (it != cache_.end()) {
            auto elapsed =
                std::chrono::duration_cast<std::chrono::milliseconds>(now - it->second.timestamp);
            if (elapsed.count() < cache_window_ms_) {
                cached_response = it->second.response;
                return true;
            } else {
                // Expired entry, remove it
                cache_.erase(it);
            }
        }
        return false;
    }

    /**
     * @brief Caches a response for a request.
     * @param request_hash A hash or unique identifier for the request.
     * @param response The response to cache.
     */
    void cache_response(const std::string &request_hash, const std::string &response) {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        CacheEntry entry;
        entry.response = response;
        entry.timestamp = std::chrono::system_clock::now();
        cache_[request_hash] = entry;
    }

    /**
     * @brief Clears expired entries from the cache.
     */
    void cleanup_expired() {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        auto now = std::chrono::system_clock::now();

        for (auto it = cache_.begin(); it != cache_.end();) {
            auto elapsed =
                std::chrono::duration_cast<std::chrono::milliseconds>(now - it->second.timestamp);
            if (elapsed.count() >= cache_window_ms_) {
                it = cache_.erase(it);
            } else {
                ++it;
            }
        }
    }

  private:
    struct CacheEntry {
        std::string response;
        std::chrono::system_clock::time_point timestamp;
    };

    std::unordered_map<std::string, CacheEntry> cache_;
    std::mutex cache_mutex_;
    int cache_window_ms_;
};

}  // namespace aevum::util::deduplication
