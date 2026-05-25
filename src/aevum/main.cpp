// Copyright (c) 2026 Ananda Firmansyah.
// Licensed under the AEVUMDB COMMUNITY LICENSE, Version 1.0. See LICENSE file in the root
// directory.

/**
 * @file main.cpp
 * @brief The primary orchestration entry point for the AevumDB daemon process.
 * @details This file implements the high-level lifecycle management of the AevumDB server. It
 * is responsible for the systematic initialization of the core database engine, the activation
 * of the multi-threaded network server, and the configuration of a robust POSIX signal-handling
 * mechanism. The architecture employed here ensures that the database maintains structural
 * integrity by intercepting termination signals and initiating a coordinated, graceful shutdown
 * sequence across all active subsystems.
 */
#include <csignal>
#include <fstream>
#include <iostream>
#include <pthread.h>
#include <stdexcept>
#include <string>
#include <thread>

#include "aevum/client/net/server.hpp"
#include "aevum/db/core/core.hpp"
#include "aevum/util/log/logger.hpp"

/**
 * @namespace aevum::daemon
 * @brief Logic and state management specific to the execution of the AevumDB background daemon.
 */
namespace aevum::daemon {

/**
 * @brief A simple helper to parse basic key-value pairs from the config file.
 */
void parse_config(const std::string &config_path, std::string &data_path, int &port) {
    std::ifstream config_file(config_path);
    if (!config_file.is_open()) {
        throw std::runtime_error("Could not open configuration file: " + config_path);
    }

    std::string line;
    while (std::getline(config_file, line)) {
        // Simple manual parsing for YAML-like keys
        if (line.find("dbPath:") != std::string::npos) {
            size_t pos = line.find("dbPath:") + 7;
            data_path = line.substr(pos);
            // Trim whitespace
            size_t start = data_path.find_first_not_of(" \t");
            size_t end = data_path.find_last_not_of(" \t");
            if (start != std::string::npos && end != std::string::npos) {
                data_path = data_path.substr(start, end - start + 1);
            } else {
                data_path.clear();
            }
        } else if (line.find("port:") != std::string::npos) {
            size_t pos = line.find("port:") + 5;
            std::string port_str = line.substr(pos);
            size_t start = port_str.find_first_not_of(" \t");
            size_t end = port_str.find_last_not_of(" \t");
            if (start != std::string::npos && end != std::string::npos) {
                port_str = port_str.substr(start, end - start + 1);
                int parsed_port = std::stoi(port_str);
                // Validate port is within valid range [1, 65535]
                if (parsed_port < 1 || parsed_port > 65535) {
                    throw std::out_of_range("Port number must be between 1 and 65535");
                }
                port = parsed_port;
            }
        }
    }
}

/**
 * @brief A global pointer to the active server instance, utilized for signal-driven management.
 * @details This static pointer facilitates the communication between the asynchronous signal
 * handling thread and the main network server instance. It allows the `signal_wait_thread` to
 * invoke the `stop()` method on the server, ensuring that the network listener and all worker
 * threads are terminated correctly upon receipt of external interruption signals.
 */
static aevum::net::server::Server *g_server_instance = nullptr;

/**
 * @brief The execution routine for the dedicated system signal monitoring thread.
 * @details This thread function employs a synchronous signal handling model using `sigwait`.
 * It masks the `SIGINT` and `SIGTERM` signals in the main execution path and delegates their
 * management to this specialized thread. Upon receiving a signal, it logs the event with high
 * technical detail and triggers the graceful shutdown of the server instance pointed to by
 * `g_server_instance`. This approach avoids the inherent risks of asynchronous signal
 * handlers (reentrancy issues) and ensures thread-safe application termination.
 */
void signal_wait_thread() {
    sigset_t signal_set;
    sigemptyset(&signal_set);
    sigaddset(&signal_set, SIGINT);   // Intercept the interrupt signal (typically Ctrl+C).
    sigaddset(&signal_set, SIGTERM);  // Intercept the software termination signal.

    aevum::util::log::Logger::info(
        "System: Synchronous signal monitoring thread has been initialized and is now active.");

    int signal_number;
    // Block until one of the signals in the set is delivered to the process.
    if (sigwait(&signal_set, &signal_number) == 0) {
        aevum::util::log::Logger::warn("System: Interruption event detected. Received signal " +
                                       std::to_string(signal_number) +
                                       ". Initiating the global graceful shutdown sequence...");
        if (g_server_instance) {
            g_server_instance->stop();
        }
    }
}

/**
 * @brief Emits the standardized command-line interface usage guide to the standard output stream.
 */
void print_help(const char *binary_name) {
    std::cout
        << "AevumDB Daemon - High-Performance Distributed Document Database Engine\n\n"
        << "Usage: " << binary_name << " [OPTIONS]\n\n"
        << "Options:\n"
        << "  --config <path> : Path to the YAML-based configuration file.\n"
        << "  --help          : Display this technical reference guide.\n\n"
        << "Positional Arguments (Legacy Mode):\n"
        << "  [DATA_PATH]     : Filesystem directory for physical storage (Default: ./aevum_data)\n"
        << "  [PORT]          : TCP/IP port for network communication (Default: 55001)\n\n"
        << "Documentation: https://github.com/aevumdb/aevum\n";
}

}  // namespace aevum::daemon

/**
 * @brief The canonical entry point of the AevumDB daemon application.
 * @details The execution sequence begins by masking terminal signals (`SIGINT`, `SIGTERM`)
 * to prevent default asynchronous interruption of the process. It then transitions into the
 * configuration phase, where it parses command-line arguments to define the operational
 * environment (storage directory and listening port).
 *
 * The core initialization phase instantiates the `aevum::db::Core` engine, which prepares
 * the storage and indexing layers. Subsequently, the `aevum::net::server::Server` is
 * initialized and linked to the database core. A dedicated signal-handling thread is then
 * spawned to monitor system-level termination requests.
 *
 * Finally, the application enters the blocking server loop via `network_server.run()`. This
 * loop persists until an external signal is received or a critical failure occurs, at which
 * point the application performs an exhaustive cleanup, joins all threads, and exits with a
 * status code reflecting the success or failure of the operation.
 *
 * @param argc The count of command-line arguments.
 * @param argv The vector of command-line argument strings.
 * @return Returns `0` if the daemon completes its lifecycle and shuts down without error.
 * @return Returns `1` if a critical exception or fatal initialization error is encountered.
 */
int main(int argc, char *argv[]) {
    // Configure the initial signal mask for the main thread and all its future descendants.
    sigset_t signal_set;
    sigemptyset(&signal_set);
    sigaddset(&signal_set, SIGINT);
    sigaddset(&signal_set, SIGTERM);
    pthread_sigmask(SIG_BLOCK, &signal_set, nullptr);

    // Evaluate if the user requested the manual.
    if (argc > 1 && std::string(argv[1]) == "--help") {
        aevum::daemon::print_help(argv[0]);
        return 0;
    }

    // Initialize global logging parameters.
    aevum::util::log::Logger::set_level(aevum::util::log::LogLevel::INFO);
    std::string data_path = "./aevum_data";
    int port = 55001;

    try {
        // Dynamically resolve configuration from command-line arguments.
        if (argc > 1) {
            std::string arg1 = argv[1];
            if (arg1 == "--config" && argc > 2) {
                // If --config is used, parse the data path and port from the config file.
                aevum::util::log::Logger::info("Daemon: Configuration provided via file: " +
                                               std::string(argv[2]));
                aevum::daemon::parse_config(argv[2], data_path, port);
            } else {
                // Otherwise, treat arguments as positional: [DATA_PATH] [PORT]
                data_path = arg1;
                if (argc > 2) {
                    int parsed_port = std::stoi(argv[2]);
                    // Validate port is within valid range [1, 65535]
                    if (parsed_port < 1 || parsed_port > 65535) {
                        throw std::out_of_range("Port number must be between 1 and 65535");
                    }
                    port = parsed_port;
                }
            }
        }

        aevum::util::log::Logger::info(
            "Daemon: Initiating AevumDB high-performance bootstrap sequence...");

        // Initialize the central database orchestration engine.
        aevum::db::Core database_instance(data_path);
        aevum::util::log::Logger::info("Core: Storage engine initialized with data path: " +
                                       data_path);

        // Configure the high-performance network server subsystem.
        aevum::net::server::Server network_server(database_instance, port);
        aevum::util::log::Logger::info("Network: Listening for incoming connections on port " +
                                       std::to_string(port));

        // Register the server instance with the global signal manager.
        aevum::daemon::g_server_instance = &network_server;

        // Activate the dedicated signal monitoring thread.
        std::thread signal_thread(aevum::daemon::signal_wait_thread);

        try {
            // Enter the main blocking loop of the network server.
            network_server.run();
        } catch (...) {
            // In the event of an unhandled exception within the server loop,
            // ensure the signal thread is detached to prevent application hang.
            if (signal_thread.joinable()) {
                signal_thread.detach();
            }
            throw;  // Re-propagate the exception for global error handling.
        }

        // Wait for the signal handling thread to conclude its shutdown duties.
        if (signal_thread.joinable()) {
            signal_thread.join();
        }

    } catch (const std::exception &e) {
        aevum::util::log::Logger::fatal("Daemon: A critical runtime exception has occurred: " +
                                        std::string(e.what()));
        return 1;
    }

    aevum::util::log::Logger::info(
        "Daemon: AevumDB daemon has successfully concluded all operations. Exiting gracefully.");
    return 0;
}
