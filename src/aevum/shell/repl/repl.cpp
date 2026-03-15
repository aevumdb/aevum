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

#include <cstdlib>
#include <iostream>
#include <string>

#include "aevum/shell/parser/command_parser.hpp"
#include "aevum/util/string/trim.hpp"
#include "linenoise.h"

namespace aevum::shell::repl {

/**
 * @brief Emits a comprehensive, structured help manual to the standard output.
 * @details This function provides the primary user documentation for the shell interface. It
 * outlines the canonical syntax for all supported database operations (CRUD, schema management,
 * and user administration) and provides clarity on how to interact with collections and the
 * root database object.
 */
void print_help() {
    std::cout << "\nShell Commands:\n"
              << "  help                          Display this help message\n"
              << "  clear                         Clear the terminal screen\n"
              << "  exit | quit                   Exit the AevumDB shell\n\n"
              << "Data Operations:\n"
              << "  db.<coll>.find(<query>)       Find documents matching the query\n"
              << "  db.<coll>.insert(<doc>)       Insert a new document into the collection\n"
              << "  db.<coll>.update(<q>, <u>)    Update documents matching the query\n"
              << "  db.<coll>.delete(<query>)     Delete documents matching the query\n"
              << "  db.<coll>.count(<query>)      Count documents matching the query\n\n"
              << "Administrative:\n"
              << "  db.<coll>.set_schema(<json>)  Set validation schema for a collection\n"
              << "  db.create_user(u, r)          Create a database user with a role\n"
              << "                                Roles: ADMIN, READ_WRITE, READ_ONLY\n\n"
              << "Documentation: https://github.com/aevumdb/aevum\n\n";
}

/**
 * @brief Executes the interactive Read-Eval-Print Loop.
 * @details This function implements the persistent operational state of the shell. It manages
 * the terminal interface using the linenoise library for enhanced UX.
 *
 * @param client A reference to the initialized `AevumClient` which maintains the active
 *        network session with the AevumDB daemon.
 */
void run(client::AevumClient &client) {
    const char *history_file = ".aevum_history";
    std::string history_path;

    // Resolve the full path for the history file in the user's home directory.
    const char *home = std::getenv("HOME");
    if (home) {
        history_path = std::string(home) + "/" + history_file;
        linenoiseHistoryLoad(history_path.c_str());
    }

    char *line;
    while ((line = linenoise("> ")) != nullptr) {
        if (line[0] != '\0') {
            // Perform basic sanitization by stripping leading and trailing whitespace.
            auto trimmed_line_view = aevum::util::string::trim(line);
            if (trimmed_line_view.empty()) {
                free(line);
                continue;
            }

            std::string trimmed_line(trimmed_line_view);

            // Add non-empty commands to history.
            linenoiseHistoryAdd(line);
            if (!history_path.empty()) {
                linenoiseHistorySave(history_path.c_str());
            }

            // Process high-level built-in shell control commands.
            if (trimmed_line == "exit" || trimmed_line == "quit") {
                free(line);
                break;
            }
            if (trimmed_line == "help") {
                print_help();
                free(line);
                continue;
            }
            if (trimmed_line == "clear") {
                linenoiseClearScreen();
                free(line);
                continue;
            }

            // Delegate database-specific operations to the dedicated command parser.
            parser::process_command(trimmed_line, client);
        }
        free(line);
    }
}

}  // namespace aevum::shell::repl
