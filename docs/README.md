# AevumDB Documentation

Welcome to AevumDB - a document database designed for simplicity and performance.

## Quick Start

**New to AevumDB?** Start here:

1. **[Getting Started (5 minutes)](GETTING_STARTED.md)** - Install and run immediately
2. **[Shell Reference](SHELL_REFERENCE.md)** - All available commands
3. **[Troubleshooting](TROUBLESHOOTING.md)** - Fix common issues

## All Documentation

### Quick Navigation
- **[Getting Started](GETTING_STARTED.md)** - 5-minute setup
- **[Shell Reference](SHELL_REFERENCE.md)** - Commands and examples
- **[Troubleshooting](TROUBLESHOOTING.md)** - Fix common problems

### User Guides
- **[Getting Started](GETTING_STARTED.md)** - Installation and first steps
- **[Shell Reference](SHELL_REFERENCE.md)** - Complete command documentation
- **[Troubleshooting](TROUBLESHOOTING.md)** - Solutions to common problems

### Developer Resources
- **[Development](DEVELOPMENT.md)** - Set up development environment
- **[Architecture](ARCHITECTURE.md)** - System design and internals
- **[API Reference](API.md)** - C++ client library

### Operations & Reference
- **[Building](BUILDING.md)** - Compile from source with options
- **[Deployment](DEPLOYMENT.md)** - Production setup and management
- **[Project Structure](PROJECT_STRUCTURE.md)** - Directory layout explained
- **[Scripts](SCRIPTS.md)** - Build and utility scripts
- **[Third-Party Libraries](THIRD_PARTY.md)** - External dependencies
- **[GitHub Configuration](GITHUB_CONFIG.md)** - CI/CD workflows and GitHub setup

## Quick Command Examples

```bash
# Start the server
./build/bin/aevumdb

# In another terminal, connect
./build/bin/aevumsh

# Try these commands
> db.users.insert({name: "Alice", age: 30})
> db.users.find({})
> db.users.update({name: "Alice"}, {age: 31})
> db.users.count({})
> db.users.delete({age: {$lt: 18}})
> help
> exit
```

See [Shell Reference](SHELL_REFERENCE.md) for all commands.

## I want to...

| Goal | Go To |
|------|-------|
| Get AevumDB running | [Getting Started](GETTING_STARTED.md) |
| Learn shell commands | [Shell Reference](SHELL_REFERENCE.md) |
| Fix a problem | [Troubleshooting](TROUBLESHOOTING.md) |
| Set up development | [Development](DEVELOPMENT.md) |
| Understand the system | [Architecture](ARCHITECTURE.md) |
| Use the C++ API | [API Reference](API.md) |
| Build from source | [Building](BUILDING.md) |
| Deploy to production | [Deployment](DEPLOYMENT.md) |

## Key Concepts

- **Collections** - Groups of documents
- **Documents** - JSON-like data objects with `_id` field
- **Queries** - Find documents with filters like `{age: {$gt: 25}}`
- **Schemas** - Optional validation rules per collection
- **Roles** - `READ_ONLY`, `READ_WRITE`, `ADMIN` user permissions

## Architecture Overview

AevumDB consists of:

1. **aevumdb** - Server daemon (port 55001)
   - Manages data persistence
   - Coordinates all subsystems
   - See [Architecture](ARCHITECTURE.md)

2. **aevumsh** - Interactive shell client
   - Command-line interface
   - Connects to daemon
   - See [Shell Reference](SHELL_REFERENCE.md)

3. **C++ Client Library** - For programmatic access
   - High-level API
   - See [API Reference](API.md)

## File Layout

```
docs/
├── README.md (you are here)
├── GETTING_STARTED.md
├── SHELL_REFERENCE.md
├── ARCHITECTURE.md
├── DEVELOPMENT.md
├── BUILDING.md
├── DEPLOYMENT.md
├── API.md
├── TROUBLESHOOTING.md
├── GITHUB_CONFIG.md
└── PROJECT_STRUCTURE.md
```

## Documentation by Role

### Database Users
1. [Getting Started](GETTING_STARTED.md) - Setup
2. [Shell Reference](SHELL_REFERENCE.md) - Commands
3. [Troubleshooting](TROUBLESHOOTING.md) - Fix issues

### Developers
1. [Development](DEVELOPMENT.md) - Dev setup
2. [Architecture](ARCHITECTURE.md) - How it works
3. [API Reference](API.md) - C++ client

### Operations
1. [Building](BUILDING.md) - Compile
2. [Deployment](DEPLOYMENT.md) - Production
3. [Troubleshooting](TROUBLESHOOTING.md) - Issues

## Troubleshooting

Quick links to solutions:

- **Build won't compile** - [Building Issues](BUILDING.md#common-build-issues)
- **Shell won't connect** - [Troubleshooting](TROUBLESHOOTING.md)
- **Need help with commands** - [Shell Reference](SHELL_REFERENCE.md)
- **Production deployment** - [Deployment](DEPLOYMENT.md)

## Features

- ✅ Simple document operations (insert, find, update, delete)
- ✅ Optional schema validation
- ✅ User management with role-based access
- ✅ Query filters with operators ($gt, $in, etc)
- ✅ ACID storage with WiredTiger
- ✅ C++ client library for applications
- ✅ Interactive shell for exploration

## Next Steps

👉 **Start here:** [Getting Started in 5 minutes](GETTING_STARTED.md)

---

Last updated: 2026-03-11
