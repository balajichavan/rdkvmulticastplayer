#!/bin/bash
#
# test-path-resolution.sh - Comprehensive test of path resolution
#

set -euo pipefail

SCRIPT_PATH="${BASH_SOURCE[0]}"
echo "Script path: $SCRIPT_PATH"
echo ""

# Method 1: Using dirname
echo "=== Method 1: Using dirname ==="
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
echo "Script dir (dirname): $SCRIPT_DIR"

# Test 1: Run from project root
echo ""
echo "Test 1: Running from project root (/home/user/Projects/rdkvmulticastplayer)"
cd /home/user/Projects/rdkvmulticastplayer
RESULTS_DIR="$(cd provisioning && echo "$(dirname "${BASH_SOURCE[0]}")")/../.."
echo "Results dir: $RESULTS_DIR"
echo "README exists: $(test -f "$RESULTS_DIR/README.md" && echo "YES" || echo "NO")"
echo ""

# Test 2: If script is in provisioning directory and we want to go up
echo "Test 2: If script is in provisioning/...")
echo "cd provisioning"
echo "Then SCRIPT_DIR='${SCRIPT_DIR}' (from provisioning/emulator-setup.sh)"

# Based on how bash resolves paths, let's check the actual path
actual_script_path=$(realpath "/home/user/Projects/rdkvmulticastplayer/provisioning/emulator-setup.sh")
echo ""
echo "=== Real path verification ==="
echo "Actual script path: $actual_script_path"
echo "Script location dir: $(dirname "$actual_script_path")"
echo "Parent directory: $(dirname "$(dirname "$actual_script_path")")"

# What we want: project root
echo ""
echo "=== What we want ==="
echo "Project root should be: /home/user/Projects/rdkvmulticastplayer"
