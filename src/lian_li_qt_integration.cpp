/*---------------------------------------------------------*\
|| lian_li_qt_integration.cpp                             |
||                                                         |
||   Qt Integration for Lian Li SL Infinity Controller    |
||   Provides easy-to-use Qt interface for RGB control    |
||                                                         |
||   This file is part of the L-Connect project           |
||   SPDX-License-Identifier: GPL-2.0-or-later            |
\*---------------------------------------------------------*/

#include "lian_li_qt_integration.h"
#include "utils/qtdebugutil.h"
#include <QDebug>
#include <QThread>
#include <QApplication>

LianLiQtIntegration::LianLiQtIntegration(QObject *parent)
    : QObject(parent)
    , m_controller(std::make_unique<SLInfinityHIDController>())
    , m_deviceCheckTimer(new QTimer(this))
    , m_wasConnected(false)
{
    // Set up device monitoring timer
    m_deviceCheckTimer->setInterval(2000); // Check every 2 seconds
    connect(m_deviceCheckTimer, &QTimer::timeout, this, &LianLiQtIntegration::onDeviceCheck);
}

LianLiQtIntegration::~LianLiQtIntegration()
{
    shutdown();
}

bool LianLiQtIntegration::initialize()
{
    if (!m_controller) {
        m_controller = std::make_unique<SLInfinityHIDController>();
    }
    
    if (m_controller->Initialize()) {
        m_wasConnected = true;
        m_deviceCheckTimer->start();
        emit deviceConnected();
        DEBUG_LOG("Lian Li device connected successfully");
        return true;
    } else {
        emit errorOccurred("Failed to initialize Lian Li device");
        DEBUG_LOG("Failed to initialize Lian Li device");
        return false;
    }
}

void LianLiQtIntegration::shutdown()
{
    if (m_deviceCheckTimer) {
        m_deviceCheckTimer->stop();
    }
    
    if (m_controller) {
        m_controller->Close();
        m_controller.reset();
    }
    
    m_wasConnected = false;
}

bool LianLiQtIntegration::isConnected() const
{
    return m_controller && m_controller->IsConnected();
}

QString LianLiQtIntegration::getDeviceName() const
{
    if (!m_controller) return "Unknown";
    return QString::fromStdString(m_controller->GetDeviceName());
}

QString LianLiQtIntegration::getFirmwareVersion() const
{
    if (!m_controller) return "Unknown";
    return QString::fromStdString(m_controller->GetFirmwareVersion());
}

QString LianLiQtIntegration::getSerialNumber() const
{
    if (!m_controller) return "Unknown";
    return QString::fromStdString(m_controller->GetSerialNumber());
}

bool LianLiQtIntegration::setChannelColor(int channel, const QColor &color, int brightness)
{
    DEBUG_LOG("======================================");
    DEBUG_LOG("setChannelColor: channel=", channel, "color=", color, "brightness=", brightness);
    DEBUG_LOG("RGB values: R=", color.red(), "G=", color.green(), "B=", color.blue());
    
    if (!isChannelValid(channel) || !isConnected()) {
        DEBUG_LOG("setChannelColor: Invalid channel or not connected. Channel valid:", isChannelValid(channel), "Connected:", isConnected());
        DEBUG_LOG("======================================");
        return false;
    }
    
    SLInfinityColor slColor = qColorToSLInfinity(color);
    
    // Pass just 1 color - SetChannelColors will fill all LEDs with it (colors.size() == 1 branch)
    std::vector<SLInfinityColor> colors = {slColor};
    
    uint8_t hwBrightness = convertBrightness(brightness);
    DEBUG_LOG("Brightness conversion: input=", brightness, "output=", hwBrightness);
    
    // Static mode uses LED data brightness (unlike Breathing/Rainbow which use commit action brightness)
    float brightness_scale = static_cast<float>(brightness) / 100.0f;
    DEBUG_LOG("Brightness scale for LED data:", brightness_scale);
    bool success = m_controller->SetChannelColors(static_cast<uint8_t>(channel), colors, brightness_scale);
    DEBUG_LOG("SetChannelColors for channel", channel, "result:", success);
    
    if (success) {
        // Add delay to ensure colors are fully sent before committing
        QThread::msleep(10);
        
        // Send commit action for static color mode with brightness
        success = m_controller->SendCommitAction(
            static_cast<uint8_t>(channel), 
            0x01, // Static color mode
            0x00, // Speed doesn't matter for static
            0x00, // Direction doesn't matter for static
            hwBrightness
        );
        DEBUG_LOG("SendCommitAction for channel", channel, "result:", success);
        DEBUG_LOG("Command sent: channel=", channel, "effect=0x01 speed=0x00 direction=0x00 brightness=", hwBrightness);
        
        if (success) {
            emit colorChanged(channel, color);
        }
    }
    
    DEBUG_LOG("Final result for channel", channel, ":", (success ? "SUCCESS" : "FAILED"));
    DEBUG_LOG("======================================");
    return success;
}

bool LianLiQtIntegration::setChannelStaticWithFanColors(int channel, const QColor colors[4], int brightness)
{
    if (!isChannelValid(channel) || !isConnected()) {
        return false;
    }
    
    uint8_t hwBrightness = convertBrightness(brightness);
    
    // For Static mode, each fan should be a solid color (all its LEDs the same color)
    // Pass 4 colors - SetChannelColors will assign one color per fan when colors.size() == 4
    // Fan 1 = LEDs 0-15 gets colors[0]
    // Fan 2 = LEDs 16-31 gets colors[1]
    // Fan 3 = LEDs 32-47 gets colors[2]
    // Fan 4 = LEDs 48-63 gets colors[3]
    
    // Convert QColors to SLInfinityColor (4 colors, one per fan)
    std::vector<SLInfinityColor> slColors;
    slColors.reserve(4);
    for (int i = 0; i < 4; ++i) {
        slColors.push_back(qColorToSLInfinity(colors[i]));
    }
    
    // Set the colors using the 4-color handler (assigns one color per fan, 16 LEDs each)
    // SetChannelColors already sends StartAction and ColorData
    // We just need to send the CommitAction with the mode and brightness
    // Use interleavedPattern=false for Static mode (solid per fan)
    float brightness_scale = static_cast<float>(brightness) / 100.0f;
    bool success = m_controller->SetChannelColors(static_cast<uint8_t>(channel), slColors, brightness_scale, false);
    
    if (success) {
        // Increased delay to ensure colors are fully sent before committing
        // The hardware may need time to process the color data before accepting the mode change
        QThread::msleep(30);
        
        // Send commit action for static color mode with brightness
        // Note: SetChannelMode sends a commit with brightness 0x00, so we don't use it here
        success = m_controller->SendCommitAction(
            static_cast<uint8_t>(channel), 
            0x01, // Static color mode
            0x00, // Speed doesn't matter for static
            0x00, // Direction doesn't matter for static
            hwBrightness
        );
    }
    
    return success;
}

bool LianLiQtIntegration::setChannelMode(int channel, int mode)
{
    if (!isChannelValid(channel) || !isConnected()) {
        return false;
    }
    
    return m_controller->SetChannelMode(static_cast<uint8_t>(channel), static_cast<uint8_t>(mode));
}

bool LianLiQtIntegration::turnOffChannel(int channel)
{
    if (!isChannelValid(channel) || !isConnected()) {
        return false;
    }
    
    bool success = m_controller->TurnOffChannel(static_cast<uint8_t>(channel));
    
    if (success) {
        emit colorChanged(channel, QColor(0, 0, 0));
    }
    
    return success;
}

bool LianLiQtIntegration::turnOffAllChannels()
{
    if (!isConnected()) {
        return false;
    }
    
    bool success = m_controller->TurnOffAllChannels();
    
    if (success) {
        for (int i = 0; i < getChannelCount(); i++) {
            emit colorChanged(i, QColor(0, 0, 0));
        }
    }
    
    return success;
}

bool LianLiQtIntegration::setAllChannelsColor(const QColor &color, int brightness)
{
    if (!isConnected()) {
        return false;
    }
    
    SLInfinityColor slColor = qColorToSLInfinity(color);
    
    // For static color, we need to set ALL 16 LEDs per fan to the same color
    // Each channel can have up to 4 fans, so we need 16 * 4 = 64 LEDs total
    std::vector<SLInfinityColor> colors;
    colors.resize(64, slColor); // Fill all 64 LEDs with the same color
    
    uint8_t hwBrightness = convertBrightness(brightness);
    
    bool allSuccess = true;
    for (int channel = 0; channel < getChannelCount(); channel++) {
        if (!isChannelValid(channel)) continue;
        
        // Set the color
        if (!m_controller->SetChannelColors(static_cast<uint8_t>(channel), colors)) {
            allSuccess = false;
            continue;
        }
        
        // Send commit action for static color mode with brightness
        if (!m_controller->SendCommitAction(
            static_cast<uint8_t>(channel), 
            0x01, // Static color mode
            0x00, // Speed doesn't matter for static
            0x00, // Direction doesn't matter for static
            hwBrightness)) {
            allSuccess = false;
        }
    }
    
    return allSuccess;
}

bool LianLiQtIntegration::setRainbowEffect(int speed, int brightness, bool directionLeft)
{
    if (!isConnected()) {
        return false;
    }
    
    uint8_t hwSpeed = convertSpeed(speed);
    uint8_t hwBrightness = convertBrightness(brightness);
    uint8_t hwDirection = convertDirection(directionLeft);
    
    bool allSuccess = true;
    for (int channel = 0; channel < getChannelCount(); channel++) {
        if (!isChannelValid(channel)) continue;
        
        // Send commit action for rainbow effect
        bool success = m_controller->SendCommitAction(
            static_cast<uint8_t>(channel),
            0x05, // Rainbow mode
            hwSpeed,
            hwDirection,
            hwBrightness
        );
        
        if (!success) {
            allSuccess = false;
        }
    }
    
    return allSuccess;
}

bool LianLiQtIntegration::setRainbowMorphEffect(int speed, int brightness)
{
    if (!isConnected()) {
        return false;
    }
    
    uint8_t hwSpeed = convertSpeed(speed);
    uint8_t hwBrightness = convertBrightness(brightness);
    
    bool allSuccess = true;
    for (int channel = 0; channel < getChannelCount(); channel++) {
        if (!isChannelValid(channel)) continue;
        
        // Send commit action for rainbow morph effect (no direction control)
        bool success = m_controller->SendCommitAction(
            static_cast<uint8_t>(channel),
            0x04, // Rainbow Morph mode
            hwSpeed,
            0x00, // No direction control for morph
            hwBrightness
        );
        
        if (!success) {
            allSuccess = false;
        }
    }
    
    return allSuccess;
}

bool LianLiQtIntegration::setMeteorEffect(int speed, int brightness, bool directionLeft)
{
    if (!isConnected()) {
        return false;
    }
    
    uint8_t hwSpeed = convertSpeed(speed);
    uint8_t hwBrightness = convertBrightness(brightness);
    uint8_t hwDirection = convertDirection(directionLeft);
    
    bool allSuccess = true;
    for (int channel = 0; channel < getChannelCount(); channel++) {
        if (!isChannelValid(channel)) continue;
        
        // Send commit action for meteor effect
        bool success = m_controller->SendCommitAction(
            static_cast<uint8_t>(channel),
            0x24, // Meteor mode
            hwSpeed,
            hwDirection,
            hwBrightness
        );
        
        if (!success) {
            allSuccess = false;
        }
    }
    
    return allSuccess;
}

bool LianLiQtIntegration::setRunwayEffect(int speed, int brightness, bool directionLeft)
{
    // Runway uses a default orange color - but we allow user to set color via setChannelRunway
    // For all channels, use default orange
    QColor defaultColor(255, 200, 100); // Orange
    return setAllChannelsEffect(0x1C, defaultColor, speed, brightness, directionLeft);
}

bool LianLiQtIntegration::setChannelRunway(int channel, const QColor &color, int speed, int brightness, bool directionLeft)
{
    // Legacy support - use 2 colors (same color twice)
    QColor colors[2] = {color, color};
    return setChannelRunwayWithColors(channel, colors, speed, brightness, directionLeft);
}

bool LianLiQtIntegration::setChannelRunwayWithColors(int channel, const QColor colors[2], int speed, int brightness, bool directionLeft)
{
    if (!isConnected() || !isChannelValid(channel)) {
        return false;
    }
    
    uint8_t hwSpeed = convertSpeed(speed);
    uint8_t hwBrightness = convertBrightness(brightness);
    // Runway does NOT have direction control per OpenRGB - always use false
    uint8_t hwDirection = 0x00;
    
    // Runway mode uses MODE_SPECIFIC_COLOR with 2 colors
    // Runway needs brightness in LED data (like other 2-color modes)
    SLInfinityColor slColor1 = qColorToSLInfinity(colors[0]);
    SLInfinityColor slColor2 = qColorToSLInfinity(colors[1]);
    std::vector<SLInfinityColor> colorVec = {slColor1, slColor2};
    
    // Apply brightness to LED data
    float brightness_scale = static_cast<float>(brightness) / 100.0f;
    
    // Set the colors for this channel with brightness scaling
    if (!m_controller->SetChannelColors(static_cast<uint8_t>(channel), colorVec, brightness_scale)) {
        return false;
    }
    
    // Send commit action for runway effect
    return m_controller->SendCommitAction(
        static_cast<uint8_t>(channel),
        0x1C, // Runway mode
        hwSpeed,
        hwDirection,
        hwBrightness
    );
}

bool LianLiQtIntegration::setChannelBreathing(int channel, const QColor &color, int speed, int brightness)
{
    if (!isConnected() || !isChannelValid(channel)) {
        return false;
    }
    
    SLInfinityColor slColor = qColorToSLInfinity(color);
    std::vector<SLInfinityColor> colors = {slColor};
    uint8_t hwSpeed = convertSpeed(speed);
    uint8_t hwBrightness = convertBrightness(brightness);
    
    // Set the color for this channel
    if (!m_controller->SetChannelColors(static_cast<uint8_t>(channel), colors)) {
        return false;
    }
    
    // Send commit action for breathing mode (no direction control)
    bool success = m_controller->SendCommitAction(
        static_cast<uint8_t>(channel),
        0x02, // Breathing mode
        hwSpeed,
        0x00, // No direction control for breathing
        hwBrightness
    );
    
    return success;
}

bool LianLiQtIntegration::setChannelMeteor(int channel, const QColor &color, int speed, int brightness, bool directionLeft)
{
    // For backward compatibility - use single color for all fans
    QColor colors[4] = {color, color, color, color};
    return setChannelMeteorWithFanColors(channel, colors, speed, brightness, directionLeft);
}

bool LianLiQtIntegration::setChannelMeteorWithFanColors(int channel, const QColor colors[4], int speed, int brightness, bool directionLeft)
{
    if (!isConnected() || !isChannelValid(channel)) {
        return false;
    }
    
    uint8_t hwSpeed = convertSpeed(speed);
    uint8_t hwBrightness = convertBrightness(brightness);
    uint8_t hwDirection = convertDirection(directionLeft);
    
    // For Meteor mode, we need to send 4 colors (one per fan)
    // OpenRGB pattern for modes with < 6 colors: resize to 4 colors, create pattern
    // Convert QColors to SLInfinityColor
    std::vector<SLInfinityColor> slColors;
    slColors.reserve(4);
    for (int i = 0; i < 4; ++i) {
        slColors.push_back(qColorToSLInfinity(colors[i]));
    }
    
    // Set colors using the 4-color pattern
    // This sends StartAction + ColorData
    if (!m_controller->SetChannelColors(static_cast<uint8_t>(channel), slColors)) {
        return false;
    }
    
    // Small delay to ensure colors are sent
    QThread::msleep(50);
    
    // Send Meteor mode commit action
    uint8_t meteorMode = 0x24;
    bool success = m_controller->SendCommitAction(
        static_cast<uint8_t>(channel),
        meteorMode,
        hwSpeed,
        hwDirection,
        hwBrightness
    );
    
    DEBUG_LOG("Meteor with 4 fan colors: channel=", channel, "mode=0x", QString::number(meteorMode, 16).toUpper(),
             "speed=", hwSpeed, "dir=", hwDirection, "bright=", hwBrightness);
    
    return success;
}

bool LianLiQtIntegration::setChannelMeteorWithColors(int channel, const QColor colors[2], int speed, int brightness, bool directionLeft)
{
    if (!isConnected() || !isChannelValid(channel)) {
        return false;
    }
    
    uint8_t hwSpeed = convertSpeed(speed);
    uint8_t hwBrightness = convertBrightness(brightness);
    // Meteor does NOT have direction control per OpenRGB - always use false
    uint8_t hwDirection = 0x00;
    
    // Meteor mode uses MODE_SPECIFIC_COLOR with 2 colors (per OpenRGB)
    SLInfinityColor slColor1 = qColorToSLInfinity(colors[0]);
    SLInfinityColor slColor2 = qColorToSLInfinity(colors[1]);
    
    DEBUG_LOG("setChannelMeteorWithColors: channel=", channel,
             "Color1(RGB):", colors[0].red(), colors[0].green(), colors[0].blue(),
             "Color2(RGB):", colors[1].red(), colors[1].green(), colors[1].blue(),
             "After conversion - Color1(RGB):", slColor1.r, slColor1.g, slColor1.b,
             "Color2(RGB):", slColor2.r, slColor2.g, slColor2.b);
    
    std::vector<SLInfinityColor> colorVec = {slColor1, slColor2};
    
    // Apply brightness to LED data like OpenRGB does (brightness/100 converts 0-100% to 0-1)
    float brightness_scale = static_cast<float>(brightness) / 100.0f;
    
    // Set the colors for this channel with brightness scaling
    if (!m_controller->SetChannelColors(static_cast<uint8_t>(channel), colorVec, brightness_scale)) {
        return false;
    }
    
    // Increased delay to ensure colors are fully processed before committing mode
    // Meteor mode needs the colors to be set first, then the mode is applied
    QThread::msleep(50);
    
    // Send Meteor mode commit action (no direction control)
    uint8_t meteorMode = 0x24;
    bool success = m_controller->SendCommitAction(
        static_cast<uint8_t>(channel),
        meteorMode,
        hwSpeed,
        hwDirection, // Always 0x00 for Meteor
        hwBrightness
    );
    
    // Additional delay after commit to ensure mode is applied
    QThread::msleep(10);
    
    DEBUG_LOG("Meteor with 2 colors (OpenRGB style): channel=", channel, "mode=0x", QString::number(meteorMode, 16).toUpper(),
             "speed=", hwSpeed, "dir=", hwDirection, "bright=", hwBrightness);
    
    return success;
}

bool LianLiQtIntegration::setBreathingEffect(const QColor &color, int speed, int brightness, bool directionLeft)
{
    if (!isConnected()) {
        return false;
    }
    
    SLInfinityColor slColor = qColorToSLInfinity(color);
    std::vector<SLInfinityColor> colors = {slColor};
    uint8_t hwSpeed = convertSpeed(speed);
    uint8_t hwBrightness = convertBrightness(brightness);
    uint8_t hwDirection = convertDirection(directionLeft);
    
    bool allSuccess = true;
    for (int channel = 0; channel < getChannelCount(); channel++) {
        if (!isChannelValid(channel)) continue;
        
        // Set the color
        if (!m_controller->SetChannelColors(static_cast<uint8_t>(channel), colors)) {
            allSuccess = false;
            continue;
        }
        
        // Send commit action for breathing mode
        bool success = m_controller->SendCommitAction(
            static_cast<uint8_t>(channel),
            0x02, // Breathing mode
            hwSpeed,
            hwDirection,
            hwBrightness
        );
        
        if (!success) {
            allSuccess = false;
        }
    }
    
    return allSuccess;
}

void LianLiQtIntegration::onDeviceCheck()
{
    bool currentlyConnected = isConnected();
    
    if (currentlyConnected != m_wasConnected) {
        m_wasConnected = currentlyConnected;
        
        if (currentlyConnected) {
            emit deviceConnected();
            DEBUG_LOG("Lian Li device connected");
        } else {
            emit deviceDisconnected();
            DEBUG_LOG("Lian Li device disconnected");
        }
    }
}

SLInfinityColor LianLiQtIntegration::qColorToSLInfinity(const QColor &color) const
{
    return SLInfinityColor::fromRGB(
        static_cast<uint8_t>(color.red()),
        static_cast<uint8_t>(color.green()),
        static_cast<uint8_t>(color.blue())
    );
}

QColor LianLiQtIntegration::slInfinityToQColor(const SLInfinityColor &color) const
{
    return QColor(color.r, color.g, color.b);
}

uint8_t LianLiQtIntegration::convertSpeed(int speedPercent)
{
    // Convert 0-100% to hardware values (25% increments only)
    // Hardware values from lian_li_sl_infinity_controller.h:
    // UNIHUB_SLINF_LED_SPEED_000 = 0x02, SPEED_025 = 0x01, SPEED_050 = 0x00
    // UNIHUB_SLINF_LED_SPEED_075 = 0xFF, SPEED_100 = 0xFE
    if (speedPercent <= 12) return 0x02;       // 0-12% → 0% (very slow)
    else if (speedPercent <= 37) return 0x01;  // 13-37% → 25% (slow)
    else if (speedPercent <= 62) return 0x00;  // 38-62% → 50% (medium)
    else if (speedPercent <= 87) return 0xFF;  // 63-87% → 75% (fast)
    else return 0xFE;                          // 88-100% → 100% (very fast)
}

uint8_t LianLiQtIntegration::convertBrightness(int brightnessPercent)
{
    // Convert 0-100% to hardware values (25% increments only)
    // Hardware values from lian_li_sl_infinity_controller.h:
    // UNIHUB_SLINF_LED_BRIGHTNESS_000 = 0x08, BRIGHTNESS_025 = 0x03, BRIGHTNESS_050 = 0x02
    // UNIHUB_SLINF_LED_BRIGHTNESS_075 = 0x01, BRIGHTNESS_100 = 0x00
    if (brightnessPercent <= 12) return 0x08;       // 0-12% → 0% (off)
    else if (brightnessPercent <= 37) return 0x03;  // 13-37% → 25% (dim)
    else if (brightnessPercent <= 62) return 0x02;  // 38-62% → 50% (medium)
    else if (brightnessPercent <= 87) return 0x01;  // 63-87% → 75% (bright)
    else return 0x00;                               // 88-100% → 100% (full)
}

uint8_t LianLiQtIntegration::convertDirection(bool directionLeft)
{
    // Hardware: 0x00 (left-to-right), 0x01 (right-to-left)
    return directionLeft ? 0x01 : 0x00;
}

// Generic helper methods for all effects
bool LianLiQtIntegration::setChannelEffect(int channel, uint8_t mode, const QColor &color, int speed, int brightness, bool directionLeft)
{
    if (!isConnected() || !isChannelValid(channel)) {
        return false;
    }
    
    uint8_t hwSpeed = convertSpeed(speed);
    uint8_t hwBrightness = convertBrightness(brightness);
    uint8_t hwDirection = convertDirection(directionLeft);
    
    // If color is valid, set it first
    if (color.isValid()) {
        SLInfinityColor slColor = qColorToSLInfinity(color);
        std::vector<SLInfinityColor> colors = {slColor};
        if (!m_controller->SetChannelColors(static_cast<uint8_t>(channel), colors)) {
            return false;
        }
    }
    
    // Send commit action
    return m_controller->SendCommitAction(
        static_cast<uint8_t>(channel),
        mode,
        hwSpeed,
        hwDirection,
        hwBrightness
    );
}

bool LianLiQtIntegration::setChannelEffectWithColors(int channel, uint8_t mode, const QColor colors[4], int speed, int brightness, bool directionLeft)
{
    if (!isConnected() || !isChannelValid(channel)) {
        return false;
    }
    
    uint8_t hwSpeed = convertSpeed(speed);
    uint8_t hwBrightness = convertBrightness(brightness);
    uint8_t hwDirection = convertDirection(directionLeft);
    
    // Convert QColors to SLInfinityColor
    std::vector<SLInfinityColor> slColors;
    slColors.reserve(4);
    for (int i = 0; i < 4; ++i) {
        slColors.push_back(qColorToSLInfinity(colors[i]));
    }
    
    // Set colors
    if (!m_controller->SetChannelColors(static_cast<uint8_t>(channel), slColors)) {
        return false;
    }
    
    QThread::msleep(10);
    
    // Send commit action
    return m_controller->SendCommitAction(
        static_cast<uint8_t>(channel),
        mode,
        hwSpeed,
        hwDirection,
        hwBrightness
    );
}

bool LianLiQtIntegration::setAllChannelsEffect(uint8_t mode, const QColor &color, int speed, int brightness, bool directionLeft)
{
    if (!isConnected()) {
        return false;
    }
    
    bool allSuccess = true;
    for (int channel = 0; channel < getChannelCount(); channel++) {
        if (!isChannelValid(channel)) continue;
        
        if (!setChannelEffect(channel, mode, color, speed, brightness, directionLeft)) {
            allSuccess = false;
        }
    }
    
    return allSuccess;
}

// Specific effect implementations
bool LianLiQtIntegration::setStaggeredEffect(const QColor &color, int speed, int brightness)
{
    // Staggered uses 2 colors - use same color twice for backward compatibility
    QColor colors[2] = {color, color};
    if (!isConnected()) {
        return false;
    }
    
    bool allSuccess = true;
    for (int channel = 0; channel < getChannelCount(); channel++) {
        if (!isChannelValid(channel)) continue;
        
        if (!setChannelStaggered(channel, colors, speed, brightness)) {
            allSuccess = false;
        }
    }
    
    return allSuccess;
}

bool LianLiQtIntegration::setChannelStaggered(int channel, const QColor colors[2], int speed, int brightness)
{
    if (!isConnected() || !isChannelValid(channel)) {
        return false;
    }
    
    uint8_t hwSpeed = convertSpeed(speed);
    uint8_t hwBrightness = convertBrightness(brightness);
    
    // Staggered mode uses MODE_SPECIFIC_COLOR with 2 colors
    // Staggered needs brightness in LED data (like Static, Meteor, and Tide)
    SLInfinityColor slColor1 = qColorToSLInfinity(colors[0]);
    SLInfinityColor slColor2 = qColorToSLInfinity(colors[1]);
    std::vector<SLInfinityColor> colorVec = {slColor1, slColor2};
    
    // Apply brightness to LED data
    float brightness_scale = static_cast<float>(brightness) / 100.0f;
    
    // Set the colors for this channel with brightness scaling
    if (!m_controller->SetChannelColors(static_cast<uint8_t>(channel), colorVec, brightness_scale)) {
        return false;
    }
    
    // Send commit action for staggered effect (no direction control)
    return m_controller->SendCommitAction(
        static_cast<uint8_t>(channel),
        0x18, // Staggered mode
        hwSpeed,
        0x00, // No direction
        hwBrightness
    );
}

bool LianLiQtIntegration::setTideEffect(const QColor colors[2], int speed, int brightness)
{
    if (!isConnected()) {
        return false;
    }
    
    bool allSuccess = true;
    for (int channel = 0; channel < getChannelCount(); channel++) {
        if (!isChannelValid(channel)) continue;
        
        if (!setChannelTide(channel, colors, speed, brightness)) {
            allSuccess = false;
        }
    }
    
    return allSuccess;
}

bool LianLiQtIntegration::setChannelTide(int channel, const QColor colors[2], int speed, int brightness)
{
    if (!isConnected() || !isChannelValid(channel)) {
        return false;
    }
    
    uint8_t hwSpeed = convertSpeed(speed);
    uint8_t hwBrightness = convertBrightness(brightness);
    
    // Tide mode uses MODE_SPECIFIC_COLOR with 2 colors
    // Tide needs brightness in LED data (like Static and Meteor)
    SLInfinityColor slColor1 = qColorToSLInfinity(colors[0]);
    SLInfinityColor slColor2 = qColorToSLInfinity(colors[1]);
    std::vector<SLInfinityColor> colorVec = {slColor1, slColor2};
    
    // Apply brightness to LED data like Meteor does
    float brightness_scale = static_cast<float>(brightness) / 100.0f;
    
    // Set the colors for this channel with brightness scaling
    if (!m_controller->SetChannelColors(static_cast<uint8_t>(channel), colorVec, brightness_scale)) {
        return false;
    }
    
    // Send commit action for tide effect (brightness also in commit action for compatibility)
    return m_controller->SendCommitAction(
        static_cast<uint8_t>(channel),
        0x1A, // Tide mode
        hwSpeed,
        0x00, // No direction
        hwBrightness
    );
}

bool LianLiQtIntegration::setMixingEffect(const QColor &color, int speed, int brightness)
{
    // Mixing uses 2 colors - use same color for both for single-color mode
    QColor colors[2] = {color, color};
    if (!isConnected()) {
        return false;
    }
    
    bool allSuccess = true;
    for (int channel = 0; channel < getChannelCount(); channel++) {
        if (!isChannelValid(channel)) continue;
        
        if (!setChannelMixing(channel, colors, speed, brightness)) {
            allSuccess = false;
        }
    }
    
    return allSuccess;
}

bool LianLiQtIntegration::setChannelMixing(int channel, const QColor colors[2], int speed, int brightness)
{
    if (!isConnected() || !isChannelValid(channel)) {
        return false;
    }
    
    uint8_t hwSpeed = convertSpeed(speed);
    uint8_t hwBrightness = convertBrightness(brightness);
    
    // Mixing mode uses MODE_SPECIFIC_COLOR with 2 colors
    // Mixing needs brightness in LED data (like other 2-color modes)
    SLInfinityColor slColor1 = qColorToSLInfinity(colors[0]);
    SLInfinityColor slColor2 = qColorToSLInfinity(colors[1]);
    std::vector<SLInfinityColor> colorVec = {slColor1, slColor2};
    
    // Apply brightness to LED data
    float brightness_scale = static_cast<float>(brightness) / 100.0f;
    
    // Set the colors for this channel with brightness scaling
    if (!m_controller->SetChannelColors(static_cast<uint8_t>(channel), colorVec, brightness_scale)) {
        return false;
    }
    
    // Send commit action for mixing effect (no direction control)
    return m_controller->SendCommitAction(
        static_cast<uint8_t>(channel),
        0x1E, // Mixing mode
        hwSpeed,
        0x00, // No direction
        hwBrightness
    );
}

bool LianLiQtIntegration::setStackEffect(const QColor &color, int speed, int brightness, bool directionLeft)
{
    if (!isConnected()) {
        return false;
    }
    
    bool allSuccess = true;
    for (int channel = 0; channel < getChannelCount(); channel++) {
        if (!isChannelValid(channel)) continue;
        
        if (!setChannelStack(channel, color, speed, brightness, directionLeft)) {
            allSuccess = false;
        }
    }
    
    return allSuccess;
}

bool LianLiQtIntegration::setChannelStack(int channel, const QColor &color, int speed, int brightness, bool directionLeft)
{
    if (!isConnected() || !isChannelValid(channel)) {
        return false;
    }
    
    uint8_t hwSpeed = convertSpeed(speed);
    uint8_t hwBrightness = convertBrightness(brightness);
    uint8_t hwDirection = convertDirection(directionLeft);
    
    // Stack mode uses MODE_SPECIFIC_COLOR with 1 color
    SLInfinityColor slColor = qColorToSLInfinity(color);
    std::vector<SLInfinityColor> colorVec = {slColor};
    
    // Set the color for this channel
    if (!m_controller->SetChannelColors(static_cast<uint8_t>(channel), colorVec)) {
        return false;
    }
    
    // Send commit action for stack effect (has direction control)
    return m_controller->SendCommitAction(
        static_cast<uint8_t>(channel),
        0x20, // Stack mode
        hwSpeed,
        hwDirection,
        hwBrightness
    );
}

bool LianLiQtIntegration::setNeonEffect(int speed, int brightness)
{
    // Neon doesn't use MODE_SPECIFIC_COLOR
    return setAllChannelsEffect(0x22, QColor(), speed, brightness, false);
}

bool LianLiQtIntegration::setColorCycleEffect(const QColor colors[3], int speed, int brightness, bool directionLeft)
{
    if (!isConnected()) {
        return false;
    }
    
    bool allSuccess = true;
    for (int channel = 0; channel < getChannelCount(); channel++) {
        if (!isChannelValid(channel)) continue;
        
        if (!setChannelColorCycle(channel, colors, speed, brightness, directionLeft)) {
            allSuccess = false;
        }
    }
    
    return allSuccess;
}

bool LianLiQtIntegration::setChannelColorCycle(int channel, const QColor colors[3], int speed, int brightness, bool directionLeft)
{
    if (!isConnected() || !isChannelValid(channel)) {
        DEBUG_LOG("setChannelColorCycle: Device not connected or invalid channel", channel);
        return false;
    }
    
    uint8_t hwSpeed = convertSpeed(speed);
    uint8_t hwBrightness = convertBrightness(brightness);
    uint8_t hwDirection = convertDirection(directionLeft);
    
    // ColorCycle mode uses MODE_SPECIFIC_COLOR with 3 colors
    SLInfinityColor slColor1 = qColorToSLInfinity(colors[0]);
    SLInfinityColor slColor2 = qColorToSLInfinity(colors[1]);
    SLInfinityColor slColor3 = qColorToSLInfinity(colors[2]);
    std::vector<SLInfinityColor> colorVec = {slColor1, slColor2, slColor3};
    
    DEBUG_LOG("setChannelColorCycle: Channel", channel, 
             "Color1:", colors[0], "Color2:", colors[1], "Color3:", colors[2],
             "Speed:", speed, "Brightness:", brightness, "Direction:", (directionLeft ? "Left" : "Right"));
    
    // Set the colors for this channel
    if (!m_controller->SetChannelColors(static_cast<uint8_t>(channel), colorVec)) {
        DEBUG_LOG("setChannelColorCycle: Failed to set colors for channel", channel);
        return false;
    }
    
    // Add delay to ensure colors are set before sending commit action
    QThread::msleep(10);
    
    // Send commit action for color cycle effect
    bool result = m_controller->SendCommitAction(
        static_cast<uint8_t>(channel),
        0x23, // ColorCycle mode
        hwSpeed,
        hwDirection,
        hwBrightness
    );
    
    if (result) {
        DEBUG_LOG("setChannelColorCycle: Successfully applied ColorCycle to channel", channel);
    } else {
        DEBUG_LOG("setChannelColorCycle: Failed to send commit action for channel", channel);
    }
    
    return result;
}

bool LianLiQtIntegration::setVoiceEffect(int speed, int brightness)
{
    // Voice doesn't use MODE_SPECIFIC_COLOR - no colors needed
    if (!isConnected()) {
        return false;
    }
    
    uint8_t hwSpeed = convertSpeed(speed);
    uint8_t hwBrightness = convertBrightness(brightness);
    
    bool allSuccess = true;
    for (int channel = 0; channel < getChannelCount(); channel++) {
        if (!isChannelValid(channel)) continue;
        
        // Send commit action directly without setting colors
        bool success = m_controller->SendCommitAction(
            static_cast<uint8_t>(channel),
            0x26, // Voice mode
            hwSpeed,
            0x00, // No direction
            hwBrightness
        );
        
        if (!success) {
            allSuccess = false;
        }
    }
    
    return allSuccess;
}

bool LianLiQtIntegration::setGrooveEffect(const QColor colors[2], int speed, int brightness, bool directionLeft)
{
    if (!isConnected()) {
        return false;
    }
    
    bool allSuccess = true;
    for (int channel = 0; channel < getChannelCount(); channel++) {
        if (!isChannelValid(channel)) continue;
        
        if (!setChannelGroove(channel, colors, speed, brightness, directionLeft)) {
            allSuccess = false;
        }
    }
    
    return allSuccess;
}

bool LianLiQtIntegration::setChannelGroove(int channel, const QColor colors[2], int speed, int brightness, bool directionLeft)
{
    if (!isConnected() || !isChannelValid(channel)) {
        return false;
    }
    
    uint8_t hwSpeed = convertSpeed(speed);
    uint8_t hwBrightness = convertBrightness(brightness);
    uint8_t hwDirection = convertDirection(directionLeft);
    
    SLInfinityColor slColor1 = qColorToSLInfinity(colors[0]);
    SLInfinityColor slColor2 = qColorToSLInfinity(colors[1]);
    std::vector<SLInfinityColor> colorVec = {slColor1, slColor2};
    
    float brightness_scale = static_cast<float>(brightness) / 100.0f;
    
    if (!m_controller->SetChannelColors(static_cast<uint8_t>(channel), colorVec, brightness_scale)) {
        return false;
    }
    
    return m_controller->SendCommitAction(
        static_cast<uint8_t>(channel),
        0x27, // Groove mode
        hwSpeed,
        hwDirection,
        hwBrightness
    );
}

bool LianLiQtIntegration::setTunnelEffect(const QColor colors[4], int speed, int brightness, bool directionLeft)
{
    if (!isConnected()) {
        return false;
    }
    
    bool allSuccess = true;
    for (int channel = 0; channel < getChannelCount(); channel++) {
        if (!isChannelValid(channel)) continue;
        
        if (!setChannelTunnel(channel, colors, speed, brightness, directionLeft)) {
            allSuccess = false;
        }
    }
    
    return allSuccess;
}

bool LianLiQtIntegration::setChannelTunnel(int channel, const QColor colors[4], int speed, int brightness, bool directionLeft)
{
    if (!isConnected() || !isChannelValid(channel)) {
        return false;
    }
    
    uint8_t hwSpeed = convertSpeed(speed);
    uint8_t hwBrightness = convertBrightness(brightness);
    uint8_t hwDirection = convertDirection(directionLeft);
    
    std::vector<SLInfinityColor> slColors;
    slColors.reserve(4);
    for (int i = 0; i < 4; ++i) {
        slColors.push_back(qColorToSLInfinity(colors[i]));
    }
    
    float brightness_scale = static_cast<float>(brightness) / 100.0f;
    if (!m_controller->SetChannelColors(static_cast<uint8_t>(channel), slColors, brightness_scale, true)) {
        return false;
    }
    
    QThread::msleep(10);
    
    return m_controller->SendCommitAction(
        static_cast<uint8_t>(channel),
        0x29, // Tunnel mode
        hwSpeed,
        hwDirection,
        hwBrightness
    );
}

bool LianLiQtIntegration::setRenderEffect(const QColor colors[4], int speed, int brightness, bool directionLeft)
{
    if (!isConnected()) {
        return false;
    }
    
    bool allSuccess = true;
    for (int channel = 0; channel < getChannelCount(); channel++) {
        if (!isChannelValid(channel)) continue;
        
        if (!setChannelRender(channel, colors, speed, brightness, directionLeft)) {
            allSuccess = false;
        }
    }
    
    return allSuccess;
}

bool LianLiQtIntegration::setChannelRender(int channel, const QColor colors[4], int speed, int brightness, bool directionLeft)
{
    if (!isConnected() || !isChannelValid(channel)) {
        return false;
    }
    
    uint8_t hwSpeed = convertSpeed(speed);
    uint8_t hwBrightness = convertBrightness(brightness);
    uint8_t hwDirection = convertDirection(directionLeft);
    
    std::vector<SLInfinityColor> slColors;
    slColors.reserve(4);
    for (int i = 0; i < 4; ++i) {
        slColors.push_back(qColorToSLInfinity(colors[i]));
    }
    
    float brightness_scale = static_cast<float>(brightness) / 100.0f;
    if (!m_controller->SetChannelColors(static_cast<uint8_t>(channel), slColors, brightness_scale, true)) {
        return false;
    }
    
    QThread::msleep(10);
    
    return m_controller->SendCommitAction(
        static_cast<uint8_t>(channel),
        0x28, // Render mode
        hwSpeed,
        hwDirection,
        hwBrightness
    );
}

bool LianLiQtIntegration::setStackMultiColorEffect(int speed, int brightness, bool directionLeft)
{
    if (!isConnected()) {
        return false;
    }
    
    uint8_t hwSpeed = convertSpeed(speed);
    uint8_t hwBrightness = convertBrightness(brightness);
    uint8_t hwDirection = convertDirection(directionLeft);
    
    bool allSuccess = true;
    for (int channel = 0; channel < getChannelCount(); channel++) {
        if (!isChannelValid(channel)) continue;
        
        bool success = m_controller->SendCommitAction(
            static_cast<uint8_t>(channel),
            0x21, // Stack Multi Color mode
            hwSpeed,
            hwDirection,
            hwBrightness
        );
        
        if (!success) {
            allSuccess = false;
        }
    }
    
    return allSuccess;
}
