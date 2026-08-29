#!/bin/bash

# L-Connect 3 Uninstallation Script
# This script removes both the kernel driver and the Qt application

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
KERNEL_DIR="$SCRIPT_DIR/kernel"
BUILD_DIR="$SCRIPT_DIR/build"

# Function to print colored messages
print_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Check if running as root (we'll use sudo when needed)
check_sudo() {
    if ! sudo -n true 2>/dev/null; then
        print_info "This script requires sudo privileges. You may be prompted for your password."
        sudo -v
    fi
}

# Uninstall kernel driver
uninstall_kernel_driver() {
    print_info "Uninstalling kernel driver..."
    
    # Remove auto-load configuration
    if [ -f "/etc/modules-load.d/lian-li-sl-infinity.conf" ]; then
        print_info "Removing auto-load configuration..."
        sudo rm -f /etc/modules-load.d/lian-li-sl-infinity.conf
        print_success "Auto-load configuration removed"
    else
        print_info "Auto-load configuration not found, skipping..."
    fi

    # Remove udev rule for HID access
    if [ -f "/etc/udev/rules.d/60-lianli-sl-infinity.rules" ]; then
        print_info "Removing udev HID access rule..."
        sudo rm -f /etc/udev/rules.d/60-lianli-sl-infinity.rules
        sudo udevadm control --reload
        sudo udevadm trigger
        print_success "udev rule removed"
    else
        print_info "udev HID access rule not found, skipping..."
    fi
    
    # Unload module if loaded
    if lsmod | grep -q "Lian_Li_SL_INFINITY"; then
        print_info "Unloading kernel module..."
        sudo rmmod Lian_Li_SL_INFINITY 2>/dev/null || {
            print_warning "Failed to unload module (may be in use or already unloaded)"
        }
    else
        print_info "Kernel module is not loaded"
    fi
    
    # Remove DKMS registration (if the driver was installed via DKMS)
    if command -v dkms &> /dev/null && dkms status 2>/dev/null | grep -q "^lian-li-sl-infinity"; then
        print_info "Removing kernel module from DKMS..."
        # Derive registered version(s) from dkms status rather than hardcoding
        # (handles a bumped PACKAGE_VERSION and any stale older registrations)
        dkms status 2>/dev/null | grep "^lian-li-sl-infinity" \
            | sed -E 's#^lian-li-sl-infinity[/,] *([^,/ ]+).*#\1#' | sort -u \
            | while read -r ver; do
                [ -n "$ver" ] || continue
                sudo dkms remove -m lian-li-sl-infinity -v "$ver" --all || true
                sudo rm -rf "/usr/src/lian-li-sl-infinity-$ver"
            done
        sudo depmod -a
        print_success "DKMS registration removed"
    fi

    # Remove module from system
    if [ -d "$KERNEL_DIR" ]; then
        cd "$KERNEL_DIR"
        print_info "Removing kernel module from system..."
        make uninstall 2>/dev/null || {
            # Fallback manual removal
            MODULE_DIR="/lib/modules/$(uname -r)/extra"
            if [ -f "$MODULE_DIR/Lian_Li_SL_INFINITY.ko" ]; then
                sudo rm -f "$MODULE_DIR/Lian_Li_SL_INFINITY.ko"
                sudo depmod -a
                print_success "Kernel module removed"
            else
                print_info "Kernel module not found in system (may already be removed)"
            fi
        }
        cd "$SCRIPT_DIR"
    else
        print_warning "Kernel directory not found: $KERNEL_DIR"
        # Try manual removal
        MODULE_DIR="/lib/modules/$(uname -r)/extra"
        if [ -f "$MODULE_DIR/Lian_Li_SL_INFINITY.ko" ]; then
            print_info "Removing kernel module manually..."
            sudo rm -f "$MODULE_DIR/Lian_Li_SL_INFINITY.ko"
            sudo depmod -a
            print_success "Kernel module removed"
        fi
    fi
    
    print_success "Kernel driver uninstalled"
}

# Uninstall application
uninstall_application() {
    print_info "Uninstalling L-Connect3 application..."
    
    # Remove application using CMake uninstall
    if [ -d "$BUILD_DIR" ]; then
        cd "$BUILD_DIR"
        if [ -f "install_manifest.txt" ]; then
            print_info "Removing installed files..."
            sudo xargs rm -f < install_manifest.txt 2>/dev/null || true
            print_success "Application files removed"
        else
            # Manual removal if install_manifest.txt doesn't exist
            print_info "Removing application files manually..."
            sudo rm -f /usr/bin/LLConnect3 2>/dev/null || true
            sudo rm -f /usr/local/bin/LLConnect3 2>/dev/null || true
            sudo rm -f /usr/share/applications/lconnect3.desktop 2>/dev/null || true
            sudo rm -f /usr/share/pixmaps/lconnect3.png 2>/dev/null || true
            print_success "Application files removed"
        fi
        cd "$SCRIPT_DIR"
    else
        # Manual removal if build directory doesn't exist
        print_info "Removing application files manually..."
        sudo rm -f /usr/bin/LLConnect3 2>/dev/null || true
        sudo rm -f /usr/local/bin/LLConnect3 2>/dev/null || true
        sudo rm -f /usr/share/applications/lconnect3.desktop 2>/dev/null || true
        sudo rm -f /usr/share/pixmaps/lconnect3.png 2>/dev/null || true
        print_success "Application files removed"
    fi
    
    # Update desktop database
    if command -v update-desktop-database &> /dev/null; then
        print_info "Updating desktop database..."
        sudo update-desktop-database 2>/dev/null || true
    fi
    
    print_success "Application uninstalled"
}

# Clean build files
clean_build_files() {
    print_info "Cleaning build files..."
    
    if [ -d "$BUILD_DIR" ]; then
        rm -rf "$BUILD_DIR"
        print_success "Application build directory removed"
    fi
    
    if [ -d "$KERNEL_DIR" ]; then
        cd "$KERNEL_DIR"
        make clean 2>/dev/null || true
        print_success "Kernel build files cleaned"
        cd "$SCRIPT_DIR"
    fi
}

# Verify uninstallation
verify_uninstallation() {
    print_info "Verifying uninstallation..."
    
    # Check kernel module
    if lsmod | grep -q "Lian_Li_SL_INFINITY"; then
        print_error "✗ Kernel module is still loaded"
    else
        print_success "✓ Kernel module is not loaded"
    fi
    
    # Check /proc entries
    if [ -d "/proc/Lian_li_SL_INFINITY" ]; then
        print_warning "✗ Kernel driver /proc entries still exist (module may need to be unloaded)"
    else
        print_success "✓ Kernel driver /proc entries removed"
    fi
    
    # Check auto-load configuration
    if [ -f "/etc/modules-load.d/lian-li-sl-infinity.conf" ]; then
        print_error "✗ Auto-load configuration still exists"
    else
        print_success "✓ Auto-load configuration removed"
    fi
    
    # Check application
    if command -v LLConnect3 &> /dev/null; then
        print_error "✗ LLConnect3 application is still installed"
    else
        print_success "✓ LLConnect3 application removed"
    fi
    
    # Check desktop file
    if [ -f "/usr/share/applications/lconnect3.desktop" ]; then
        print_error "✗ Desktop file still exists"
    else
        print_success "✓ Desktop file removed"
    fi
}

# Main uninstallation process
main() {
    echo ""
    echo "=========================================="
    echo "  L-Connect 3 Uninstallation Script"
    echo "=========================================="
    echo ""
    
    print_warning "This will remove:"
    echo "  - L-Connect3 application"
    echo "  - Kernel driver module"
    echo "  - Auto-load configuration"
    echo "  - Build files"
    echo ""
    
    read -p "Are you sure you want to continue? [y/N] " -n 1 -r
    echo ""
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        print_info "Uninstallation cancelled."
        exit 0
    fi
    
    check_sudo
    
    print_info "Starting uninstallation process..."
    echo ""
    
    # Step 1: Uninstall application
    uninstall_application
    echo ""
    
    # Step 2: Uninstall kernel driver
    uninstall_kernel_driver
    echo ""
    
    # Step 3: Clean build files
    clean_build_files
    echo ""
    
    # Step 4: Verify uninstallation
    verify_uninstallation
    echo ""
    
    print_success "Uninstallation complete!"
    echo ""
    print_info "Note: Build dependencies (Qt, CMake, etc.) were not removed."
    print_info "If you want to remove dependencies, do so manually using your package manager."
    echo ""
}

# Run main function
main

