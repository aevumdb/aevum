# AevumDB Shell Command Reference

The AevumDB interactive shell (`aevumsh`) allows you to interact with an AevumDB server directly. All commands follow this syntax:

```
db.<collection>.<operation>(<arguments>)
```

## Quick Command Examples

```bash
# Connect to database
$ aevumsh

# Insert a document
> db.users.insert({name: "Alice", age: 30})

# Find documents
> db.users.find({})
> db.users.find({age: {$gt: 25}})

# Update documents
> db.users.update({name: "Alice"}, {age: 31})

# Count documents
> db.users.count({})

# Delete documents
> db.users.delete({age: {$lt: 18}})

# Define schema
> db.users.set_schema({type: "object", properties: {name: {type: "string"}}})

# Create user
> db.create_user("newuser", "READ_WRITE")

# Help
> help

# Exit
> exit
```

## Shell Features (v1.2.2+)

### Command History
AevumDB shell now supports persistent command history. 
- Use **Up/Down arrow keys** to navigate previous commands.
- History is saved automatically to `~/.aevum_history` and persists across sessions.

### Line Editing
Advanced line editing is supported via the `linenoise` library:
- **Left/Right arrows**: Move the cursor within the current line.
- **Ctrl-A / Ctrl-E**: Jump to the beginning or end of the line.
- **Ctrl-K**: Delete from cursor to the end of the line.
- **Ctrl-U**: Delete the entire line.
- **Ctrl-L**: Clear the terminal screen (same as `clear` command).

## Data Operations

### insert

Insert a new document into a collection.

**Syntax:**
```
db.<collection>.insert({ <document> })
```

**Parameters:**
- `<document>`: A JSON object representing the document to insert. The document will automatically get a `_id` field if not provided.

**Examples:**
```bash
> db.users.insert({name: "Alice", age: 30})
Success: Document inserted with _id: "550e8400-e29b-41d4-a716-446655440000".

> db.products.insert({title: "Laptop", price: 999.99, stock: 5})
Success: Document inserted with _id: "...".

> db.orders.insert({userId: 1, items: [{productId: 1, qty: 2}], total: 1998})
Success: Document inserted with _id: "...".
```

### find

Query and retrieve documents matching criteria.

**Syntax:**
```
db.<collection>.find({ <query> })
```

**Parameters:**
- `<query>`: A JSON object representing the query filter. Use `{}` to match all documents.

**Query Operators:**
- `$gt` - Greater than
- `$gte` - Greater than or equal
- `$lt` - Less than
- `$lte` - Less than or equal
- `$eq` - Equal to
- `$ne` - Not equal
- `$in` - Match any value in array
- `$nin` - Not in array

**Examples:**
```bash
# Find all documents
> db.users.find({})
Found 3 document(s).
  {"_id":"...", "name":"Alice", "age":30}
  {"_id":"...", "name":"Bob", "age":25}
  {"_id":"...", "name":"Charlie", "age":35}

# Find with comparison
> db.users.find({age: {$gt: 25}})
Found 2 document(s).
  {"_id":"...", "name":"Alice", "age":30}
  {"_id":"...", "name":"Charlie", "age":35}

# Find with equality
> db.users.find({name: "Alice"})
Found 1 document(s).
  {"_id":"...", "name":"Alice", "age":30}

# Find with $in operator
> db.users.find({age: {$in: [25, 30]}})
Found 2 document(s).
  {"_id":"...", "name":"Alice", "age":30}
  {"_id":"...", "name":"Bob", "age":25}
```

### update

Modify one or more existing documents.

**Syntax:**
```
db.<collection>.update({ <query> }, { <update> })
```

**Parameters:**
- `<query>`: JSON object to find documents to update
- `<update>`: JSON object with new field values

**Examples:**
```bash
# Update single document
> db.users.update({name: "Alice"}, {age: 31})
Success: 1 document(s) updated.

# Update multiple documents
> db.users.update({age: {$lt: 30}}, {status: "junior"})
Success: 2 document(s) updated.

# Update nested fields
> db.orders.update({_id: "order123"}, {status: "shipped"})
Success: 1 document(s) updated.
```

### count

Count the number of documents matching a query.

**Syntax:**
```
db.<collection>.count({ <query> })
```

**Parameters:**
- `<query>`: JSON object filter (use `{}` to count all)

**Examples:**
```bash
# Count all documents
> db.users.count({})
3

# Count with filter
> db.users.count({age: {$gt: 25}})
2

# Count with condition
> db.orders.count({status: "completed"})
15
```

### delete

Remove documents matching a query.

**Syntax:**
```
db.<collection>.delete({ <query> })
```

**Parameters:**
- `<query>`: JSON object to identify documents to delete

**Examples:**
```bash
# Delete specific document
> db.users.delete({name: "Charlie"})
Success: 1 document(s) removed.

# Delete with condition
> db.users.delete({age: {$lt: 18}})
Success: 2 document(s) removed.

# Delete all from collection
> db.users.delete({})
Success: 3 document(s) removed.
```

## Schema Operations

### set_schema

Define a schema for a collection to enforce document structure.

**Syntax:**
```
db.<collection>.set_schema({ <schema> })
```

**Parameters:**
- `<schema>`: A JSON Schema object defining validation rules

**Schema Structure:**
```json
{
  "type": "object",
  "properties": {
    "fieldName": {
      "type": "string|number|boolean|array|object",
      "description": "Field description"
    }
  },
  "required": ["fieldName1", "fieldName2"]
}
```

**Examples:**
```bash
# Simple schema
> db.users.set_schema({type: "object", properties: {name: {type: "string"}, age: {type: "number"}}})
Success: Operation 'set_schema' completed.

# Schema with required fields
> db.products.set_schema({type: "object", properties: {title: {type: "string"}, price: {type: "number"}}, required: ["title", "price"]})
Success: Operation 'set_schema' completed.

# Complex schema with validation
> db.orders.set_schema({type: "object", properties: {userId: {type: "string"}, items: {type: "array"}, total: {type: "number"}}, required: ["userId", "items", "total"]})
Success: Operation 'set_schema' completed.
```

## User and Security Operations

### create_user

Create a new user account with specified role.

**Syntax:**
```
db.create_user("<api_key>", "<role>")
```

**Parameters:**
- `<api_key>`: Username/key for the user (alphanumeric and underscore only)
- `<role>`: One of: `READ_ONLY`, `READ_WRITE`, `ADMIN`

**Role Permissions:**
- `READ_ONLY`: Can execute find and count operations
- `READ_WRITE`: Can execute find, count, insert, update, delete, and set_schema
- `ADMIN`: Full access to all operations including user management

**Examples:**
```bash
# Create read-only user
> db.create_user("analyst", "READ_ONLY")
Success: Operation 'create_user' completed.

# Create read-write user
> db.create_user("editor", "READ_WRITE")
Success: Operation 'create_user' completed.

# Create admin user
> db.create_user("admin2", "ADMIN")
Success: Operation 'create_user' completed.
```

## Built-in Commands

### help

Display the command reference in the shell.

```bash
> help

Shell Commands:
  help                          Display this help message
  clear                         Clear the terminal screen
  exit | quit                   Exit the AevumDB shell

Data Operations:
  db.<coll>.find(<query>)       Find documents matching the query
  db.<coll>.insert(<doc>)       Insert a new document into the collection
  db.<coll>.update(<q>, <u>)    Update documents matching the query
  db.<coll>.delete(<query>)     Delete documents matching the query
  db.<coll>.count(<query>)      Count documents matching the query

Administrative:
  db.<coll>.set_schema(<json>)  Set validation schema for a collection
  db.create_user(u, r)          Create a database user with a role
                                Roles: ADMIN, READ_WRITE, READ_ONLY

Documentation: https://github.com/aevumdb/aevum
```

### clear

Clears the terminal screen.

```bash
> clear
```

### exit / quit

Terminate the shell session.

```bash
> exit
```

Or:

```bash
> quit
```

## Connection Options

When starting the shell, you can specify connection parameters:

```bash
# Default (localhost:55001, user: root)
$ aevumsh

# Specify host and port
$ aevumsh 192.168.1.100 55001

# Specify all parameters (host, port, api_key)
$ aevumsh 192.168.1.100 55001 customuser

# Show help
$ aevumsh --help
```

## Error Handling

When a command fails, the shell displays an error message:

```bash
> db.users.find({age: "invalid"})
Error: Invalid query format. Expected JSON object.

> db.nonexistent.find({})
Error: Collection 'nonexistent' not found.
```

## Tips and Tricks

### Complex Queries

```bash
# Multiple conditions (AND)
> db.users.find({age: {$gte: 25}, status: "active"})

# Using $nin (not in)
> db.users.find({status: {$nin: ["deleted", "suspended"]}})
```

### Batch Operations

```bash
# Delete multiple with condition
> db.logs.delete({timestamp: {$lt: 1234567890}})

# Update multiple fields
> db.users.update({age: {$lt: 18}}, {status: "minor"})
```

### Checking Schema

Since there's no explicit "get_schema" command, create documents and let validation tell you the schema:

```bash
# If schema validation fails, it reveals what's required:
> db.users.insert({name: "Test"})
Error: Document validation failed: 'age' is required.
```

## Common Use Cases

### User Management

```bash
# Create a standard user
> db.create_user("john_doe", "READ_WRITE")

# Create multiple users for different roles
> db.create_user("readonly_analyst", "READ_ONLY")
> db.create_user("data_manager", "READ_WRITE")
> db.create_user("system_admin", "ADMIN")
```

### Data Validation

```bash
# Set schema before inserting
> db.users.set_schema({type: "object", properties: {email: {type: "string"}, verified: {type: "boolean"}}, required: ["email"]})

# Now inserts must comply
> db.users.insert({email: "alice@example.com", verified: true})
```

### Finding and Updating Data

```bash
# Find all users older than 30
> db.users.find({age: {$gt: 30}})

# Update their status
> db.users.update({age: {$gt: 30}}, {tier: "premium"})

# Verify the changes
> db.users.find({tier: "premium"})
```

## Performance Tips

1. **Use specific queries**: `db.users.find({status: "active"})` is faster than `db.users.find({})`
2. **Count before delete**: `db.users.count({...})` to verify before deletion
3. **Update strategically**: Update only necessary fields to reduce data transfer
4. **Regular cleanup**: Delete old/archived records to maintain performance

## Limitations

- Shell commands are synchronous (no async/await)
- Large result sets will print all documents at once
- No joining between collections (normalize data or embed related documents)
- No transactions in shell (use C++ client for multi-operation transactions)

## See Also

- [Getting Started](GETTING_STARTED.md) - Quick start guide
- [Architecture](ARCHITECTURE.md) - How AevumDB works internally
- [API Reference](API.md) - C++ Client API for programmatic access
- [Troubleshooting](TROUBLESHOOTING.md) - Common issues and solutions
- [Project Structure](PROJECT_STRUCTURE.md) - Understanding the codebase
