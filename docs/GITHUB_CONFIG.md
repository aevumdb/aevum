# GitHub Configuration

This directory contains GitHub-specific workflows, templates, and configuration files for AevumDB.

## Contents

### Workflows ([`../.github/workflows/`](../.github/workflows/))
- `ci.yml` - Continuous Integration pipeline
  - Builds on Ubuntu and macOS
  - Tests with GCC and Clang compilers
  - Runs code formatting checks
  - Performs security scans

### Templates ([`../.github/ISSUE_TEMPLATE/`](../.github/ISSUE_TEMPLATE/) and [`../.github/`](../.github/))
- `bug_report.md` - Bug report template
- `feature_request.md` - Feature request template
- `documentation.md` - Documentation improvement template
- `PULL_REQUEST_TEMPLATE.md` - Pull request template

### Configuration
- [`dependabot.yml`](../.github/dependabot.yml) - Dependency update automation
  - Weekly GitHub Actions updates
  - Automatic security updates
  - Pull request limits to avoid spam

## Usage

### Contributing Code
1. Create an issue describing your change
2. Fork the repository
3. Create a branch: `git checkout -b feature/my-feature`
4. Make your changes and push
5. Open a pull request using the PR template
6. CI/CD checks will run automatically

### Reporting Issues
- Use bug_report.md template for bugs
- Use feature_request.md template for features
- Use documentation.md template for doc improvements

### CI/CD
- Workflows run on push and pull request
- Tests must pass for merge
- Code formatting enforced
- Security checks enabled

## See Also
- [CONTRIBUTING.md](../CONTRIBUTING.md) - Contribution guidelines
- [CODE_OF_CONDUCT.md](../CODE_OF_CONDUCT.md) - Community standards
