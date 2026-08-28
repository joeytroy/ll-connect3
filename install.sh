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
USE_DKMS=0

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
    echo -e "  ${BOLD}4)${NC} Bazzite OS (immutable / rpm-ostree)"
    echo -e "  ${BOLD}5)${NC} CachyOS / Arch with clang kernel (builds with LLVM=1)"
    echo -e "  ${BOLD}6)${NC} Exit"
    echo ""
    
    while true; do
        read -p "Enter your choice [1-6]: " choice
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
                DISTRO_TYPE="bazzite"
                print_info "Selected: Bazzite OS (immutable system)"
                break
                ;;
            5)
                DISTRO_TYPE="cachyos"
                print_info "Selected: CachyOS / Arch with clang kernel (LLVM=1)"
                break
                ;;
            6)
                print_info "Installation cancelled."
                exit 0
                ;;
            *)
                print_error "Invalid option. Please enter 1, 2, 3, 4, 5, or 6."
                ;;
        esac
    done
    echo ""
}

# Check if running as root (we'll use sudo when needed)
# Ask how the kernel driver should be installed
select_driver_install_method() {
    if [[ "$DISTRO_TYPE" == "bazzite" ]]; then
        # Immutable /lib/modules — DKMS cannot install there; use the manual path.
        USE_DKMS=0
        return
    fi
    echo ""
    echo -e "${CYAN}Select how to install the kernel driver:${NC}"
    echo ""
    echo -e "  ${BOLD}1)${NC} DKMS (recommended) - rebuilt automatically on every kernel update"
    echo -e "  ${BOLD}2)${NC} Manual - built for the current kernel only; re-run this script after kernel updates"
    echo ""
    while true; do
        read -p "Enter your choice [1-2] (default 1): " dchoice
        case "${dchoice:-1}" in
            1) USE_DKMS=1; print_success "Selected: DKMS"; break ;;
            2) USE_DKMS=0; print_success "Selected: Manual install"; break ;;
            *) print_error "Invalid choice. Please enter 1 or 2." ;;
        esac
    done
}

check_sudo() {
    if ! sudo -n true 2>/dev/null; then
        print_info "This script requires sudo privileges. You may be prompted for your password."
        sudo -v
    fi
}

# --- Bazzite OS (immutable / rpm-ostree) support ---
check_immutable_system() {
    if ! command -v rpm-ostree &> /dev/null; then
        print_error "rpm-ostree not found. Option 4 is for Bazzite OS and other immutable Fedora-based systems."
        print_info "If you're on a regular Fedora system, please choose option 2 (RHEL-based) instead."
        exit 1
    fi
    print_success "Detected immutable system (rpm-ostree)"
    if rpm-ostree status 2>/dev/null | grep -q "PendingUpdate\|UpdatePending"; then
        print_warning "There are pending rpm-ostree updates that require a reboot."
        print_info "Run 'rpm-ostree status' to see details."
        read -p "Do you want to continue anyway? (y/n): " confirm
        if [[ ! "$confirm" =~ ^[Yy]$ ]]; then
            print_info "Please reboot first, then run this script again."
            exit 0
        fi
    fi
}

# Layer packages via rpm-ostree (exits after prompting to reboot)
install_bazzite_packages() {
    local packages=("$@")
    print_info "Installing dependencies for Bazzite OS..."
    print_warning "This will layer packages using rpm-ostree, which may require a reboot."
    echo ""
    read -p "Do you want to proceed? (y/n): " confirm
    if [[ ! "$confirm" =~ ^[Yy]$ ]]; then
        print_info "Installation cancelled."
        exit 0
    fi
    RUNNING_KERNEL=$(uname -r)
    local packages_to_install=("${packages[@]}")
    if [[ " ${packages[@]} " =~ " kernel-devel-" ]]; then
        packages_to_install+=("kernel-headers-$RUNNING_KERNEL")
    fi
    print_info "Layering: ${packages_to_install[*]}"
    if ! sudo rpm-ostree install --allow-inactive "${packages_to_install[@]}"; then
        print_error "Failed to install packages."
        exit 1
    fi
    print_success "Packages layered successfully"
    echo ""
    print_warning "IMPORTANT: You must reboot for the packages to be available."
    print_info "After rebooting, run this script again and choose option 4 (Bazzite OS):"
    print_info "  ./install.sh"
    print_info "To reboot now: sudo systemctl reboot"
    exit 0
}

# Check Bazzite dependencies; layer missing ones (may exit)
check_bazzite_dependencies() {
    print_info "Checking dependencies for Bazzite OS..."
    local missing=()
    command -v gcc &> /dev/null  || missing+=("gcc")
    command -v make &> /dev/null  || missing+=("make")
    command -v cmake &> /dev/null || missing+=("cmake")
    RUNNING_KERNEL=$(uname -r)
    [ -d "/lib/modules/$RUNNING_KERNEL/build" ] || missing+=("kernel-devel-$RUNNING_KERNEL")
    local qt6_found=false
    if command -v pkg-config &> /dev/null && pkg-config --exists Qt6Core 2>/dev/null; then qt6_found=true; fi
    if [ "$qt6_found" = false ] && { [ -d /usr/include/qt6 ] || [ -d /usr/include/Qt6 ]; }; then qt6_found=true; fi
    if [ "$qt6_found" = false ] && { command -v qmake6 &> /dev/null || command -v qmake-qt6 &> /dev/null; }; then qt6_found=true; fi
    [ "$qt6_found" = true ] || missing+=("qt6-qtbase-devel")
    if command -v pkg-config &> /dev/null; then
        pkg-config --exists libusb-1.0 2>/dev/null || missing+=("libusb1-devel")
        pkg-config --exists hidapi-hidraw 2>/dev/null || missing+=("hidapi-devel")
    else
        missing+=("pkgconfig")
    fi
    if [ ${#missing[@]} -gt 0 ]; then
        if rpm-ostree status 2>/dev/null | grep -q "PendingUpdate\|UpdatePending"; then
            print_warning "Pending rpm-ostree updates require a reboot. Missing deps may be installed but not active."
            print_info "Reboot first: sudo systemctl reboot"
            print_info "Then run ./install.sh again and choose option 4."
            exit 0
        fi
        install_bazzite_packages "${missing[@]}"
    fi
    print_success "All dependencies are installed"
}

install_bazzite_dependencies() {
    check_immutable_system
    check_bazzite_dependencies
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
    if [ "$USE_DKMS" = "1" ]; then
        sudo apt install -y dkms
    fi
    
    print_success "Dependencies installed"
}

# Install dependencies for RHEL-based systems
install_rhel_dependencies() {
    print_info "Installing dependencies for RHEL-based system..."
    
    # Check if running on immutable system (Bazzite OS)
    if command -v rpm-ostree &> /dev/null; then
        print_error "Detected immutable system (rpm-ostree)."
        print_info "Please run this script again and choose option 4 (Bazzite OS) instead of RHEL-based."
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
    if [ "$USE_DKMS" = "1" ]; then
        sudo pacman -S --needed --noconfirm dkms
    fi
    
    print_success "Dependencies installed"
}

# Install dependencies for CachyOS / Arch with clang kernel
install_cachyos_dependencies() {
    print_info "Installing dependencies for CachyOS / Arch with clang kernel..."

    sudo pacman -S --needed --noconfirm \
        base-devel \
        linux-headers \
        clang \
        llvm \
        lld \
        cmake \
        qt6-base \
        qt6-charts \
        lm_sensors \
        libusb \
        hidapi \
        pkgconf
    if [ "$USE_DKMS" = "1" ]; then
        sudo pacman -S --needed --noconfirm dkms
    fi

    print_success "Dependencies installed (including LLVM/clang toolchain)"
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
        bazzite)
            install_bazzite_dependencies
            ;;
        cachyos)
            install_cachyos_dependencies
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
        if [[ "$DISTRO_TYPE" == "bazzite" ]]; then
            print_info "On Bazzite OS, try:"
            print_info "  sudo rpm-ostree install kernel-devel-\$(uname -r) && sudo systemctl reboot"
        elif [[ "$DISTRO_TYPE" == "rhel" ]]; then
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

    # Pass USE_LLVM=1 for clang-built kernels (CachyOS)
    local MAKE_ARGS=""
    if [[ "$DISTRO_TYPE" == "cachyos" ]]; then
        MAKE_ARGS="USE_LLVM=1"
        print_info "Building with LLVM=1 for clang-built kernel..."
    fi
    
    # Build the module
    print_info "Building kernel module..."
    if ! make $MAKE_ARGS; then
        print_error "Kernel module build failed!"
        
        if [[ "$DISTRO_TYPE" == "cachyos" ]]; then
            print_info "Common fixes for CachyOS / clang kernel:"
            print_info "1. Ensure clang and llvm are installed:"
            print_info "   sudo pacman -S clang llvm lld"
            print_info "2. Ensure linux-headers matches your running kernel:"
            print_info "   sudo pacman -S linux-headers"
        elif [[ "$DISTRO_TYPE" == "rhel" ]]; then
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
    
    RUNNING_KERNEL=$(uname -r)
    
    if [[ "$DISTRO_TYPE" == "bazzite" ]]; then
        # Bazzite: install to writable location (immutable systems have read-only /lib/modules)
        print_info "Installing kernel module to writable location..."
        MODULE_INSTALL_DIR="/usr/local/lib/modules/$RUNNING_KERNEL/extra"
        sudo mkdir -p "$MODULE_INSTALL_DIR"
        sudo cp Lian_Li_SL_INFINITY.ko "$MODULE_INSTALL_DIR/"
        print_info "Updating module dependencies..."
        sudo depmod -a
        print_info "Loading kernel module..."
        sudo rmmod Lian_Li_SL_INFINITY 2>/dev/null || true
        if ! sudo modprobe Lian_Li_SL_INFINITY 2>/dev/null; then
            print_warning "modprobe failed, trying insmod with full path..."
            if ! sudo insmod "$MODULE_INSTALL_DIR/Lian_Li_SL_INFINITY.ko" 2>/dev/null; then
                print_error "Failed to load kernel module!"
                print_info "Check dmesg for errors: sudo dmesg | tail -20"
                exit 1
            fi
        fi
    elif [ "$USE_DKMS" = "1" ]; then
        # DKMS: register source so the module is rebuilt on every kernel update
        if ! command -v dkms &> /dev/null; then
            print_error "dkms command not found. Install the 'dkms' package or choose the manual install."
            exit 1
        fi
        print_info "Registering kernel module with DKMS..."
        if ! make $MAKE_ARGS dkms-install; then
            print_error "DKMS build/install failed!"
            print_info "Check the DKMS log: /var/lib/dkms/lian-li-sl-infinity/1.0/build/make.log"
            exit 1
        fi
        print_info "Loading kernel module..."
        sudo rmmod Lian_Li_SL_INFINITY 2>/dev/null || true
        if ! sudo modprobe Lian_Li_SL_INFINITY 2>/dev/null; then
            print_error "Failed to load kernel module!"
            print_info "Check dmesg for errors: sudo dmesg | tail -20"
            exit 1
        fi
    else
        # Standard: install using the Makefile (which handles depmod)
        print_info "Installing kernel module to system..."
        make $MAKE_ARGS install
        print_info "Loading kernel module..."
        sudo rmmod Lian_Li_SL_INFINITY 2>/dev/null || true
        if ! sudo modprobe Lian_Li_SL_INFINITY 2>/dev/null; then
            print_warning "modprobe failed, trying insmod..."
            if ! sudo insmod Lian_Li_SL_INFINITY.ko 2>/dev/null; then
                print_error "Failed to load kernel module!"
                print_info "Check dmesg for errors: sudo dmesg | tail -20"
                exit 1
            fi
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
    
    # Configure with CMake (use /usr/local for Bazzite/immutable)
    local CMAKE_PREFIX="/usr"
    if [[ "$DISTRO_TYPE" == "bazzite" ]]; then
        CMAKE_PREFIX="/usr/local"
    fi
    print_info "Configuring build with CMake..."
    if ! cmake -DCMAKE_INSTALL_PREFIX="$CMAKE_PREFIX" ..; then
        print_error "CMake configuration failed!"
        if [[ "$DISTRO_TYPE" == "rhel" ]]; then
            print_info "On RHEL-based systems, ensure Qt6 development packages are installed:"
            print_info "  sudo dnf install qt6-qtbase-devel qt6-qtcharts-devel"
        fi
        if [[ "$DISTRO_TYPE" == "bazzite" ]]; then
            print_info "On Bazzite, ensure Qt6 is layered: sudo rpm-ostree install qt6-qtbase-devel && reboot"
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
    
    # Check desktop file (/usr or /usr/local for Bazzite)
    if [ -f "/usr/share/applications/lconnect3.desktop" ] || [ -f "/usr/local/share/applications/lconnect3.desktop" ]; then
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
    select_driver_install_method
    
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
    
    if [[ "$DISTRO_TYPE" == "bazzite" ]]; then
        print_warning "Bazzite OS notes:"
        echo "  - After kernel updates via rpm-ostree, rebuild the module:"
        echo "    cd $KERNEL_DIR && make clean && make"
        echo "    sudo cp Lian_Li_SL_INFINITY.ko /usr/local/lib/modules/\$(uname -r)/extra/"
        echo "    sudo depmod -a && sudo modprobe -r Lian_Li_SL_INFINITY && sudo modprobe Lian_Li_SL_INFINITY"
        echo "  - Kernel module is in /usr/local/lib/modules/ (writable)"
        echo ""
    elif [ "$USE_DKMS" = "1" ]; then
        print_info "Kernel driver is managed by DKMS - it is rebuilt automatically on kernel updates."
        echo "  - Check status any time with: dkms status | grep lian-li"
        echo ""
    elif [[ "$DISTRO_TYPE" == "cachyos" ]]; then
        print_warning "CachyOS / clang kernel notes:"
        echo "  - After kernel updates, rebuild the module with:"
        echo "    cd $KERNEL_DIR && make clean && make USE_LLVM=1 && sudo make USE_LLVM=1 install"
        echo "  - The USE_LLVM=1 flag is required for clang-built kernels"
        echo ""
    elif [[ "$DISTRO_TYPE" == "rhel" ]]; then
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
