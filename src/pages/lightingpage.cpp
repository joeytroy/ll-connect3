#include "lightingpage.h"
#include "widgets/customslider.h"
#include "widgets/toggleswitch.h"
#include "lian_li_qt_integration.h"
#include "utils/qtdebugutil.h"
#include <QFont>
#include <QDebug>
#include <QColorDialog>
#include <QSettings>
#include <QShowEvent>
#include <QGridLayout>
#include <QGroupBox>
#include <QThread>

LightingPage::LightingPage(QWidget *parent)
    : QWidget(parent)
    , m_currentEffect("Rainbow Wave")
    , m_currentSpeed(75)
    , m_currentBrightness(100)
    , m_directionLeft(false)
    , m_selectedPort(-1)
    , m_isLoading(false)
    , m_lightingEnabled(true)
    , m_lianLi(nullptr)
{
    // Initialize port colors (2D array: [port][color_index])
    // All ports start with the same default color (white) for consistency
    QColor defaultColor(255, 255, 255);  // White
    for (int port = 0; port < 4; port++) {
        m_portColors[port][0] = defaultColor;
        m_portColors[port][1] = defaultColor;
        m_portColors[port][2] = defaultColor;
        m_portColors[port][3] = defaultColor;
    }
    
    // Initialize all ports as enabled by default
    m_portEnabled[0] = true;
    m_portEnabled[1] = true;
    m_portEnabled[2] = true;
    m_portEnabled[3] = true;
    
    // Initialize Lian Li integration
    m_lianLi = new LianLiQtIntegration(this);
    connect(m_lianLi, &LianLiQtIntegration::deviceConnected, this, &LightingPage::onDeviceConnected);
    connect(m_lianLi, &LianLiQtIntegration::deviceDisconnected, this, &LightingPage::onDeviceDisconnected);
    
    setupUI();
    setupControls();
    // Load saved lighting settings
    loadLightingSettings();
    
    // Load fan configuration and update button states
    loadFanConfiguration();
    updatePortButtonStates();
    
    
    // Try to initialize the device
    if (m_lianLi->initialize()) {
        onDeviceConnected();
    } else {
        DEBUG_LOG("Lian Li device not connected");
    }
}

void LightingPage::setupUI()
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(20, 20, 20, 20);
    m_mainLayout->setSpacing(20);
    
    // Header with lighting toggle
    QHBoxLayout *headerLayout = new QHBoxLayout();
    headerLayout->setContentsMargins(0, 0, 0, 0);
    
    QLabel *titleLabel = new QLabel("Lighting Control");
    titleLabel->setStyleSheet("color: #ffffff; font-size: 18px; font-weight: bold;");
    
    m_lightingToggle = new ToggleSwitch();
    m_lightingToggle->setChecked(true);
    connect(m_lightingToggle, &ToggleSwitch::toggled, this, &LightingPage::onLightingToggled);
    
    m_lightingStatusLabel = new QLabel("ON");
    m_lightingStatusLabel->setFixedWidth(30);
    m_lightingStatusLabel->setStyleSheet("color: #cccccc; font-size: 13px; font-weight: bold;");
    
    headerLayout->addWidget(titleLabel);
    headerLayout->addStretch();
    headerLayout->addWidget(m_lightingToggle);
    headerLayout->addWidget(m_lightingStatusLabel);
    
    m_mainLayout->addLayout(headerLayout);
    
    // Content layout
    m_contentLayout = new QHBoxLayout();
    m_contentLayout->setSpacing(30);
    
    m_leftLayout = new QVBoxLayout();
    m_rightLayout = new QVBoxLayout();
    
    m_contentLayout->addLayout(m_leftLayout, 1);
    m_contentLayout->addLayout(m_rightLayout, 1);
    
    m_mainLayout->addLayout(m_contentLayout);
}

void LightingPage::setupControls()
{
    // Lighting Effects group
    m_lightingGroup = new QGroupBox("Lighting Effects");
    m_lightingGroup->setObjectName("controlGroup");
    
    QVBoxLayout *lightingLayout = new QVBoxLayout(m_lightingGroup);
    lightingLayout->setSpacing(15);
    
    // Effect selection
    QLabel *effectLabel = new QLabel("Lighting Effects");
    effectLabel->setObjectName("controlLabel");
    
    m_effectCombo = new QComboBox();
    m_effectCombo->setObjectName("effectCombo");
    m_effectCombo->addItems({
        "Breathing",           // 0x02 - per-port color
        "Color Cycle",         // 0x23 - 3 colors, direction
        "Groove",              // 0x27 - 2 colors, direction
        "Meteor",              // 0x24 - 2 colors
        "Mixing",              // 0x1E - 2 colors
        "Neon",                // 0x22 - no colors
        "Rainbow Wave",        // 0x05 - no colors, direction
        "Render",              // 0x28 - 4 colors, direction
        "Runway",              // 0x1C - 2 colors
        "Spectrum Cycle",      // 0x04 - no colors
        "Stack",               // 0x20 - 1 color, direction
        "Stack Multi Color",   // 0x21 - no colors, direction
        "Staggered",           // 0x18 - 2 colors
        "Static",              // 0x01 - per-port color
        "Tide",                // 0x1A - 2 colors
        "Tunnel",              // 0x29 - 4 colors, direction
        "Voice"                // 0x26 - no colors
    });
    m_effectCombo->setCurrentText("Rainbow Wave");
    
    connect(m_effectCombo, QOverload<const QString &>::of(&QComboBox::currentTextChanged),
            this, &LightingPage::onEffectChanged);
    
    lightingLayout->addWidget(effectLabel);
    lightingLayout->addWidget(m_effectCombo);
    
    // Static Color specific controls (also used for Breathing and Meteor)
    m_staticColorWidget = new QWidget();
    QVBoxLayout *staticColorLayout = new QVBoxLayout(m_staticColorWidget);
    staticColorLayout->setContentsMargins(0, 0, 0, 0);
    
    m_colorLabel = new QLabel("PORT COLORS");
    m_colorLabel->setObjectName("controlLabel");
    staticColorLayout->addWidget(m_colorLabel);
    
    m_colorBoxLayout = new QHBoxLayout();
    m_colorBoxLayout->setSpacing(15);
    
    for (int i = 0; i < 4; ++i) {
        // Create a vertical layout for button + label
        QVBoxLayout *portLayout = new QVBoxLayout();
        portLayout->setSpacing(5);
        portLayout->setAlignment(Qt::AlignCenter);
        
        // Color button
        m_colorButtons[i] = new QPushButton();
        m_colorButtons[i]->setObjectName("colorButton");
        m_colorButtons[i]->setFixedSize(40, 40);
        m_colorButtons[i]->setProperty("portIndex", i);
        updateColorButton(i);
        
        connect(m_colorButtons[i], &QPushButton::clicked, this, &LightingPage::onColorButtonClicked);
        
        // Port/Fan label (will be updated based on effect)
        m_colorLabels[i] = new QLabel(QString("Port %1").arg(i + 1));
        m_colorLabels[i]->setObjectName("portLabel");
        m_colorLabels[i]->setAlignment(Qt::AlignCenter);
        
        portLayout->addWidget(m_colorButtons[i]);
        portLayout->addWidget(m_colorLabels[i]);
        
        m_colorBoxLayout->addLayout(portLayout);
    }
    m_colorBoxLayout->addStretch();
    staticColorLayout->addLayout(m_colorBoxLayout);
    
    lightingLayout->addWidget(m_staticColorWidget);
    
    // Initially hide static color widget (only show for Static Color effect)
    m_staticColorWidget->setVisible(false);
    
    // Speed slider (25% increments: 0, 25, 50, 75, 100)
    m_speedSlider = new CustomSlider("SPEED");
    m_speedSlider->setSnapToIncrements(true, 25);  // Enable 25% snapping (0, 25, 50, 75, 100)
    m_speedSlider->setRange(0, 100);  // This will be converted to 0-4 internally (5 positions)
    m_speedSlider->setPageStep(1);  // Single step moves one position at a time (25%)
    m_speedSlider->setTickInterval(1);  // Show tick marks at each position
    m_speedSlider->setValue(50);  // Default to 50% (medium speed, position 2)
    connect(m_speedSlider, &CustomSlider::valueChanged, this, &LightingPage::onSpeedChanged);
    lightingLayout->addWidget(m_speedSlider);
    
    // Brightness slider (25% increments: 0, 25, 50, 75, 100)
    m_brightnessSlider = new CustomSlider("BRIGHTNESS");
    m_brightnessSlider->setSnapToIncrements(true, 25);  // Enable 25% snapping (0, 25, 50, 75, 100)
    m_brightnessSlider->setRange(0, 100);  // This will be converted to 0-4 internally (5 positions)
    m_brightnessSlider->setPageStep(1);  // Single step moves one position at a time (25%)
    m_brightnessSlider->setTickInterval(1);  // Show tick marks at each position
    m_brightnessSlider->setValue(100);  // Default to 100% (full brightness, position 4)
    connect(m_brightnessSlider, &CustomSlider::valueChanged, this, &LightingPage::onBrightnessChanged);
    lightingLayout->addWidget(m_brightnessSlider);
    
    // Direction controls (wrapped in a widget for show/hide)
    m_directionWidget = new QWidget();
    QVBoxLayout *directionWidgetLayout = new QVBoxLayout(m_directionWidget);
    directionWidgetLayout->setContentsMargins(0, 0, 0, 0);
    
    QLabel *directionLabel = new QLabel("DIRECTION");
    directionLabel->setObjectName("controlLabel");
    directionWidgetLayout->addWidget(directionLabel);
    
    m_directionLayout = new QHBoxLayout();
    m_directionLayout->setSpacing(10);
    
    m_leftDirectionBtn = new QPushButton("<<<<");
    m_leftDirectionBtn->setObjectName("directionButton");
    m_leftDirectionBtn->setCheckable(true);
    
    m_rightDirectionBtn = new QPushButton(">>>>");
    m_rightDirectionBtn->setObjectName("directionButton");
    m_rightDirectionBtn->setCheckable(true);
    m_rightDirectionBtn->setChecked(true);
    
    connect(m_leftDirectionBtn, &QPushButton::clicked, this, &LightingPage::onDirectionChanged);
    connect(m_rightDirectionBtn, &QPushButton::clicked, this, &LightingPage::onDirectionChanged);
    
    m_directionLayout->addWidget(m_leftDirectionBtn);
    m_directionLayout->addWidget(m_rightDirectionBtn);
    m_directionLayout->addStretch();
    
    directionWidgetLayout->addLayout(m_directionLayout);
    lightingLayout->addWidget(m_directionWidget);
    
    // Apply button
    m_applyBtn = new QPushButton("Apply");
    m_applyBtn->setObjectName("applyButton");
    connect(m_applyBtn, &QPushButton::clicked, this, &LightingPage::onApply);
    
    lightingLayout->addWidget(m_applyBtn);
    lightingLayout->addStretch();
    
    m_leftLayout->addWidget(m_lightingGroup);
    
    // Apply control styles
    setStyleSheet(R"(
        #controlGroup {
            color: #ffffff;
            font-size: 14px;
            font-weight: bold;
            border: 1px solid #404040;
            border-radius: 8px;
            padding: 15px;
        }
        
        #controlLabel {
            color: #cccccc;
            font-size: 12px;
            font-weight: bold;
        }
        
        #effectCombo {
            background-color: #404040;
            color: #ffffff;
            border: 1px solid #555555;
            border-radius: 4px;
            padding: 8px;
            font-size: 12px;
        }
        
        #effectCombo::drop-down {
            border: none;
        }
        
        #effectCombo::down-arrow {
            image: url(:/icons/resources/dropdown-arrow.svg);
            width: 12px;
            height: 12px;
            margin-right: 8px;
        }
        
        #effectCombo:disabled {
            background-color: #2a2a2a;
            color: #666666;
            border-color: #383838;
        }
        
        #effectCombo::down-arrow:disabled {
            image: none;
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
        
        #applyButton {
            background-color: #2a82da;
            color: #ffffff;
            border: none;
            padding: 10px 20px;
            border-radius: 4px;
            font-size: 14px;
            font-weight: bold;
        }
        
        #applyButton:hover {
            background-color: #1e6bb8;
        }
        
        #colorButton {
            border: 2px solid #555555;
            border-radius: 4px;
            min-width: 40px;
            min-height: 40px;
        }
        
        #colorButton:hover {
            border-color: #2a82da;
        }
        
        #portLabel {
            color: #cccccc;
            font-size: 11px;
            font-weight: normal;
        }
        
        
        #directionButton:disabled {
            background-color: #2a2a2a;
            color: #555555;
            border-color: #383838;
        }
        
        #applyButton:disabled {
            background-color: #2a2a2a;
            color: #555555;
        }
        
        #colorButton:disabled {
            border-color: #383838;
            background-color: #2a2a2a;
        }
        
        #controlLabel:disabled {
            color: #555555;
        }
        
        #portLabel:disabled {
            color: #555555;
        }
    )");
}

void LightingPage::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    
    // Reload fan configuration when the page becomes visible
    // This ensures we pick up any changes made in Settings
    loadFanConfiguration();
    updatePortButtonStates();
}


bool LightingPage::isPerPortEffect() const
{
    return m_currentEffect == "Static" || m_currentEffect == "Breathing";
}

int LightingPage::getEffectColorCount() const
{
    if (m_currentEffect == "Static" || m_currentEffect == "Breathing")
        return 4;  // per-port: 4 port buttons
    if (m_currentEffect == "Staggered" || m_currentEffect == "Tide" ||
        m_currentEffect == "Runway" || m_currentEffect == "Mixing")
        return 2;
    if (m_currentEffect == "Meteor" || m_currentEffect == "Groove")
        return 1;
    if (m_currentEffect == "Stack")
        return 1;
    if (m_currentEffect == "Color Cycle")
        return 3;
    if (m_currentEffect == "Tunnel" || m_currentEffect == "Render")
        return 4;
    return 0;  // Rainbow Wave, Spectrum Cycle, Neon, Voice, Stack Multi Color
}

bool LightingPage::effectHasDirection() const
{
    return m_currentEffect == "Rainbow Wave" ||
           m_currentEffect == "Stack" ||
           m_currentEffect == "Stack Multi Color" ||
           m_currentEffect == "Color Cycle" ||
           m_currentEffect == "Groove" ||
           m_currentEffect == "Tunnel" ||
           m_currentEffect == "Render";
}

bool LightingPage::effectHasSpeed() const
{
    return m_currentEffect != "Static";
}

void LightingPage::updateEffectUI()
{
    int colorCount = getEffectColorCount();
    bool perPort = isPerPortEffect();
    bool showColors = (colorCount > 0);

    if (m_colorLabel) m_colorLabel->setVisible(false);

    if (m_staticColorWidget) {
        m_staticColorWidget->setVisible(showColors);

        if (showColors) {
            int buttonsToShow = perPort ? 4 : colorCount;
            for (int i = 0; i < 4; i++) {
                if (m_colorButtons[i])
                    m_colorButtons[i]->setVisible(i < buttonsToShow);
                if (m_colorLabels[i]) {
                    if (perPort)
                        m_colorLabels[i]->setText(QString("Port %1").arg(i + 1));
                    else
                        m_colorLabels[i]->setText(QString("Color %1").arg(i + 1));
                }
            }
        }
    }

    if (m_speedSlider) m_speedSlider->setVisible(effectHasSpeed());
    if (m_directionWidget) m_directionWidget->setVisible(effectHasDirection());

    for (int i = 0; i < 4; ++i) {
        updateColorButton(i);
    }
}

void LightingPage::onEffectChanged()
{
    if (m_isLoading) return;

    QString oldEffect = m_currentEffect;

    if (!oldEffect.isEmpty()) {
        saveEffectColors(oldEffect);
    }

    m_currentEffect = m_effectCombo->currentText();

    loadEffectColors(m_currentEffect);

    clearOldEffectSettings(oldEffect, m_currentEffect);

    updateEffectUI();

    applyCurrentEffect();

    saveLightingSettings();
}

void LightingPage::onSpeedChanged(int value)
{
    m_currentSpeed = value;
    saveLightingSettings();
}

void LightingPage::onBrightnessChanged(int value)
{
    m_currentBrightness = value;
    applyCurrentEffect();
    saveLightingSettings();
}

void LightingPage::onDirectionChanged()
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
    saveLightingSettings();
}

void LightingPage::onApply()
{
    // Apply effect to device
    applyCurrentEffect();
    
    // Save settings when applying
    saveLightingSettings();
}

void LightingPage::onLightingToggled(bool enabled)
{
    m_lightingEnabled = enabled;
    m_lightingStatusLabel->setText(enabled ? "ON" : "OFF");
    
    // Enable/disable all lighting controls
    m_effectCombo->setEnabled(enabled);
    m_speedSlider->setEnabled(enabled);
    m_brightnessSlider->setEnabled(enabled);
    m_directionWidget->setEnabled(enabled);
    m_staticColorWidget->setEnabled(enabled);
    m_applyBtn->setEnabled(enabled);
    
    saveLightingSettings();
    
    if (enabled && m_lianLi && m_lianLi->isConnected()) {
        applyCurrentEffect();
    }
}

void LightingPage::applyCurrentEffect()
{
    if (!m_lightingEnabled) return;
    
    if (!m_lianLi || !m_lianLi->isConnected()) {
        DEBUG_LOG("Device not connected - cannot apply lighting");
        return;
    }
    
    bool success = false;
    
    DEBUG_LOG("Applying effect:", m_currentEffect, 
             "Speed:", m_currentSpeed, 
             "Brightness:", m_currentBrightness, 
             "Direction:", (m_directionLeft ? "Left" : "Right"));

    // ── No-color effects ──────────────────────────────────────
    if (m_currentEffect == "Rainbow Wave") {
        success = m_lianLi->setRainbowEffect(m_currentSpeed, m_currentBrightness, m_directionLeft);
    }
    else if (m_currentEffect == "Spectrum Cycle") {
        success = m_lianLi->setRainbowMorphEffect(m_currentSpeed, m_currentBrightness);
    }
    else if (m_currentEffect == "Neon") {
        success = m_lianLi->setAllChannelsEffect(0x22, QColor(), m_currentSpeed, m_currentBrightness, false);
    }
    else if (m_currentEffect == "Voice") {
        success = m_lianLi->setVoiceEffect(m_currentSpeed, m_currentBrightness);
    }
    else if (m_currentEffect == "Stack Multi Color") {
        success = m_lianLi->setStackMultiColorEffect(m_currentSpeed, m_currentBrightness, m_directionLeft);
    }

    // ── Per-port color effects ────────────────────────────────
    else if (m_currentEffect == "Static") {
        success = true;
        for (int port = 0; port < 4; port++) {
            if (!m_portEnabled[port]) continue;
            QColor portColor = m_portColors[port][0];
            if (!portColor.isValid()) portColor = QColor(255, 0, 0);
            int ch = port * 2;
            if (!m_lianLi->setChannelColor(ch, portColor, m_currentBrightness)) success = false;
            if (!m_lianLi->setChannelColor(ch + 1, portColor, m_currentBrightness)) success = false;
        }
    }
    else if (m_currentEffect == "Breathing") {
        success = true;
        for (int port = 0; port < 4; port++) {
            if (!m_portEnabled[port]) continue;
            QColor portColor = m_portColors[port][0];
            if (!portColor.isValid()) portColor = QColor(255, 0, 0);
            int ch = port * 2;
            m_lianLi->setChannelBreathing(ch, portColor, m_currentSpeed, m_currentBrightness);
            m_lianLi->setChannelBreathing(ch + 1, portColor, m_currentSpeed, m_currentBrightness);
        }
    }

    // ── 1-color mode-specific effects ─────────────────────────
    else if (m_currentEffect == "Stack") {
        QColor color = m_portColors[0][0];
        if (!color.isValid()) color = QColor(255, 0, 0);
        success = m_lianLi->setStackEffect(color, m_currentSpeed, m_currentBrightness, m_directionLeft);
    }

    // ── 2-color mode-specific effects ─────────────────────────
    else if (m_currentEffect == "Staggered") {
        QColor colors[2] = { m_portColors[0][0], m_portColors[0][1] };
        success = true;
        for (int port = 0; port < 4; port++) {
            if (!m_portEnabled[port]) continue;
            int ch = port * 2;
            m_lianLi->setChannelStaggered(ch, colors, m_currentSpeed, m_currentBrightness);
            m_lianLi->setChannelStaggered(ch + 1, colors, m_currentSpeed, m_currentBrightness);
        }
    }
    else if (m_currentEffect == "Tide") {
        QColor colors[2] = { m_portColors[0][0], m_portColors[0][1] };
        success = true;
        for (int port = 0; port < 4; port++) {
            if (!m_portEnabled[port]) continue;
            int ch = port * 2;
            m_lianLi->setChannelTide(ch, colors, m_currentSpeed, m_currentBrightness);
            m_lianLi->setChannelTide(ch + 1, colors, m_currentSpeed, m_currentBrightness);
        }
    }
    else if (m_currentEffect == "Runway") {
        QColor colors[2] = { m_portColors[0][0], m_portColors[0][1] };
        success = true;
        for (int port = 0; port < 4; port++) {
            if (!m_portEnabled[port]) continue;
            int ch = port * 2;
            m_lianLi->setChannelRunwayWithColors(ch, colors, m_currentSpeed, m_currentBrightness, false);
            m_lianLi->setChannelRunwayWithColors(ch + 1, colors, m_currentSpeed, m_currentBrightness, false);
        }
    }
    else if (m_currentEffect == "Mixing") {
        QColor colors[2] = { m_portColors[0][0], m_portColors[0][1] };
        success = true;
        for (int port = 0; port < 4; port++) {
            if (!m_portEnabled[port]) continue;
            int ch = port * 2;
            m_lianLi->setChannelMixing(ch, colors, m_currentSpeed, m_currentBrightness);
            m_lianLi->setChannelMixing(ch + 1, colors, m_currentSpeed, m_currentBrightness);
        }
    }
    else if (m_currentEffect == "Meteor") {
        QColor colors[2] = { m_portColors[0][0], QColor(0, 0, 0) };
        success = true;
        for (int port = 0; port < 4; port++) {
            if (!m_portEnabled[port]) continue;
            int ch = port * 2;
            m_lianLi->setChannelMeteorWithColors(ch, colors, m_currentSpeed, m_currentBrightness, false);
            QThread::msleep(10);
            m_lianLi->setChannelMeteorWithColors(ch + 1, colors, m_currentSpeed, m_currentBrightness, false);
            QThread::msleep(10);
        }
    }
    else if (m_currentEffect == "Groove") {
        QColor colors[2] = { m_portColors[0][0], QColor(0, 0, 0) };
        success = true;
        for (int port = 0; port < 4; port++) {
            if (!m_portEnabled[port]) continue;
            int ch = port * 2;
            m_lianLi->setChannelGroove(ch, colors, m_currentSpeed, m_currentBrightness, m_directionLeft);
            m_lianLi->setChannelGroove(ch + 1, colors, m_currentSpeed, m_currentBrightness, m_directionLeft);
        }
    }

    // ── 3-color mode-specific effects ─────────────────────────
    else if (m_currentEffect == "Color Cycle") {
        QColor colors[3] = { m_portColors[0][0], m_portColors[0][1], m_portColors[0][2] };
        success = m_lianLi->setColorCycleEffect(colors, m_currentSpeed, m_currentBrightness, m_directionLeft);
    }

    // ── 4-color mode-specific effects ─────────────────────────
    else if (m_currentEffect == "Tunnel") {
        QColor colors[4] = { m_portColors[0][0], m_portColors[0][1],
                             m_portColors[0][2], m_portColors[0][3] };
        success = m_lianLi->setTunnelEffect(colors, m_currentSpeed, m_currentBrightness, m_directionLeft);
    }
    else if (m_currentEffect == "Render") {
        QColor colors[4] = { m_portColors[0][0], m_portColors[0][1],
                             m_portColors[0][2], m_portColors[0][3] };
        success = m_lianLi->setRenderEffect(colors, m_currentSpeed, m_currentBrightness, m_directionLeft);
    }
    
    if (success) {
        DEBUG_LOG("Successfully applied effect:", m_currentEffect);
    } else {
        DEBUG_LOG("Failed to apply effect:", m_currentEffect);
    }
}

void LightingPage::onDeviceConnected()
{
    DEBUG_LOG("Lian Li device connected");
    if (!m_lightingEnabled) return;
    applyCurrentEffect();
}

void LightingPage::onDeviceDisconnected()
{
    DEBUG_LOG("Lian Li device disconnected");
}

void LightingPage::onColorButtonClicked()
{
    QPushButton *button = qobject_cast<QPushButton*>(sender());
    if (!button) return;
    
    int buttonIndex = button->property("portIndex").toInt();
    if (buttonIndex < 0 || buttonIndex >= 4) return;
    
    int port, colorIdx;
    QString label;
    
    if (isPerPortEffect()) {
        port = buttonIndex;
        colorIdx = 0;
        label = QString("Select Color for Port %1").arg(port + 1);
    } else {
        port = 0;
        colorIdx = buttonIndex;
        label = QString("Select Color %1").arg(colorIdx + 1);
    }
    
    QColor currentColor = m_portColors[port][colorIdx];
    QColor newColor = QColorDialog::getColor(currentColor, this, label);
    
    if (newColor.isValid()) {
        m_portColors[port][colorIdx] = newColor;
        updateColorButton(buttonIndex);
        applyCurrentEffect();
        saveLightingSettings();
    }
}

void LightingPage::updateColorButton(int buttonIndex)
{
    if (buttonIndex < 0 || buttonIndex >= 4) return;
    
    QColor color;
    
    if (isPerPortEffect()) {
        color = m_portColors[buttonIndex][0];
    } else {
        color = m_portColors[0][buttonIndex];
    }
    
    QString style = QString("QPushButton { background-color: %1; border: 2px solid #555555; border-radius: 4px; }")
                   .arg(color.name());
    m_colorButtons[buttonIndex]->setStyleSheet(style);
}

void LightingPage::saveLightingSettings()
{
    if (m_isLoading) return;

    QSettings settings("LConnect3", "Lighting");
    
    // Save basic settings
    settings.setValue("LightingEnabled", m_lightingEnabled);
    settings.setValue("Effect", m_currentEffect);
    settings.setValue("Speed", m_currentSpeed);
    settings.setValue("Brightness", m_currentBrightness);
    settings.setValue("DirectionLeft", m_directionLeft);
    
    // Save colors for the current effect (effect-specific colors)
    saveEffectColors(m_currentEffect);
    
    // Also save to legacy "PortColors" for backward compatibility
    settings.beginWriteArray("PortColors");
    for (int port = 0; port < 4; ++port) {
        for (int colorIdx = 0; colorIdx < 4; ++colorIdx) {
            settings.setArrayIndex(port * 4 + colorIdx);
            settings.setValue("R", m_portColors[port][colorIdx].red());
            settings.setValue("G", m_portColors[port][colorIdx].green());
            settings.setValue("B", m_portColors[port][colorIdx].blue());
        }
    }
    settings.endArray();
    
    // Save selected port
    settings.setValue("SelectedPort", m_selectedPort);
    
    DEBUG_LOG("Saved lighting settings: Effect=", m_currentEffect, 
             "Speed=", m_currentSpeed, 
             "Brightness=", m_currentBrightness);
}

void LightingPage::loadLightingSettings()
{
    m_isLoading = true;

    QSettings settings("LConnect3", "Lighting");

    m_lightingEnabled = settings.value("LightingEnabled", true).toBool();
    m_currentEffect = settings.value("Effect", "Rainbow Wave").toString();
    m_currentSpeed = settings.value("Speed", 75).toInt();
    m_currentBrightness = settings.value("Brightness", 100).toInt();
    m_directionLeft = settings.value("DirectionLeft", false).toBool();

    loadEffectColors(m_currentEffect);

    // Legacy fallback: if no effect-specific colors exist, try old PortColors array
    QString effectKey = QString("EffectColors/%1").arg(m_currentEffect);
    int effectSize = settings.beginReadArray(effectKey);
    settings.endArray();

    if (effectSize == 0) {
        int size = settings.beginReadArray("PortColors");
        if (size > 0) {
            for (int i = 0; i < size && i < 16; ++i) {
                settings.setArrayIndex(i);
                int port = i / 4;
                int colorIdx = i % 4;
                if (port < 4 && colorIdx < 4) {
                    int r = settings.value("R", 255).toInt();
                    int g = settings.value("G", 255).toInt();
                    int b = settings.value("B", 255).toInt();
                    m_portColors[port][colorIdx] = QColor(r, g, b);
                }
            }
            saveEffectColors(m_currentEffect);
        }
        settings.endArray();
    }

    m_selectedPort = settings.value("SelectedPort", -1).toInt();

    // Update UI without triggering signal handlers
    m_effectCombo->blockSignals(true);
    m_effectCombo->setCurrentText(m_currentEffect);
    m_effectCombo->blockSignals(false);

    m_speedSlider->blockSignals(true);
    m_speedSlider->setValue(m_currentSpeed);
    m_speedSlider->blockSignals(false);

    m_brightnessSlider->blockSignals(true);
    m_brightnessSlider->setValue(m_currentBrightness);
    m_brightnessSlider->blockSignals(false);

    if (m_directionLeft) {
        m_leftDirectionBtn->setChecked(true);
        m_rightDirectionBtn->setChecked(false);
    } else {
        m_leftDirectionBtn->setChecked(false);
        m_rightDirectionBtn->setChecked(true);
    }

    for (int i = 0; i < 4; ++i) {
        updateColorButton(i);
    }

    // Update lighting toggle state
    m_lightingToggle->blockSignals(true);
    m_lightingToggle->setChecked(m_lightingEnabled);
    m_lightingToggle->blockSignals(false);
    m_lightingStatusLabel->setText(m_lightingEnabled ? "ON" : "OFF");
    
    m_effectCombo->setEnabled(m_lightingEnabled);
    m_speedSlider->setEnabled(m_lightingEnabled);
    m_brightnessSlider->setEnabled(m_lightingEnabled);
    m_directionWidget->setEnabled(m_lightingEnabled);
    m_staticColorWidget->setEnabled(m_lightingEnabled);
    m_applyBtn->setEnabled(m_lightingEnabled);

    m_isLoading = false;

    updateEffectUI();
}

void LightingPage::resetToDefaults()
{
    QColor defaultColor(255, 255, 255);

    for (int port = 0; port < 4; port++) {
        for (int c = 0; c < 4; c++) {
            m_portColors[port][c] = defaultColor;
        }
    }

    m_currentEffect = "Rainbow Wave";
    m_currentSpeed = 75;
    m_currentBrightness = 100;
    m_directionLeft = false;
    m_selectedPort = -1;
    m_lightingEnabled = true;

    m_lightingToggle->blockSignals(true);
    m_lightingToggle->setChecked(true);
    m_lightingToggle->blockSignals(false);
    m_lightingStatusLabel->setText("ON");

    m_effectCombo->setEnabled(true);
    m_speedSlider->setEnabled(true);
    m_brightnessSlider->setEnabled(true);
    m_directionWidget->setEnabled(true);
    m_staticColorWidget->setEnabled(true);
    m_applyBtn->setEnabled(true);

    m_effectCombo->blockSignals(true);
    m_effectCombo->setCurrentText(m_currentEffect);
    m_effectCombo->blockSignals(false);

    m_speedSlider->blockSignals(true);
    m_speedSlider->setValue(m_currentSpeed);
    m_speedSlider->blockSignals(false);

    m_brightnessSlider->blockSignals(true);
    m_brightnessSlider->setValue(m_currentBrightness);
    m_brightnessSlider->blockSignals(false);

    m_leftDirectionBtn->setChecked(false);
    m_rightDirectionBtn->setChecked(true);

    for (int i = 0; i < 4; ++i) {
        updateColorButton(i);
    }

    updateEffectUI();
    applyCurrentEffect();
    saveLightingSettings();
}

void LightingPage::loadFanConfiguration()
{
    QSettings settings("LianLi", "LConnect3");
    
    // Load which ports have fans connected
    m_portEnabled[0] = settings.value("FanConfig/Port1", true).toBool();
    m_portEnabled[1] = settings.value("FanConfig/Port2", true).toBool();
    m_portEnabled[2] = settings.value("FanConfig/Port3", true).toBool();
    m_portEnabled[3] = settings.value("FanConfig/Port4", true).toBool();
}

void LightingPage::updatePortButtonStates()
{
    bool perPort = isPerPortEffect();
    
    for (int i = 0; i < 4; ++i) {
        if (m_colorButtons[i]) {
            // For per-port effects, disable buttons for ports without fans.
            // For mode-specific effects, all color buttons stay enabled
            // since they represent effect colors, not physical ports.
            bool enabled = perPort ? m_portEnabled[i] : true;
            m_colorButtons[i]->setEnabled(enabled);
            
            if (enabled) {
                updateColorButton(i);
            } else {
                m_colorButtons[i]->setStyleSheet(
                    "background-color: #404040; "
                    "border: 2px solid #555555; "
                    "border-radius: 4px;"
                );
            }
        }
    }
}

void LightingPage::clearOldEffectSettings(const QString &/*oldEffect*/, const QString &newEffect)
{
    // Save the current m_currentEffect temporarily to query the new effect's color count
    QString saved = m_currentEffect;
    m_currentEffect = newEffect;
    int newNumColors = getEffectColorCount();
    m_currentEffect = saved;

    QColor defaultColor(255, 255, 255);
    for (int port = 0; port < 4; port++) {
        for (int colorIdx = newNumColors; colorIdx < 4; colorIdx++) {
            m_portColors[port][colorIdx] = defaultColor;
        }
    }
}

void LightingPage::saveEffectColors(const QString &effectName)
{
    // Save colors for this specific effect under a key that includes the effect name
    QSettings settings("LConnect3", "Lighting");
    
    QString effectKey = QString("EffectColors/%1").arg(effectName);
    settings.beginWriteArray(effectKey);
    
    for (int port = 0; port < 4; ++port) {
        for (int colorIdx = 0; colorIdx < 4; ++colorIdx) {
            settings.setArrayIndex(port * 4 + colorIdx);
            settings.setValue("R", m_portColors[port][colorIdx].red());
            settings.setValue("G", m_portColors[port][colorIdx].green());
            settings.setValue("B", m_portColors[port][colorIdx].blue());
        }
    }
    settings.endArray();
    
    DEBUG_LOG("Saved colors for effect:", effectName);
}

void LightingPage::loadEffectColors(const QString &effectName)
{
    // Load colors for this specific effect, or use defaults if not saved
    QSettings settings("LConnect3", "Lighting");
    
    QString effectKey = QString("EffectColors/%1").arg(effectName);
    int size = settings.beginReadArray(effectKey);
    
    if (size > 0) {
        // Load saved colors for this effect
        for (int i = 0; i < size && i < 16; ++i) {
            settings.setArrayIndex(i);
            int port = i / 4;
            int colorIdx = i % 4;
            if (port < 4 && colorIdx < 4) {
                int r = settings.value("R", 255).toInt();
                int g = settings.value("G", 255).toInt();
                int b = settings.value("B", 255).toInt();
                m_portColors[port][colorIdx] = QColor(r, g, b);
            }
        }
        DEBUG_LOG("Loaded saved colors for effect:", effectName);
    } else {
        // No saved colors for this effect - use default white for all colors
        QColor defaultColor(255, 255, 255);  // White
        for (int port = 0; port < 4; port++) {
            for (int colorIdx = 0; colorIdx < 4; colorIdx++) {
                m_portColors[port][colorIdx] = defaultColor;
            }
        }
        DEBUG_LOG("No saved colors for effect:", effectName, "- using defaults");
    }
    settings.endArray();
    
    // Update color buttons to reflect the loaded colors
    for (int i = 0; i < 4; ++i) {
        updateColorButton(i);
    }
}

