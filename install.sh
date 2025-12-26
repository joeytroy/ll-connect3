#!/bin/bash

# L-Connect 3 Installation Script
# This script installs both the kernel driver and the Qt application

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m' # No Color

# Script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
KERNEL_DIR="$SCRIPT_DIR/kernel"
BUILD_DIR="$SCRIPT_DIR/build"

# Distribution type (set by menu)
DISTRO_TYPE=""

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

# Display menu and get user selection
show_menu() {
    echo ""
    echo -e "${BOLD}=========================================="
    echo -e "  L-Connect 3 Installation Script"
    echo -e "==========================================${NC}"
    echo ""
    echo -e "${CYAN}Select your Linux distribution type:${NC}"
    echo ""
    echo -e "  ${BOLD}1)${NC} Debian-based (Ubuntu, Kubuntu, Linux Mint, Pop!_OS, etc.)"
    echo -e "  ${BOLD}2)${NC} RHEL-based (Fedora, CentOS, Rocky Linux, AlmaLinux, RHEL, etc.)"
    echo -e "  ${BOLD}3)${NC} Arch-based (Arch Linux, Manjaro, EndeavourOS, etc.)"
    echo -e "  ${BOLD}4)${NC} Exit"
    echo ""
    
    while true; do
        read -p "Enter your choice [1-4]: " choice
        case $choice in
            1)
                DISTRO_TYPE="debian"
                print_info "Selected: Debian-based distribution"
                break
                ;;
            2)
                DISTRO_TYPE="rhel"
                print_info "Selected: RHEL-based distribution"
                break
                ;;
            3)
                DISTRO_TYPE="arch"
                print_info "Selected: Arch-based distribution"
                break
                ;;
            4)
                print_info "Installation cancelled."
                exit 0
                ;;
            *)
                print_error "Invalid option. Please enter 1, 2, 3, or 4."
                ;;
        esac
    done
    echo ""
}

# Check if running as root (we'll use sudo when needed)
check_sudo() {
    if ! sudo -n true 2>/dev/null; then
        print_info "This script requires sudo privileges. You may be prompted for your password."
        sudo -v
    fi
}

# Install dependencies for Debian-based systems
install_debian_dependencies() {
    print_info "Installing dependencies for Debian-based system..."
    
    sudo apt update
    sudo apt install -y \
        build-essential \
        make \
        gcc \
        linux-headers-$(uname -r) \
        cmake \
        qt6-base-dev \
        qt6-charts-dev \
        lm-sensors \
        libusb-1.0-0-dev \
        libhidapi-dev \
        pkg-config
    
    print_success "Dependencies installed"
}

# Install dependencies for RHEL-based systems
install_rhel_dependencies() {
    print_info "Installing dependencies for RHEL-based system..."
    
    # Check if running on immutable system (Bazzite OS)
    if command -v rpm-ostree &> /dev/null; then
        print_error "Detected immutable system (rpm-ostree)."
        print_info "Bazzite OS and other immutable systems require a different installer."
        print_info "Please use the Bazzite-specific installer instead:"
        print_info "  ./install-bazzite.sh"
        exit 1
    fi
    
    # Determine package manager (dnf or yum)
    if command -v dnf &> /dev/null; then
        PKG_MGR="dnf"
    elif command -v yum &> /dev/null; then
        PKG_MGR="yum"
    else
        print_error "Neither dnf nor yum found. Cannot install dependencies."
        exit 1
    fi
    
    print_info "Using package manager: $PKG_MGR"
    
    # First, install basic build tools
    sudo $PKG_MGR groupinstall -y "Development Tools" "C Development Tools and Libraries" 2>/dev/null || \
        sudo $PKG_MGR groupinstall -y "Development Tools" 2>/dev/null || \
        sudo $PKG_MGR install -y gcc make
    
    # Install kernel development packages
    # On RHEL/Fedora, we need kernel-devel that matches the running kernel
    RUNNING_KERNEL=$(uname -r)
    print_info "Running kernel version: $RUNNING_KERNEL"
    
    # Try to install kernel-devel for exact kernel version first
    print_info "Installing kernel development packages..."
    if ! sudo $PKG_MGR install -y kernel-devel-$RUNNING_KERNEL kernel-headers-$RUNNING_KERNEL 2>/dev/null; then
        print_warning "Could not install kernel-devel for exact kernel version."
        print_info "Installing latest kernel-devel packages..."
        sudo $PKG_MGR install -y kernel-devel kernel-headers
        
        # Check if we need to reboot
        INSTALLED_KERNEL_DEVEL=$(rpm -q kernel-devel --qf '%{VERSION}-%{RELEASE}.%{ARCH}\n' 2>/dev/null | head -1)
        if [[ "$INSTALLED_KERNEL_DEVEL" != "$RUNNING_KERNEL" ]]; then
            print_warning "Kernel-devel version ($INSTALLED_KERNEL_DEVEL) doesn't match running kernel ($RUNNING_KERNEL)"
            print_warning "You may need to reboot and run this script again after updating your kernel."
        fi
    fi
    
    # Install other dependencies
    print_info "Installing Qt6 and other dependencies..."
    sudo $PKG_MGR install -y \
        gcc \
        gcc-c++ \
        make \
        cmake \
        elfutils-libelf-devel \
        qt6-qtbase-devel \
        qt6-qtcharts-devel \
        lm_sensors \
        libusb1-devel \
        hidapi-devel \
        pkgconfig \
        dkms
    
    # For Fedora, might need to enable RPM Fusion for some packages
    if ! rpm -q hidapi-devel &>/dev/null; then
        print_warning "hidapi-devel not found. Trying alternative package names..."
        sudo $PKG_MGR install -y hidapi hidapi-devel 2>/dev/null || true
    fi
    
    print_success "Dependencies installed"
}

# Install dependencies for Arch-based systems
install_arch_dependencies() {
    print_info "Installing dependencies for Arch-based system..."
    
    sudo pacman -S --needed --noconfirm \
        base-devel \
        linux-headers \
        cmake \
        qt6-base \
        qt6-charts \
        lm_sensors \
        libusb \
        hidapi \
        pkgconf
    
    print_success "Dependencies installed"
}

# Install dependencies based on selected distribution type
install_dependencies() {
    case $DISTRO_TYPE in
        debian)
            install_debian_dependencies
            ;;
        rhel)
            install_rhel_dependencies
            ;;
        arch)
            install_arch_dependencies
            ;;
        *)
            print_error "Unknown distribution type: $DISTRO_TYPE"
            exit 1
            ;;
    esac
}

# Verify kernel build environment
verify_kernel_build_env() {
    print_info "Verifying kernel build environment..."
    
    RUNNING_KERNEL=$(uname -r)
    KERNEL_BUILD_DIR="/lib/modules/$RUNNING_KERNEL/build"
    
    if [ ! -d "$KERNEL_BUILD_DIR" ]; then
        print_error "Kernel build directory not found: $KERNEL_BUILD_DIR"
        
        if [[ "$DISTRO_TYPE" == "rhel" ]]; then
            print_info "On RHEL-based systems, try:"
            print_info "  sudo dnf install kernel-devel-\$(uname -r)"
            print_info "Or update your system and reboot to get matching kernel-devel:"
            print_info "  sudo dnf update && sudo reboot"
        else
            print_info "Please install kernel headers for your running kernel."
        fi
        exit 1
    fi
    
    # Check for required kernel build files
    if [ ! -f "$KERNEL_BUILD_DIR/Makefile" ]; then
        print_error "Kernel Makefile not found in $KERNEL_BUILD_DIR"
        print_error "Kernel development package may be incomplete."
        exit 1
    fi
    
    print_success "Kernel build environment verified"
}

# Install kernel driver
install_kernel_driver() {
    print_info "Installing kernel driver..."
    
    if [ ! -d "$KERNEL_DIR" ]; then
        print_error "Kernel directory not found: $KERNEL_DIR"
        exit 1
    fi
    
    # Verify build environment first
    verify_kernel_build_env
    
    cd "$KERNEL_DIR"
    
    # Clean any previous build
    make clean 2>/dev/null || true
    
    # Build the module
    print_info "Building kernel module..."
    if ! make; then
        print_error "Kernel module build failed!"
        
        if [[ "$DISTRO_TYPE" == "rhel" ]]; then
            print_info "Common fixes for RHEL-based systems:"
            print_info "1. Ensure kernel-devel matches running kernel:"
            print_info "   sudo dnf install kernel-devel-\$(uname -r)"
            print_info "2. If that fails, update and reboot:"
            print_info "   sudo dnf update && sudo reboot"
            print_info "3. Check SELinux is not blocking:"
            print_info "   sudo setenforce 0  (temporarily disable)"
        fi
        exit 1
    fi
    
    if [ ! -f "Lian_Li_SL_INFINITY.ko" ]; then
        print_error "Kernel module build failed - .ko file not created!"
        exit 1
    fi
    
    # Install using the Makefile (which handles depmod)
    print_info "Installing kernel module to system..."
    make install
    
    # Load the module
    print_info "Loading kernel module..."
    sudo rmmod Lian_Li_SL_INFINITY 2>/dev/null || true
    
    # For RHEL systems, try insmod first if modprobe fails
    if ! sudo modprobe Lian_Li_SL_INFINITY 2>/dev/null; then
        print_warning "modprobe failed, trying insmod..."
        if ! sudo insmod Lian_Li_SL_INFINITY.ko 2>/dev/null; then
            print_error "Failed to load kernel module!"
            print_info "Check dmesg for errors: sudo dmesg | tail -20"
            exit 1
        fi
    fi
    
    # Verify module is loaded
    if lsmod | grep -q "Lian_Li_SL_INFINITY"; then
        print_success "Kernel module loaded successfully"
    else
        print_error "Kernel module failed to load!"
        print_info "Check dmesg for errors: sudo dmesg | tail -20"
        exit 1
    fi
    
    # Configure auto-load on boot
    print_info "Configuring kernel module to load on boot..."
    echo "Lian_Li_SL_INFINITY" | sudo tee /etc/modules-load.d/lian-li-sl-infinity.conf > /dev/null
    print_success "Kernel module will load automatically on boot"
    
    # Verify /proc entries exist
    if [ -d "/proc/Lian_li_SL_INFINITY" ]; then
        print_success "Kernel driver is working (found /proc/Lian_li_SL_INFINITY)"
    else
        print_warning "Warning: /proc/Lian_li_SL_INFINITY not found. Driver may not be working correctly."
        print_info "This might be normal if no Lian Li SL-Infinity hub is connected."
    fi
    
    cd "$SCRIPT_DIR"
}

# Configure sensors
configure_sensors() {
    print_info "Configuring sensors..."
    
    if ! command -v sensors &> /dev/null; then
        print_warning "sensors command not found. Skipping sensor configuration."
        return
    fi
    
    if [ ! -f /etc/sensors3.conf ] && [ ! -f /etc/sensors.conf ]; then
        print_info "Running sensors-detect (this may be interactive)..."
        print_warning "You may be prompted for sensor detection. Accept defaults by pressing ENTER."
        if sudo sensors-detect --auto; then
            print_success "Sensors configured"
        else
            print_warning "Sensor detection cancelled or failed. You can run 'sudo sensors-detect' manually later."
        fi
    else
        print_info "Sensors already configured, skipping detection."
    fi
}

# Configure udev rule for HID access to SL-Infinity (RGB)
configure_udev() {
    print_info "Configuring udev rule for SL-Infinity HID access..."
    local RULE_FILE="/etc/udev/rules.d/60-lianli-sl-infinity.rules"
    echo 'SUBSYSTEM=="hidraw", ATTRS{idVendor}=="0cf2", ATTRS{idProduct}=="a102", TAG+="uaccess", MODE="0666"' | sudo tee "$RULE_FILE" > /dev/null
    sudo udevadm control --reload
    sudo udevadm trigger
    print_success "udev rule installed at $RULE_FILE"
}

# Install Qt application
install_application() {
    print_info "Installing L-Connect3 application..."
    
    # Create build directory
    if [ -d "$BUILD_DIR" ]; then
        print_info "Cleaning previous build..."
        rm -rf "$BUILD_DIR"
    fi
    
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"
    
    # Configure with CMake
    print_info "Configuring build with CMake..."
    if ! cmake -DCMAKE_INSTALL_PREFIX=/usr ..; then
        print_error "CMake configuration failed!"
        
        if [[ "$DISTRO_TYPE" == "rhel" ]]; then
            print_info "On RHEL-based systems, ensure Qt6 development packages are installed:"
            print_info "  sudo dnf install qt6-qtbase-devel qt6-qtcharts-devel"
        fi
        exit 1
    fi
    
    # Build
    print_info "Building application..."
    if ! make -j$(nproc); then
        print_error "Application build failed!"
        exit 1
    fi
    
    # Install
    print_info "Installing application to system..."
    sudo make install
    
    # Verify installation
    if command -v LLConnect3 &> /dev/null; then
        print_success "Application installed successfully"
    else
        print_error "Application installation verification failed!"
        exit 1
    fi
    
    cd "$SCRIPT_DIR"
}

# Verify installation
verify_installation() {
    print_info "Verifying installation..."
    
    echo ""
    
    # Check kernel module
    if lsmod | grep -q "Lian_Li_SL_INFINITY"; then
        print_success "✓ Kernel module is loaded"
    else
        print_error "✗ Kernel module is not loaded"
    fi
    
    # Check /proc entries
    if [ -d "/proc/Lian_li_SL_INFINITY" ]; then
        print_success "✓ Kernel driver /proc entries exist"
    else
        print_warning "✗ Kernel driver /proc entries not found (normal if no hub connected)"
    fi
    
    # Check auto-load configuration
    if [ -f "/etc/modules-load.d/lian-li-sl-infinity.conf" ]; then
        print_success "✓ Auto-load configuration exists"
    else
        print_warning "✗ Auto-load configuration not found"
    fi
    
    # Check application
    if command -v LLConnect3 &> /dev/null; then
        print_success "✓ LLConnect3 application is installed"
    else
        print_error "✗ LLConnect3 application not found"
    fi
    
    # Check desktop file
    if [ -f "/usr/share/applications/lconnect3.desktop" ]; then
        print_success "✓ Desktop file installed"
    else
        print_warning "✗ Desktop file not found"
    fi
    
    # Check udev rule
    if [ -f "/etc/udev/rules.d/60-lianli-sl-infinity.rules" ]; then
        print_success "✓ udev rule installed"
    else
        print_warning "✗ udev rule not found"
    fi
}

# Main installation process
main() {
    # Show menu and get user selection
    show_menu
    
    check_sudo
    
    print_info "Starting installation process..."
    echo ""
    
    # Step 1: Install dependencies
    install_dependencies
    echo ""
    
    # Step 2: Install kernel driver
    install_kernel_driver
    echo ""
    
    # Step 3: Configure sensors
    configure_sensors
    echo ""
    
    # Step 4: Configure udev (RGB HID access)
    configure_udev
    echo ""
    
    # Step 5: Install application
    install_application
    echo ""
    
    # Step 6: Verify installation
    verify_installation
    echo ""
    
    print_success "Installation complete!"
    echo ""
    print_info "You can now:"
    echo "  - Launch the application: LLConnect3"
    echo "  - Control fans via /proc/Lian_li_SL_INFINITY/Port_X/fan_speed"
    echo "  - Check module status: lsmod | grep Lian_Li"
    echo ""
    
    if [[ "$DISTRO_TYPE" == "rhel" ]]; then
        print_warning "RHEL-based system note:"
        echo "  - If SELinux is enforcing, you may need to create a policy for the driver"
        echo "  - After kernel updates, rebuild the module with:"
        echo "    cd $KERNEL_DIR && make clean && make && sudo make install"
        echo ""
    else
        print_warning "Note: After kernel updates, you may need to rebuild the kernel module:"
        echo "  cd $KERNEL_DIR && make clean && make && sudo make install"
        echo ""
    fi
}

# Run main function
main
