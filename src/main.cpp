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
 * @file main.cpp
 * @brief Application Entry Point (Bootstrap).
 *
 * @details
 * This file contains the `main` function which orchestrates the startup sequence:
 * 1. Argument Parsing.
 * 2. Signal Handling Registration (SIGINT/SIGTERM).
 * 3. Subsystem Initialization (Storage & Network).
 * 4. Main Event Loop Execution.
 */

#include "aevum/infra/logger.hpp"
#include "aevum/network/server.hpp"
#include "aevum/storage/db.hpp"

#include <csignal>
#include <iostream>
#include <stdexcept>
#include <string>

/// @brief Global pointer to the active server instance (Used by ISR/Signal Handler).
/// @warning Access strictly controlled via atomic signal flags in production.
static aevum::network::Server* g_server = nullptr;

/**
 * @brief System Signal Handler (Interrupt Service Routine).
 *
 * Catches OS signals (Ctrl+C, Kill) to perform a graceful shutdown
 * instead of an abrupt process termination.
 *
 * @param signum The signal identifier (e.g., SIGINT).
 */
void signal_handler(int signum)
{
    aevum::infra::Logger::log(aevum::infra::LogLevel::WARN,
                              "System: Interrupt received (Signal " + std::to_string(signum) +
                                  "). Initiating graceful shutdown...");

    if (g_server) {
        g_server->stop();
    }
}

/**
 * @brief Prints usage instructions to stdout.
 */
void print_help(const char* binary_name)
{
    std::cout << "Usage: " << binary_name << " [DATA_PATH] [PORT]\n"
              << "Options:\n"
              << "  DATA_PATH   Directory to store database files (Default: ./aevum_data)\n"
              << "  PORT        TCP port to listen on (Default: 5555)\n"
              << "  --help      Show this help message\n";
}

/**
 * @brief Main Execution Entry Point.
 */
int main(int argc, char* argv[])
{
    // 0. Argument Pre-check
    if (argc > 1 && std::string(argv[1]) == "--help") {
        print_help(argv[0]);
        return 0;
    }

    // 1. Configuration Defaults
    std::string data_path = "./aevum_data";
    int port = 5555;

    // 2. Register Signal Handlers
    std::signal(SIGINT, signal_handler);  // Ctrl+C
    std::signal(SIGTERM, signal_handler); // Docker Stop / Kill

    try {
        // 3. Parse Command Line Arguments
        if (argc > 1)
            data_path = argv[1];
        if (argc > 2)
            port = std::stoi(argv[2]);

        // 4. System Bootstrap & Logging
        aevum::infra::Logger::log(aevum::infra::LogLevel::INFO,
                                  "System: Booting AevumDB Kernel v1.1.1 (RC1)...");
        aevum::infra::Logger::log(aevum::infra::LogLevel::INFO,
                                  "Config: Persistence Path set to '" + data_path + "'");
        aevum::infra::Logger::log(aevum::infra::LogLevel::INFO,
                                  "Config: Network Interface binding to port " +
                                      std::to_string(port));

        // 5. Initialize Storage Subsystem (Loads logs, builds indexes)
        aevum::storage::Db db(data_path);

        // 6. Initialize Network Subsystem (Binds sockets)
        aevum::network::Server server(db, port);

        // Assign global pointer for signal handling
        g_server = &server;

        // 7. Enter Main Execution Loop (Blocking)
        // This will return only when stop() is called via signal handler.
        server.run();

    } catch (const std::exception& e) {
        aevum::infra::Logger::log(aevum::infra::LogLevel::FATAL,
                                  "System: Critical Failure: " + std::string(e.what()));
        return 1;
    } catch (...) {
        aevum::infra::Logger::log(aevum::infra::LogLevel::FATAL,
                                  "System: Unknown unhandled exception occurred.");
        return 1;
    }

    aevum::infra::Logger::log(aevum::infra::LogLevel::INFO,
                              "System: Shutdown complete. Goodnight.");
    return 0;
}
