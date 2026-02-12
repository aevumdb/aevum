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
 * @file server.cpp
 * @brief Implementation of the multi-threaded TCP server logic.
 *
 * @details
 * This file implements the network listener loop, client connection management,
 * and the delegation of request processing to the worker scheduler. It handles
 * the raw BSD socket API calls.
 */

#include "aevum/network/server.hpp"

#include "aevum/infra/logger.hpp"
#include "aevum/network/handler.hpp"

#include <algorithm>
#include <arpa/inet.h>
#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

namespace aevum::network {

/**
 * @brief Constructs the Server instance.
 * @param db Reference to the storage database engine.
 * @param port Port number to bind.
 */
Server::Server(aevum::storage::Db& db, int port)
    : db_(db), port_(port), server_fd_(-1), running_(false)
{
}

/**
 * @brief Destructor. Ensures clean shutdown of resources.
 */
Server::~Server()
{
    stop();
}

/**
 * @brief Gracefully terminates the server.
 *
 * 1. Sets the running flag to false to stop new accepts.
 * 2. Closes the main listener socket to unblock the `accept()` call in the main thread.
 * 3. Iterates through all active client sockets and forces them closed to release workers.
 */
void Server::stop()
{
    if (!running_)
        return;
    running_ = false;

    infra::Logger::log(infra::LogLevel::INFO,
                       "Network: Shutdown signal received. Stopping server...");

    // 1. Terminate the main listener socket
    if (server_fd_ >= 0) {
        shutdown(server_fd_, SHUT_RDWR);
        close(server_fd_);
        server_fd_ = -1;
    }

    // 2. Forcefully close all active client sessions to unblock worker threads
    {
        std::lock_guard<std::mutex> lock(client_mutex_);
        for (int sock : client_sockets_) {
            shutdown(sock, SHUT_RDWR);
            close(sock);
        }
        client_sockets_.clear();
    }
}

/**
 * @brief Main Server Event Loop.
 *
 * Initializes the socket, binds to the port, and enters the accept loop.
 * This function blocks until `stop()` is called.
 */
void Server::run()
{
    // Create an IPv4 TCP stream socket
    if ((server_fd_ = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        infra::Logger::log(infra::LogLevel::FATAL, "Network: Failed to create socket.");
        return;
    }

    // Allow immediate address/port reuse to facilitate quick restarts
    int opt = 1;
    if (setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        infra::Logger::log(infra::LogLevel::ERROR, "Network: setsockopt failed.");
        return;
    }

    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port_);

    // Bind to the specified port
    if (bind(server_fd_, (struct sockaddr*)&address, sizeof(address)) < 0) {
        infra::Logger::log(infra::LogLevel::FATAL,
                           "Network: Failed to bind to port " + std::to_string(port_));
        return;
    }

    // Start listening (Backlog of 128 pending connections is standard for high-throughput)
    if (listen(server_fd_, 128) < 0) {
        infra::Logger::log(infra::LogLevel::FATAL, "Network: Failed to listen.");
        return;
    }

    running_ = true;
    infra::Logger::log(infra::LogLevel::INFO,
                       "Network: AevumDB listening on port " + std::to_string(port_));

    // Accept Loop: The Delegator
    while (running_) {
        struct sockaddr_in client_addr;
        socklen_t len = sizeof(client_addr);

        // Block here until a client connects
        int sock = accept(server_fd_, (struct sockaddr*)&client_addr, &len);

        if (sock >= 0) {
            if (!running_) {
                // If shutdown occurred while blocked on accept
                close(sock);
                break;
            }

            char* client_ip_raw = inet_ntoa(client_addr.sin_addr);
            std::string client_ip = client_ip_raw ? client_ip_raw : "unknown";

            infra::Logger::log(infra::LogLevel::INFO, "Network: New connection from " + client_ip);

            // 1. Register socket for tracking
            add_client(sock);

            // 2. Dispatch handling to the worker pool (Non-blocking for main thread)
            // Note: We pass 'sock' by value to the lambda to ensure it's captured correctly.
            scheduler_.enqueue([this, sock]() { this->handle_client(sock); });

        } else {
            // sock < 0 indicates error or interrupt
            if (running_) {
                infra::Logger::log(infra::LogLevel::ERROR, "Network: Accept failed (Error code: " +
                                                               std::to_string(errno) + ")");
            } else {
                // Intentional shutdown
                break;
            }
        }
    }

    infra::Logger::log(infra::LogLevel::INFO, "Network: Server event loop terminated.");
}

/**
 * @brief Client Handler Routine (Worker Thread Context).
 *
 * Processes requests for a single client connection until disconnection.
 * Reads raw bytes, parses them via Handler, and writes the response.
 */
void Server::handle_client(int sock)
{
    // 8KB buffer for incoming requests (Sufficient for typical JSON payloads)
    char buffer[8192];

    while (running_) {
        std::memset(buffer, 0, sizeof(buffer));

        // Blocking Read
        ssize_t read_len = recv(sock, buffer, sizeof(buffer), 0);

        if (read_len > 0) {
            // 1. Process request (Blocking operation, potentially I/O heavy)
            std::string request_str(buffer, read_len);
            std::string resp = Handler::process(db_, request_str);

            // 2. Send response
            send(sock, resp.c_str(), resp.length(), 0);

            // 3. Check for protocol-level disconnect command ("goodbye")
            // This allows the client to gracefully close the session via JSON.
            if (resp.find("\"status\":\"goodbye\"") != std::string::npos) {
                infra::Logger::log(infra::LogLevel::INFO,
                                   "Network: Client requested disconnect via protocol.");
                break;
            }

        } else if (read_len == 0) {
            // Zero bytes read means the peer closed the connection gracefully (FIN packet).
            infra::Logger::log(infra::LogLevel::INFO, "Network: Client disconnected cleanly.");
            break;
        } else {
            // Negative value indicates socket error.
            infra::Logger::log(infra::LogLevel::DEBUG, "Network: Socket read error or timeout.");
            break;
        }
    }

    // Cleanup: Deregister and close socket
    remove_client(sock);
}

/**
 * @brief Thread-safe client registration.
 */
void Server::add_client(int sock)
{
    std::lock_guard<std::mutex> lock(client_mutex_);
    client_sockets_.push_back(sock);
}

/**
 * @brief Thread-safe client removal and resource cleanup.
 */
void Server::remove_client(int sock)
{
    std::lock_guard<std::mutex> lock(client_mutex_);
    auto it = std::find(client_sockets_.begin(), client_sockets_.end(), sock);
    if (it != client_sockets_.end()) {
        // Only close if still in the list (avoids double-close)
        close(sock);
        client_sockets_.erase(it);
    }
}

} // namespace aevum::network
