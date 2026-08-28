# Lian Li SL-INFINITY (ENE 0cf2:a102) — Fan Control Driver

**Fan-Only Driver** - RGB control handled by OpenRGB to avoid conflicts.

## Quick Start

```bash
# Build the module
cd kernel
make

# Install and load
make install

# Or manually reload for testing
make reload
```

## Build & Install

### Option 0: DKMS (Recommended — survives kernel updates)
A plain `make install` copies the module into `/lib/modules/$(uname -r)/extra/`
for the *current* kernel only. After the next kernel update the module is gone
and the fans stop until you rebuild. DKMS rebuilds it automatically on every
kernel install:

```bash
sudo apt install dkms          # Debian/Ubuntu
cd kernel
make dkms-install
dkms status                    # should list lian-li-sl-infinity/1.0 for your kernel
```

To remove: `make dkms-uninstall`. When bumping the driver version, update
`PACKAGE_VERSION` in `dkms.conf` and re-run `make dkms-install`.

### Option 1: Using Makefile (current kernel only)
```bash
# Build the module
make

# Install to system
make install

# Reload the module
make reload

# Uninstall
make uninstall

# Clean build files
make clean
```

### Option 2: Manual Installation
```bash
# 1. Build the module
make

# 2. Copy it into the kernel module tree
sudo install -m 644 Lian_Li_SL_INFINITY.ko /lib/modules/$(uname -r)/extra/

# 3. Refresh module dependencies
sudo depmod -a

# 4. Remove any old version (ignore error if not loaded)
sudo rmmod Lian_Li_SL_INFINITY 2>/dev/null || true

# 5. Load the new module
sudo modprobe Lian_Li_SL_INFINITY
```

## Fan Configuration

**Important**: The hardware does not provide feedback about which ports have fans connected. You must manually configure which ports have fans.

### Using the L-Connect Application (Recommended)
The L-Connect application provides a user-friendly interface to configure which ports have fans. This is the easiest method and settings are saved automatically.

### Manual Configuration via Command Line
You can also configure fan ports using the proc filesystem:

```bash
# Configure which ports have fans (1 = connected, 0 = not connected)
echo 1 > /proc/Lian_li_SL_INFINITY/Port_1/fan_config  # Port 1 has a fan
echo 1 > /proc/Lian_li_SL_INFINITY/Port_2/fan_config  # Port 2 has a fan
echo 0 > /proc/Lian_li_SL_INFINITY/Port_3/fan_config  # Port 3 is empty
echo 1 > /proc/Lian_li_SL_INFINITY/Port_4/fan_config  # Port 4 has a fan

# Check current configuration
for i in {1..4}; do 
  echo "Port $i: $(cat /proc/Lian_li_SL_INFINITY/Port_$i/fan_connected)"
done
```

**Note**: Configuration is not persistent across reboots when using command line. Use the L-Connect application for persistent settings.

## Testing the Fans

```bash
# Set per-port speed (0-100%) - no sudo needed!
echo 100 > /proc/Lian_li_SL_INFINITY/Port_1/fan_speed
echo 75 > /proc/Lian_li_SL_INFINITY/Port_2/fan_speed
echo 50 > /proc/Lian_li_SL_INFINITY/Port_3/fan_speed
echo 25 > /proc/Lian_li_SL_INFINITY/Port_4/fan_speed

# Read back current speed settings
cat /proc/Lian_li_SL_INFINITY/Port_1/fan_speed
cat /proc/Lian_li_SL_INFINITY/Port_2/fan_speed
cat /proc/Lian_li_SL_INFINITY/Port_3/fan_speed
cat /proc/Lian_li_SL_INFINITY/Port_4/fan_speed
```

## Verify Installation

```bash
# Check that /proc entries exist
ls -R /proc/Lian_li_SL_INFINITY

# Check kernel logs
sudo dmesg | grep -i "sli" | tail -20

# Check module is loaded
lsmod | grep Lian_Li
```

## Troubleshooting

### Module Not Loading or HID Errors

If you see errors like "Function not implemented" (-38) or "Broken pipe" (-32), the module may need to be completely unloaded and reloaded:

```bash
# 1. Remove the old module
sudo rmmod Lian_Li_SL_INFINITY

# 2. Load the new module directly (not via modprobe)
cd /home/dev/Documents/GitHub/ll-connect3/kernel
sudo insmod Lian_Li_SL_INFINITY.ko

# 3. Test it works
echo 50 > /proc/Lian_li_SL_INFINITY/Port_1/fan_speed
sudo dmesg | grep -i "sli" | tail -10
```

**Note**: If the module was previously installed via `make install`, you may need to reboot or use `insmod` instead of `modprobe` to ensure the new version is loaded.

### Check Kernel Logs
```bash
# View driver messages
sudo dmesg | grep -i "sli" | tail -20

# Check for errors
sudo dmesg | grep -i "sli.*fail\|sli.*error" | tail -10
```

### Reload Module
```bash
# Quick reload for testing
make reload

# Or manually
sudo rmmod Lian_Li_SL_INFINITY
sudo insmod Lian_Li_SL_INFINITY.ko

# Or if installed via make install
sudo rmmod Lian_Li_SL_INFINITY
sudo modprobe Lian_Li_SL_INFINITY
```

### Complete Clean Reinstall

If you're having persistent issues:

```bash
# 1. Remove old module
sudo rmmod Lian_Li_SL_INFINITY 2>/dev/null || true

# 2. Remove from system
sudo rm -f /lib/modules/$(uname -r)/extra/Lian_Li_SL_INFINITY.ko
sudo depmod -a

# 3. Rebuild and install fresh
cd /home/dev/Documents/GitHub/ll-connect3/kernel
make clean
make
sudo insmod Lian_Li_SL_INFINITY.ko

# 4. If that works, install permanently
make install
```

### After System Updates

After kernel updates, you'll need to rebuild the module:

```bash
cd /home/dev/Documents/GitHub/ll-connect3/kernel
make clean
make
make install
```

### Check Port Status
```bash
# View port information
for i in {1..4}; do
  echo "=== Port $i ==="
  echo "Connected: $(cat /proc/Lian_li_SL_INFINITY/Port_$i/fan_connected)"
  echo "Speed: $(cat /proc/Lian_li_SL_INFINITY/Port_$i/fan_speed)%"
  echo ""
done
```

### Permissions Issues
If you get permission errors when writing to proc files:

```bash
# Check file permissions
ls -la /proc/Lian_li_SL_INFINITY/Port_1/

# The driver creates files with 0666 permissions (read/write for all users)
# If you still have issues, the module may not be loaded correctly
```

## Key Features

- **Individual Port Control**: Each port controlled independently (0-100%)
- **Manual Configuration**: Configure which ports have fans via L-Connect UI or command line
- **User-Friendly Permissions**: No sudo needed for fan control
- **OpenRGB Compatible**: RGB control via OpenRGB (no conflicts)
- **Simple & Reliable**: Clean driver focused on fan speed control

## Protocol Details

**Individual Port Control Commands:**
```
Port 1: e0 20 00 <duty> 00 00 00
Port 2: e0 21 00 <duty> 00 00 00  
Port 3: e0 22 00 <duty> 00 00 00
Port 4: e0 23 00 <duty> 00 00 00
```
Where `<duty>` is the percentage (0-100) directly.

## Why Manual Configuration?

The Lian Li SL-Infinity hardware does not provide feedback about whether a fan is physically connected to a port. Commands are accepted successfully regardless of fan presence. Therefore, users must manually configure which ports have fans using either:

1. **L-Connect Application** (recommended) - User-friendly UI with persistent settings
2. **Command Line** - Direct proc filesystem access (not persistent across reboots)

This is a hardware limitation, not a driver limitation.