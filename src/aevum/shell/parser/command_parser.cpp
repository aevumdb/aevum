// Copyright (c) 2026 Ananda Firmansyah.
// Licensed under the AEVUMDB COMMUNITY LICENSE, Version 1.0. See LICENSE file in the root
// directory.

/**
 * @file command_parser.cpp
 * @brief Implements the command parsing and execution logic for the AevumDB interactive shell.
 * @details This file contains the sophisticated logic required to deconstruct user input strings
 * into structured database commands. It uses a combination of string manipulation and regular
 * expressions to handle the varied syntax of AevumDB's shell commands, including the parsing of
 * nested JSON arguments.
 */
#include "aevum/shell/parser/command_parser.hpp"

#include <iostream>
#include <regex>
#include <string>

#include "aevum/util/string/trim.hpp"
#include "simdjson.h"

namespace aevum::shell::parser {

/**
 * @brief Internal helper to provide a default value for simdjson results.
 * @tparam T The expected value type.
 * @param result The result object from a simdjson query.
 * @param default_value The value to return if the result contains an error.
 * @return Either the value from the result or the provided default.
 */
template <typename T>
T value_or(simdjson::simdjson_result<T> result, T default_value) {
    T value;
    if (std::move(result).get(value) == simdjson::SUCCESS) {
        return value;
    }
    return default_value;
}

/**
 * @brief Scans a string to find the position of a matching closing brace or bracket.
 * @details This is a crucial helper for parsing commands that contain nested JSON objects or
 * arrays as arguments. It correctly handles nested structures by maintaining a balance counter,
 * ensuring it finds the true closing delimiter of the initial opening one.
 *
 * @param str The input string to search within.
 * @param start_pos The index from which to begin the scan, typically the position of the
 *        initial opening brace.
 * @param open_char The opening character to match (e.g., '{' or '[').
 * @param close_char The closing character to find (e.g., '}' or ']').
 * @return The index of the matching closing character.
 * @return `std::string::npos` if a balanced closing character is not found.
 */
size_t find_matching_brace(const std::string &str, size_t start_pos, char open_char,
                           char close_char) {
    int balance = 0;
    for (size_t i = start_pos; i < str.length(); ++i) {
        if (str[i] == open_char) {
            balance++;
        } else if (str[i] == close_char) {
            balance--;
            if (balance == 0) {
                return i;  // Found the final matching brace.
            }
        }
    }
    return std::string::npos;  // Unbalanced braces.
}

/**
 * @brief Parses, executes, and formats the output for a single user command.
 * @details This is the shell's primary command-and-control function. It performs a multi-stage
 * process:
 * 1.  **Special Case Handling**: It first checks for unique command formats, like `db.create_user`,
 *     and uses a dedicated regex for precise argument extraction.
 * 2.  **General Parsing**: For standard `db.collection.operation` commands, it dissects the string
 *     to isolate the collection name, operation, and raw argument string.
 * 3.  **Argument Extraction**: Based on the identified operation, it intelligently parses the
 *     argument string, using `find_matching_brace` to correctly extract JSON objects for queries,
 *     updates, and schemas.
 * 4.  **Client Dispatch**: It invokes the appropriate method on the `aevum::client::AevumClient`
 *     instance, passing the parsed arguments.
 * 5.  **Response Formatting**: It receives the raw JSON response from the client, parses it with
 *     `simdjson`, and generates a user-friendly, readable summary of the result, which is then
 *     printed to standard output. Errors are formatted and sent to standard error.
 *
 * @param line The raw, trimmed command string from the user.
 * @param client The client instance used to communicate with the AevumDB daemon.
 */
void process_command(const std::string &line, client::AevumClient &client) {
    try {
        if (line.rfind("db.create_user(", 0) == 0) {
            const std::regex user_regex(
                R"(db\.create_user\(\s*\"([a-zA-Z0-9_]+)\"\s*,\s*\"([A-Z_]+)\"\s*\))");
            std::smatch user_matches;
            if (std::regex_match(line, user_matches, user_regex) && user_matches.size() == 3) {
                std::string extra = R"("key":")" + user_matches[1].str() + R"(", "role":")" +
                                    user_matches[2].str() + R"(")";
                std::cout << client.send_request(client.build_payload("create_user", "", extra))
                          << std::endl;
            } else {
                std::cerr << "Error: Invalid format. Expected: db.create_user(\"<key>\", "
                             "\"<ADMIN|READ_WRITE|READ_ONLY>\")\n";
            }
            return;
        }

        size_t collection_end = line.find('.', 3);
        size_t op_start =
            (collection_end != std::string::npos) ? collection_end + 1 : std::string::npos;
        size_t paren_start =
            (op_start != std::string::npos) ? line.find('(', op_start) : std::string::npos;

        if (line.rfind("db.", 0) != 0 || paren_start == std::string::npos || line.back() != ')' ||
            collection_end == std::string::npos || collection_end <= 3) {
            std::cerr << "Error: Invalid command format. Type 'help' for usage.\n";
            return;
        }

        std::string collection = line.substr(3, collection_end - 3);
        std::string operation = line.substr(op_start, paren_start - op_start);
        std::string args_str(aevum::util::string::trim(
            line.substr(paren_start + 1, line.length() - paren_start - 2)));

        std::string response;
        if (operation == "insert") {
            response = client.insert(collection, args_str);
        } else if (operation == "find") {
            std::string query = "{}";
            if (!args_str.empty() && args_str[0] == '{') {
                size_t query_end = find_matching_brace(args_str, 0, '{', '}');
                if (query_end != std::string::npos) {
                    query = args_str.substr(0, query_end + 1);
                }
            }
            response = client.find(collection, query);
        } else if (operation == "update") {
            size_t query_end = find_matching_brace(args_str, 0, '{', '}');
            if (query_end == std::string::npos) {
                std::cerr << "Error: Malformed query object in update command.\n";
                return;
            }
            std::string query = args_str.substr(0, query_end + 1);

            size_t update_start = args_str.find('{', query_end + 1);
            if (update_start == std::string::npos) {
                std::cerr << "Error: Missing update object in update command.\n";
                return;
            }
            size_t update_end = find_matching_brace(args_str, update_start, '{', '}');
            if (update_end == std::string::npos) {
                std::cerr << "Error: Malformed update object in update command.\n";
                return;
            }
            std::string update = args_str.substr(update_start, update_end - update_start + 1);
            response = client.update(collection, query, update);
        } else if (operation == "count" || operation == "delete") {
            response = (operation == "count") ? client.count(collection, args_str)
                                              : client.remove(collection, args_str);
        } else if (operation == "set_schema") {
            std::string extra = R"("schema":)" + args_str;
            response = client.send_request(client.build_payload("set_schema", collection, extra));
        } else {
            std::cerr << "ERROR: Unknown operation '" << operation
                      << "'. Type 'help' for a list of commands.\n";
            return;
        }

        simdjson::dom::parser parser;
        try {
            simdjson::dom::element doc = parser.parse(response);
            std::string_view status;
            if (doc["status"].get_string().get(status) == simdjson::SUCCESS && status == "ok") {
                if (operation == "find") {
                    simdjson::dom::array arr;
                    if (doc["data"].get_array().get(arr) == simdjson::SUCCESS) {
                        std::cout << "Found " << arr.size() << " document(s).\n";
                        for (auto item : arr) {
                            std::cout << "  " << simdjson::to_string(item) << std::endl;
                        }
                    }
                } else if (operation == "insert") {
                    std::cout << "Success: Document inserted with _id: \""
                              << value_or<std::string_view>(doc["_id"].get_string(), "") << "\".\n";
                } else if (operation == "update") {
                    std::cout << "Success: "
                              << value_or<int64_t>(doc["updated_count"].get_int64(), 0)
                              << " document(s) updated.\n";
                } else if (operation == "delete") {
                    std::cout << "Success: "
                              << value_or<int64_t>(doc["deleted_count"].get_int64(), 0)
                              << " document(s) removed.\n";
                } else if (operation == "count") {
                    std::cout << value_or<int64_t>(doc["count"].get_int64(), 0) << std::endl;
                } else {
                    std::cout << "Success: Operation '" << operation << "' completed.\n";
                }
            } else {
                std::cerr << "Error: "
                          << value_or<std::string_view>(doc["message"].get_string(),
                                                        "An unknown error occurred.")
                          << std::endl;
            }
        } catch (...) {
            std::cout << response << std::endl;
        }

    } catch (const std::exception &e) {
        std::cerr << "An unexpected internal error occurred: " << e.what() << std::endl;
    }
}

}  // namespace aevum::shell::parser
