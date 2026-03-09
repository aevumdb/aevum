# Contributing to AevumDB

Thank you for your interest in contributing to AevumDB! We welcome contributions from the community, including bug reports, feature requests, code improvements, documentation, and more.

## Getting Started

1. **Fork the repository**: Click the "Fork" button on GitHub
2. **Clone your fork**: `git clone https://github.com/yourusername/aevum.git`
3. **Create a branch**: `git checkout -b feature/my-feature` or `git checkout -b fix/my-fix`
4. **Read the documentation**: Check [docs/DEVELOPMENT.md](docs/DEVELOPMENT.md) for setup instructions

## Development Setup

```bash
git clone https://github.com/aevumdb/aevum.git
cd aevum
./scripts/build.sh
```

See [Building AevumDB](docs/BUILDING.md) for detailed build instructions.

## Code Style

We follow these conventions:

### C++ Code

- **Style**: Follow existing code style in the repository
- **Formatting**: Use `./scripts/format.sh cpp` before committing
- **Tool**: Configured via `.clang-format`
- **Standards**: C++17 or later
- **Headers**: `.hpp` for headers, `.cpp` for implementation

### Commit Messages

- Use clear, descriptive messages
- Start with a capital letter
- Keep first line under 50 characters
- Add details in subsequent paragraphs if needed

Example:
```
Add database indexing support

Implement B-tree indexing for collection queries.
Improves find() performance by ~50% on large datasets.

Fixes #123
```

### File Organization

- `src/aevum/db/` - Database engine
- `src/aevum/shell/` - Interactive shell
- `src/aevum/client/` - Client library
- `src/aevum/bson/` - BSON serialization
- `src/aevum/util/` - Utilities
- `docs/` - Documentation

## Making Changes

### Before You Start

1. Check [GitHub Issues](https://github.com/aevumdb/aevum/issues) for related work
2. For major features, open an issue first to discuss the approach
3. For bug fixes, reference the issue number in your commits

### While Developing

1. Write clear, self-documenting code
2. Add comments for complex logic
3. Keep commits logical and focused (one feature/fix per commit)
4. Test your changes thoroughly
5. Format code: `./scripts/format.sh`
6. Run tests: `./scripts/test.sh` (for FFI changes)
7. Lint code: `./scripts/lint.sh` (for C++ code analysis)
8. Build successfully: `./scripts/build.sh`

### Testing

- Add unit tests for new functionality (especially for FFI modules)
- Run FFI tests: `./scripts/test.sh`
- Run existing tests to ensure no regressions
- Include test cases in your pull request description

## Submitting Changes

1. **Format your code**: `./scripts/format.sh cpp`
2. **Push to your fork**: `git push origin feature/my-feature`
3. **Create a Pull Request** on GitHub
4. **Fill in the PR template** with:
   - Description of changes
   - Related issue number (if any)
   - Testing approach
   - Any breaking changes

## Pull Request Guidelines

- Reference the issue your PR addresses (e.g., "Fixes #123")
- Provide clear description of what changed and why
- Keep PRs focused (one feature/fix per PR)
- Ensure CI/CD checks pass (if applicable)
- Be open to feedback and iterate on suggestions

## Code Review

- All contributions go through code review
- Be respectful and constructive in discussions
- Respond to feedback promptly
- Small, focused PRs are reviewed faster

## Documentation

We appreciate documentation improvements!

- Fix typos and improve clarity
- Add examples or clarifications
- Document new features
- Update outdated information

See [docs/README.md](docs/README.md) for documentation structure.

## Reporting Bugs

When reporting a bug, include:

1. **Version**: `./build/bin/aevumdb --version`
2. **OS and Environment**: Output of `uname -a`
3. **Steps to Reproduce**: Exact commands that trigger the bug
4. **Expected Behavior**: What should happen
5. **Actual Behavior**: What actually happened
6. **Error Output**: Any error messages or logs

Create the issue at: https://github.com/aevumdb/aevum/issues

## Feature Requests

We'd love to hear your ideas! When proposing a feature:

1. **Describe the feature**: What problem does it solve?
2. **Current behavior**: How do you currently work around it?
3. **Proposed behavior**: How should it work?
4. **Alternative approaches**: Any other solutions?

## Community

- **Discussions**: https://github.com/aevumdb/aevum/discussions
- **Issues**: https://github.com/aevumdb/aevum/issues
- **Code of Conduct**: See [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md)

## Legal

By submitting a pull request, you agree that your contributions will be licensed under the AEVUMDB COMMUNITY LICENSE. See [LICENSE](LICENSE) for details.

## Questions?

- Check [docs/DEVELOPMENT.md](docs/DEVELOPMENT.md) for development questions
- See [docs/TROUBLESHOOTING.md](docs/TROUBLESHOOTING.md) for common issues
- Open an issue on GitHub for anything else

Thank you for contributing to AevumDB!
