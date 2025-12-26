#!/bin/bash

# L-Connect 3 Installation Script for Bazzite OS
# This script installs both the kernel driver and the Qt application on immutable systems
# Bazzite OS uses rpm-ostree instead of dnf for package management

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

# Check if running on Bazzite/immutable system
check_immutable_system() {
    if ! command -v rpm-ostree &> /dev/null; then
        print_error "rpm-ostree not found. This script is for Bazzite OS and other immutable Fedora-based systems."
        print_info "If you're on a regular Fedora system, please use install.sh instead."
        exit 1
    fi
    
    print_success "Detected immutable system (rpm-ostree)"
    
    # Check if there are pending updates that require reboot
    if rpm-ostree status 2>/dev/null | grep -q "PendingUpdate\|UpdatePending"; then
        print_warning "There are pending rpm-ostree updates that require a reboot."
        print_info "Run 'rpm-ostree status' to see details."
        print_info "After rebooting, run this script again."
        echo ""
        read -p "Do you want to continue anyway? (y/n): " confirm
        if [[ ! "$confirm" =~ ^[Yy]$ ]]; then
            print_info "Please reboot first, then run this script again."
            exit 0
        fi
    fi
}

# Check if running as root (we'll use sudo when needed)
check_sudo() {
    if ! sudo -n true 2>/dev/null; then
        print_info "This script requires sudo privileges. You may be prompted for your password."
        sudo -v
    fi
}

# Install dependencies using rpm-ostree
install_dependencies() {
    local missing_packages=("$@")
    
    print_info "Installing dependencies for Bazzite OS..."
    print_warning "This will layer packages using rpm-ostree, which requires a reboot."
    print_warning "The installation will continue after you reboot and run this script again."
    echo ""
    
    read -p "Do you want to proceed with installing dependencies? (y/n): " confirm
    if [[ ! "$confirm" =~ ^[Yy]$ ]]; then
        print_info "Installation cancelled."
        exit 0
    fi
    
    RUNNING_KERNEL=$(uname -r)
    print_info "Running kernel version: $RUNNING_KERNEL"
    
    # Build the package list - include kernel headers if missing
    local packages_to_install=("${missing_packages[@]}")
    
    # Always ensure kernel headers are included if kernel-devel is missing
    if [[ " ${missing_packages[@]} " =~ " kernel-devel-" ]]; then
        packages_to_install+=("kernel-headers-$RUNNING_KERNEL")
    fi
    
    # Layer packages using rpm-ostree
    print_info "Layering packages with rpm-ostree (this may take a few minutes)..."
    print_info "Installing: ${packages_to_install[*]}"
    
    # Use --allow-inactive to handle packages that might already be installed
    # but are being explicitly requested (this is safe on immutable systems)
    if ! sudo rpm-ostree install --allow-inactive "${packages_to_install[@]}"; then
        print_error "Failed to install packages!"
        print_info "You may need to install them manually:"
        print_info "  sudo rpm-ostree install ${packages_to_install[*]}"
        exit 1
    fi
    
    print_success "Packages layered successfully"
    echo ""
    print_warning "IMPORTANT: You must reboot your system for the packages to be available."
    print_info "After rebooting, run this script again to continue with the installation:"
    print_info "  ./install-bazzite.sh"
    echo ""
    print_info "To reboot now, run: sudo systemctl reboot"
    exit 0
}

# Check if dependencies are installed
check_dependencies() {
    print_info "Checking if dependencies are installed..."
    
    local missing_packages=()
    
    # Check for required commands
    if ! command -v gcc &> /dev/null; then
        missing_packages+=("gcc")
    fi
    if ! command -v make &> /dev/null; then
        missing_packages+=("make")
    fi
    if ! command -v cmake &> /dev/null; then
        missing_packages+=("cmake")
    fi
    
    # Check for kernel headers
    RUNNING_KERNEL=$(uname -r)
    KERNEL_BUILD_DIR="/lib/modules/$RUNNING_KERNEL/build"
    if [ ! -d "$KERNEL_BUILD_DIR" ]; then
        missing_packages+=("kernel-devel-$RUNNING_KERNEL")
    fi
    
    # Check for Qt6 - try multiple methods
    local qt6_found=false
    
    # Method 1: Check via pkg-config (if pkg-config is available)
    if command -v pkg-config &> /dev/null; then
        if pkg-config --exists Qt6Core 2>/dev/null; then
            qt6_found=true
        fi
    fi
    
    # Method 2: Check for Qt6 headers directly
    if [ "$qt6_found" = false ]; then
        if [ -f "/usr/include/qt6/QtCore/QtCore" ] || [ -f "/usr/include/Qt6/QtCore/QtCore" ] || [ -d "/usr/include/qt6" ] || [ -d "/usr/include/Qt6" ]; then
            qt6_found=true
        fi
    fi
    
    # Method 3: Check for qmake6 or cmake Qt6 finder
    if [ "$qt6_found" = false ]; then
        if command -v qmake6 &> /dev/null || command -v qmake-qt6 &> /dev/null; then
            qt6_found=true
        fi
    fi
    
    if [ "$qt6_found" = false ]; then
        missing_packages+=("qt6-qtbase-devel")
    fi
    
    # Check for libusb-1.0 via pkg-config
    if command -v pkg-config &> /dev/null; then
        if ! pkg-config --exists libusb-1.0 2>/dev/null; then
            missing_packages+=("libusb1-devel")
        fi
        
        # Check for hidapi-hidraw via pkg-config
        if ! pkg-config --exists hidapi-hidraw 2>/dev/null; then
            missing_packages+=("hidapi-devel")
        fi
    else
        # If pkg-config is not available, assume packages might be missing
        missing_packages+=("pkgconfig")
    fi
    
    if [ ${#missing_packages[@]} -gt 0 ]; then
        print_error "Missing dependencies: ${missing_packages[*]}"
        echo ""
        
        # Check if packages might be pending a reboot
        if rpm-ostree status 2>/dev/null | grep -q "PendingUpdate\|UpdatePending"; then
            print_warning "You have pending rpm-ostree updates that require a reboot."
            print_info "The missing dependencies may already be installed but not yet active."
            print_info ""
            print_info "Please reboot first:"
            print_info "  sudo systemctl reboot"
            print_info ""
            print_info "Then run this script again:"
            print_info "  ./install-bazzite.sh"
            exit 0
        fi
        
        print_info "Installing missing dependencies..."
        install_dependencies "${missing_packages[@]}"
        # install_dependencies will exit, so we won't reach here
    fi
    
    print_success "All dependencies are installed"
    return 0
}

# Verify kernel build environment
verify_kernel_build_env() {
    print_info "Verifying kernel build environment..."
    
    RUNNING_KERNEL=$(uname -r)
    KERNEL_BUILD_DIR="/lib/modules/$RUNNING_KERNEL/build"
    
    if [ ! -d "$KERNEL_BUILD_DIR" ]; then
        print_error "Kernel build directory not found: $KERNEL_BUILD_DIR"
        print_info "This usually means kernel-devel doesn't match your running kernel."
        print_info "Try: sudo rpm-ostree install kernel-devel-$(uname -r) && sudo systemctl reboot"
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
        print_info "Common fixes for Bazzite OS:"
        print_info "1. Ensure kernel-devel matches running kernel:"
        print_info "   sudo rpm-ostree install kernel-devel-\$(uname -r) && sudo systemctl reboot"
        print_info "2. After kernel updates, rebuild the module:"
        print_info "   cd kernel && make clean && make && sudo make install"
        exit 1
    fi
    
    if [ ! -f "Lian_Li_SL_INFINITY.ko" ]; then
        print_error "Kernel module build failed - .ko file not created!"
        exit 1
    fi
    
    # Install to writable location (immutable systems have read-only /lib/modules)
    print_info "Installing kernel module to writable location..."
    RUNNING_KERNEL=$(uname -r)
    MODULE_INSTALL_DIR="/usr/local/lib/modules/$RUNNING_KERNEL/extra"
    
    sudo mkdir -p "$MODULE_INSTALL_DIR"
    sudo cp Lian_Li_SL_INFINITY.ko "$MODULE_INSTALL_DIR/"
    
    # Update module dependencies (depmod looks in /usr/local/lib/modules too)
    print_info "Updating module dependencies..."
    sudo depmod -a
    
    # Load the module
    print_info "Loading kernel module..."
    sudo rmmod Lian_Li_SL_INFINITY 2>/dev/null || true
    
    # Try modprobe first (should find it in /usr/local/lib/modules)
    if ! sudo modprobe Lian_Li_SL_INFINITY 2>/dev/null; then
        print_warning "modprobe failed, trying insmod with full path..."
        if ! sudo insmod "$MODULE_INSTALL_DIR/Lian_Li_SL_INFINITY.ko" 2>/dev/null; then
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
    # Use /usr/local for immutable systems (writable location)
    print_info "Configuring build with CMake..."
    if ! cmake -DCMAKE_INSTALL_PREFIX=/usr/local ..; then
        print_error "CMake configuration failed!"
        print_info "Ensure Qt6 development packages are layered:"
        print_info "  sudo rpm-ostree install qt6-qtbase-devel qt6-qtcharts-devel && sudo systemctl reboot"
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
    
    # Check desktop file (check both /usr and /usr/local for compatibility)
    if [ -f "/usr/local/share/applications/lconnect3.desktop" ] || [ -f "/usr/share/applications/lconnect3.desktop" ]; then
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
    echo ""
    echo -e "${BOLD}=========================================="
    echo -e "  L-Connect 3 Installation Script"
    echo -e "  For Bazzite OS (Immutable System)"
    echo -e "==========================================${NC}"
    echo ""
    
    # Check if running on immutable system
    check_immutable_system
    
    check_sudo
    
    print_info "Starting installation process..."
    echo ""
    
    # Check if dependencies are installed
    if ! check_dependencies; then
        # install_dependencies will exit after prompting for reboot
        exit 0
    fi
    echo ""
    
    # Step 1: Install kernel driver
    install_kernel_driver
    echo ""
    
    # Step 2: Configure sensors
    configure_sensors
    echo ""
    
    # Step 3: Configure udev (RGB HID access)
    configure_udev
    echo ""
    
    # Step 4: Install application
    install_application
    echo ""
    
    # Step 5: Verify installation
    verify_installation
    echo ""
    
    print_success "Installation complete!"
    echo ""
    print_info "You can now:"
    echo "  - Launch the application: LLConnect3"
    echo "  - Control fans via /proc/Lian_li_SL_INFINITY/Port_X/fan_speed"
    echo "  - Check module status: lsmod | grep Lian_Li"
    echo ""
    
    print_warning "Bazzite OS Notes:"
    echo "  - After kernel updates via rpm-ostree, rebuild the module:"
    echo "    cd $KERNEL_DIR && make clean && make"
    echo "    sudo cp Lian_Li_SL_INFINITY.ko /usr/local/lib/modules/\$(uname -r)/extra/"
    echo "    sudo depmod -a && sudo modprobe -r Lian_Li_SL_INFINITY && sudo modprobe Lian_Li_SL_INFINITY"
    echo "  - The kernel module is installed to /usr/local/lib/modules/ (writable location)"
    echo "  - The kernel module persists across reboots once installed"
    echo "  - Application and udev rules persist across reboots"
    echo ""
}

# Run main function
main

