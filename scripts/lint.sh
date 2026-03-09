#!/bin/bash

##
# lint.sh
#
# Convenience wrapper for running the lint script from the scripts root directory.
# This redirects to scripts/lint/lint.sh for actual linting operations.
##

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" &> /dev/null && pwd)"
bash "$SCRIPT_DIR/lint/lint.sh" "$@"
