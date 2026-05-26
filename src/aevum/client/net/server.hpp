// Copyright (c) 2026 Ananda Firmansyah.
// Licensed under the AEVUMDB COMMUNITY LICENSE, Version 1.0. See LICENSE file in the root
// directory.

/**
 * @file server.hpp
 * @brief Declares the `Server` class, a multi-threaded TCP network server for AevumDB.
 * @details This header defines the main server component responsible for accepting and handling
 * client connections. The `Server` class encapsulates the logic for socket binding, listening,
 * and dispatching incoming connections to a pool of worker threads for concurrent processing.
 */
#pragma once

#include <atomic>
#include <mutex>
#include <thread>
#include <vector>

#include "aevum/db/core/core.hpp"
#include "aevum/util/deduplication/request_cache.hpp"

namespace aevum::net::server {

/**
 * @class Server
 * @brief Manages the full lifecycle of the AevumDB network server, handling client connections
 * and dispatching requests to the core database engine.
 *
 * @details This class implements a robust, multi-threaded TCP server. Upon calling `run()`, it
 * establishes a listening socket on a specified port. It then enters a loop, accepting new client
 * connections and spawning a dedicated `std::thread` for each one. This architecture allows for
 * concurrent handling of multiple clients.
 *
 * The server ensures graceful shutdown via the `stop()` method, which closes all active sockets and
 * joins all worker threads. Thread-safe management of client connections is handled using a
 * `std::mutex`.
 */
class Server {
  public:
    /**
     * @brief Constructs a `Server` instance, linking it to the database core.
     * @param db_core A reference to the active `aevum::db::Core` engine, which will be used to
     *        process all authenticated client requests.
     * @param port The TCP port number on which the server will listen for incoming connections.
     */
    Server(db::Core &db_core, int port);

    /**
     * @brief Destroys the `Server` object, ensuring a graceful shutdown if it is still running.
     */
    ~Server();

    // The Server is non-copyable and non-movable to ensure a single instance controls the
    // network listener and associated resources.
    Server(const Server &) = delete;
    Server &operator=(const Server &) = delete;
    Server(Server &&) = delete;
    Server &operator=(Server &&) = delete;

    /**
     * @brief Starts the server's main execution loop.
     * @details This method initializes and binds the listening socket, then enters a blocking
     * loop to `accept()` new client connections. Each accepted connection is immediately
     * dispatched to a new worker thread running `handle_client`.
     * @throws `std::runtime_error` if socket creation, option setting, binding, or listening fails.
     */
    void run();

    /**
     * @brief Initiates a graceful shutdown of the server.
     * @details This method is thread-safe. It sets an atomic flag to terminate the `run()` loop,
     * forcefully shuts down the main listening socket to unblock the `accept()` call, closes all
     * active client sockets, and waits for all worker threads to complete execution by joining
     * them.
     */
    void stop();

  private:
    /**
     * @brief The dedicated function for handling all communication with a single connected client.
     * @details This method runs in a separate thread for each client. It enters a loop, blocking on
     * `recv()` to wait for requests. Upon receiving data, it dispatches it to `process_request` and
     * sends the returned response back to the client. The loop terminates if the client disconnects
     * or sends an "exit" command.
     * @param client_socket The file descriptor for the connected client's socket.
     */
    void handle_client(int client_socket);

    /**
     * @brief The central request processing and dispatching logic.
     * @details This function takes a raw request string from a client, parses it as JSON using
     * `simdjson` for high performance, authenticates the request using the `db_core_`, checks
     * permissions, and then calls the appropriate `db_core_` method (e.g., `insert`, `find`).
     * Finally, it serializes the result of the database operation into a JSON response string.
     * @param request The raw JSON request data received from the client.
     * @return A `std::string` containing the JSON-formatted response to be sent to the client.
     */
    std::string process_request(std::string_view request);

    /**
     * @brief Configuration structure for connection pool settings.
     * @details Manages connection pooling limits, idle timeouts, and per-IP rate limiting.
     */
    struct ConnectionPoolConfig {
        int max_connections_total{1000};      ///< Total concurrent connections allowed
        int max_connections_per_ip{100};      ///< Max connections from a single IP
        int max_idle_timeout_sec{300};        ///< Connection idle timeout in seconds
        int max_request_size_bytes{8388608};  ///< Max request size (8MB)
        int request_timeout_sec{30};          ///< Request processing timeout
    } conn_config_;

    /**
     * @brief Metrics collection structure for monitoring and observability.
     * @details Tracks database and network operation metrics for Prometheus integration.
     */
    struct MetricsCollector {
        std::atomic<uint64_t> total_requests{0};
        std::atomic<uint64_t> total_errors{0};
        std::atomic<uint64_t> total_bytes_received{0};
        std::atomic<uint64_t> total_bytes_sent{0};
        std::atomic<int> active_connections{0};
        uint64_t startup_timestamp{0};
    } metrics_;

    /// A reference to the central database engine instance.
    db::Core &db_core_;
    /// The TCP port number on which the server listens.
    int port_;
    /// The file descriptor for the main server listening socket.
    int server_socket_fd_{-1};
    /// An atomic flag used to signal the server's main loop and worker threads to terminate.
    std::atomic<bool> is_running_{false};

    /// A vector to manage the `std::thread` objects for all active client connections.
    std::vector<std::thread> worker_threads_;
    /// A vector to store the file descriptors of all active client sockets, for cleanup during
    /// shutdown.
    std::vector<int> client_sockets_;
    /// A mutex to provide thread-safe access to the `client_sockets_` vector.
    std::mutex client_sockets_mutex_;
    /// Request deduplication cache to handle retried requests idempotently
    aevum::util::deduplication::RequestCache request_cache_{1000};
};

}  // namespace aevum::net::server
