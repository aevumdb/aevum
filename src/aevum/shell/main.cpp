// Copyright (c) 2026 Ananda Firmansyah.
// Licensed under the AEVUMDB COMMUNITY LICENSE, Version 1.0. See LICENSE file in the root
// directory.

/**
 * @file main.cpp
 * @brief Implements the primary entry point for the AevumDB interactive shell client.
 * @details This file contains the `main` function that initializes the client, parses command-line
 * arguments, establishes a connection to an AevumDB daemon, and launches the interactive
 * Read-Eval-Print Loop (REPL) for direct user interaction with the database.
 */
#include <iostream>
#include <stdexcept>
#include <string>

#include "aevum/client/aevum_client.hpp"
#include "aevum/shell/repl/repl.hpp"

/**
 * @brief The main entry point for the AevumDB interactive shell.
 * @details This function orchestrates the entire client application lifecycle. It begins by
 * parsing command-line arguments, allowing the user to specify the server's host, port, and an
 * API key for authentication. Sensible defaults are provided for ease of use.
 *
 * It then instantiates the `AevumClient`, attempts to connect to the specified AevumDB daemon,
 * and provides clear feedback to the user regarding the connection status. If the connection is
 * successful, it transfers control to the `aevum::shell::repl::run` function, which manages the
 * interactive session. Upon the user's exit from the REPL, it ensures a graceful disconnection
 * before terminating.
 *
 * @param argc The count of command-line arguments supplied to the program.
 * @param argv A pointer to an array of C-style strings representing the command-line arguments.
 * @return Returns `0` on successful execution and graceful shutdown.
 * @return Returns `1` if command-line arguments are invalid or if a connection to the
 *         database server cannot be established.
 */
int main(int argc, char *argv[]) {
    std::string host = "127.0.0.1";
    int port = 55001;
    std::string api_key = "root";

    // Provide a helpful usage message if requested via command-line flags.
    if (argc > 1 && (std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h")) {
        std::cout << "AevumDB Shell Client\n\n"
                  << "Usage: " << argv[0] << " [host] [port] [api_key]\n\n"
                  << "Arguments:\n"
                  << "  host    : AevumDB daemon hostname or IP (default: 127.0.0.1)\n"
                  << "  port    : Port number of the daemon (default: 55001)\n"
                  << "  api_key : API key for authentication (default: 'root')\n\n"
                  << "Documentation: https://github.com/aevumdb/aevum\n";
        return 0;
    }

    // Safely parse command-line arguments with robust error handling.
    try {
        if (argc > 1) host = argv[1];
        if (argc > 2) {
            int parsed_port = std::stoi(argv[2]);
            // Validate port is within valid range [1, 65535]
            if (parsed_port < 1 || parsed_port > 65535) {
                std::cerr << "Error: Port number must be between 1 and 65535." << std::endl;
                return 1;
            }
            port = parsed_port;
        }
        if (argc > 3) api_key = argv[3];
    } catch (const std::invalid_argument &e) {
        std::cerr << "Error: Invalid number format for port. " << e.what() << std::endl;
        return 1;
    } catch (const std::out_of_range &e) {
        std::cerr << "Error: Port number out of range. " << e.what() << std::endl;
        return 1;
    }

    // Initialize the high-level database client.
    aevum::client::AevumClient client(host, port, api_key);

    std::cout << "Connecting to AevumDB at " << host << ":" << port << "..." << std::endl;
    if (!client.connect()) {
        std::cerr << "Failed to connect to the AevumDB daemon. Is it running?\n"
                  << "Please ensure the AevumDB daemon is started and accessible." << std::endl;
        return 1;
    }
    std::cout << "Connection successful. Type 'help' for a list of commands." << std::endl;

    // Enter the main interactive loop.
    aevum::shell::repl::run(client);

    // Gracefully disconnect upon exiting the REPL.
    client.disconnect();
    std::cout << "Disconnected. Goodbye!" << std::endl;
    return 0;
}
