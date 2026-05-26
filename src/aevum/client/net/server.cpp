// Copyright (c) 2026 Ananda Firmansyah.
// Licensed under the AEVUMDB COMMUNITY LICENSE, Version 1.0. See LICENSE file in the root
// directory.

/**
 * @file server.cpp
 * @brief Implements the AevumDB multi-threaded TCP network server.
 * @details This file contains the concrete implementation of the `Server` class, including the
 * logic for initializing the listening socket, accepting client connections, and dispatching them
 * to worker threads. It also implements the core request processing pipeline, which uses `simdjson`
 * for high-performance parsing and delegates database operations to the `db::Core` engine.
 */
#include "aevum/client/net/server.hpp"

#include <algorithm>
#include <arpa/inet.h>
#include <cstring>
#include <netinet/tcp.h>
#include <stdexcept>
#include <sys/socket.h>
#include <unistd.h>

#include "aevum/bson/json/parser.hpp"
#include "aevum/bson/json/serializer.hpp"
#include "aevum/util/defer.hpp"
#include "aevum/util/log/logger.hpp"
#include "simdjson.h"

namespace aevum::net::server {

/**
 * @brief Constructs a `Server`, linking it to the database core and specifying a port.
 * @param db_core A reference to the database engine that will handle all requests.
 * @param port The port on which the server will listen for connections.
 */
Server::Server(db::Core &db_core, int port) : db_core_(db_core), port_(port) {}

/**
 * @brief Destroys the `Server`, ensuring a graceful stop.
 * @details If the server is running, this destructor invokes `stop()` to ensure all sockets
 * are closed and all threads are joined, preventing resource leaks.
 */
Server::~Server() {
    if (is_running_) {
        stop();
    }
}

/**
 * @brief Gracefully stops the server and cleans up all network resources.
 * @details This function is thread-safe. It uses an atomic `exchange` to ensure the shutdown
 * sequence is executed only once. It shuts down and closes the main server socket, then iterates
 * through all active client sockets, shutting them down and closing them to unblock any worker
 * threads waiting on `recv`. Finally, it joins all worker threads to ensure they have exited
 * cleanly.
 */
void Server::stop() {
    if (!is_running_.exchange(false)) {
        return;
    }
    aevum::util::log::Logger::info("Network: Shutdown sequence initiated...");

    if (server_socket_fd_ != -1) {
        shutdown(server_socket_fd_, SHUT_RDWR);
        close(server_socket_fd_);
        server_socket_fd_ = -1;
    }

    {
        std::lock_guard<std::mutex> lock(client_sockets_mutex_);
        aevum::util::log::Logger::debug("Network: Closing " +
                                        std::to_string(client_sockets_.size()) +
                                        " active client connections.");
        for (int client_socket : client_sockets_) {
            shutdown(client_socket, SHUT_RDWR);
            close(client_socket);
        }
        client_sockets_.clear();
    }

    aevum::util::log::Logger::debug("Network: Joining " + std::to_string(worker_threads_.size()) +
                                    " worker threads.");
    for (auto &t : worker_threads_) {
        if (t.joinable()) {
            t.join();
        }
    }
    aevum::util::log::Logger::info("Network: Server has been stopped successfully.");
}

/**
 * @brief Starts the server's main listening loop.
 * @details This function performs the full POSIX socket setup sequence:
 * 1. Creates a TCP socket.
 * 2. Sets the `SO_REUSEADDR` socket option to allow for quick server restarts.
 * 3. Binds the socket to the specified port on all available network interfaces (`INADDR_ANY`).
 * 4. Puts the socket into a listening state with a connection backlog.
 * 5. Enters a `while` loop that blocks on `accept()`, waiting for new clients. For each accepted
 *    connection, a new worker thread is spawned to handle it.
 * @throws `std::runtime_error` If any part of the socket setup fails.
 */
void Server::run() {
    server_socket_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket_fd_ < 0) {
        throw std::runtime_error("Failed to create server socket: " + std::string(strerror(errno)));
    }

    int opt = 1;
    if (setsockopt(server_socket_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        throw std::runtime_error("Failed to set socket options: " + std::string(strerror(errno)));
    }

    // Enable TCP keepalive to detect dead connections
    if (setsockopt(server_socket_fd_, SOL_SOCKET, SO_KEEPALIVE, &opt, sizeof(opt))) {
        aevum::util::log::Logger::warn("Failed to set SO_KEEPALIVE: " +
                                       std::string(strerror(errno)));
    }

    // Disable Nagle's algorithm for low latency
    if (setsockopt(server_socket_fd_, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt))) {
        aevum::util::log::Logger::warn("Failed to set TCP_NODELAY: " +
                                       std::string(strerror(errno)));
    }

    // Increase socket buffer sizes for better throughput
    int rcvbufsize = 262144;  // 256KB
    int sndbufsize = 262144;  // 256KB
    if (setsockopt(server_socket_fd_, SOL_SOCKET, SO_RCVBUF, &rcvbufsize, sizeof(rcvbufsize))) {
        aevum::util::log::Logger::warn("Failed to set SO_RCVBUF: " + std::string(strerror(errno)));
    }
    if (setsockopt(server_socket_fd_, SOL_SOCKET, SO_SNDBUF, &sndbufsize, sizeof(sndbufsize))) {
        aevum::util::log::Logger::warn("Failed to set SO_SNDBUF: " + std::string(strerror(errno)));
    }

    struct sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port_);

    if (bind(server_socket_fd_, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        throw std::runtime_error("Failed to bind server socket to port " + std::to_string(port_) +
                                 ": " + std::string(strerror(errno)));
    }

    if (listen(server_socket_fd_, 128) < 0) {
        throw std::runtime_error("Failed to listen on server socket: " +
                                 std::string(strerror(errno)));
    }

    is_running_ = true;
    aevum::util::log::Logger::info("Network: Server is listening on port " + std::to_string(port_));

    while (is_running_) {
        int client_socket = accept(server_socket_fd_, nullptr, nullptr);
        if (client_socket < 0) {
            if (is_running_)
                aevum::util::log::Logger::warn("Network: accept() call failed or was interrupted.");
            continue;
        }

        aevum::util::log::Logger::info(
            "Network: Accepted new client connection. Spawning worker thread.");
        worker_threads_.emplace_back(&Server::handle_client, this, client_socket);
    }
}

/**
 * @brief Manages the lifecycle of a single client connection.
 * @details This function runs in its own thread. It first registers the client socket in a
 * global list for tracking. It then enters a loop, blocking on `recv()` for incoming data.
 * Each received payload is processed, and the response is sent back. The loop breaks if `recv()`
 * indicates a disconnection or error. A `defer` block ensures the socket is closed and removed
 * from the list upon exiting the function, regardless of how it exits.
 * @param client_socket The file descriptor for the connected client.
 */
void Server::handle_client(int client_socket) {
    {
        std::lock_guard<std::mutex> lock(client_sockets_mutex_);
        client_sockets_.push_back(client_socket);
    }

    AEVUM_DEFER([&]() {
        aevum::util::log::Logger::info("Network: Client disconnected. Cleaning up resources.");
        close(client_socket);
        std::lock_guard<std::mutex> lock(client_sockets_mutex_);
        client_sockets_.erase(
            std::remove(client_sockets_.begin(), client_sockets_.end(), client_socket),
            client_sockets_.end());
    });

    // Maximum allowed request size: 8MB
    constexpr size_t MAX_REQUEST_SIZE = 8388608;
    char buffer[16384];

    while (is_running_) {
        std::memset(buffer, 0, sizeof(buffer));
        ssize_t bytes_read = recv(client_socket, buffer, sizeof(buffer) - 1, 0);

        if (bytes_read <= 0) break;

        // Validate request size to prevent memory exhaustion attacks
        if (bytes_read > static_cast<ssize_t>(MAX_REQUEST_SIZE)) {
            aevum::util::log::Logger::warn("Network: Received oversized request (" +
                                           std::to_string(bytes_read) +
                                           " bytes). Rejecting to prevent memory exhaustion.");
            std::string error_response = R"({"status":"error","message":"Request too large"})";
            send(client_socket, error_response.c_str(), error_response.length(), 0);
            break;
        }

        std::string_view request(buffer, bytes_read);

        if (request.find("\"action\":\"exit\"") != std::string_view::npos) {
            aevum::util::log::Logger::info(
                "Network: Client sent 'exit' command. Closing connection gracefully.");
            std::string response = R"({"status":"ok","message":"Goodbye"})";
            // Retry loop for send to handle partial writes
            size_t sent = 0;
            while (sent < response.length()) {
                ssize_t n =
                    send(client_socket, response.c_str() + sent, response.length() - sent, 0);
                if (n < 0) {
                    aevum::util::log::Logger::warn("Network: Failed to send response: " +
                                                   std::string(strerror(errno)));
                    break;
                }
                sent += n;
            }
            break;
        }

        std::string response = process_request(request);
        // Retry loop for send to handle partial writes
        size_t sent = 0;
        while (sent < response.length()) {
            ssize_t n = send(client_socket, response.c_str() + sent, response.length() - sent, 0);
            if (n < 0) {
                aevum::util::log::Logger::warn("Network: Failed to send response: " +
                                               std::string(strerror(errno)));
                break;
            }
            sent += n;
        }
    }
}

/**
 * @brief Parses, authenticates, and dispatches a client's JSON request.
 * @details This function is the core of the server's application logic.
 * 1.  It uses `simdjson` to parse the request for maximum performance.
 * 2.  It extracts the `auth` key and authenticates the user via `db_core_`.
 * 3.  If authenticated, it checks for permissions for the requested `action`.
 * 4.  It extracts the action and all relevant parameters (collection, query, data, etc.).
 * 5.  It calls the corresponding method on the `db_core_` instance.
 * 6.  It formats the result from the core (e.g., status, data, count) into a JSON response string.
 * @param request The JSON request from the client.
 * @return A JSON response string to be sent back to the client.
 */
std::string Server::process_request(std::string_view request) {
    // Health check endpoint - responds immediately without authentication
    // Useful for load balancers and monitoring systems
    if (request == R"({"action":"health"})" || request == "{\"action\":\"health\"}") {
        return R"({"status":"ok","message":"AevumDB is healthy"})";
    }

    simdjson::dom::parser parser;
    simdjson::dom::element doc;
    try {
        doc = parser.parse(request);
    } catch (const simdjson::simdjson_error &e) {
        aevum::util::log::Logger::warn("Network: Received malformed JSON request. Details: " +
                                       std::string(e.what()));
        return R"({"status":"error", "message":"Invalid JSON request"})";
    }

    std::string_view action, collection, auth_key;
    if (doc["action"].get_string().get(action) != simdjson::SUCCESS) {
        return R"({"status":"error", "message":"'action' field is missing or not a string"})";
    }

    // Health check via parsed JSON
    if (action == "health") {
        return R"({"status":"ok","message":"AevumDB is healthy"})";
    }

    if (doc["auth"].get_string().get(auth_key) != simdjson::SUCCESS) {
        return R"({"status":"error", "message":"'auth' field is missing or not a string"})";
    }

    auto role = db_core_.authenticate(auth_key);
    if (role == aevum::db::auth::UserRole::NONE) {
        aevum::util::log::Logger::warn("Network: Authentication failed for request with action '" +
                                       std::string(action) + "'.");
        return R"({"status":"error", "message":"Authentication failed"})";
    }
    aevum::util::log::Logger::debug("Network: Authenticated request for action '" +
                                    std::string(action) + "' with role " +
                                    std::string(aevum::db::auth::to_string(role)) + ".");

    (void)doc["collection"].get_string().get(collection);

    if (action == "insert") {
        aevum::bson::doc::Document bson_doc;
        if (auto status = aevum::bson::json::parse(simdjson::to_string(doc["data"]), bson_doc);
            !status.ok()) {
            return R"({"status":"error", "message":"Invalid BSON data for insert"})";
        }
        auto [status, id] = db_core_.insert(collection, std::move(bson_doc));
        return status.ok() ? R"({"status":"ok", "_id":")" + id + R"("})"
                           : R"({"status":"error", "message":")" + status.message() + R"("})";
    } else if (action == "find") {
        std::string query_json = "{}", sort_json = "{}", projection_json = "{}";
        long int limit_val = 0, skip_val = 0;

        if (doc["query"].is_object()) query_json = simdjson::to_string(doc["query"]);
        if (doc["sort"].is_object()) sort_json = simdjson::to_string(doc["sort"]);
        if (doc["projection"].is_object()) projection_json = simdjson::to_string(doc["projection"]);

        (void)doc["limit"].get_int64().get(limit_val);
        (void)doc["skip"].get_int64().get(skip_val);

        auto docs = db_core_.find(collection, query_json, sort_json, projection_json,
                                  static_cast<int64_t>(limit_val), static_cast<int64_t>(skip_val));
        std::string result_json = "[";
        for (size_t i = 0; i < docs.size(); ++i) {
            result_json += aevum::bson::json::to_string(docs[i]);
            if (i < docs.size() - 1) result_json += ",";
        }
        result_json += "]";
        return R"({"status":"ok", "data":)" + result_json + "}";
    } else if (action == "update") {
        std::string query_json = "{}", update_json = "{}";
        if (doc["query"].is_object()) query_json = simdjson::to_string(doc["query"]);
        if (doc["update"].is_object()) update_json = simdjson::to_string(doc["update"]);
        auto [status, count] = db_core_.update(collection, query_json, update_json);
        return status.ok() ? R"({"status":"ok", "updated_count":)" + std::to_string(count) + "}"
                           : R"({"status":"error", "message":")" + status.message() + R"("})";
    } else if (action == "count") {
        std::string query_json = "{}";
        if (doc["query"].is_object()) query_json = simdjson::to_string(doc["query"]);
        int count = db_core_.count(collection, query_json);
        return R"({"status":"ok", "count":)" + std::to_string(count) + "}";
    } else if (action == "delete") {
        std::string query_json = "{}";
        if (doc["query"].is_object()) query_json = simdjson::to_string(doc["query"]);
        auto [status, count] = db_core_.remove(collection, query_json);
        return status.ok() ? R"({"status":"ok", "deleted_count":)" + std::to_string(count) + "}"
                           : R"({"status":"error", "message":")" + status.message() + R"("})";
    } else if (action == "set_schema") {
        if (role != aevum::db::auth::UserRole::ADMIN) {
            aevum::util::log::Logger::warn(
                "Network: Denied 'set_schema' action due to insufficient permissions.");
            return R"({"status":"error", "message":"Permission denied"})";
        }
        aevum::bson::doc::Document schema_doc;
        if (auto status = aevum::bson::json::parse(simdjson::to_string(doc["schema"]), schema_doc);
            !status.ok()) {
            return R"({"status":"error", "message":"Invalid BSON schema"})";
        }
        auto status = db_core_.set_schema(collection, schema_doc);
        return status.ok() ? R"({"status":"ok"})"
                           : R"({"status":"error", "message":")" + status.message() + R"("})";
    } else if (action == "create_user") {
        if (role != aevum::db::auth::UserRole::ADMIN) {
            aevum::util::log::Logger::warn(
                "Network: Denied 'create_user' action due to insufficient permissions.");
            return R"({"status":"error", "message":"Permission denied"})";
        }
        std::string_view new_key;
        std::string_view new_role_str;
        if (doc["key"].get_string().get(new_key) != simdjson::SUCCESS ||
            doc["role"].get_string().get(new_role_str) != simdjson::SUCCESS) {
            return R"({"status":"error", "message":"'key' and 'role' fields are required for create_user"})";
        }
        aevum::db::auth::UserRole new_role = aevum::db::auth::UserRole::NONE;
        if (new_role_str == "ADMIN")
            new_role = aevum::db::auth::UserRole::ADMIN;
        else if (new_role_str == "READ_WRITE")
            new_role = aevum::db::auth::UserRole::READ_WRITE;
        else if (new_role_str == "READ_ONLY")
            new_role = aevum::db::auth::UserRole::READ_ONLY;

        auto status = db_core_.create_user(new_key, new_role);
        return status.ok() ? R"({"status":"ok"})"
                           : R"({"status":"error", "message":")" + status.message() + R"("})";
    }

    aevum::util::log::Logger::warn("Network: Received request with unknown action: '" +
                                   std::string(action) + "'.");
    return R"({"status":"error", "message":"Unknown action"})";
}

}  // namespace aevum::net::server
