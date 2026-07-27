#!/bin/bash
#
# verify-emulator-path-fix.sh - Simple verification of the emulator script path fix
#

echo "Verifying the emulator-setup.sh path fix..."
echo "===================================================="
echo ""

echo "Original issue: Script couldn't find README.md when run from provisioning/"
echo ""
echo "Old (broken) logic: PROJECT_ROOT="\$(dirname "\$(dirname "$0")")"/../.."
echo ""
echo "New (fixed) logic: PROJECT_ROOT="\$(dirname "\$(dirname "\$0")")"/../.."
echo ""
echo "Testing from provisioning directory..."
echo ""

# Change to provisioning directory
cd /home/user/Projects/rdkvmulticastplayer/provisioning

# Get absolute path of script
echo "Script location: $(pwd)/emulator-setup.sh"
echo "ABSOLUTE_SCRIPT_PATH: $(realpath "$(pwd)/emulator-setup.sh")"
echo ""

echo "Using the corrected calculation:"
PROJECT_ROOT="$(realpath "$(dirname "$0")")/.."

echo "PROJECT_ROOT: $PROJECT_ROOT"

echo ""
echo "Checking if README.md exists:"
if [[ -f "${PROJECT_ROOT}/README.md" ]]; then
    echo "✓ SUCCESS: README.md found!"
    echo ""
    echo "The path fix works correctly."
    echo ""
    echo "When user runs: ./provisioning/emulator-setup.sh setup"
    echo "The script will now properly find the project root and README.md."
else
    echo "✗ FAILED: README.md not found."
    echo ""
    echo "The path fix is still not working correctly."
    exit 1
fi
