#!/bin/bash
#
# rdkv-emulator-setup.sh - Quick RDK-V emulator setup for testing MulticastPlayer
#
# Option 2: Build natively inside the emulator/Thunder container
#
# Usage: sudo ./provisioning/emulator-setup.sh
#
# This script sets up everything needed to test the RDKV MulticastPlayer plugin
# in an emulator/Thunder container without building the full RDK-V image.
#

set -euo pipefail

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Configuration
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE:-$0}")" && pwd)/.."

# Print colored messages
print_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

print_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Display help
help() {
    cat <<EOF
RDK-V Emulator Setup - Option 2 (Native Build)

This script sets up everything needed to test the RDKV MulticastPlayer plugin
in an emulator/Thunder container without building the full RDK-V image.

Options:
  setup    Run full emulator setup (build, install, test)
  build    Build the plugin only
  install  Install the plugin with emulator configuration
  test     Run the JSON-RPC tests only
  help     Show this help message

This script expects to be run from or placed inside the project root directory.
EOF
}

# Check prerequisites
check_prerequisites() {
    print_info "Checking prerequisites..."
    
    # Check if we're in the right directory - always check from project root
    if [[ ! -f "${PROJECT_ROOT}/README.md" ]]; then
        print_error "README.md not found. Please run this from the project root."
        exit 1
    fi
    
    # Check for required tools
    if ! command -v gcc &> /dev/null; then
        print_error "gcc not found. Please install build-essential."
        exit 1
    fi
    
    if ! command -v cmake &> /dev/null; then
        print_error "cmake not found. Please install cmake."
        exit 1
    fi
    
    if ! command -v make &> /dev/null; then
        print_error "make not found. Please install build-essential."
        exit 1
    fi
    
    if ! command -v pkg-config &> /dev/null; then
        print_error "pkg-config not found. Please install pkg-config."
        exit 1
    fi
    
    # Check for GStreamer development packages
    if ! pkg-config --exists gstreamer-1.0; then
        print_error "GStreamer development packages not found."
        print_error "Please install: sudo apt install gstreamer1.0-plugins-base gstreamer1.0-plugins-good gstreamer1.0-libav"
        exit 1
    fi
    
    if ! pkg-config --exists glib-2.0; then
        print_error "GLib development packages not found."
        exit 1
    fi
    
    print_info "All prerequisites are met."
}

# Build the plugin
build_plugin() {
    print_info "Building RDKV MulticastPlayer plugin..."
    
    PLUGIN_DIR="${PROJECT_ROOT}/plugin/MulticastPlayer"
    cd "${PROJECT_ROOT}/${PLUGIN_DIR}"
    
    # Create build directory
    mkdir -p build
    cd build
    
    # Configure and build
    cmake .. \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/usr \
        -DCMAKE_CXX_STANDARD=14
    
    # Build with all available CPU cores
    make -j$(nproc)
    
    # Verify build succeeded
    if [[ -f "libWPEFrameworkMulticastPlayer.so" ]]; then
        print_info "✓ Plugin built successfully: $(pwd)/libWPEFrameworkMulticastPlayer.so"
        
        # Show build statistics
        local size_kb=$(du -k "libWPEFrameworkMulticastPlayer.so" | cut -f1)
        print_info "  Plugin size: ${size_kb} KB"
    else
        print_error "Plugin build failed. Output not found."
        exit 1
    fi
}

# Install the plugin with emulator configuration
install_plugin() {
    print_info "Installing plugin with emulator configuration..."
    
    # Install the shared library
    print_info "Installing ${PROJECT_ROOT}/${PLUGIN_DIR}/build/libWPEFrameworkMulticastPlayer.so to /usr/lib/wpeframework/plugins/..."
    
    if [[ -f "${PROJECT_ROOT}/${PLUGIN_DIR}/build/libWPEFrameworkMulticastPlayer.so" ]]; then
        # Create plugin directory if it doesn't exist
        sudo mkdir -p /usr/lib/wpeframework/plugins
        sudo cp "${PROJECT_ROOT}/${PLUGIN_DIR}/build/libWPEFrameworkMulticastPlayer.so" /usr/lib/wpeframework/plugins/
        
        # Set permissions
        sudo chmod 644 /usr/lib/wpeframework/plugins/libWPEFrameworkMulticastPlayer.so
        
        # Install emulator configuration
        sudo cp "${PROJECT_ROOT}/provisioning/MulticastPlayer.plugin-config.emulator.json" /etc/WPEFramework/plugins/MulticastPlayer.json
        
        # Restart WPEFramework
        print_info "Restarting WPEFramework service..."
        sudo systemctl restart wpeframework
        
        # Wait for service to start
        sleep 3
        
        # Verify plugin is loaded
        if sudo systemctl is-active --quiet wpeframework; then
            print_info "✓ WPEFramework service is running."
            
            # Check if plugin is loaded
            if sudo curl -s http://127.0.0.1:9998/jsonrpc \
                   -H 'Content-Type: application/json' \
                   -d '{"jsonrpc":"2.0","id":1,"method":"Controller.1.status@org.rdk.MulticastPlayer"}' | grep -q '"success":true'; then
                print_info "✓ Plugin successfully loaded and active."
            else
                print_warn "Plugin may not be loaded properly. Check WPEFramework logs."
                print_warn "Run: sudo journalctl -u wpeframework -f --lines=50"
            fi
        else
            print_error "WPEFramework service failed to start."
            print_error "Check logs: sudo journalctl -u wpeframework -f"
            exit 1
        fi
    else
        print_error "Plugin shared library not found at ${PROJECT_ROOT}/${PLUGIN_DIR}/build/libWPEFrameworkMulticastPlayer.so"
        exit 1
    fi
}

# Start multicast server if not already running
start_multicast_server() {
    print_info "Setting up multicast server..."
    
    cd "${PROJECT_ROOT}"
    
    # Check if multicast server is already running
    if ss -tuln | grep -q ':5000'; then
        print_warn "Port 5000 is already in use. Multicast server may already be running."
        MULTICAST_SERVER_PID=$(ss -tuln "sport = :5000" | grep LISTEN | awk '{print $7}' | cut -d: -f2)
        if [[ -n "$MULTICAST_SERVER_PID" ]]; then
            echo $MULTICAST_SERVER_PID > /tmp/multicast-server.pid
            print_info "  Multicast server already running (PID: $MULTICAST_SERVER_PID)"
            return
        fi
    fi
    
    print_info "Starting multicast test server on udp://239.1.1.1:5000"
    print_info "Press Ctrl-C to stop it when done."
    echo ""
    
    # Start multicast server in background
    test/multicast-server.sh &
    MULTICAST_PID=$!
    
    # Store PID to kill later
    echo $MULTICAST_PID > /tmp/multicast-server.pid
    
    # Wait a bit for server to start
    sleep 3
    
    print_info "✓ Multicast server is running. (PID: $MULTICAST_PID)"
    print_info "  To stop it when done: kill \$(cat /tmp/multicast-server.pid)"
}

# Check if multicast server is running
check_multicast_server() {
    if [[ -f /tmp/multicast-server.pid ]]; then
        MULTICAST_PID=$(cat /tmp/multicast-server.pid)
        if kill -0 $MULTICAST_PID 2>/dev/null; then
            print_info "✓ Multicast server is running (PID: $MULTICAST_PID)"
            return 0
        else
            rm -f /tmp/multicast-server.pid
        fi
    fi
    return 1
}

# Stop multicast server
stop_multicast_server() {
    if [[ -f /tmp/multicast-server.pid ]]; then
        MULTICAST_PID=$(cat /tmp/multicast-server.pid)
        if kill -0 $MULTICAST_PID 2>/dev/null; then
            print_info "Stopping multicast server (PID: $MULTICAST_PID)..."
            kill $MULTICAST_PID
            rm -f /tmp/multicast-server.pid
            print_info "✓ Multicast server stopped."
        else
            rm -f /tmp/multicast-server.pid
        fi
    fi
}

# Run tests
run_tests() {
    print_info "Running JSON-RPC tests..."
    
    cd "${PROJECT_ROOT}"
    
    # Wait a bit for everything to stabilize
    sleep 5
    
    print_info "Executing test/jsonrpc-test.sh (will run for 15 seconds)..."
    echo ""
    
    # Run tests with default host (127.0.0.1)
    test/jsonrpc-test.sh HOST=127.0.0.1
}

# Cleanup function
cleanup() {
    print_info "Cleaning up..."
    
    # Stop multicast server if we started it
    stop_multicast_server
    
    print_info "Cleanup completed."
}

# Main execution
main() {
    # Parse command line arguments
    if [[ $# -eq 0 ]]; then
        # Default: run full setup
        COMMAND="setup"
    else
        COMMAND="$1"
    fi
    
    case $COMMAND in
        "setup")
            echo "============================================"
            echo "RDK-V Emulator Setup - Option 2 (Native Build)"
            echo "============================================"
            echo ""
            
            check_prerequisites
            echo ""
            
            build_plugin
            echo ""
            
            install_plugin
            echo ""
            
            # Only start multicast server if not already running
            if ! check_multicast_server; then
                start_multicast_server
                echo ""
            fi
            
            print_info "Setup complete! Ready to run tests."
            echo ""
            echo "You can now run the tests with:"
            echo "  ${YELLOW}test/jsonrpc-test.sh${NC} HOST=<emulator-ip>"
            echo ""
            echo "To stop the multicast server when done:"
            echo "  ${YELLOW}stop_multicast_server${NC}"
            echo ""
            
            # Run tests
            run_tests
            ;;
        
        "build")
            echo "============================================"
            echo "Building RDK-V MulticastPlayer Plugin"
            echo "============================================"
            echo ""
            
            check_prerequisites
            echo ""
            
            build_plugin
            ;;
        
        "install")
            echo "============================================"
            echo "Installing RDK-V MulticastPlayer Plugin"
            echo "============================================"
            echo ""
            
            install_plugin
            ;;
        
        "test")
            echo "============================================"
            echo "Running JSON-RPC Tests"
            echo "============================================"
            echo ""
            
            check_multicast_server || {
                print_warn "Multicast server not running."
                print_warn "Starting multicast server..."
                start_multicast_server
                echo ""
            }
            
            run_tests
            ;;
        
        "help"|"-h"|"--help")
            help
            ;;
        
        *)
            print_error "Unknown command: $COMMAND"
            echo ""
            help
            exit 1
            ;;
    esac
}

# Register cleanup function
trap cleanup EXIT

# Run main function
main "$@"