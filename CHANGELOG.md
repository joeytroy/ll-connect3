# Changelog

All notable changes to LL-Connect 3 are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

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
