# Changelog

All notable changes to LL-Connect 3 are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

---

## [1.2.0] - 2026-02-28 — Lighting overhaul, real RPM, version check & driver fixes

### Added
- **3 new lighting effects**: Color Cycle (3 colors, direction), Render (4 colors, direction), and Stack Multi Color (no colors, direction) — now 17 effects total, matching the full OpenRGB mode set for this device.
- **Correct per-effect color controls**: Effects now show the right number of color buttons matching the hardware protocol — per-port colors for Static/Breathing, 2 mode-specific colors for Staggered/Tide/Runway/Mixing, 3 for Color Cycle, 4 for Tunnel/Render, 1 for Stack/Meteor/Groove, and no color controls for Rainbow Wave/Spectrum Cycle/Neon/Voice/Stack Multi Color.
- **Real fan RPM from hardware**: Kernel driver now queries the hub via USB GET_REPORT (Input Report 0xe0) and exposes actual RPM per port at `/proc/Lian_li_SL_INFINITY/Port_X/fan_rpm` (read-only). The app reads these instead of faking RPM from the curve.
- **Update checker**: New "Check for updates on startup" toggle in Settings (on by default). On launch the app fetches `VERSION.md` from GitHub, compares with the running version using semantic versioning, and offers to open the releases page when an update is available.
- **CachyOS / Clang kernel support**: New installer option (5) for CachyOS and Arch systems built with a Clang kernel. Passes `LLVM=1` to the kernel module build automatically.

### Fixed
- **Lighting color data layout**: The interleaved 72-byte color pattern used by all multi-color effects (Staggered, Tide, Runway, Mixing, Meteor, Groove, Color Cycle, Tunnel, Render) was incorrectly treating OpenRGB's byte offsets as LED indices and multiplying by 3, spacing the data 3x too wide. The hardware received garbled color data. Now matches OpenRGB's `SetChannelMode` exactly.
- **Color button enable state**: Mode-specific color buttons (Color 1/2/3/4) were incorrectly disabled based on which physical fan ports were connected. Now only per-port effects (Static, Breathing) disable buttons for empty ports; mode-specific buttons are always enabled.
- **LED settings persistence**: `resetToDefaults()` now explicitly saves via `saveLightingSettings()` so reset values survive a restart. Added `m_isLoading` guard and `blockSignals()` in `loadLightingSettings()` to prevent spurious saves during load. `onSpeedChanged()` now saves. `onDeviceConnected()` now reapplies the current effect to hardware.
- **Fan speed commit command**: Kernel driver now sends the `0xe0 0x50` commit command after each per-port speed write, matching the protocol observed in Windows USB captures. Without this the hub firmware may not apply buffered speed changes.
- **RPM query via direct USB**: The initial `hid_hw_raw_request(HID_INPUT_REPORT)` returned `-EAGAIN` because the Linux HID subsystem routes Input reports through the interrupt endpoint. Replaced with a direct `usb_control_msg()` to bypass this restriction.

### Changed
- **Version**: Bumped to 1.2.0.

---

## [1.1.0] - 2026-02-25 — Fan profile controls & versioning

### Added
- **CPU/GPU fan control source**: New selector on the Fan Profile page to choose whether curves follow CPU or GPU temperature, with GPU temperature support for NVIDIA/AMD/Intel (falling back gracefully when unavailable).
- **Per-port target temp/RPM editor**: Numeric controls for temperature (°C/°F, following Settings) and RPM per fan port, wired into the custom fan curves so you can set concrete points like “50 °C → 1000 RPM” and have the curve update accordingly.
- **Custom port names**: Port column in the fan table is now editable, allowing per-port labels (e.g. "CPU Rad", "Top Intake"), persisted across sessions.
- **Global °C/°F toggle**: New fan temperature unit option on the Settings page; Fan Profile table, Temp editor, curve X‑axis, and System Info CPU/GPU temperatures all respect the selected unit.

### Changed
- **Versioning**: Application version is now read from `VERSION.md` at build time; a generated header drives `QApplication::applicationVersion()` and the sidebar version label so they always match the current build.
- **Installer**: Bazzite OS support is integrated into the main `install.sh` menu (option 4), and the dedicated `install-bazzite.sh` script has been removed.

### Fixed
- **Version label**: Sidebar version text now reflects the actual application version instead of a hardcoded `v1.0.0`.

---

## [1.0.0] - 2025-12-26 — Bazzite & UI polish

### Added
- **Bazzite OS support**: Installer for immutable (rpm-ostree) systems.
  - Kernel module installed to `/usr/local/lib/modules/` (writable location).
  - Application installed to `/usr/local` prefix.
  - Installs only missing dependencies; proper checks for `libusb1-devel` and `hidapi-devel`.
  - Uses `--allow-inactive` flag for `rpm-ostree install`.
- **Multi-distro installer**: Interactive menu to select distribution type (Debian, RHEL, Arch).
- **Minimize to tray**: App can minimize to system tray instead of closing.
- **Kernel logging toggle**: In-app toggle to enable/disable kernel module logging; hardened debug print behavior.
- **Logo light/dark mode**: Logo adapts to light and dark themes; logo fix for correct display.

### Changed
- Updated Bazzite installer script (`install-bazzite.sh`) for immutable system constraints.

### Fixed
- Logo display in light and dark mode.

---

## 2025-11-03 — Lighting, docs & packaging

### Added
- **Per-port RGB colors**: Per-port color control for lighting.
- **Per-port breathing colors**: Breathing effect configurable per port.
- **Lighting debug toggle**: UI option to aid lighting debugging.
- **Lighting persistence**: Lighting settings are saved and restored.
- **Desktop integration**: Desktop file and install rules for Application Launcher (e.g. KDE/GNOME).
- **Install/uninstall scripts**: `install.sh` and `uninstall.sh` for driver + app.
- **Auto-load kernel module**: Module loads on boot (`Lian_Li_SL_INFINITY`).

### Changed
- **Executable rename**: Application binary renamed to **LLConnect3**.
- **README overhaul**: Clearer install steps, supported distros, and usage.
- **System Info page**: Text moved lower below circles; reduced bottom spacing.
- **Fan profile assignment**: Correct profile tracked and displayed per port.

### Fixed
- **RGB lighting**: Added udev rule and improved HID device detection.
- **Static color**: Correct behavior for all 4 ports with proper dual-channel mapping.
- **Demo display**: Shows all 4 fans correctly.

---

## 2025-11-01

### Added
- Desktop file and install rules for Application Launcher integration.

---

## 2025-10-20

### Changed
- README updates.

---

## 2025-10-05 — Fan profiles & static color

### Added
- **Static Color lighting**: Static color mode for RGB control.
- **Customizable fan profiles**: User-defined fan profiles.
- **Per-port fan size**: Configuration for fan size per port.
- **Per-port custom fan curves**: Custom curves with dBA-calibrated defaults.

### Changed
- Documentation updated for new lighting and fan features.

---

## 2025-10-04

### Added
- **Manual fan configuration UI**: UI for manual fan setup.
- **Fan profile display**: Improved display of fan profiles.
- **Fan-only kernel driver**: Kernel driver focused on fan control with OpenRGB compatibility.

---

## 2025-10-02 — Protocol & per-port control

### Added
- **Protocol documentation**: Wireshark-based protocol documentation.
- **Individual port control**: Per-port control for SL-INFINITY fan hub.

---

## 2025-09-30 — Initial driver & app

### Added
- **L-Connect3 Linux driver and application**: Complete Linux support for Lian Li SL-Infinity hub.
- **Lian Li SL-Infinity fan control**: Kernel driver and Qt app for fan and lighting control.

---

## 2025-09-29 — Project bootstrap

### Added
- **Qt main window**: Qt Creator–compatible main window UI.
- **GPLv2 license**: Project under GPLv2; GitHub repository references updated.

### Changed
- Build files cleaned up; proper `.gitignore` added.

### Fixed
- CMake conditional target include directories corrected.

### Notes
- Initial commit: L-Connect3 Linux — native Lian Li device control application.

---

*Changelog generated from git history. For exact commits, see the repository log.*
