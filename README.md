# AevumDB

A high-performance, embedded NoSQL database engine featuring a hybrid kernel. It combines the low-latency I/O and networking of C++17 with the memory safety and complex query logic of Rust.

[![Build Status](https://github.com/actions/workflows/ci.yml/badge.svg)](https://github.com/aevumdb/aevum/actions/workflows/ci.yml)
[![Latest Release](https://img.shields.io/github/v/release/aevumdb/aevum?sort=semver)](https://github.com/aevumdb/aevum/releases)
[![C++ Language Standard](https://img.shields.io/badge/Language-C%2B%2B17-blue)](https://isocpp.org)
[![Rust Language](https://img.shields.io/badge/Language-Rust-orange)](https://www.rust-lang.org)
[![License](https://img.shields.io/github/license/aevumdb/aevum)](LICENSE)

## Table of Contents

- [Features](#features)
- [Getting Started](#getting-started)
  - [Prerequisites](#prerequisites)
  - [Build Instructions](#build-instructions)
- [Running the Standalone Server](#running-the-standalone-server)
- [Connecting to the Server](#connecting-to-the-server)
- [C++ API Tutorial](#c-api-tutorial)
- [Testing](#testing)
- [Documentation](#documentation)
- [Contributing](#contributing)
- [License](#license)

## Features

- **JSON Document Store**: Store and query data using flexible JSON documents.
- **CRUD Operations**: Full support for `insert`, `find`, `update`, and `remove` operations.
- **In-Memory Performance**: High-speed reads and writes with data held in memory, backed by a persistent log.
- **Append-Only Persistence**: Data is saved to disk using a resilient append-only log format to prevent corruption.
- **Indexing**: Create indexes on fields to dramatically accelerate query performance.
- **Schema Validation**: Enforce data structure integrity using JSON schemas.
- **Thread-Safe**: Designed for concurrent access from multiple threads.

## Getting Started

### Prerequisites

Ensure the following tools are installed on your system:
- **Git**: For cloning the repository.
- **CMake**: Version 3.15 or higher.
- **Cargo**: The Rust package manager and compiler.
- **C++ Compiler**: A modern compiler with C++17 support (e.g., GCC, Clang, MSVC).

### Build Instructions

#### Method 1: Using the Build Script (Recommended)

The repository includes a convenience script that automates the entire build process.

```sh
# Grant execute permissions
chmod +x build.sh

# Run the script
./build.sh
```

#### Method 2: Manual CMake Build

1.  **Clone the Repository** (with submodules):
    ```sh
    git clone --recursive https://github.com/aevumdb/aevum.git
    cd aevum
    ```
2.  **Configure and Build**:
    ```sh
    mkdir build && cd build
    cmake ..
    make
    ```
The primary build artifacts will be located in the `build/` directory.

## Running the Standalone Server

The build process creates a standalone server executable. To run it, specify a data directory and a port.

```sh
# Usage: ./build/aevum_server <data-directory> <port>
./build/aevum_server ./my-data 8080
```

## Connecting to the Server

Once the server is running, you can interact with it using network tools like `telnet` or `netcat` (`nc`). The server accepts raw JSON commands, and each command must be sent as a single line.

> **Authentication**: All commands require an `auth` field containing a valid API key. On first launch, the database creates a default administrative user with the API key `root`.

### Using `telnet` (Interactive Session)

`telnet` is ideal for interactive sessions where you can issue multiple commands.

1.  **Connect to the server**:
    ```sh
    telnet localhost 8080
    ```

2.  **Issue commands**:
    Once connected, type or paste a JSON command and press Enter.

    *   **Insert a document**:
        ```json
        {"action": "insert", "collection": "users", "auth": "root", "data": {"user_id": "u-123", "plan": "premium", "credits": 100}}
        ```
        The server will respond:
        ```json
        {"status":"ok","message":"Document inserted"}
        ```

    *   **Find the document**:
        ```json
        {"action": "find", "collection": "users", "auth": "root", "query": {"plan": "premium"}}
        ```
        The server's response:
        ```json
        {"status":"ok","data":[{"_id":"...","user_id":"u-123","plan":"premium","credits":100}]}
        ```

3.  **End the session**:
    To close the connection, use the `exit` command.
    ```json
    {"action": "exit", "auth": "root"}
    ```
    The server will confirm and close the connection:
    ```json
    {"status":"goodbye","message":"Closing connection"}
    ```

### Command Reference (`netcat` Examples)

These examples use `netcat` (`nc`) for single, non-interactive commands.

#### CRUD Operations

*   **`insert`**: Add a new document.
    ```sh
    echo '{"action": "insert", "collection": "users", "auth": "root", "data": {"user_id": "u-456", "plan": "free"}}' | nc localhost 8080
    ```

*   **`find`**: Query for documents.
    ```sh
    echo '{"action": "find", "collection": "users", "auth": "root", "query": {"plan": "premium"}}' | nc localhost 8080
    ```

*   **`update`**: Modify existing documents using operators like `$set`.
    ```sh
    echo '{"action": "update", "collection": "users", "auth": "root", "query": {"user_id": "u-123"}, "update": {"$set": {"credits": 90}}}' | nc localhost 8080
    ```

*   **`upsert`**: Atomically update a document or insert it if it doesn't exist.
    ```sh
    echo '{"action": "upsert", "collection": "users", "auth": "root", "query": {"user_id": "u-789"}, "data": {"user_id": "u-789", "plan": "premium", "credits": 50}}' | nc localhost 8080
    ```

*   **`delete`**: Remove documents matching a query.
    ```sh
    echo '{"action": "delete", "collection": "users", "auth": "root", "query": {"user_id": "u-456"}}' | nc localhost 8080
    ```

*   **`count`**: Get the number of documents matching a query.
    ```sh
    echo '{"action": "count", "collection": "users", "auth": "root", "query": {}}' | nc localhost 8080
    ```

#### Administration (Admin Role Required)

*   **`create_user`**: Create a new user with a specific role (`read`, `read_write`, `admin`).
    ```sh
    echo '{"action": "create_user", "auth": "root", "key": "my_new_key", "role": "read_write"}' | nc localhost 8080
    ```

*   **`create_index`**: Create an index on a field to accelerate queries.
    ```sh
    echo '{"action": "create_index", "collection": "users", "auth": "root", "field": "plan"}' | nc localhost 8080
    ```

*   **`set_schema`**: Define a JSON schema to enforce data validation.
    ```sh
    echo '{"action": "set_schema", "collection": "users", "auth": "root", "schema": {"type": "object", "properties": {"user_id": {"type": "string"}}, "required": ["user_id"]}}' | nc localhost 8080
    ```

### Error Handling

If a command fails due to a missing key, bad syntax, or insufficient permissions, the server will return an error message.

**Example: Unauthorized Request**
```sh
echo '{"action": "find", "collection": "users", "auth": "invalid_key", "query": {}}' | nc localhost 8080
```

**Server Response:**
```json
{"status":"error","message":"Unauthorized: Invalid or missing API Key"}
```

## C++ API Tutorial

`aevum` provides a simple C++ API for embedding a powerful document database in your applications.

### 1. Initialization

Include the main header and create an instance of the `Db` class, providing a path for your data directory.

```cpp
#include "aevum/storage/db.hpp"

// The database will be stored in a directory named "my_app_data"
aevum::storage::Db db("my_app_data");
```

### 2. Inserting Documents

Documents are standard `cJSON` objects. The database manages the memory of any document passed to `insert`.

```cpp
cJSON* doc = cJSON_CreateObject();
cJSON_AddStringToObject(doc, "user_id", "u-123");
cJSON_AddStringToObject(doc, "plan", "premium");
cJSON_AddNumberToObject(doc, "credits", 100);

if (db.insert("users", doc)) {
    std::cout << "Document inserted." << std::endl;
}
```

### 3. Finding Documents

Use a `cJSON` object to specify a query. The caller is responsible for freeing the `cJSON` object returned by `find`.

```cpp
cJSON* query = cJSON_CreateObject();
cJSON_AddStringToObject(query, "plan", "premium");

cJSON* results = db.find("users", query);

// `results` is a cJSON array containing all matching documents.
if (results) {
    char* results_str = cJSON_Print(results);
    std::cout << "Query results: " << results_str << std::endl;
    free(results_str);
    cJSON_Delete(results); // Free the results array
}

cJSON_Delete(query);
```

### 4. Updating Documents

Update documents matching a query using operators like `$set`.

```cpp
cJSON* query = cJSON_CreateObject();
cJSON_AddStringToObject(query, "user_id", "u-123");

cJSON* update_op = cJSON_CreateObject();
cJSON* set_op = cJSON_CreateObject();
cJSON_AddNumberToObject(set_op, "credits", 95); // Set credits to 95
cJSON_AddItemToObject(update_op, "$set", set_op);

if (db.update("users", query, update_op)) {
    std::cout << "Document updated." << std::endl;
}

cJSON_Delete(query);
cJSON_Delete(update_op);
```

### 5. Removing Documents

Remove documents that match a query. To remove all documents in a collection, use an empty query object.

```cpp
cJSON* query = cJSON_CreateObject();
cJSON_AddStringToObject(query, "user_id", "u-123");

if (db.remove("users", query)) {
    std::cout << "Document removed." << std::endl;
}

cJSON_Delete(query);
```

### 6. Creating an Index

To speed up queries on frequently searched fields, create an index.

```cpp
// Create an index on the 'plan' field in the 'users' collection
if (db.create_index("users", "plan")) {
    std::cout << "Index on 'plan' created." << std::endl;
}
// Queries on 'plan' will now be significantly faster.
```

### 7. Schema Validation

Enforce a specific structure for documents in a collection by setting a JSON schema.

```cpp
const char* schema_str = R"({
    "type": "object",
    "properties": {
        "user_id": {"type": "string"},
        "plan": {"type": "string", "enum": ["free", "premium"]},
        "credits": {"type": "number", "minimum": 0}
    },
    "required": ["user_id", "plan"]
})";

cJSON* schema = cJSON_Parse(schema_str);
if (db.set_schema("users", schema)) {
    std::cout << "Schema for 'users' has been set." << std::endl;
}
// Insert/update operations that do not conform to this schema will now fail.
```

## Testing

AevumDB includes comprehensive tests for both its C++ and Rust components. For detailed instructions on how to run these tests, please refer to the [Testing Documentation](./docs/testing.md).

## Documentation

For more in-depth information about AevumDB's architecture, API usage, and advanced topics, please visit our main [Documentation site](./docs/index.md).

## Contributing

Contributions are welcome! Please read our [CONTRIBUTING.md](CONTRIBUTING.md) for details on our code of conduct and the process for submitting pull requests.

## License

This project is licensed under the AevumDB Community License. See the [LICENSE](LICENSE) file for details.
