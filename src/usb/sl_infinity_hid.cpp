/*---------------------------------------------------------*\
|| sl_infinity_hid.cpp                                     |
||                                                         |
||   HID Controller for Lian Li SL Infinity (simplified)  |
||   Based on OpenRGB implementation                       |
||                                                         |
||   This file is part of the L-Connect project           |
||   SPDX-License-Identifier: GPL-2.0-or-later            |
\*---------------------------------------------------------*/

#include "sl_infinity_hid.h"
#include "../utils/debugutil.h"
#include <iostream>
#include <cstring>
#include <fstream>
#include <chrono>
#include <thread>
#include <algorithm>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <limits.h>

using namespace std::chrono_literals;

// HID Device Implementation
bool HIDDevice::Open(const std::string& devicePath) {
    Close();
    path = devicePath;
    
    fd = open(devicePath.c_str(), O_RDWR);
    if (fd < 0) {
        return false;
    }
    
    isOpen = true;
    return true;
}

void HIDDevice::Close() {
    if (fd >= 0) {
        close(fd);
        fd = -1;
    }
    isOpen = false;
}

bool HIDDevice::Write(const uint8_t* data, size_t length) {
    if (!isOpen || fd < 0) {
        return false;
    }
    
    ssize_t result = write(fd, data, length);
    return result == static_cast<ssize_t>(length);
}

// SL Infinity HID Controller Implementation
SLInfinityHIDController::SLInfinityHIDController() {
}

SLInfinityHIDController::~SLInfinityHIDController() {
    Close();
}

bool SLInfinityHIDController::Initialize() {
    if (!FindDevice()) {
        std::cerr << "SL Infinity device not found" << std::endl;
        return false;
    }
    
    m_deviceName = "Lian Li UNI HUB SL Infinity";
    m_firmwareVersion = "Unknown";
    m_serialNumber = "Unknown";
    
    return true;
}

void SLInfinityHIDController::Close() {
    m_device.Close();
}

bool SLInfinityHIDController::IsConnected() const {
    return m_device.IsOpen();
}

std::string SLInfinityHIDController::GetDeviceName() const {
    return m_deviceName;
}

std::string SLInfinityHIDController::GetFirmwareVersion() const {
    return m_firmwareVersion;
}

std::string SLInfinityHIDController::GetSerialNumber() const {
    return m_serialNumber;
}

static bool readSmallFile(const std::string& path, std::string& out) {
    std::ifstream f(path);
    if (!f.is_open()) return false;
    std::getline(f, out);
    // trim
    while (!out.empty() && (out.back()=='\n' || out.back()=='\r' || out.back()==' ')) out.pop_back();
    return true;
}

bool SLInfinityHIDController::FindDevice() {
    // Match by VID/PID via sysfs to select the correct hidraw node
    const std::string targetVid = "0cf2";
    const std::string targetPid = "a102";

    for (int i = 0; i < 32; i++) {
        std::string hidraw = "/dev/hidraw" + std::to_string(i);
        std::string sysBase = "/sys/class/hidraw/hidraw" + std::to_string(i) + "/device";

        // Walk up to find idVendor/idProduct
        std::string current = sysBase;
        for (int up = 0; up < 6; ++up) {
            std::string vidPath = current + "/idVendor";
            std::string pidPath = current + "/idProduct";
            std::string vid, pid;
            if (readSmallFile(vidPath, vid) && readSmallFile(pidPath, pid)) {
                // Lowercase for safety
                std::transform(vid.begin(), vid.end(), vid.begin(), ::tolower);
                std::transform(pid.begin(), pid.end(), pid.begin(), ::tolower);
                if (vid == targetVid && pid == targetPid) {
                    if (m_device.Open(hidraw)) {
                        return true;
                    }
                }
            }
            current += "/..";
        }
    }

    return false;
}

bool SLInfinityHIDController::SendStartAction(uint8_t channel, uint8_t numFans) {
    if (!m_device.IsOpen()) {
        return false;
    }

    uint8_t usb_buf[65];
    memset(usb_buf, 0x00, sizeof(usb_buf));

    usb_buf[0x00] = 0xE0;  // Transaction ID
    usb_buf[0x01] = 0x10;
    usb_buf[0x02] = 0x60;
    usb_buf[0x03] = 1 + (channel / 2); // Every fan-array uses two channels
    usb_buf[0x04] = 0x04; // Number of fans (hardcoded to 4 like OpenRGB)

    bool result = m_device.Write(usb_buf, sizeof(usb_buf));
    std::this_thread::sleep_for(5ms);
    return result;
}

bool SLInfinityHIDController::SendColorData(uint8_t channel, uint8_t numLeds, const uint8_t* ledData) {
    if (!m_device.IsOpen()) {
        return false;
    }

    uint8_t usb_buf[353];
    memset(usb_buf, 0x00, sizeof(usb_buf));

    usb_buf[0x00] = 0xE0;  // Transaction ID
    usb_buf[0x01] = 0x30 + channel; // Action + channel (30 = channel 1, 31 = channel 2, etc.)

    // Copy color data bytes (limit to buffer size)
    size_t dataSize = std::min(static_cast<size_t>(numLeds * 3), sizeof(usb_buf) - 2);
    memcpy(&usb_buf[0x02], ledData, dataSize);

    bool result = m_device.Write(usb_buf, sizeof(usb_buf));
    std::this_thread::sleep_for(5ms);
    return result;
}

bool SLInfinityHIDController::SendCommitAction(uint8_t channel, uint8_t effect, uint8_t speed, uint8_t direction, uint8_t brightness) {
    if (!m_device.IsOpen()) {
        return false;
    }

    uint8_t usb_buf[65];
    memset(usb_buf, 0x00, sizeof(usb_buf));

    usb_buf[0x00] = 0xE0;  // Transaction ID
    usb_buf[0x01] = 0x10 + channel; // Channel + device (10 = channel 1, 11 = channel 2, etc.)
    usb_buf[0x02] = effect;         // Effect
    usb_buf[0x03] = speed;          // Speed
    usb_buf[0x04] = direction;      // Direction
    usb_buf[0x05] = brightness;     // Brightness

    DEBUG_PRINTF("SendCommitAction: channel=%d, effect=0x%02X, speed=0x%02X, direction=0x%02X, brightness=0x%02X\n", 
                 channel, effect, speed, direction, brightness);

    bool result = m_device.Write(usb_buf, sizeof(usb_buf));
    std::this_thread::sleep_for(5ms);
    return result;
}

void SLInfinityHIDController::ApplyColorLimiter(SLInfinityColor& color) const {
    // Apply color limiter to protect LEDs (from OpenRGB)
    if ((color.r + color.b + color.g) > 460) {
        float scale = 460.0f / (color.r + color.b + color.g);
        color.r = static_cast<uint8_t>(color.r * scale);
        color.b = static_cast<uint8_t>(color.b * scale);
        color.g = static_cast<uint8_t>(color.g * scale);
    }
}

float SLInfinityHIDController::CalculateBrightnessLimit(const SLInfinityColor& color) const {
    if ((color.r + color.b + color.g) > 460) {
        return 460.0f / (color.r + color.b + color.g);
    }
    return 1.0f;
}

bool SLInfinityHIDController::SetChannelColors(uint8_t channel, const std::vector<SLInfinityColor>& colors, float brightness, bool interleavedPattern) {
    DEBUG_PRINTF("SetChannelColors: channel=%d, colors.size()=%zu, brightness=%f, interleavedPattern=%d\n", channel, colors.size(), brightness, interleavedPattern);
    
    if (!m_device.IsOpen() || channel >= 8) {
        DEBUG_PRINTF("SetChannelColors: Device not open or invalid channel\n");
        return false;
    }

    // Prepare LED data buffer
    // OpenRGB uses (num_fans + 1) * 16 LEDs, so for 4 fans that's 80 LEDs
    // But we'll use 64 LEDs for 4 fans to match the hardware
    uint8_t led_data[80 * 3]; // 80 LEDs * 3 bytes per LED (max for 5 fans)
    memset(led_data, 0x00, sizeof(led_data));

    if (colors.empty()) {
        // No colors - fill with black
        memset(led_data, 0x00, sizeof(led_data));
    } else if (colors.size() == 1) {
        // Single color - fill all LEDs with this color
        // IMPORTANT: Don't apply color limiter first - calculate brightness scale using original color
        // then apply both brightness and limiter together (like OpenRGB does)
        SLInfinityColor color = colors[0];  // Keep original for brightness calculation
        
        // Calculate brightness scale like OpenRGB: brightness * infinityBrightnessLimit(color)
        // Use ORIGINAL color values (before limiting) for the brightness limit calculation
        float infinityBrightnessLimit = 1.0f;
        if ((color.r + color.b + color.g) > 460) {
            infinityBrightnessLimit = 460.0f / (color.r + color.b + color.g);
        }
        float color_brightness_scale = brightness * infinityBrightnessLimit;
        
        for (int i = 0; i < 64; i++) {
            int led_idx = i * 3;
            // Apply brightness and limiter together (like OpenRGB)
            led_data[led_idx + 0] = (unsigned char)(color.r * color_brightness_scale);  // Red
            led_data[led_idx + 1] = (unsigned char)(color.b * color_brightness_scale);  // Blue (RBG format!)
            led_data[led_idx + 2] = (unsigned char)(color.g * color_brightness_scale);  // Green
        }
        
        DEBUG_PRINTF("SetChannelColors: Set 1 color (single, solid) for channel %d: Color=%d,%d,%d brightness=%f (scale=%f, limit=%f)\n", 
                     channel, color.r, color.g, color.b, brightness, color_brightness_scale, infinityBrightnessLimit);
    } else if (colors.size() == 4 && !interleavedPattern) {
        // Solid per-fan pattern for Static mode
        // Fan 0 = LEDs 0-15, Fan 1 = LEDs 16-31, Fan 2 = LEDs 32-47, Fan 3 = LEDs 48-63
        std::vector<SLInfinityColor> colorArray = colors;
        for (int i = 0; i < 4; i++) {
            ApplyColorLimiter(colorArray[i]);
        }
        
        for (int fan = 0; fan < 4; fan++) {
            SLInfinityColor color = colorArray[fan];
            int base = fan * 16 * 3;
            for (int led = 0; led < 16; led++) {
                int idx = base + led * 3;
                led_data[idx + 0] = color.r;
                led_data[idx + 1] = color.b;
                led_data[idx + 2] = color.g;
            }
        }
        
        memset(&led_data[64 * 3], 0x00, 16 * 3);
        
        DEBUG_PRINTF("SetChannelColors: Set 4 fan colors (solid per fan) for channel %d\n", channel);
    } else if (colors.size() >= 2 && colors.size() <= 4) {
        // 2, 3, or 4 mode-specific colors — OpenRGB interleaved 72-byte pattern
        // All mode-specific effects use the SAME interleaved format:
        //   colors are resized to 4 (padded with black), then laid out as
        //   fan_led_data[ (i*12) + (j*3) + 0/1/2 ] = R/B/G
        //   where i=0..5 (6 slots) and j=0..3 (4 colors)
        // CRITICAL: (i*12)+(j*3) is a BYTE offset into led_data, NOT an LED index.
        
        SLInfinityColor colorArray[4] = {
            colors[0],
            colors.size() > 1 ? colors[1] : SLInfinityColor(),
            colors.size() > 2 ? colors[2] : SLInfinityColor(),
            colors.size() > 3 ? colors[3] : SLInfinityColor()
        };
        
        memset(led_data, 0x00, 80 * 3);
        
        for (unsigned int j = 0; j < 4; j++) {
            float limit = 1.0f;
            int sum = colorArray[j].r + colorArray[j].b + colorArray[j].g;
            if (sum > 460) limit = 460.0f / sum;
            float scale = brightness * limit;
            
            for (unsigned int i = 0; i < 6; i++) {
                int byteOff = (i * 12) + (j * 3);
                led_data[byteOff + 0] = (unsigned char)(colorArray[j].r * scale);
                led_data[byteOff + 1] = (unsigned char)(colorArray[j].b * scale);
                led_data[byteOff + 2] = (unsigned char)(colorArray[j].g * scale);
            }
        }
        
        DEBUG_PRINTF("SetChannelColors: Set %zu colors (interleaved 72-byte) for channel %d, brightness=%f\n",
                     colors.size(), channel, brightness);
    } else {
        // Multiple colors (5+ colors) - distribute them by cycling
        // For Static/Breathing with up to 6 colors, cycle through them
        for (int i = 0; i < 64; i++) {
            SLInfinityColor color = colors[i % colors.size()];
            ApplyColorLimiter(color);
            
            int led_idx = i * 3;
            led_data[led_idx + 0] = color.r;  // Red
            led_data[led_idx + 1] = color.b;  // Blue (RBG format!)
            led_data[led_idx + 2] = color.g;  // Green
        }
    }

    // Send start action - OpenRGB passes (num_fans + 1) but ignores it and hardcodes usb_buf[0x04] = 0x04
    // For 4 fans, OpenRGB calculates: fan_idx = (leds_count/16 - 1) = 3, then passes (fan_idx + 1) = 4
    // But SendStartAction ignores the parameter and hardcodes 4
    DEBUG_PRINTF("SetChannelColors: Sending start action for channel %d\n", channel);
    if (!SendStartAction(channel, 4)) {  // Pass 4 (OpenRGB ignores this and hardcodes it anyway)
        DEBUG_PRINTF("SetChannelColors: SendStartAction failed for channel %d\n", channel);
        return false;
    }

    // Send color data - OpenRGB sends (num_fans + 1) * 16 = 80 LEDs for 4 fans
    // This matches OpenRGB's SendColorData call exactly
    int num_leds_to_send = 80; // (num_fans + 1) * 16 = 80 for 4 fans (matches OpenRGB)
    DEBUG_PRINTF("SetChannelColors: Sending color data for channel %d (%d LEDs)\n", channel, num_leds_to_send);
    if (!SendColorData(channel, num_leds_to_send, led_data)) {
        DEBUG_PRINTF("SetChannelColors: SendColorData failed for channel %d\n", channel);
        return false;
    }

    DEBUG_PRINTF("SetChannelColors: Success for channel %d\n", channel);
    return true;
}

bool SLInfinityHIDController::SetChannelMode(uint8_t channel, uint8_t mode) {
    DEBUG_PRINTF("SetChannelMode: channel=%d, mode=0x%02X\n", channel, mode);
    
    if (!m_device.IsOpen() || channel >= 8) {
        DEBUG_PRINTF("SetChannelMode: Device not open or invalid channel\n");
        return false;
    }

    // Send commit action with the specified mode
    bool result = SendCommitAction(channel, mode, 0x00, 0x00, 0x00); // Static color mode
    DEBUG_PRINTF("SetChannelMode: result=%s for channel %d\n", result ? "success" : "failed", channel);
    return result;
}

bool SLInfinityHIDController::TurnOffChannel(uint8_t channel) {
    if (!m_device.IsOpen() || channel >= 8) {
        return false;
    }

    // Set black color
    std::vector<SLInfinityColor> blackColor = {SLInfinityColor::fromRGB(0, 0, 0)};
    
    if (!SetChannelColors(channel, blackColor)) {
        return false;
    }

    // Send commit action to turn off (brightness 8 = 0% brightness)
    return SendCommitAction(channel, 0x01, 0x00, 0x00, 0x08); // Static color, off brightness
}

bool SLInfinityHIDController::TurnOffAllChannels() {
    bool success = true;
    for (uint8_t channel = 0; channel < 8; channel++) {
        success &= TurnOffChannel(channel);
    }
    return success;
}
