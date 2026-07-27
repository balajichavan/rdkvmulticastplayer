#!/bin/bash
#
# test-emulator-run.sh - Test the actual emulator script run
#

echo "Testing actual script execution..."
echo "============================"
echo ""

echo "Test case: Running script from provisioning directory"
echo "Command: cd /home/user/Projects/rdkvmulticastplayer/provisioning && ./emulator-setup.sh setup"
echo ""

echo "What we'll do:")
echo "1. Navigate to provisioning directory"
echo "2. Get absolute path of current script"
echo "3. Calculate project root"
echo "4. Check if README exists"
echo ""

# Navigate to provisioning directory
cd /home/user/Projects/rdkvmulticastplayer/provisioning

# Get the absolute path of this script
cd "$(dirname "$0")"
abs_script="$(realpath "$0")"

echo "After running 'cd $(dirname \"$0\")':"
echo "Absolute script path: $abs_script"

echo ""
echo "Now calculate PROJECT_ROOT (go up two levels from /provisioning/):"
PROJECT_ROOT_DIR="$(cd "$(dirname "$abs_script")" && pwd)/.."

echo "PROJECT_ROOT: $PROJECT_ROOT_DIR"

echo ""
echo "Check if README.md exists:"
if [[ -f "$PROJECT_ROOT_DIR/README.md" ]]; then
    echo "✓ SUCCESS: README.md found at $PROJECT_ROOT_DIR/README.md"
else
    echo "✗ FAILURE: README.md not found at $PROJECT_ROOT_DIR/README.md"
fi

echo ""
echo "Test complete."
