#!/bin/bash
#
# test-path-fix.sh - Test the fixed path logic
#

echo "Testing fixed path logic from /home/user/Projects/rdkvmulticastplayer/provisioning"
echo ""

echo "Script directory: $(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
echo "Script BASEDIR: ${BASH_SOURCE[0]}"
echo ""

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

 echo "Derived PROJECT_ROOT: $PROJECT_ROOT"
echo "README.md exists: $(test -f "$PROJECT_ROOT/README.md" && echo "YES" || echo "NO")"
echo ""

echo "Check with expr command:"
echo "$(expr "$SCRIPT_DIR" : '\(.*\)/provisioning')/../README.md"
