# AevumDB

Welcome to AevumDB!

## Components

- `aevumdb` - The database server.
- `aevumsh` - The interactive shell.

## Download AevumDB

Clone and build from source:

```bash
git clone https://github.com/aevumdb/aevum.git
cd aevum

# Make all scripts executable
chmod +x ./scripts/*.sh ./scripts/*/*.sh

# Build the project
./scripts/build.sh
```

Binaries will be in `build/bin/`:
- `aevumdb` - Database server
- `aevumsh` - Shell client

## Building

See [Building AevumDB](docs/BUILDING.md).

## Running

For command line options:

```bash
$ ./build/bin/aevumdb --help
```

To run a single server database:

```bash
$ ./build/bin/aevumdb
$
$ # The shell connects to localhost by default:
$ ./build/bin/aevumsh
> db.users.insert({name: "Alice", age: 30})
> db.users.find({})
> db.users.update({name: "Alice"}, {age: 31})
> db.users.delete({age: {$lt: 18}})
> quit
```

## Bug Reports

See https://github.com/aevumdb/aevum/issues.

## Learn AevumDB

- [Getting Started](docs/GETTING_STARTED.md) - 5-minute quick start
- [Documentation](docs/README.md) - Complete guides
- [Shell Reference](docs/SHELL_REFERENCE.md) - All available commands
- [Architecture](docs/ARCHITECTURE.md) - System design
- [Development](docs/DEVELOPMENT.md) - Setup and contribution
- [Deployment](docs/DEPLOYMENT.md) - Production deployment
- [Troubleshooting](docs/TROUBLESHOOTING.md) - Common issues

## License

AevumDB is licensed under the AEVUMDB COMMUNITY LICENSE. See [LICENSE](LICENSE) for details.
