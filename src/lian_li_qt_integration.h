/*---------------------------------------------------------*\
|| lian_li_qt_integration.h                               |
||                                                         |
||   Qt Integration for Lian Li SL Infinity Controller    |
||   Provides easy-to-use Qt interface for RGB control    |
||                                                         |
||   This file is part of the L-Connect project           |
||   SPDX-License-Identifier: GPL-2.0-or-later            |
\*---------------------------------------------------------*/

#pragma once

#include <QObject>
#include <QColor>
#include <QTimer>
#include <QString>
#include <memory>
#include "usb/sl_infinity_hid.h"

class LianLiQtIntegration : public QObject
{
    Q_OBJECT

public:
    explicit LianLiQtIntegration(QObject *parent = nullptr);
    ~LianLiQtIntegration();

    // Device management
    bool initialize();
    void shutdown();
    bool isConnected() const;
    
    // Device information
    QString getDeviceName() const;
    QString getFirmwareVersion() const;
    QString getSerialNumber() const;
    
    // RGB Control
    bool setChannelColor(int channel, const QColor &color, int brightness = 100);
    bool setChannelStaticWithFanColors(int channel, const QColor colors[4], int brightness = 100);
    bool setChannelMode(int channel, int mode);
    bool setChannelBreathing(int channel, const QColor &color, int speed = 50, int brightness = 100);
    bool setChannelMeteor(int channel, const QColor &color, int speed = 50, int brightness = 100, bool directionLeft = false);
    bool setChannelMeteorWithFanColors(int channel, const QColor colors[4], int speed = 50, int brightness = 100, bool directionLeft = false);
    bool setChannelMeteorWithColors(int channel, const QColor colors[2], int speed = 50, int brightness = 100, bool directionLeft = false);
    bool turnOffChannel(int channel);
    bool turnOffAllChannels();
    
    // Convenience methods
    bool setAllChannelsColor(const QColor &color, int brightness = 100);
    bool setRainbowEffect(int speed = 50, int brightness = 100, bool directionLeft = false);
    bool setRainbowMorphEffect(int speed = 50, int brightness = 100);
    bool setBreathingEffect(const QColor &color, int speed = 50, int brightness = 100, bool directionLeft = false);
    bool setMeteorEffect(int speed = 50, int brightness = 100, bool directionLeft = false);
    bool setRunwayEffect(int speed = 50, int brightness = 100, bool directionLeft = false);
    bool setChannelRunway(int channel, const QColor &color, int speed = 50, int brightness = 100, bool directionLeft = false);
    
    // All OpenRGB effects
    bool setChannelEffect(int channel, uint8_t mode, const QColor &color = QColor(), int speed = 50, int brightness = 100, bool directionLeft = false);
    bool setChannelEffectWithColors(int channel, uint8_t mode, const QColor colors[4], int speed = 50, int brightness = 100, bool directionLeft = false);
    bool setAllChannelsEffect(uint8_t mode, const QColor &color = QColor(), int speed = 50, int brightness = 100, bool directionLeft = false);
    
    // Specific effect methods
    bool setStaggeredEffect(const QColor &color, int speed = 50, int brightness = 100);
    bool setChannelStaggered(int channel, const QColor colors[2], int speed = 50, int brightness = 100);
    bool setTideEffect(const QColor colors[2], int speed = 50, int brightness = 100);
    bool setChannelTide(int channel, const QColor colors[2], int speed = 50, int brightness = 100);
    bool setMixingEffect(const QColor &color, int speed = 50, int brightness = 100);
    bool setChannelMixing(int channel, const QColor colors[2], int speed = 50, int brightness = 100);
    bool setStackEffect(const QColor &color, int speed = 50, int brightness = 100, bool directionLeft = false);
    bool setChannelStack(int channel, const QColor &color, int speed = 50, int brightness = 100, bool directionLeft = false);
    bool setNeonEffect(int speed = 50, int brightness = 100);
    bool setColorCycleEffect(const QColor colors[3], int speed = 50, int brightness = 100, bool directionLeft = false);
    bool setChannelColorCycle(int channel, const QColor colors[3], int speed = 50, int brightness = 100, bool directionLeft = false);
    bool setVoiceEffect(int speed = 50, int brightness = 100);
    bool setChannelRunwayWithColors(int channel, const QColor colors[2], int speed = 50, int brightness = 100, bool directionLeft = false);
    bool setGrooveEffect(const QColor colors[2], int speed = 50, int brightness = 100, bool directionLeft = false);
    bool setChannelGroove(int channel, const QColor colors[2], int speed = 50, int brightness = 100, bool directionLeft = false);
    bool setTunnelEffect(const QColor colors[4], int speed = 50, int brightness = 100, bool directionLeft = false);
    bool setChannelTunnel(int channel, const QColor colors[4], int speed = 50, int brightness = 100, bool directionLeft = false);
    bool setRenderEffect(const QColor colors[4], int speed = 50, int brightness = 100, bool directionLeft = false);
    bool setChannelRender(int channel, const QColor colors[4], int speed = 50, int brightness = 100, bool directionLeft = false);
    bool setStackMultiColorEffect(int speed = 50, int brightness = 100, bool directionLeft = false);
    
    // Helper to convert percentage values to hardware values
    static uint8_t convertSpeed(int speedPercent);
    static uint8_t convertBrightness(int brightnessPercent);
    static uint8_t convertDirection(bool directionLeft);
    
    // Channel management
    int getChannelCount() const { return 8; }
    bool isChannelValid(int channel) const { return channel >= 0 && channel < 8; }

signals:
    void deviceConnected();
    void deviceDisconnected();
    void errorOccurred(const QString &error);
    void colorChanged(int channel, const QColor &color);

private slots:
    void onDeviceCheck();

private:
    std::unique_ptr<SLInfinityHIDController> m_controller;
    QTimer *m_deviceCheckTimer;
    bool m_wasConnected;
    
    // Helper methods
    SLInfinityColor qColorToSLInfinity(const QColor &color) const;
    QColor slInfinityToQColor(const SLInfinityColor &color) const;
};
