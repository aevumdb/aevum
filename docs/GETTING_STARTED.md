# Getting Started with AevumDB

Get AevumDB running in 5 minutes and start using it immediately!

## Prerequisites

- **Linux** (Ubuntu 20.04+, Fedora 33+, or other Linux distributions)
- **Git**
- **Build tools**: CMake 3.21+, C++17 compiler (g++), Make
- **WiredTiger dependencies**: Python 3, pkg-config

## Installation

### 1. Clone and Build (2 minutes)

```bash
# Clone the repository
git clone https://github.com/aevumdb/aevum.git
cd aevum

# Make scripts executable
chmod +x ./scripts/*.sh ./scripts/*/*.sh

# Build AevumDB
./scripts/build.sh
```

Binaries will be at:
- `build/bin/aevumdb` - Database server
- `build/bin/aevumsh` - Interactive shell

### 2. Start the Server (30 seconds)

In one terminal:

```bash
# Start the daemon
./build/bin/aevumdb
```

You should see:
```
[INFO] Initializing AevumDB core with data directory: './data'
[INFO] Starting AevumDB server on 127.0.0.1:55001
```

### 3. Connect with Shell (1 minute)

In another terminal:

```bash
# Connect to the database
./build/bin/aevumsh
```

You should see:
```
Connected to AevumDB at 127.0.0.1:55001
Type 'help' for command reference.
> 
```

### 4. Create Your First Database (30 seconds)

Try these commands:

```bash
# Insert a document
> db.users.insert({name: "Alice", age: 30})
Success: Document inserted with _id: "550e8400-e29b-41d4-a716-446655440000".

# Find the document
> db.users.find({})
Found 1 document(s).
  {"_id":"550e8400-e29b-41d4-a716-446655440000","name":"Alice","age":30}

# Update it
> db.users.update({name: "Alice"}, {age: 31})
Success: 1 document(s) updated.

# Count documents
> db.users.count({})
1

# Delete it
> db.users.delete({age: {$gt: 30}})
Success: 1 document(s) removed.

# Get help
> help

# Clear screen
> clear

# Exit
> exit
```

## Next Steps

### Learn More
- **[Shell Commands](SHELL_REFERENCE.md)** - Complete command reference
- **[Architecture](ARCHITECTURE.md)** - How AevumDB works
- **[Building](BUILDING.md)** - Build from source with options
- **[Development](DEVELOPMENT.md)** - Set up for development

### Deploy to Production
- **[Deployment](DEPLOYMENT.md)** - Run in production
- **[Troubleshooting](TROUBLESHOOTING.md)** - Fix common issues

### Use the C++ API
- **[API Reference](API.md)** - Programmatic C++ API

## Troubleshooting

### Port already in use
If port 55001 is already taken:
```bash
# Kill existing process
lsof -ti:55001 | xargs kill -9

# Or start server on different port (edit code)
```

### Build fails
```bash
# Clean and rebuild
rm -rf build
mkdir build && cd build
cmake .. && make
```

### Shell won't connect
```bash
# Check if server is running
ps aux | grep aevumdb

# Start server if needed
./build/bin/aevumdb
```

## Common Commands

Once connected to the shell:

```bash
# View all commands
help

# Clear screen
clear

# Insert data
db.myCollection.insert({field1: "value1", field2: 42})

# Query data
db.myCollection.find({field1: "value1"})
db.myCollection.find({field2: {$gt: 40}})

# Update data
db.myCollection.update({field1: "value1"}, {field2: 99})

# Count
db.myCollection.count({})

# Delete
db.myCollection.delete({field1: "value1"})

# Define schema
db.myCollection.set_schema({type: "object", properties: {field1: {type: "string"}}})

# Create user
db.create_user("newuser", "READ_WRITE")
```

## Performance Tips

1. Start simple - use default configuration first
2. Add indexes for frequently queried fields
3. Use specific queries instead of `find({})`
4. Check queries return expected count before delete

## Next Commands

```bash
# See extensive examples
less SHELL_REFERENCE.md

# View system architecture
less ARCHITECTURE.md

# Check for solutions to issues
less TROUBLESHOOTING.md
```

## Getting Help

- **Commands in shell**: Type `help`
- **Documentation**: See [README.md](README.md)
- **Shell commands**: [Shell Reference](SHELL_REFERENCE.md)
- **Issues**: Check [TROUBLESHOOTING.md](TROUBLESHOOTING.md)
- **GitHub**: Report at https://github.com/aevumdb/aevum/issues

## Success!

🎉 You now have AevumDB running! You can:

- ✅ Insert, find, update, delete documents
- ✅ Create users with different role levels
- ✅ Define schemas for validation
- ✅ Query with filters and operators
- ✅ Back up and restore data

Explore the [Shell Reference](SHELL_REFERENCE.md) to learn about all available operations.
