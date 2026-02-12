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
 * @file scheduler.cpp
 * @brief Implementation of the asynchronous task orchestration logic.
 *
 * @details
 * This file contains the concrete implementation of the `Scheduler` class.
 * It manages the lifecycle of a fixed worker thread pool, implementing the
 * producer-consumer synchronization pattern using condition variables to
 * ensure optimal CPU utilization and graceful system teardown.
 */

#include "aevum/infra/scheduler.hpp"

namespace aevum::infra {

/**
 * @brief Constructs the scheduler and initializes the worker cohort.
 *
 * @details
 * The constructor spawns a specified number of threads. Each thread enters a
 * dormant state ("Event Loop"), waiting efficiently for tasks without consuming
 * CPU cycles until signaled.
 *
 * @param threads The number of persistent worker threads to initialize.
 */
Scheduler::Scheduler(size_t threads) : stop_(false)
{
    for (size_t i = 0; i < threads; ++i) {
        // Instantiate and register worker threads into the cohort.
        workers_.emplace_back([this] {
            /* * ============================================================
             * Worker Thread Event Loop
             * ============================================================
             */
            while (true) {
                std::function<void()> task;

                // --- Critical Section: Task Acquisition ---
                {
                    // Synchronize access to the shared task repository.
                    std::unique_lock<std::mutex> lock(this->queue_mutex_);

                    /* * Block the thread until a task is enqueued or a shutdown
                     * signal is received. This prevents busy-waiting.
                     */
                    this->condition_.wait(lock,
                                          [this] { return this->stop_ || !this->tasks_.empty(); });

                    /* * Termination Logic:
                     * Exit the loop only if the scheduler is stopping AND all
                     * pending tasks have been drained. This guarantees that
                     * background jobs like WAL flushing are completed.
                     */
                    if (this->stop_ && this->tasks_.empty()) {
                        return;
                    }

                    // Ownership Transfer: Move the task to the local context.
                    task = std::move(this->tasks_.front());
                    this->tasks_.pop();
                }
                // --- End Critical Section: Lock released automatically ---

                /* * Execution:
                 * Task execution occurs outside the mutex lock to maximize
                 * concurrency, allowing other workers to acquire new tasks
                 * while this thread is busy processing.
                 */
                if (task) {
                    task();
                }
            }
        });
    }
}

/**
 * @brief Destructor. Orchestrates a graceful pool teardown.
 *
 * @details
 * Ensures that all worker threads are notified of the shutdown and allowed to
 * finish any remaining work in the queue before the object is destroyed.
 */
Scheduler::~Scheduler()
{
    {
        // Atomically update the state to prevent new tasks from being accepted.
        std::unique_lock<std::mutex> lock(queue_mutex_);
        stop_ = true;
    }

    // Broadcast the shutdown signal to all dormant worker threads.
    condition_.notify_all();

    // Synchronization: Block until every worker thread has returned.
    for (std::thread& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

/**
 * @brief Dispatches a new task to the worker pool.
 *
 * @details
 * Acts as the "Producer". It appends the callable object to the shared queue
 * and wakes up a single worker thread.
 *
 * @param task The callable unit of work (lambda, function, or functor).
 */
void Scheduler::enqueue(std::function<void()> task)
{
    {
        // Acquire lock to safely modify the FIFO task queue.
        std::unique_lock<std::mutex> lock(queue_mutex_);

        // Emplace the task using move semantics for zero-copy efficiency.
        tasks_.emplace(std::move(task));
    }

    /* * Wake up exactly one worker. notify_one() is used to mitigate the
     * "Thundering Herd" problem, ensuring only the necessary resources
     * are mobilized.
     */
    condition_.notify_one();
}

} // namespace aevum::infra
