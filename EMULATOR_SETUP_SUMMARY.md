# Summary: RDKV Emulator Environment Setup

## Overview

This document summarizes the work done to help set up the environment for the RDK-V emulator and update the README with comprehensive documentation.

## What Was Done

### 1. Project Structure Analysis ✅
- Explored the entire project structure including:
  - Plugin directory (`plugin/MulticastPlayer/`) with C++ source files
  - Widget directory (`widget/`) with Lightning.js UI
  - Testing directory (`test/`) with multicast server and JSON-RPC tests
  - Provisioning directory (`provisioning/`) with packaging scripts
  - Configuration files and source documentation

### 2. Environment Setup for RDK Emulator ✅

**Option 2: Build natively inside the emulator/Thunder container** (Recommended)

Created a comprehensive setup script: `/home/user/Projects/rdkvmulticastplayer/provisioning/emulator-setup.sh`

Key Features:
- Validates prerequisites (gcc, cmake, make, pkg-config, GStreamer)
- Builds the plugin with proper CMake configuration
- Installs with the emulator profile configuration
- Manages multicast server lifecycle
- Provides multiple modes: setup (full), build (compile only), install (install only), test (run tests only)
- Includes detailed colored output and help documentation

**Usage:**
```bash
# Run full setup inside the emulator/Thunder container
sudo ./provisioning/emulator-setup.sh setup

# Just build the plugin
cd /home/user/rdkvmulticastplayer
sudo ./provisioning/emulator-setup.sh build

# Show help
sudo ./provisioning/emulator-setup.sh help
```

**Main Setup Steps:**

1. **Prerequisites Check**
   - Verifies build tools (gcc, cmake, make, pkg-config)
   - Checks for GStreamer development packages
   - Validates project structure

2. **Build Plugin**
   - Creates CMake build directory
   - Configures with release settings and standard C++14
   - Builds with all available CPU cores
   - Validates successful build

3. **Install Plugin**
   - Copies shared library to `/usr/lib/wpeframework/plugins/`
   - Installs emulator configuration (`MulticastPlayer.plugin-config.emulator.json`)
   - Restarts WPEFramework service
   - Verifies plugin is loaded and active

4. **Multicast Server Setup**
   - Starts test multicast server (Big Buck Bunny by default)
   - Manages server lifecycle (start/stop)
   - Checks if server is already running

5. **Run Tests**
   - Executes JSON-RPC test harness
   - Tests full lifecycle: open → play → status → stop → close
   - Validates IGMP join/leave behavior

### 3. README Documentation Updates ✅

**Quick Start Section**
- Added concise overview of project components
- Highlighted the four main project areas
- Pointed to the Quick Emulator Setup section

**Quick Emulator Setup Section (New)**
- Streamlined guide for emulator setup
- Covers prerequisites, VM creation, network configuration, plugin installation, and testing
- Provides concrete example commands with shell scripts
- Emphasizes that emulator uses software decode (avdec_h264 + autovideosink)

**Section Reorganization**
- Renumbered sections to accommodate new content
- Updated all cross-references
- Maintained consistent formatting throughout

## Key Emulator Features Explained

### Cross-Platform Detection
The same `.so` file runs on multiple platforms:

| Platform | Video Decoder | Audio Decoder | Video Sink |
|----------|--------------|--------------|------------|
| Broadcom STB | `brcmvideodecoder` | `brcmaudiodecoder` | `westerossink` |
| Raspberry Pi | `v4l2h264dec` | `brcmaudiodecoder` | `westerossink` |
| Emulator | `avdec_h264` | `avdec_aac` | `autovideosink` |
| Software Only | `avdec_h264` | `avdec_aac` | `glimagesink` |

### Architecture Benefits
- **Separation of Concerns**: Widget handles UI/controls, native plugin handles all media processing
- **Flexibility**: Plugin auto-detects best components for the target platform
- **Portability**: Same code runs on hardware, Pi, emulator, and software-only builds
- **Testability**: Quick emulator setup without full RDK image build

## Testing Workflow

```bash
# Inside the emulator/Thunder container:
sudo apt install -y gstreamer1.0-libav gstreamer1.0-plugins-good gstreamer1.0-plugins-bad
sudo ./provisioning/emulator-setup.sh setup
```

This will:
1. Build the plugin (one-time)
2. Install it with emulator configuration  
3. Start the multicast server
4. Run the JSON-RPC tests
5. Clean up and exit

## Files Created/Updated

### New Files
- `/home/user/Projects/rdkvmulticastplayer/provisioning/emulator-setup.sh` (375 lines) - Complete emulator setup script

### Updated Files
- `/home/user/Projects/rdkvmulticastplayer/README.md` - Enhanced with Quick Start and Quick Emulator Setup sections

### Documentation References
- Plugin source code in `plugin/MulticastPlayer/`
- Widget frontend in `widget/src/`
- Testing utilities in `test/`
- Configuration in `provisioning/`

## Prerequisites for Users

### To Build the Plugin
```bash
# Ubuntu/Debian (required)
sudo apt update
sudo apt install -y build-essential cmake pkg-config \
   libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev \
   libglib2.0-dev libgobject-2.0-dev

# Global prerequisites for the project
sudo apt install -y gawk wget git diffstat unzip texinfo \
   chrpath socat cpio python3 python3-pip python3-pexpect \
   xz-utils debianutils iputils-ping python3-git python3-jinja2 \
   libegl1-mesa libsdl1.2-dev python3-subunit zstd liblz4-tool \
   file locales repo bmaptool
```

### Alternative: Use Pre-built Binary
If you have the pre-compiled `libWPEFrameworkMulticastPlayer.so`, you can:

```bash
# Inside the emulator/Thunder container
sudo mkdir -p /usr/lib/wpeframework/plugins
sudo cp /path/to/libWPEFrameworkMulticastPlayer.so /usr/lib/wpeframework/plugins/
sudo cp provisioning/MulticastPlayer.plugin-config.emulator.json /etc/WPEFramework/plugins/MulticastPlayer.json
sudo systemctl restart wpeframework
```

## Verification

After running the setup script, you should see:

```
✓ Plugin built successfully: /path/to/plugin/MulticastPlayer/build/libWPEFrameworkMulticastPlayer.so
✓ Plugin shared library not found at plugin/MulticastPlayer/build/libWPEFrameworkMulticastPlayer.so
✓ WPEFramework service is running.
✓ Plugin successfully loaded and active.
✓ Multicast server is running. (PID: 12345)
Setup complete! Ready to run tests.
```

The JSON-RPC test will confirm the plugin works end-to-end:
- Build the pipeline with the test stream
- Join the multicast group
- Play video with status transitions
- Leave the group and clean up