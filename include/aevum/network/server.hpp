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
 * @file server.hpp
 * @brief Multi-threaded TCP network listener and connection dispatcher.
 *
 * @details
 * This header declares the `Server` class, which acts as the network entry point
 * for the AevumDB database. It handles the low-level BSD socket operations
 * (bind, listen, accept) and orchestrates the dispatching of client connections
 * to the background worker pool (`Scheduler`).
 */

#pragma once

#include "aevum/infra/scheduler.hpp"
#include "aevum/storage/db.hpp"

#include <atomic>
#include <mutex>
#include <string>
#include <vector>

namespace aevum::network {

/**
 * @class Server
 * @brief A high-concurrency TCP server for handling database client sessions.
 *
 * @details
 * The Server implements a Thread-per-Client model (via a thread pool) to handle
 * concurrent connections.
 *
 * **Operational Workflow:**
 * 1. **Accept:** The main thread blocks on `accept()`, waiting for incoming TCP handshakes.
 * 2. **Dispatch:** Upon connection, the socket file descriptor is wrapped and submitted to the
 * `infra::Scheduler`.
 * 3. **Process:** A worker thread executes the `handle_client` routine, managing the
 * request-response loop.
 * 4. **Cleanup:** Active sockets are tracked in a registry to ensure clean termination during
 * shutdown.
 */
class Server {
  public:
    /**
     * @brief Constructs the TCP Server instance.
     *
     * @param db A reference to the core `Db` storage engine instance. This reference
     * is shared safely across all concurrent client sessions.
     * @param port The TCP port number to bind to (e.g., 5555).
     */
    Server(aevum::storage::Db& db, int port);

    /**
     * @brief Destructor. Initiates the graceful shutdown sequence.
     *
     * Implicitly calls `stop()` to close sockets and terminate worker threads
     * before the object is destroyed.
     */
    ~Server();

    /**
     * @brief Starts the main server event loop.
     *
     * This method initializes the server socket options (SO_REUSEADDR), binds to the port,
     * listens for connections, and enters an infinite loop to accept clients.
     *
     * @note This function is **blocking**. It runs until `stop()` is triggered or a
     * fatal socket error occurs.
     */
    void run();

    /**
     * @brief Signals the server to shut down.
     *
     * **Shutdown Sequence:**
     * 1. Sets the `running_` atomic flag to false.
     * 2. Closes the main server file descriptor (unblocking the `accept` call).
     * 3. Iterates through the client registry and forces closure of all active peer sockets.
     */
    void stop();

  private:
    /// @brief Reference to the backend storage engine (Shared Resource).
    aevum::storage::Db& db_;

    /// @brief The configured listening port.
    int port_;

    /// @brief File descriptor for the main listening socket.
    int server_fd_;

    /// @brief Atomic flag controlling the lifecycle of the main acceptance loop.
    std::atomic<bool> running_;

    /// @brief Thread pool for asynchronous task execution.
    aevum::infra::Scheduler scheduler_;

    /// @brief Registry of currently connected client socket file descriptors.
    std::vector<int> client_sockets_;

    /// @brief Synchronization primitive protecting the `client_sockets_` registry.
    std::mutex client_mutex_;

    /**
     * @brief The core logic for the client session loop.
     *
     * This function is the entry point for worker threads. It reads raw bytes
     * from the socket, passes them to the `Handler`, and writes the response back.
     * It persists until the client disconnects or an I/O error occurs.
     *
     * @param socket The client's socket file descriptor.
     */
    void handle_client(int socket);

    /**
     * @brief Registers a new client socket for lifecycle tracking.
     * @param socket The file descriptor to add to the registry.
     */
    void add_client(int socket);

    /**
     * @brief Deregisters a client socket upon disconnection.
     * @param socket The file descriptor to remove from the registry.
     */
    void remove_client(int socket);
};

} // namespace aevum::network
