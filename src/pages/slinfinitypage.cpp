#include "slinfinitypage.h"
#include "widgets/fanwidget.h"
#include "lian_li_qt_integration.h"
#include <QTimer>
#include <QColorDialog>
#include <QMessageBox>
#include <QThread>
#include <QSettings>

SLInfinityPage::SLInfinityPage(QWidget *parent)
    : QWidget(parent)
    , m_currentProfile("Quiet")
    , m_currentEffect("Rainbow")
    , m_currentSpeed(75)
    , m_currentBrightness(100)
    , m_directionLeft(false)
    , m_selectedPort(-1) // No port selected initially
    , m_lianLi(nullptr)
    , m_colorButton(nullptr)
    , m_statusLabel(nullptr)
    , m_statusTimer(nullptr)
    , m_multiColorWidget(nullptr)
{
    // Initialize port colors with defaults
    for (int port = 0; port < 4; port++) {
        m_portColors[port][0] = QColor(255, 0, 0);      // Red
        m_portColors[port][1] = QColor(0, 0, 255);      // Blue
        m_portColors[port][2] = QColor(0, 255, 0);      // Green
        m_portColors[port][3] = QColor(255, 255, 0);    // Yellow
    }
    
    // Initialize color buttons array
    for (int i = 0; i < 4; i++) {
        m_colorButtons[i] = nullptr;
    }
    
    // Initialize Lian Li integration
    m_lianLi = new LianLiQtIntegration(this);
    connect(m_lianLi, &LianLiQtIntegration::deviceConnected, this, &SLInfinityPage::onDeviceConnected);
    connect(m_lianLi, &LianLiQtIntegration::deviceDisconnected, this, &SLInfinityPage::onDeviceDisconnected);
    
    setupUI();
    setupFanVisualization();
    setupControls();
    
    // Load saved lighting settings (must be after UI setup)
    loadLightingSettings();
    
    updateFanVisualization();
    
    // Try to initialize the device
    if (m_lianLi->initialize()) {
        onDeviceConnected();
    } else {
        onDeviceDisconnected();
    }
}

void SLInfinityPage::setupUI()
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(20, 20, 20, 20);
    m_mainLayout->setSpacing(20);
    
    // Header
    m_headerLayout = new QHBoxLayout();
    m_headerLayout->setContentsMargins(0, 0, 0, 0);
    
    m_controllerLabel = new QLabel("SL-Inf Controller01");
    m_controllerLabel->setObjectName("controllerLabel");
    
    m_headerLayout->addWidget(m_controllerLabel);
    m_headerLayout->addStretch();
    
    m_mainLayout->addLayout(m_headerLayout);
    
    // Content layout
    m_contentLayout = new QHBoxLayout();
    m_contentLayout->setSpacing(20);
    
    m_leftLayout = new QVBoxLayout();
    m_rightLayout = new QVBoxLayout();
    
    m_contentLayout->addLayout(m_leftLayout, 2);
    m_contentLayout->addLayout(m_rightLayout, 1);
    
    m_mainLayout->addLayout(m_contentLayout);
    
    // Apply styles
    setStyleSheet(R"(
        #controllerLabel {
            color: #ffffff;
            font-size: 18px;
            font-weight: bold;
        }
        
        #actionButton {
            background-color: #2a82da;
            color: #ffffff;
            border: none;
            padding: 8px 16px;
            border-radius: 4px;
            margin-left: 8px;
        }
        
        #actionButton:hover {
            background-color: #1e6bb8;
        }
    )");
}

void SLInfinityPage::setupFanVisualization()
{
    m_fanGroup = new QGroupBox("Fan Configuration");
    m_fanGroup->setObjectName("fanGroup");
    
    m_fanGrid = new QGridLayout(m_fanGroup);
    m_fanGrid->setSpacing(10);
    
    // Create fan widgets for 4 ports, 4 fans each
    for (int port = 0; port < 4; ++port) {
        for (int fan = 0; fan < 4; ++fan) {
            m_fanWidgets[port][fan] = new FanWidget(port, fan);
            m_fanGrid->addWidget(m_fanWidgets[port][fan], port, fan);
            
            // Clicking any fan in a port selects that entire port
            connect(m_fanWidgets[port][fan], &FanWidget::clicked, [this, port]() {
                selectPort(port);
            });
        }
    }
    
    m_leftLayout->addWidget(m_fanGroup);
    
    // Apply fan group styles
    setStyleSheet(R"(
        #fanGroup {
            color: #ffffff;
            font-size: 14px;
            font-weight: bold;
            border: 1px solid #404040;
            border-radius: 8px;
            padding: 15px;
        }
    )");
}

void SLInfinityPage::setupControls()
{
    // Lighting Effects controls
    m_lightingGroup = new QGroupBox("Lighting Effects");
    m_lightingGroup->setObjectName("controlGroup");
    
    QVBoxLayout *lightingLayout = new QVBoxLayout(m_lightingGroup);
    lightingLayout->setSpacing(10);
    
    // Effect selection - All OpenRGB effects
    m_effectCombo = new QComboBox();
    m_effectCombo->setObjectName("effectCombo");
    m_effectCombo->addItems({
        "Static",              // 0x01
        "Breathing",           // 0x02
        "Spectrum Cycle",      // 0x04 (Rainbow Morph)
        "Rainbow Wave",        // 0x05 (Rainbow)
        "Staggered",           // 0x18
        "Tide",                // 0x1A
        "Runway",              // 0x1C
        "Mixing",              // 0x1E
        "Stack",               // 0x20
        "Neon",                // 0x22
        "ColorCycle",          // 0x23
        "Meteor",              // 0x24
        "Voice",               // 0x26
        "Groove",              // 0x27
        "Tunnel"               // 0x29
    });
    m_effectCombo->setCurrentText("Rainbow Wave");
    
    connect(m_effectCombo, QOverload<const QString &>::of(&QComboBox::currentTextChanged),
            this, &SLInfinityPage::onEffectChanged);
    
    lightingLayout->addWidget(m_effectCombo);
    
    // Speed slider
    QHBoxLayout *speedLayout = new QHBoxLayout();
    QLabel *speedLabel = new QLabel("SPEED");
    speedLabel->setObjectName("sliderLabel");
    
    m_speedSlider = new QSlider(Qt::Horizontal);
    m_speedSlider->setObjectName("customSlider");
    m_speedSlider->setRange(0, 100);
    m_speedSlider->setValue(75);
    
    QLabel *speedValueLabel = new QLabel("75%");
    speedValueLabel->setObjectName("sliderValue");
    
    connect(m_speedSlider, &QSlider::valueChanged, [this, speedValueLabel](int value) {
        speedValueLabel->setText(QString::number(value) + "%");
        onSpeedChanged(value);
    });
    
    speedLayout->addWidget(speedLabel);
    speedLayout->addWidget(m_speedSlider);
    speedLayout->addWidget(speedValueLabel);
    
    lightingLayout->addLayout(speedLayout);
    
    // Brightness slider
    QHBoxLayout *brightnessLayout = new QHBoxLayout();
    QLabel *brightnessLabel = new QLabel("BRIGHTNESS");
    brightnessLabel->setObjectName("sliderLabel");
    
    m_brightnessSlider = new QSlider(Qt::Horizontal);
    m_brightnessSlider->setObjectName("customSlider");
    m_brightnessSlider->setRange(0, 100);
    m_brightnessSlider->setValue(100);
    
    QLabel *brightnessValueLabel = new QLabel("100%");
    brightnessValueLabel->setObjectName("sliderValue");
    
    connect(m_brightnessSlider, &QSlider::valueChanged, [this, brightnessValueLabel](int value) {
        brightnessValueLabel->setText(QString::number(value) + "%");
        onBrightnessChanged(value);
    });
    
    brightnessLayout->addWidget(brightnessLabel);
    brightnessLayout->addWidget(m_brightnessSlider);
    brightnessLayout->addWidget(brightnessValueLabel);
    
    lightingLayout->addLayout(brightnessLayout);
    
    // Direction controls
    m_directionLabel = new QLabel("DIRECTION");
    m_directionLabel->setObjectName("sliderLabel");
    lightingLayout->addWidget(m_directionLabel);
    
    m_directionLayout = new QHBoxLayout();
    m_directionLayout->setSpacing(10);
    
    m_leftDirectionBtn = new QPushButton("<<<<");
    m_leftDirectionBtn->setObjectName("directionButton");
    m_leftDirectionBtn->setCheckable(true);
    
    m_rightDirectionBtn = new QPushButton(">>>>");
    m_rightDirectionBtn->setObjectName("directionButton");
    m_rightDirectionBtn->setCheckable(true);
    m_rightDirectionBtn->setChecked(true);
    
    connect(m_leftDirectionBtn, &QPushButton::clicked, this, &SLInfinityPage::onDirectionChanged);
    connect(m_rightDirectionBtn, &QPushButton::clicked, this, &SLInfinityPage::onDirectionChanged);
    
    m_directionLayout->addWidget(m_leftDirectionBtn);
    m_directionLayout->addWidget(m_rightDirectionBtn);
    m_directionLayout->addStretch();
    
    lightingLayout->addLayout(m_directionLayout);
    
    // Color picker button (single color)
    m_colorLabel = new QLabel("COLOR");
    m_colorLabel->setObjectName("sliderLabel");
    lightingLayout->addWidget(m_colorLabel);
    
    m_colorButton = new QPushButton("Choose Color");
    m_colorButton->setObjectName("colorButton");
    m_colorButton->setStyleSheet("background-color: #ff0000; border: 2px solid #333; color: white; font-weight: bold;");
    m_colorButton->setMinimumHeight(40);
    
    connect(m_colorButton, &QPushButton::clicked, this, &SLInfinityPage::onColorButtonClicked);
    
    lightingLayout->addWidget(m_colorButton);
    
    // Multi-color widget (for Tide: 2 colors, ColorCycle: 3 colors)
    m_multiColorWidget = new QWidget();
    QVBoxLayout *multiColorLayout = new QVBoxLayout(m_multiColorWidget);
    multiColorLayout->setContentsMargins(0, 0, 0, 0);
    multiColorLayout->setSpacing(5);
    
    QLabel *multiColorLabel = new QLabel("COLORS");
    multiColorLabel->setObjectName("sliderLabel");
    multiColorLayout->addWidget(multiColorLabel);
    
    QHBoxLayout *colorButtonsLayout = new QHBoxLayout();
    colorButtonsLayout->setSpacing(5);
    
    for (int i = 0; i < 4; i++) {
        m_colorButtons[i] = new QPushButton(QString("Color %1").arg(i + 1));
        m_colorButtons[i]->setObjectName("colorButton");
        m_colorButtons[i]->setMinimumHeight(40);
        m_colorButtons[i]->setMinimumWidth(80);
        
        QColor defaultColor = m_portColors[0][i];
        m_colorButtons[i]->setStyleSheet(QString("background-color: %1; border: 2px solid #333; color: white; font-weight: bold;").arg(defaultColor.name()));
        
        connect(m_colorButtons[i], &QPushButton::clicked, [this, i]() {
            // Get color from selected port, or port 0 if none selected
            int portToUse = (m_selectedPort >= 0 && m_selectedPort < 4) ? m_selectedPort : 0;
            QColor currentColor = m_portColors[portToUse][i];
            QColor color = QColorDialog::getColor(currentColor, this, QString("Choose Color %1").arg(i + 1));
            if (color.isValid()) {
                // Update selected port(s) with this color
                if (m_selectedPort >= 0 && m_selectedPort < 4) {
                    m_portColors[m_selectedPort][i] = color;
                } else {
                    // No port selected, update all ports
                    for (int port = 0; port < 4; port++) {
                        m_portColors[port][i] = color;
                    }
                }
                m_colorButtons[i]->setStyleSheet(QString("background-color: %1; border: 2px solid #333; color: white; font-weight: bold;").arg(color.name()));
                applyCurrentEffect();
                saveLightingSettings();
            }
        });
        
        colorButtonsLayout->addWidget(m_colorButtons[i]);
    }
    
    multiColorLayout->addLayout(colorButtonsLayout);
    lightingLayout->addWidget(m_multiColorWidget);
    m_multiColorWidget->setVisible(false); // Hidden by default
    
    // Status label
    m_statusLabel = new QLabel("");
    m_statusLabel->setAlignment(Qt::AlignCenter);
    m_statusLabel->setMinimumHeight(20);
    lightingLayout->addWidget(m_statusLabel);
    
    // Action buttons
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    
    m_applyToAllLightingBtn = new QPushButton("Apply To All");
    m_applyToAllLightingBtn->setObjectName("actionButton");
    
    m_defaultBtn = new QPushButton("Default");
    m_defaultBtn->setObjectName("actionButton");
    
    connect(m_applyToAllLightingBtn, &QPushButton::clicked, this, &SLInfinityPage::onApplyToAll);
    connect(m_defaultBtn, &QPushButton::clicked, this, &SLInfinityPage::onDefault);
    
    buttonLayout->addWidget(m_applyToAllLightingBtn);
    buttonLayout->addWidget(m_defaultBtn);
    
    lightingLayout->addLayout(buttonLayout);
    lightingLayout->addStretch();
    
    m_rightLayout->addWidget(m_lightingGroup);
    
    // Apply control styles
    setStyleSheet(R"(
        #controlGroup {
            color: #ffffff;
            font-size: 14px;
            font-weight: bold;
            border: 1px solid #404040;
            border-radius: 8px;
            padding: 15px;
            margin-top: 10px;
        }
        
        #profileCombo, #effectCombo {
            background-color: #404040;
            color: #ffffff;
            border: 1px solid #555555;
            border-radius: 4px;
            padding: 8px;
            font-size: 12px;
        }
        
        #sliderLabel {
            color: #cccccc;
            font-size: 12px;
            font-weight: bold;
            min-width: 80px;
        }
        
        #customSlider {
            background-color: transparent;
        }
        
        #customSlider::groove:horizontal {
            background-color: #404040;
            height: 6px;
            border-radius: 3px;
        }
        
        #customSlider::handle:horizontal {
            background-color: #2a82da;
            width: 16px;
            height: 16px;
            border-radius: 8px;
            margin: -5px 0;
        }
        
        #customSlider::sub-page:horizontal {
            background-color: #2a82da;
            border-radius: 3px;
        }
        
        #sliderValue {
            color: #ffffff;
            font-size: 12px;
            font-weight: bold;
            min-width: 40px;
        }
        
        #directionButton {
            background-color: #404040;
            color: #cccccc;
            border: 1px solid #555555;
            border-radius: 4px;
            padding: 8px 16px;
            font-size: 12px;
            font-weight: bold;
        }
        
        #directionButton:checked {
            background-color: #2a82da;
            color: #ffffff;
        }
    )");
}

void SLInfinityPage::updateFanVisualization()
{
    // Update fan widgets with current settings
    for (int port = 0; port < 4; ++port) {
        for (int fan = 0; fan < 4; ++fan) {
            FanWidget *fanWidget = m_fanWidgets[port][fan];
            
            // Set RPM based on profile
            int rpm = 0;
            if (m_currentProfile == "Quiet") rpm = 950 + (port * 10) + (fan * 5);
            else if (m_currentProfile == "StdSP") rpm = 1100 + (port * 15) + (fan * 8);
            else if (m_currentProfile == "HighSP") rpm = 1300 + (port * 20) + (fan * 10);
            else if (m_currentProfile == "FullSP") rpm = 1500 + (port * 25) + (fan * 12);
            
            if (port == 2) rpm = 0; // Port 3 is off
            
            fanWidget->setRPM(rpm);
            fanWidget->setProfile(m_currentProfile);
            fanWidget->setLighting(m_currentEffect);
            fanWidget->setActive(rpm > 0);
        }
    }
}


void SLInfinityPage::onEffectChanged()
{
    QString oldEffect = m_currentEffect;
    m_currentEffect = m_effectCombo->currentText();
    
    // Clear old effect settings that don't apply to the new effect
    clearOldEffectSettings(oldEffect, m_currentEffect);
    
    updateFanVisualization();
    
    // Show/hide controls based on effect requirements
    bool needsColor = false;
    bool needsDirection = false;
    
    // Effects that need color (MODE_SPECIFIC_COLOR per OpenRGB)
    if (m_currentEffect == "Static" || 
        m_currentEffect == "Breathing" ||
        m_currentEffect == "Staggered" ||
        m_currentEffect == "Tide" ||
        m_currentEffect == "Runway" ||
        m_currentEffect == "Mixing" ||
        m_currentEffect == "Stack" ||
        m_currentEffect == "ColorCycle" ||
        m_currentEffect == "Meteor" ||
        m_currentEffect == "Groove" ||
        m_currentEffect == "Tunnel") {
        needsColor = true;
    }
    // Note: Spectrum Cycle, Rainbow Wave, Neon, Voice have no color controls
    
    // Effects with direction control (MODE_FLAG_HAS_DIRECTION_LR per OpenRGB)
    if (        m_currentEffect == "Rainbow Wave" ||
        m_currentEffect == "Stack" ||
        m_currentEffect == "ColorCycle" ||
        m_currentEffect == "Groove" ||
        m_currentEffect == "Tunnel") {
        needsDirection = true;
    }
    // Note: Static, Breathing, Staggered, Tide, Runway, Mixing, Meteor do NOT have direction control per OpenRGB
    
    // Update UI visibility based on effect - match OpenRGB specifications
    // Note: UI is limited to 4 color buttons, but OpenRGB supports up to 6 for Static/Breathing
    int numColorsNeeded = 1; // Default
    if (m_currentEffect == "Static" || m_currentEffect == "Breathing") {
        numColorsNeeded = 4; // OpenRGB allows up to 6 colors, but UI limited to 4 buttons
    } else if (m_currentEffect == "Staggered" || m_currentEffect == "Tide" || 
               m_currentEffect == "Runway" || m_currentEffect == "Mixing" || 
               m_currentEffect == "Meteor") {
        numColorsNeeded = 2; // OpenRGB allows up to 2 colors
    } else if (m_currentEffect == "ColorCycle") {
        numColorsNeeded = 3; // OpenRGB allows up to 3 colors
    } else if (m_currentEffect == "Tunnel") {
        numColorsNeeded = 4; // OpenRGB allows up to 4 colors
    } else if (m_currentEffect == "Stack" || m_currentEffect == "Groove") {
        numColorsNeeded = 1; // OpenRGB allows 1 color
    }
    
    // Show/hide single color button
    bool showSingleColor = needsColor && numColorsNeeded == 1;
    if (m_colorLabel) m_colorLabel->setVisible(showSingleColor);
    if (m_colorButton) m_colorButton->setVisible(showSingleColor);
    
    // Show/hide multi-color buttons
    bool showMultiColor = needsColor && numColorsNeeded > 1;
    if (m_multiColorWidget) {
        m_multiColorWidget->setVisible(showMultiColor);
        // Show only the needed number of buttons
        for (int i = 0; i < 4; i++) {
            if (m_colorButtons[i]) {
                m_colorButtons[i]->setVisible(i < numColorsNeeded);
            }
        }
    }
    
    if (m_directionLabel) m_directionLabel->setVisible(needsDirection);
    if (m_directionLayout) {
        m_leftDirectionBtn->setVisible(needsDirection);
        m_rightDirectionBtn->setVisible(needsDirection);
    }
    
    // Apply effect to device if connected
    applyCurrentEffect();
    
    // Save settings when effect changes
    saveLightingSettings();
}

void SLInfinityPage::applyCurrentEffect()
{
    if (!m_lianLi || !m_lianLi->isConnected()) {
        return;
    }
    
    QColor currentColor = m_colorButton->palette().button().color();
    if (!currentColor.isValid()) {
        currentColor = QColor(255, 0, 0); // Default red
    }
    
    if (m_currentEffect == "Static") {
        // Static supports up to 4 colors (one per fan) - apply to selected port(s)
        int portsToApply[4] = {0, 1, 2, 3};
        int portCount = 4;
        
        if (m_selectedPort >= 0 && m_selectedPort < 4) {
            portsToApply[0] = m_selectedPort;
            portCount = 1;
        }
        
        for (int i = 0; i < portCount; i++) {
            int port = portsToApply[i];
            int channel = port * 2;
            // Get 4 colors for this port (one per fan)
            QColor fanColors[4] = {
                m_portColors[port][0], 
                m_portColors[port][1], 
                m_portColors[port][2], 
                m_portColors[port][3]
            };
            // Fill in any invalid colors with defaults
            for (int j = 0; j < 4; j++) {
                if (!fanColors[j].isValid()) {
                    fanColors[j] = (j == 0) ? currentColor : fanColors[j-1];
                }
            }
            // Apply Static mode with 4 colors (one per fan) to both channels
            // Both channels (inner and outer rings) should get the same 4 colors
            // This ensures each fan is a solid color (both inner and outer match)
            m_lianLi->setChannelStaticWithFanColors(channel, fanColors, m_currentBrightness);
            // Increased delay between channels to ensure proper synchronization
            // The hardware may need time to process the first channel before accepting the second
            QThread::msleep(50);
            if (channel + 1 < 8) {
                m_lianLi->setChannelStaticWithFanColors(channel + 1, fanColors, m_currentBrightness);
                QThread::msleep(50); // Additional delay after second channel
            }
        }
    } else if (m_currentEffect == "Breathing") {
        // Breathing supports up to 6 colors per OpenRGB - apply to selected port(s)
        int portsToApply[4] = {0, 1, 2, 3};
        int portCount = 4;
        
        if (m_selectedPort >= 0 && m_selectedPort < 4) {
            portsToApply[0] = m_selectedPort;
            portCount = 1;
        }
        
        for (int i = 0; i < portCount; i++) {
            int port = portsToApply[i];
            QColor portColor = m_portColors[port][0];
            if (!portColor.isValid()) portColor = currentColor;
            int channel = port * 2;
            m_lianLi->setChannelBreathing(channel, portColor, m_currentSpeed, m_currentBrightness);
            if (channel + 1 < 8) {
                m_lianLi->setChannelBreathing(channel + 1, portColor, m_currentSpeed, m_currentBrightness);
            }
        }
    } else if (m_currentEffect == "Spectrum Cycle") {
        m_lianLi->setRainbowMorphEffect(m_currentSpeed, m_currentBrightness);
    } else if (m_currentEffect == "Rainbow Wave") {
        m_lianLi->setRainbowEffect(m_currentSpeed, m_currentBrightness, m_directionLeft);
    } else if (m_currentEffect == "Staggered") {
        // Staggered needs 2 colors per port - apply to selected port(s)
        int portsToApply[4] = {0, 1, 2, 3};
        int portCount = 4;
        
        if (m_selectedPort >= 0 && m_selectedPort < 4) {
            portsToApply[0] = m_selectedPort;
            portCount = 1;
        }
        
        for (int i = 0; i < portCount; i++) {
            int port = portsToApply[i];
            int channel = port * 2;
            QColor portColors[2] = {m_portColors[port][0], m_portColors[port][1]};
            if (!portColors[0].isValid()) portColors[0] = currentColor;
            if (!portColors[1].isValid()) portColors[1] = currentColor;
            // Staggered does NOT have direction control per OpenRGB
            m_lianLi->setChannelStaggered(channel, portColors, m_currentSpeed, m_currentBrightness);
            if (channel + 1 < 8) {
                m_lianLi->setChannelStaggered(channel + 1, portColors, m_currentSpeed, m_currentBrightness);
            }
        }
    } else if (m_currentEffect == "Tide") {
        // Tide needs 2 colors per port - apply to selected port(s)
        int portsToApply[4] = {0, 1, 2, 3};
        int portCount = 4;
        
        if (m_selectedPort >= 0 && m_selectedPort < 4) {
            portsToApply[0] = m_selectedPort;
            portCount = 1;
        }
        
        for (int i = 0; i < portCount; i++) {
            int port = portsToApply[i];
            int channel = port * 2;
            QColor portColors[2] = {m_portColors[port][0], m_portColors[port][1]};
            if (!portColors[0].isValid()) portColors[0] = QColor(255, 0, 0);
            if (!portColors[1].isValid()) portColors[1] = QColor(0, 0, 255);
            m_lianLi->setChannelTide(channel, portColors, m_currentSpeed, m_currentBrightness);
            if (channel + 1 < 8) {
                m_lianLi->setChannelTide(channel + 1, portColors, m_currentSpeed, m_currentBrightness);
            }
        }
    } else if (m_currentEffect == "Runway") {
        // Runway needs 2 colors per port - apply to selected port(s)
        int portsToApply[4] = {0, 1, 2, 3};
        int portCount = 4;
        
        if (m_selectedPort >= 0 && m_selectedPort < 4) {
            portsToApply[0] = m_selectedPort;
            portCount = 1;
        }
        
        for (int i = 0; i < portCount; i++) {
            int port = portsToApply[i];
            int channel = port * 2;
            QColor portColors[2] = {m_portColors[port][0], m_portColors[port][1]};
            if (!portColors[0].isValid()) portColors[0] = QColor(255, 200, 100);
            if (!portColors[1].isValid()) portColors[1] = QColor(255, 200, 100);
            // Runway does NOT have direction control per OpenRGB
            m_lianLi->setChannelRunwayWithColors(channel, portColors, m_currentSpeed, m_currentBrightness, false);
            if (channel + 1 < 8) {
                m_lianLi->setChannelRunwayWithColors(channel + 1, portColors, m_currentSpeed, m_currentBrightness, false);
            }
        }
    } else if (m_currentEffect == "Mixing") {
        // Mixing needs 2 colors per port - apply to selected port(s)
        int portsToApply[4] = {0, 1, 2, 3};
        int portCount = 4;
        
        if (m_selectedPort >= 0 && m_selectedPort < 4) {
            portsToApply[0] = m_selectedPort;
            portCount = 1;
        }
        
        for (int i = 0; i < portCount; i++) {
            int port = portsToApply[i];
            int channel = port * 2;
            QColor portColors[2] = {m_portColors[port][0], m_portColors[port][1]};
            if (!portColors[0].isValid()) portColors[0] = currentColor;
            if (!portColors[1].isValid()) portColors[1] = currentColor;
            // Mixing does NOT have direction control per OpenRGB
            m_lianLi->setChannelMixing(channel, portColors, m_currentSpeed, m_currentBrightness);
            if (channel + 1 < 8) {
                m_lianLi->setChannelMixing(channel + 1, portColors, m_currentSpeed, m_currentBrightness);
            }
        }
    } else if (m_currentEffect == "Stack") {
        // Stack needs 1 color per port - apply to selected port(s)
        int portsToApply[4] = {0, 1, 2, 3};
        int portCount = 4;
        
        if (m_selectedPort >= 0 && m_selectedPort < 4) {
            portsToApply[0] = m_selectedPort;
            portCount = 1;
        }
        
        for (int i = 0; i < portCount; i++) {
            int port = portsToApply[i];
            QColor portColor = m_portColors[port][0];
            if (!portColor.isValid()) portColor = currentColor;
            int channel = port * 2;
            m_lianLi->setChannelStack(channel, portColor, m_currentSpeed, m_currentBrightness, m_directionLeft);
            if (channel + 1 < 8) {
                m_lianLi->setChannelStack(channel + 1, portColor, m_currentSpeed, m_currentBrightness, m_directionLeft);
            }
        }
    } else if (m_currentEffect == "Neon") {
        m_lianLi->setNeonEffect(m_currentSpeed, m_currentBrightness);
    } else if (m_currentEffect == "ColorCycle") {
        // ColorCycle needs 3 colors per port - apply to selected port(s)
        int portsToApply[4] = {0, 1, 2, 3};
        int portCount = 4;
        
        if (m_selectedPort >= 0 && m_selectedPort < 4) {
            portsToApply[0] = m_selectedPort;
            portCount = 1;
        }
        
        for (int i = 0; i < portCount; i++) {
            int port = portsToApply[i];
            int channel = port * 2;
            QColor portColors[3] = {m_portColors[port][0], m_portColors[port][1], m_portColors[port][2]};
            if (!portColors[0].isValid()) portColors[0] = QColor(255, 0, 0);
            if (!portColors[1].isValid()) portColors[1] = QColor(0, 255, 0);
            if (!portColors[2].isValid()) portColors[2] = QColor(0, 0, 255);
            m_lianLi->setChannelColorCycle(channel, portColors, m_currentSpeed, m_currentBrightness, m_directionLeft);
            if (channel + 1 < 8) {
                m_lianLi->setChannelColorCycle(channel + 1, portColors, m_currentSpeed, m_currentBrightness, m_directionLeft);
            }
        }
    } else if (m_currentEffect == "Meteor") {
        // Meteor needs 2 colors per port (like Tide/Runway) - apply to selected port(s)
        int portsToApply[4] = {0, 1, 2, 3};
        int portCount = 4;
        
        if (m_selectedPort >= 0 && m_selectedPort < 4) {
            portsToApply[0] = m_selectedPort;
            portCount = 1;
        }
        
        for (int i = 0; i < portCount; i++) {
            int port = portsToApply[i];
            int channel = port * 2;
            QColor portColors[2] = {m_portColors[port][0], m_portColors[port][1]};
            if (!portColors[0].isValid()) portColors[0] = QColor(255, 0, 0);
            if (!portColors[1].isValid()) portColors[1] = QColor(0, 0, 255);
            // Meteor does NOT have direction control per OpenRGB
            m_lianLi->setChannelMeteorWithColors(channel, portColors, m_currentSpeed, m_currentBrightness, false);
            if (channel + 1 < 8) {
                m_lianLi->setChannelMeteorWithColors(channel + 1, portColors, m_currentSpeed, m_currentBrightness, false);
            }
        }
    } else if (m_currentEffect == "Voice") {
        m_lianLi->setVoiceEffect(m_currentSpeed, m_currentBrightness);
    } else if (m_currentEffect == "Groove") {
        // Groove needs 1 color per port - apply to selected port(s)
        int portsToApply[4] = {0, 1, 2, 3};
        int portCount = 4;
        
        if (m_selectedPort >= 0 && m_selectedPort < 4) {
            portsToApply[0] = m_selectedPort;
            portCount = 1;
        }
        
        for (int i = 0; i < portCount; i++) {
            int port = portsToApply[i];
            QColor portColor = m_portColors[port][0];
            if (!portColor.isValid()) portColor = currentColor;
            QColor grooveColors[2] = { portColor, QColor(0, 0, 0) };
            int channel = port * 2;
            m_lianLi->setChannelGroove(channel, grooveColors, m_currentSpeed, m_currentBrightness, m_directionLeft);
            if (channel + 1 < 8) {
                m_lianLi->setChannelGroove(channel + 1, grooveColors, m_currentSpeed, m_currentBrightness, m_directionLeft);
            }
        }
    } else if (m_currentEffect == "Tunnel") {
        // Tunnel needs 4 colors per port - apply to selected port(s)
        int portsToApply[4] = {0, 1, 2, 3};
        int portCount = 4;
        
        if (m_selectedPort >= 0 && m_selectedPort < 4) {
            portsToApply[0] = m_selectedPort;
            portCount = 1;
        }
        
        for (int i = 0; i < portCount; i++) {
            int port = portsToApply[i];
            int channel = port * 2;
            QColor portColors[4] = {m_portColors[port][0], m_portColors[port][1], 
                                   m_portColors[port][2], m_portColors[port][3]};
            if (!portColors[0].isValid()) portColors[0] = currentColor;
            if (!portColors[1].isValid()) portColors[1] = currentColor;
            if (!portColors[2].isValid()) portColors[2] = currentColor;
            if (!portColors[3].isValid()) portColors[3] = currentColor;
            m_lianLi->setChannelTunnel(channel, portColors, m_currentSpeed, m_currentBrightness, m_directionLeft);
            if (channel + 1 < 8) {
                m_lianLi->setChannelTunnel(channel + 1, portColors, m_currentSpeed, m_currentBrightness, m_directionLeft);
            }
        }
    }
}

void SLInfinityPage::onSpeedChanged(int value)
{
    m_currentSpeed = value;
    applyCurrentEffect();
    updateFanVisualization();
    saveLightingSettings();
}

void SLInfinityPage::onBrightnessChanged(int value)
{
    m_currentBrightness = value;
    applyCurrentEffect();
    updateFanVisualization();
    saveLightingSettings();
}

void SLInfinityPage::onDirectionChanged()
{
    QPushButton *button = qobject_cast<QPushButton*>(sender());
    if (!button) return;
    
    if (button == m_leftDirectionBtn) {
        m_leftDirectionBtn->setChecked(true);
        m_rightDirectionBtn->setChecked(false);
        m_directionLeft = true;
    } else {
        m_leftDirectionBtn->setChecked(false);
        m_rightDirectionBtn->setChecked(true);
        m_directionLeft = false;
    }
    
    applyCurrentEffect();
    updateFanVisualization();
}

void SLInfinityPage::onApplyToAll()
{
    // Apply current effect to all ports (ignore selection)
    int savedSelection = m_selectedPort;
    m_selectedPort = -1; // -1 means apply to all
    applyCurrentEffect();
    m_selectedPort = savedSelection; // Restore selection
    updateFanVisualization();
}

void SLInfinityPage::onDefault()
{
    m_effectCombo->setCurrentText("Rainbow Wave");
    m_speedSlider->setValue(75);
    m_brightnessSlider->setValue(100);
    m_rightDirectionBtn->setChecked(true);
    m_leftDirectionBtn->setChecked(false);
    
    m_currentProfile = "Quiet";
    m_currentEffect = "Rainbow Wave";
    m_currentSpeed = 75;
    m_currentBrightness = 100;
    m_directionLeft = false;
    
    applyCurrentEffect();
    updateFanVisualization();
}


void SLInfinityPage::onColorButtonClicked()
{
    if (!m_lianLi || !m_lianLi->isConnected()) {
        QMessageBox::warning(this, "Device Not Connected", 
            "Lian Li device is not connected. Please check your device and try again.");
        return;
    }
    
    QColor currentColor = m_portColors[0][0];
    if (!currentColor.isValid()) {
        currentColor = QColor(255, 0, 0);
    }
    
    QColor color = QColorDialog::getColor(currentColor, this, "Choose LED Color");
    
    if (color.isValid()) {
        // Update button color
        m_colorButton->setStyleSheet(QString("background-color: %1; border: 2px solid #333; color: white; font-weight: bold;").arg(color.name()));
        
        // Update selected port(s) with this color - if none selected, update all
        if (m_selectedPort >= 0 && m_selectedPort < 4) {
            m_portColors[m_selectedPort][0] = color;
        } else {
            // No port selected, update all ports
            for (int port = 0; port < 4; port++) {
                m_portColors[port][0] = color;
            }
        }
        
        // Apply current effect with new color
        applyCurrentEffect();
        
        // Save settings when color changes
        saveLightingSettings();
        
        if (m_lianLi->isConnected()) {
            m_statusLabel->setText("✓ Color applied successfully!");
            m_statusLabel->setStyleSheet("color: green; font-weight: bold;");
        } else {
            m_statusLabel->setText("✗ Failed to apply color");
            m_statusLabel->setStyleSheet("color: red; font-weight: bold;");
        }
        
        // Clear status after 3 seconds
        if (m_statusTimer) {
            m_statusTimer->stop();
        }
        m_statusTimer = new QTimer(this);
        m_statusTimer->setSingleShot(true);
        m_statusTimer->setInterval(3000);
        connect(m_statusTimer, &QTimer::timeout, [this]() {
            m_statusLabel->setText("");
            m_statusLabel->setStyleSheet("");
        });
        m_statusTimer->start();
    }
}

void SLInfinityPage::onDeviceConnected()
{
    if (m_statusLabel) {
        m_statusLabel->setText("✓ Lian Li SL Infinity connected");
        m_statusLabel->setStyleSheet("color: green; font-weight: bold;");
    }
    
    // Enable controls
    if (m_colorButton) {
        m_colorButton->setEnabled(true);
    }
}

void SLInfinityPage::onDeviceDisconnected()
{
    if (m_statusLabel) {
        m_statusLabel->setText("✗ Lian Li SL Infinity disconnected");
        m_statusLabel->setStyleSheet("color: red; font-weight: bold;");
    }
    
    // Disable controls
    if (m_colorButton) {
        m_colorButton->setEnabled(false);
    }
}

void SLInfinityPage::selectPort(int port)
{
    if (port < 0 || port >= 4) {
        return;
    }
    
    m_selectedPort = port;
    updatePortSelection();
}

void SLInfinityPage::updatePortSelection()
{
    // Update visual selection state for all fan widgets
    for (int port = 0; port < 4; ++port) {
        bool isSelected = (port == m_selectedPort);
        for (int fan = 0; fan < 4; ++fan) {
            if (m_fanWidgets[port][fan]) {
                m_fanWidgets[port][fan]->setSelected(isSelected);
            }
        }
    }
}

void SLInfinityPage::saveLightingSettings()
{
    QSettings settings("LianLi", "LConnect3");
    
    // Save basic settings
    settings.setValue("SLInfinity/Effect", m_currentEffect);
    settings.setValue("SLInfinity/Speed", m_currentSpeed);
    settings.setValue("SLInfinity/Brightness", m_currentBrightness);
    settings.setValue("SLInfinity/DirectionLeft", m_directionLeft);
    settings.setValue("SLInfinity/SelectedPort", m_selectedPort);
    
    // Save port colors (4 ports, 4 colors each)
    settings.beginWriteArray("SLInfinity/PortColors");
    for (int port = 0; port < 4; ++port) {
        for (int colorIdx = 0; colorIdx < 4; ++colorIdx) {
            settings.setArrayIndex(port * 4 + colorIdx);
            settings.setValue("R", m_portColors[port][colorIdx].red());
            settings.setValue("G", m_portColors[port][colorIdx].green());
            settings.setValue("B", m_portColors[port][colorIdx].blue());
        }
    }
    settings.endArray();
}

void SLInfinityPage::loadLightingSettings()
{
    QSettings settings("LianLi", "LConnect3");
    
    // Load basic settings with defaults
    m_currentEffect = settings.value("SLInfinity/Effect", "Rainbow Wave").toString();
    m_currentSpeed = settings.value("SLInfinity/Speed", 75).toInt();
    m_currentBrightness = settings.value("SLInfinity/Brightness", 100).toInt();
    m_directionLeft = settings.value("SLInfinity/DirectionLeft", false).toBool();
    m_selectedPort = settings.value("SLInfinity/SelectedPort", -1).toInt();
    
    // Load port colors
    int size = settings.beginReadArray("SLInfinity/PortColors");
    for (int i = 0; i < size && i < 16; ++i) {
        settings.setArrayIndex(i);
        int port = i / 4;
        int colorIdx = i % 4;
        if (port < 4 && colorIdx < 4) {
            int r = settings.value("R", 255).toInt();
            int g = settings.value("G", 0).toInt();
            int b = settings.value("B", 0).toInt();
            // Default colors: Red, Blue, Green, Yellow
            if (size < 16) {
                // If no saved colors, use defaults
                if (colorIdx == 0) { r = 255; g = 0; b = 0; }
                else if (colorIdx == 1) { r = 0; g = 0; b = 255; }
                else if (colorIdx == 2) { r = 0; g = 255; b = 0; }
                else if (colorIdx == 3) { r = 255; g = 255; b = 0; }
            }
            m_portColors[port][colorIdx] = QColor(r, g, b);
        }
    }
    settings.endArray();
    
    // Apply loaded settings to UI
    if (m_effectCombo) {
        m_effectCombo->setCurrentText(m_currentEffect);
    }
    if (m_speedSlider) {
        m_speedSlider->setValue(m_currentSpeed);
    }
    if (m_brightnessSlider) {
        m_brightnessSlider->setValue(m_currentBrightness);
    }
    
    // Set direction buttons
    if (m_leftDirectionBtn && m_rightDirectionBtn) {
        if (m_directionLeft) {
            m_leftDirectionBtn->setChecked(true);
            m_rightDirectionBtn->setChecked(false);
        } else {
            m_leftDirectionBtn->setChecked(false);
            m_rightDirectionBtn->setChecked(true);
        }
    }
    
    // Update color buttons
    if (m_colorButton) {
        QColor firstColor = m_portColors[0][0];
        if (!firstColor.isValid()) {
            firstColor = QColor(255, 0, 0);
        }
        m_colorButton->setStyleSheet(QString("background-color: %1; border: 2px solid #333; color: white; font-weight: bold;").arg(firstColor.name()));
    }
    
    // Update multi-color buttons
    for (int i = 0; i < 4; i++) {
        if (m_colorButtons[i]) {
            QColor color = m_portColors[0][i];
            if (!color.isValid()) {
                if (i == 0) color = QColor(255, 0, 0);
                else if (i == 1) color = QColor(0, 0, 255);
                else if (i == 2) color = QColor(0, 255, 0);
                else if (i == 3) color = QColor(255, 255, 0);
            }
            m_colorButtons[i]->setStyleSheet(QString("background-color: %1; border: 2px solid #333; color: white; font-weight: bold;").arg(color.name()));
        }
    }
    
    // Update port selection
    updatePortSelection();
    
    // Update UI visibility based on loaded effect (without triggering apply)
    // This mirrors the logic in onEffectChanged but without applying the effect
    bool needsColor = false;
    bool needsDirection = false;
    
    if (m_currentEffect == "Static" || m_currentEffect == "Breathing" ||
        m_currentEffect == "Staggered" || m_currentEffect == "Tide" ||
        m_currentEffect == "Runway" || m_currentEffect == "Mixing" ||
        m_currentEffect == "Stack" || m_currentEffect == "ColorCycle" ||
        m_currentEffect == "Meteor" || m_currentEffect == "Groove" ||
        m_currentEffect == "Tunnel") {
        needsColor = true;
    }
    
    if (m_currentEffect == "Rainbow Wave" || m_currentEffect == "Stack" ||
        m_currentEffect == "ColorCycle" ||
        m_currentEffect == "Groove" ||
        m_currentEffect == "Tunnel") {
        needsDirection = true;
    }
    
    int numColorsNeeded = 1;
    if (m_currentEffect == "Static" || m_currentEffect == "Breathing") {
        numColorsNeeded = 4;
    } else if (m_currentEffect == "Staggered" || m_currentEffect == "Tide" || 
               m_currentEffect == "Runway" || m_currentEffect == "Mixing" || 
               m_currentEffect == "Meteor") {
        numColorsNeeded = 2;
    } else if (m_currentEffect == "ColorCycle") {
        numColorsNeeded = 3;
    } else if (m_currentEffect == "Tunnel") {
        numColorsNeeded = 4;
    } else if (m_currentEffect == "Stack" || m_currentEffect == "Groove") {
        numColorsNeeded = 1;
    }
    
    bool showSingleColor = needsColor && numColorsNeeded == 1;
    if (m_colorLabel) m_colorLabel->setVisible(showSingleColor);
    if (m_colorButton) m_colorButton->setVisible(showSingleColor);
    
    bool showMultiColor = needsColor && numColorsNeeded > 1;
    if (m_multiColorWidget) {
        m_multiColorWidget->setVisible(showMultiColor);
        for (int i = 0; i < 4; i++) {
            if (m_colorButtons[i]) {
                m_colorButtons[i]->setVisible(i < numColorsNeeded);
            }
        }
    }
    
    if (m_directionLabel) m_directionLabel->setVisible(needsDirection);
    if (m_directionLayout) {
        if (m_leftDirectionBtn) m_leftDirectionBtn->setVisible(needsDirection);
        if (m_rightDirectionBtn) m_rightDirectionBtn->setVisible(needsDirection);
    }
}

void SLInfinityPage::clearOldEffectSettings(const QString &oldEffect, const QString &newEffect)
{
    // Determine how many colors each effect needs
    int oldNumColors = 0;
    int newNumColors = 0;
    
    if (oldEffect == "Static" || oldEffect == "Breathing" || oldEffect == "Tunnel") {
        oldNumColors = 4;
    } else if (oldEffect == "Staggered" || oldEffect == "Tide" || oldEffect == "Runway" || 
               oldEffect == "Mixing" || oldEffect == "Meteor") {
        oldNumColors = 2;
    } else if (oldEffect == "ColorCycle") {
        oldNumColors = 3;
    } else if (oldEffect == "Stack" || oldEffect == "Groove") {
        oldNumColors = 1;
    }
    
    if (newEffect == "Static" || newEffect == "Breathing" || newEffect == "Tunnel") {
        newNumColors = 4;
    } else if (newEffect == "Staggered" || newEffect == "Tide" || newEffect == "Runway" || 
               newEffect == "Mixing" || newEffect == "Meteor") {
        newNumColors = 2;
    } else if (newEffect == "ColorCycle") {
        newNumColors = 3;
    } else if (newEffect == "Stack" || newEffect == "Groove") {
        newNumColors = 1;
    }
    
    // Clear colors that don't apply to the new effect
    // Keep colors 0..newNumColors-1, clear colors newNumColors..3
    if (newNumColors < oldNumColors || newNumColors < 4) {
        for (int port = 0; port < 4; port++) {
            for (int colorIdx = newNumColors; colorIdx < 4; colorIdx++) {
                // Reset to default colors instead of invalid
                if (colorIdx == 0) {
                    m_portColors[port][colorIdx] = QColor(255, 0, 0); // Red
                } else if (colorIdx == 1) {
                    m_portColors[port][colorIdx] = QColor(0, 0, 255); // Blue
                } else if (colorIdx == 2) {
                    m_portColors[port][colorIdx] = QColor(0, 255, 0); // Green
                } else if (colorIdx == 3) {
                    m_portColors[port][colorIdx] = QColor(255, 255, 0); // Yellow
                }
            }
        }
        
        // Update UI buttons for cleared colors
        for (int i = newNumColors; i < 4; i++) {
            if (m_colorButtons[i]) {
                QColor defaultColor;
                if (i == 0) defaultColor = QColor(255, 0, 0);
                else if (i == 1) defaultColor = QColor(0, 0, 255);
                else if (i == 2) defaultColor = QColor(0, 255, 0);
                else if (i == 3) defaultColor = QColor(255, 255, 0);
                m_colorButtons[i]->setStyleSheet(QString("background-color: %1; border: 2px solid #333; color: white; font-weight: bold;").arg(defaultColor.name()));
            }
        }
    }
}
