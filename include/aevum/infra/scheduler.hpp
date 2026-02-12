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
 * @file scheduler.hpp
 * @brief Thread pool implementation for asynchronous task orchestration.
 *
 * @details
 * This header defines the `Scheduler` class, a high-throughput implementation of the
 * Producer-Consumer pattern. It serves as the concurrency backbone for AevumDB,
 * managing the lifecycle of worker threads and distributing background tasks (such as
 * compaction, flushing, or non-blocking I/O) to avoid stalling the main thread.
 */

#pragma once

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace aevum::infra {

/**
 * @class Scheduler
 * @brief A thread-safe worker pool for executing tasks asynchronously.
 *
 * @details
 * The Scheduler maintains a fixed cohort of worker threads and a FIFO task queue.
 * By reusing threads rather than creating them ad-hoc, AevumDB amortizes the OS-level
 * overhead of thread creation/destruction, thereby reducing latency for short-lived tasks.
 *
 * **Concurrency Model:**
 * - **Producers:** Any thread can `enqueue()` a task.
 * - **Consumers:** Worker threads sleep on a condition variable until work is available.
 */
class Scheduler {
  public:
    /**
     * @brief Initializes the thread pool and spawns worker threads.
     *
     * The constructor immediately launches the specified number of threads.
     * Each thread enters an event loop, waiting for tasks via the condition variable.
     *
     * @param threads The number of worker threads to spawn.
     * Defaults to `std::thread::hardware_concurrency()` (logical cores).
     * If the hardware concurrency cannot be detected (returns 0), the implementation
     * typically falls back to a sensible default (e.g., 2 or 4).
     */
    explicit Scheduler(size_t threads = std::thread::hardware_concurrency());

    /**
     * @brief Destructor. Initiates a graceful shutdown of the pool.
     *
     * Sets the internal stop flag to `true`, broadcasts a wake-up signal to all
     * sleeping workers, and performs a `join()` on every thread.
     *
     * @note This is a **blocking** operation. It ensures all currently running tasks
     * complete and resources are released before the object is destroyed.
     */
    ~Scheduler();

    /**
     * @brief Submits a task for asynchronous execution.
     *
     * This method accepts a callable unit of work, wraps it in a standard function
     * signature, and pushes it to the thread-safe queue. It then signals one
     * available worker to pick up the task.
     *
     * @param task A `std::function<void()>` representing the operation to execute.
     *
     * @code
     * // Example Usage:
     * scheduler.enqueue([]() {
     * aevum::infra::Logger::log(LogLevel::INFO, "Background task started.");
     * // ... heavy computation ...
     * });
     * @endcode
     */
    void enqueue(std::function<void()> task);

  private:
    /// @brief The container of active worker threads managed by this pool.
    std::vector<std::thread> workers_;

    /// @brief A FIFO queue storing pending tasks waiting for a worker.
    std::queue<std::function<void()>> tasks_;

    /// @brief Synchronization primitive protecting access to the `tasks_` queue.
    std::mutex queue_mutex_;

    /// @brief Signaling mechanism used to wake up workers or notify shutdown.
    std::condition_variable condition_;

    /// @brief Atomic flag controlling the lifecycle of the event loops.
    std::atomic<bool> stop_;
};

} // namespace aevum::infra
