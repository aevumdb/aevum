// Copyright (c) 2026 Ananda Firmansyah.
// Licensed under the AEVUMDB COMMUNITY LICENSE, Version 1.0. See LICENSE file in the root
// directory.

/**
 * @file repl.cpp
 * @brief Implements the Read-Eval-Print Loop (REPL) engine for the AevumDB interactive shell.
 * @details This file provides the concrete implementation of the interactive shell's main loop.
 * it orchestrates the process of obtaining user input, performing initial sanitization,
 * handling built-in shell directives, and delegating complex database commands to the
 * specialized command parser.
 */
#include "aevum/shell/repl/repl.hpp"

#include <iostream>
#include <string>

#include "aevum/shell/parser/command_parser.hpp"
#include "aevum/util/string/trim.hpp"

namespace aevum::shell::repl {

/**
 * @brief Emits a comprehensive, structured help manual to the standard output.
 * @details This function provides the primary user documentation for the shell interface. It
 * outlines the canonical syntax for all supported database operations (CRUD, schema management,
 * and user administration) and provides clarity on how to interact with collections and the
 * root database object.
 */
void print_help() {
    std::cout
        << "AevumDB Interactive Shell - Command Reference\n"
        << "--------------------------------------------------------------------------------\n"
        << "General Syntax: db.<collection>.<operation>(<arguments>)\n\n"
        << "Available Data Operations:\n"
        << "  db.<coll>.insert({ <document_json> })           - Ingests a new document.\n"
        << "  db.<coll>.find({ <query> }, { <sort> }, <limit>, <skip>)\n"
        << "                                                  - Queries and retrieves data.\n"
        << "  db.<coll>.update({ <query> }, { <update> })     - Modifies matching documents.\n"
        << "  db.<coll>.count({ <query> })                    - Returns document count.\n"
        << "  db.<coll>.delete({ <query> })                   - Erases matching documents.\n\n"
        << "Administrative & Schema Operations:\n"
        << "  db.<coll>.set_schema({ <schema_json> })         - Defines validation rules.\n"
        << "  db.create_user(\"<key>\", \"<role>\")               - Registers a new user account.\n"
        << "                                                    Roles: READ_ONLY, READ_WRITE, "
           "ADMIN\n\n"
        << "Shell Built-in Commands:\n"
        << "  help                                            - Displays this manual.\n"
        << "  clear                                           - Clears the terminal screen.\n"
        << "  exit | quit                                     - Terminates the session.\n"
        << "--------------------------------------------------------------------------------\n";
}

/**
 * @brief Executes the interactive Read-Eval-Print Loop.
 * @details This function implements the persistent operational state of the shell. It manages
 * the terminal interface by:
 * 1.  Providing a consistent prompt (`> `) to the user.
 * 2.  Capturing full lines of input, including handling the End-Of-File (EOF) signal (Ctrl+D)
 *     for a graceful exit.
 * 3.  Performing high-level string sanitization (trimming) to ensure robust command matching.
 * 4.  Evaluating built-in shell commands locally to maximize responsiveness.
 * 5.  Forwarding all collection-level and administrative database commands to the `parser`
 *     module for in-depth analysis and execution via the provided `AevumClient`.
 *
 * @param client A reference to the initialized `AevumClient` which maintains the active
 *        network session with the AevumDB daemon.
 */
void run(client::AevumClient &client) {
    std::string line;
    while (true) {
        std::cout << "> ";
        if (!std::getline(std::cin, line)) {
            // If getline fails (e.g., due to EOF/Ctrl+D), we perform a clean break.
            std::cout << std::endl;
            break;
        }

        // Perform basic sanitization by stripping leading and trailing whitespace.
        auto trimmed_line_view = aevum::util::string::trim(line);
        if (trimmed_line_view.empty()) {
            continue;  // Silently ignore empty or purely whitespace inputs.
        }

        std::string trimmed_line(trimmed_line_view);

        // Process high-level built-in shell control commands.
        if (trimmed_line == "exit" || trimmed_line == "quit") {
            break;
        }
        if (trimmed_line == "help") {
            print_help();
            continue;
        }
        if (trimmed_line == "clear") {
            std::cout << "\033[H\033[2J";
            continue;
        }

        // Delegate database-specific operations to the dedicated command parser.
        // This keeps the REPL logic clean and focused on user interaction.
        parser::process_command(trimmed_line, client);
    }
}

}  // namespace aevum::shell::repl
