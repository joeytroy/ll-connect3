/*---------------------------------------------------------*\
|| lian_li_sl_infinity_controller.h                        |
||                                                         |
||   HID Controller for Lian Li SL Infinity devices       |
||   Based on OpenRGB implementation                       |
||                                                         |
||   This file is part of the L-Connect project           |
||   SPDX-License-Identifier: GPL-2.0-or-later            |
\*---------------------------------------------------------*/

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <hidapi.h>

/*----------------------------------------------------------------------------*\
|| SL Infinity Specific Definitions                                            |
\*----------------------------------------------------------------------------*/

enum
{
    UNIHUB_SLINF_CHANNEL_COUNT              = 0x08,   // 8 channels
    UNIHUB_SLINF_CHANLED_COUNT              = 0x60,   // 96 LEDs per channel (16 * 6)
    UNIHUB_SLINF_TRANSACTION_ID             = 0xE0,   // Command prefix
};

enum
{
    UNIHUB_SLINF_LED_MODE_STATIC_COLOR      = 0x01,   // Static Color mode
    UNIHUB_SLINF_LED_MODE_BREATHING         = 0x02,   // Breathing mode
    UNIHUB_SLINF_LED_MODE_RAINBOW_MORPH     = 0x04,   // Rainbow morph mode
    UNIHUB_SLINF_LED_MODE_RAINBOW           = 0x05,   // Rainbow mode
    UNIHUB_SLINF_LED_MODE_STAGGERED         = 0x18,   // Staggered mode
    UNIHUB_SLINF_LED_MODE_TIDE              = 0x1A,   // Tide mode
    UNIHUB_SLINF_LED_MODE_RUNWAY            = 0x1C,   // Runway mode
    UNIHUB_SLINF_LED_MODE_MIXING            = 0x1E,   // Mixing mode
    UNIHUB_SLINF_LED_MODE_STACK             = 0x20,   // Stack mode
    UNIHUB_SLINF_LED_MODE_STACK_MULTI_COLOR = 0x21,   // Stack Multi Color mode
    UNIHUB_SLINF_LED_MODE_NEON              = 0x22,   // Neon mode
    UNIHUB_SLINF_LED_MODE_COLOR_CYCLE       = 0x23,   // Color Cycle mode
    UNIHUB_SLINF_LED_MODE_METEOR            = 0x24,   // Meteor mode
    UNIHUB_SLINF_LED_MODE_VOICE             = 0x26,   // Voice mode
    UNIHUB_SLINF_LED_MODE_GROOVE            = 0x27,   // Groove mode (2 colors)
    UNIHUB_SLINF_LED_MODE_RENDER            = 0x28,   // Render mode (4 colors)
    UNIHUB_SLINF_LED_MODE_TUNNEL            = 0x29,   // Tunnel mode (4 colors)
};

enum
{
    UNIHUB_SLINF_LED_SPEED_000              = 0x02,   // Very slow speed
    UNIHUB_SLINF_LED_SPEED_025              = 0x01,   // Rather slow speed
    UNIHUB_SLINF_LED_SPEED_050              = 0x00,   // Medium speed
    UNIHUB_SLINF_LED_SPEED_075              = 0xFF,   // Rather fast speed
    UNIHUB_SLINF_LED_SPEED_100              = 0xFE,   // Very fast speed
};

enum
{
    UNIHUB_SLINF_LED_DIRECTION_LTR          = 0x00,   // Left-to-Right direction
    UNIHUB_SLINF_LED_DIRECTION_RTL          = 0x01,   // Right-to-Left direction
};

enum
{
    UNIHUB_SLINF_LED_BRIGHTNESS_000         = 0x08,   // Very dark (off)
    UNIHUB_SLINF_LED_BRIGHTNESS_025         = 0x03,   // Rather dark
    UNIHUB_SLINF_LED_BRIGHTNESS_050         = 0x02,   // Medium bright
    UNIHUB_SLINF_LED_BRIGHTNESS_075         = 0x01,   // Rather bright
    UNIHUB_SLINF_LED_BRIGHTNESS_100         = 0x00,   // Very bright
};

/*----------------------------------------------------------------------------*\
|| Color Structure (RBG format as required by Lian Li)                         |
\*----------------------------------------------------------------------------*/

struct SLInfinityColor
{
    uint8_t r;
    uint8_t b;  // Blue comes before Green!
    uint8_t g;
    
    SLInfinityColor() : r(0), b(0), g(0) {}
    SLInfinityColor(uint8_t red, uint8_t green, uint8_t blue) : r(red), b(blue), g(green) {}
    
    // Convert from standard RGB
    static SLInfinityColor fromRGB(uint8_t red, uint8_t green, uint8_t blue)
    {
        return SLInfinityColor(red, green, blue);
    }
};

/*----------------------------------------------------------------------------*\
|| SL Infinity HID Controller Class                                           |
\*----------------------------------------------------------------------------*/

class LianLiSLInfinityController
{
public:
    LianLiSLInfinityController();
    ~LianLiSLInfinityController();

    // Device management
    bool Initialize();
    void Close();
    bool IsConnected() const;
    
    // Device information
    std::string GetDeviceName() const;
    std::string GetFirmwareVersion() const;
    std::string GetSerialNumber() const;
    
    // Channel control
    bool SetChannelColors(uint8_t channel, const std::vector<SLInfinityColor>& colors);
    bool SetChannelMode(uint8_t channel, uint8_t mode);
    bool SetChannelSpeed(uint8_t channel, uint8_t speed);
    bool SetChannelDirection(uint8_t channel, uint8_t direction);
    bool SetChannelBrightness(uint8_t channel, uint8_t brightness);
    bool SetChannelFanCount(uint8_t channel, uint8_t count);
    
    // Fan speed reading (kernel driver)
    bool GetChannelSpeed(uint8_t channel, uint8_t& speed);
    bool IsKernelDriverAvailable() const;
    
    // Synchronization
    bool Synchronize();
    
    // Commit action (needed by Qt integration for effects)
    bool SendCommitAction(uint8_t channel, uint8_t effect, uint8_t speed, uint8_t direction, uint8_t brightness);

private:
    hid_device* m_handle;
    std::string m_deviceName;
    std::string m_firmwareVersion;
    std::string m_serialNumber;
    std::string m_location;
    bool m_initialized;
    
    // Internal methods
    bool OpenDevice();
    void CloseDevice();
    bool SendStartAction(uint8_t channel, uint8_t numFans);
    bool SendColorData(uint8_t channel, uint8_t numLeds, const uint8_t* ledData);
    std::string ReadFirmwareVersion();
    std::string ReadSerial();
    void ApplyColorLimiter(SLInfinityColor& color) const;
    float CalculateBrightnessLimit(const SLInfinityColor& color) const;
};
