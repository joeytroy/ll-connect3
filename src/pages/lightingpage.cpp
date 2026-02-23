#include "lightingpage.h"
#include "widgets/customslider.h"
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
        "Breathing",
        "Groove",
        "Meteor",
        "Mixing",
        "Neon",
        "Rainbow Wave",
        "Runway",
        "Spectrum Cycle",
        "Stack",
        "Staggered",
        "Static",
        "Tide",
        "Tunnel",
        "Voice"
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
        QVBoxLayout *portLayout = new QVBoxLayout();
        portLayout->setSpacing(5);
        portLayout->setAlignment(Qt::AlignCenter);

        m_colorButtons[i] = new QPushButton();
        m_colorButtons[i]->setObjectName("colorButton");
        m_colorButtons[i]->setFixedSize(40, 40);
        m_colorButtons[i]->setProperty("portIndex", i);
        updateColorButton(i);

        connect(m_colorButtons[i], &QPushButton::clicked, this, &LightingPage::onColorButtonClicked);

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
    m_staticColorWidget->setVisible(false);

    // Speed slider
    m_speedSlider = new CustomSlider("SPEED");
    m_speedSlider->setSnapToIncrements(true, 25);
    m_speedSlider->setRange(0, 100);
    m_speedSlider->setPageStep(1);
    m_speedSlider->setTickInterval(1);
    m_speedSlider->setValue(50);
    connect(m_speedSlider, &CustomSlider::valueChanged, this, &LightingPage::onSpeedChanged);
    lightingLayout->addWidget(m_speedSlider);

    // Brightness slider
    m_brightnessSlider = new CustomSlider("BRIGHTNESS");
    m_brightnessSlider->setSnapToIncrements(true, 25);
    m_brightnessSlider->setRange(0, 100);
    m_brightnessSlider->setPageStep(1);
    m_brightnessSlider->setTickInterval(1);
    m_brightnessSlider->setValue(100);
    connect(m_brightnessSlider, &CustomSlider::valueChanged, this, &LightingPage::onBrightnessChanged);
    lightingLayout->addWidget(m_brightnessSlider);

    // Direction controls
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
            image: none;
            border-left: 5px solid transparent;
            border-right: 5px solid transparent;
            border-top: 5px solid #cccccc;
            margin-right: 8px;
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
    )");
}

void LightingPage::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    loadFanConfiguration();
    updatePortButtonStates();
}

// ---------------------------------------------------------------------------
// updateEffectUI: updates control visibility/labels for m_currentEffect only.
// Does NOT save settings or apply to hardware. Safe to call during load.
// ---------------------------------------------------------------------------
void LightingPage::updateEffectUI()
{
    bool needsColor = false;
    bool needsDirection = false;

    if (m_currentEffect == "Static" ||
        m_currentEffect == "Breathing" ||
        m_currentEffect == "Staggered" ||
        m_currentEffect == "Tide" ||
        m_currentEffect == "Runway" ||
        m_currentEffect == "Mixing" ||
        m_currentEffect == "Stack" ||
        m_currentEffect == "Meteor" ||
        m_currentEffect == "Groove" ||
        m_currentEffect == "Tunnel" ||
        m_currentEffect == "Neon") {
        needsColor = true;
    }

    if (m_currentEffect == "Rainbow Wave" ||
        m_currentEffect == "Stack" ||
        m_currentEffect == "Groove" ||
        m_currentEffect == "Tunnel") {
        needsDirection = true;
    }

    int numColorsNeeded = 1;
    if (m_currentEffect == "Breathing") {
        numColorsNeeded = 4;
    } else if (m_currentEffect == "Static") {
        numColorsNeeded = 4;
    } else if (m_currentEffect == "Groove") {
        numColorsNeeded = 4;
    } else if (m_currentEffect == "Staggered" || m_currentEffect == "Tide") {
        numColorsNeeded = 2;
    } else if (m_currentEffect == "Runway") {
        numColorsNeeded = 1;
    } else if (m_currentEffect == "Meteor" || m_currentEffect == "Mixing") {
        numColorsNeeded = 1;
    } else if (m_currentEffect == "Neon") {
        numColorsNeeded = 1;
    } else if (m_currentEffect == "Tunnel") {
        numColorsNeeded = 4;
    } else if (m_currentEffect == "Stack") {
        numColorsNeeded = 1;
    }

    bool showSingleColor = needsColor && numColorsNeeded == 1 &&
                           m_currentEffect != "Meteor" && m_currentEffect != "Mixing" &&
                           m_currentEffect != "Neon" && m_currentEffect != "Runway" &&
                           m_currentEffect != "Stack";
    if (m_colorLabel) m_colorLabel->setVisible(showSingleColor);

    bool showMultiColor = needsColor && (numColorsNeeded > 1 ||
                           m_currentEffect == "Meteor" || m_currentEffect == "Mixing" ||
                           m_currentEffect == "Neon" || m_currentEffect == "Runway" ||
                           m_currentEffect == "Stack");
    if (m_staticColorWidget) {
        m_staticColorWidget->setVisible(showMultiColor);
        int buttonsToShow;
        if (m_currentEffect == "Groove" ||
            m_currentEffect == "Static" || m_currentEffect == "Breathing" ||
            m_currentEffect == "Meteor" || m_currentEffect == "Mixing" ||
            m_currentEffect == "Neon" || m_currentEffect == "Runway" || m_currentEffect == "Stack") {
            buttonsToShow = 4;
        } else {
            buttonsToShow = numColorsNeeded;
        }
        for (int i = 0; i < 4; i++) {
            if (m_colorButtons[i]) {
                m_colorButtons[i]->setVisible(i < buttonsToShow);
            }
        }
    }

    if (m_currentEffect == "Meteor" || m_currentEffect == "Static" ||
        m_currentEffect == "Breathing" || m_currentEffect == "Groove" ||
        m_currentEffect == "Mixing" || m_currentEffect == "Neon" ||
        m_currentEffect == "Runway" || m_currentEffect == "Stack") {
        if (m_colorLabel) m_colorLabel->setText("PORT COLORS");
        for (int i = 0; i < 4; ++i) {
            if (m_colorLabels[i]) {
                m_colorLabels[i]->setText(QString("Port %1").arg(i + 1));
            }
        }
    } else {
        if (m_colorLabel) m_colorLabel->setText("PORT COLORS");
        for (int i = 0; i < 4; ++i) {
            if (m_colorLabels[i]) {
                m_colorLabels[i]->setText(QString("Port %1").arg(i + 1));
            }
        }
    }

    bool isStatic = (m_currentEffect == "Static");
    if (m_speedSlider) m_speedSlider->setVisible(!isStatic);

    if (m_directionWidget) m_directionWidget->setVisible(needsDirection);

    for (int i = 0; i < 4; ++i) {
        updateColorButton(i);
    }
}

void LightingPage::onEffectChanged()
{
    // Suppress during loadLightingSettings to prevent overwriting saved data
    if (m_isLoading) return;

    QString oldEffect = m_currentEffect;

    // Save colors for the old effect before switching
    if (!oldEffect.isEmpty()) {
        saveEffectColors(oldEffect);
    }

    m_currentEffect = m_effectCombo->currentText();

    // Load colors for the new effect (or use defaults if not saved)
    loadEffectColors(m_currentEffect);

    // Clear old effect settings that don't apply to the new effect
    clearOldEffectSettings(oldEffect, m_currentEffect);

    // Update UI visibility/labels
    updateEffectUI();

    // Apply effect to device if connected
    applyCurrentEffect();

    // Save settings when effect changes
    saveLightingSettings();
}

void LightingPage::onSpeedChanged(int value)
{
    m_currentSpeed = value;
    saveLightingSettings();  // FIX: speed was never saved before
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
    applyCurrentEffect();
    saveLightingSettings();
}

void LightingPage::applyCurrentEffect()
{
    if (!m_lianLi || !m_lianLi->isConnected()) {
        DEBUG_LOG("Device not connected - cannot apply lighting");
        return;
    }

    bool success = false;

    DEBUG_LOG("Applying effect:", m_currentEffect,
             "Speed:", m_currentSpeed,
             "Brightness:", m_currentBrightness,
             "Direction:", (m_directionLeft ? "Left" : "Right"));

    if (m_currentEffect == "Rainbow Wave") {
        success = m_lianLi->setRainbowEffect(m_currentSpeed, m_currentBrightness, m_directionLeft);
    } else if (m_currentEffect == "Spectrum Cycle") {
        success = m_lianLi->setRainbowMorphEffect(m_currentSpeed, m_currentBrightness);
    } else if (m_currentEffect == "Static") {
        int portsToApply[4] = {0, 1, 2, 3};
        int portCount = 4;

        if (m_selectedPort >= 0 && m_selectedPort < 4) {
            portsToApply[0] = m_selectedPort;
            portCount = 1;
        }

        QColor currentColor = QColor(255, 0, 0);
        success = true;

        for (int i = 0; i < portCount; i++) {
            int port = portsToApply[i];
            if (!m_portEnabled[port]) continue;

            QColor portColor = m_portColors[port][0];
            if (!portColor.isValid()) portColor = currentColor;

            int channel1 = port * 2;
            int channel2 = port * 2 + 1;

            DEBUG_LOG("Setting Static for Port", (port + 1), "via channels", channel1, "&", channel2,
                     "to color", portColor, "brightness", m_currentBrightness);

            bool channel1Success = m_lianLi->setChannelColor(channel1, portColor, m_currentBrightness);
            bool channel2Success = m_lianLi->setChannelColor(channel2, portColor, m_currentBrightness);

            if (!channel1Success) {
                DEBUG_LOG("Failed to set Static for Port", (port + 1), "channel", channel1);
                success = false;
            }
            if (!channel2Success) {
                DEBUG_LOG("Failed to set Static for Port", (port + 1), "channel", channel2);
                success = false;
            }

            if (channel1Success && channel2Success) {
                DEBUG_LOG("✓ Successfully set Port", (port + 1));
            }
        }
    } else if (m_currentEffect == "Breathing") {
        int portsToApply[4] = {0, 1, 2, 3};
        int portCount = 4;

        if (m_selectedPort >= 0 && m_selectedPort < 4) {
            portsToApply[0] = m_selectedPort;
            portCount = 1;
        }

        QColor currentColor = QColor(255, 0, 0);

        for (int i = 0; i < portCount; i++) {
            int port = portsToApply[i];
            if (!m_portEnabled[port]) continue;

            QColor portColor = m_portColors[port][0];
            if (!portColor.isValid()) portColor = currentColor;
            int channel = port * 2;
            m_lianLi->setChannelBreathing(channel, portColor, m_currentSpeed, m_currentBrightness);
            if (channel + 1 < 8) {
                m_lianLi->setChannelBreathing(channel + 1, portColor, m_currentSpeed, m_currentBrightness);
            }
            success = true;
        }
    } else if (m_currentEffect == "Meteor") {
        int portsToApply[4] = {0, 1, 2, 3};
        int portCount = 4;
        if (m_selectedPort >= 0 && m_selectedPort < 4) {
            portsToApply[0] = m_selectedPort;
            portCount = 1;
        }

        QColor currentColor = QColor(255, 0, 0);

        for (int i = 0; i < portCount; i++) {
            int port = portsToApply[i];
            if (!m_portEnabled[port]) continue;

            QColor portColor = m_portColors[port][0];
            if (!portColor.isValid()) portColor = currentColor;

            QColor portColors[2] = {portColor, portColor};

            int channel = port * 2;
            DEBUG_LOG("Applying Meteor to Port", (port + 1), "channel", channel,
                     "Color(RGB):", portColor.red(), portColor.green(), portColor.blue(),
                     "Speed:", m_currentSpeed, "Brightness:", m_currentBrightness);

            m_lianLi->setChannelMeteorWithColors(channel, portColors, m_currentSpeed, m_currentBrightness, false);
            QThread::msleep(10);
            if (channel + 1 < 8) {
                m_lianLi->setChannelMeteorWithColors(channel + 1, portColors, m_currentSpeed, m_currentBrightness, false);
            }
            QThread::msleep(50);
            success = true;
        }
    } else if (m_currentEffect == "Voice") {
        success = m_lianLi->setVoiceEffect(m_currentSpeed, m_currentBrightness);
    } else if (m_currentEffect == "Groove") {
        int portsToApply[4] = {0, 1, 2, 3};
        int portCount = 4;
        if (m_selectedPort >= 0 && m_selectedPort < 4) {
            portsToApply[0] = m_selectedPort;
            portCount = 1;
        }
        QColor currentColor = QColor(255, 0, 0);
        for (int i = 0; i < portCount; i++) {
            int port = portsToApply[i];
            if (!m_portEnabled[port]) continue;
            QColor portColor = m_portColors[port][0];
            if (!portColor.isValid()) portColor = currentColor;
            int channel = port * 2;
            m_lianLi->setChannelGroove(channel, portColor, m_currentSpeed, m_currentBrightness, m_directionLeft);
            if (channel + 1 < 8) {
                m_lianLi->setChannelGroove(channel + 1, portColor, m_currentSpeed, m_currentBrightness, m_directionLeft);
            }
            success = true;
        }
    } else if (m_currentEffect == "Tunnel") {
        int portsToApply[4] = {0, 1, 2, 3};
        int portCount = 4;
        if (m_selectedPort >= 0 && m_selectedPort < 4) {
            portsToApply[0] = m_selectedPort;
            portCount = 1;
        }
        QColor currentColor = QColor(255, 0, 0);
        for (int i = 0; i < portCount; i++) {
            int port = portsToApply[i];
            if (!m_portEnabled[port]) continue;
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
            success = true;
        }
    } else if (m_currentEffect == "Staggered") {
        int portsToApply[4] = {0, 1, 2, 3};
        int portCount = 4;
        if (m_selectedPort >= 0 && m_selectedPort < 4) {
            portsToApply[0] = m_selectedPort;
            portCount = 1;
        }
        QColor currentColor = QColor(255, 0, 0);
        for (int i = 0; i < portCount; i++) {
            int port = portsToApply[i];
            if (!m_portEnabled[port]) continue;
            int channel = port * 2;
            QColor portColors[2] = {m_portColors[port][0], m_portColors[port][1]};
            if (!portColors[0].isValid()) portColors[0] = currentColor;
            if (!portColors[1].isValid()) portColors[1] = currentColor;
            m_lianLi->setChannelStaggered(channel, portColors, m_currentSpeed, m_currentBrightness);
            if (channel + 1 < 8) {
                m_lianLi->setChannelStaggered(channel + 1, portColors, m_currentSpeed, m_currentBrightness);
            }
            success = true;
        }
    } else if (m_currentEffect == "Tide") {
        int portsToApply[4] = {0, 1, 2, 3};
        int portCount = 4;
        if (m_selectedPort >= 0 && m_selectedPort < 4) {
            portsToApply[0] = m_selectedPort;
            portCount = 1;
        }

        for (int i = 0; i < portCount; i++) {
            int port = portsToApply[i];
            if (!m_portEnabled[port]) continue;

            QColor portColors[2] = {m_portColors[port][0], m_portColors[port][1]};
            int channel = port * 2;
            m_lianLi->setChannelTide(channel, portColors, m_currentSpeed, m_currentBrightness);
            if (channel + 1 < 8) {
                m_lianLi->setChannelTide(channel + 1, portColors, m_currentSpeed, m_currentBrightness);
            }
            success = true;
        }
    } else if (m_currentEffect == "Runway") {
        int portsToApply[4] = {0, 1, 2, 3};
        int portCount = 4;
        if (m_selectedPort >= 0 && m_selectedPort < 4) {
            portsToApply[0] = m_selectedPort;
            portCount = 1;
        }

        QColor currentColor = QColor(255, 200, 100);

        for (int i = 0; i < portCount; i++) {
            int port = portsToApply[i];
            if (!m_portEnabled[port]) continue;

            QColor portColor = m_portColors[port][0];
            if (!portColor.isValid()) portColor = currentColor;

            QColor portColors[2] = {portColor, portColor};

            int channel = port * 2;
            m_lianLi->setChannelRunwayWithColors(channel, portColors, m_currentSpeed, m_currentBrightness, false);
            if (channel + 1 < 8) {
                m_lianLi->setChannelRunwayWithColors(channel + 1, portColors, m_currentSpeed, m_currentBrightness, false);
            }
            success = true;
        }
    } else if (m_currentEffect == "Mixing") {
        int portsToApply[4] = {0, 1, 2, 3};
        int portCount = 4;
        if (m_selectedPort >= 0 && m_selectedPort < 4) {
            portsToApply[0] = m_selectedPort;
            portCount = 1;
        }

        QColor currentColor = QColor(255, 0, 0);

        for (int i = 0; i < portCount; i++) {
            int port = portsToApply[i];
            if (!m_portEnabled[port]) continue;

            QColor portColor = m_portColors[port][0];
            if (!portColor.isValid()) portColor = currentColor;

            QColor portColors[2] = {portColor, portColor};

            int channel = port * 2;
            DEBUG_LOG("Applying Mixing to Port", (port + 1), "channel", channel,
                     "Color(RGB):", portColor.red(), portColor.green(), portColor.blue(),
                     "Speed:", m_currentSpeed, "Brightness:", m_currentBrightness);

            m_lianLi->setChannelMixing(channel, portColors, m_currentSpeed, m_currentBrightness);
            QThread::msleep(10);
            if (channel + 1 < 8) {
                m_lianLi->setChannelMixing(channel + 1, portColors, m_currentSpeed, m_currentBrightness);
            }
            QThread::msleep(50);
            success = true;
        }
    } else if (m_currentEffect == "Stack") {
        int portsToApply[4] = {0, 1, 2, 3};
        int portCount = 4;
        if (m_selectedPort >= 0 && m_selectedPort < 4) {
            portsToApply[0] = m_selectedPort;
            portCount = 1;
        }

        QColor currentColor = QColor(255, 0, 0);

        for (int i = 0; i < portCount; i++) {
            int port = portsToApply[i];
            if (!m_portEnabled[port]) continue;

            QColor portColor = m_portColors[port][0];
            if (!portColor.isValid()) portColor = currentColor;

            int channel = port * 2;
            m_lianLi->setChannelStack(channel, portColor, m_currentSpeed, m_currentBrightness, m_directionLeft);
            if (channel + 1 < 8) {
                m_lianLi->setChannelStack(channel + 1, portColor, m_currentSpeed, m_currentBrightness, m_directionLeft);
            }
            success = true;
        }
    } else if (m_currentEffect == "Neon") {
        int portsToApply[4] = {0, 1, 2, 3};
        int portCount = 4;
        if (m_selectedPort >= 0 && m_selectedPort < 4) {
            portsToApply[0] = m_selectedPort;
            portCount = 1;
        }

        QColor currentColor = QColor(255, 0, 0);

        for (int i = 0; i < portCount; i++) {
            int port = portsToApply[i];
            if (!m_portEnabled[port]) continue;

            QColor portColor = m_portColors[port][0];
            if (!portColor.isValid()) portColor = currentColor;

            int channel = port * 2;
            DEBUG_LOG("Applying Neon to Port", (port + 1), "channel", channel,
                     "Color(RGB):", portColor.red(), portColor.green(), portColor.blue(),
                     "Speed:", m_currentSpeed, "Brightness:", m_currentBrightness);

            m_lianLi->setChannelEffect(channel, 0x22, portColor, m_currentSpeed, m_currentBrightness, false);
            QThread::msleep(10);
            if (channel + 1 < 8) {
                m_lianLi->setChannelEffect(channel + 1, 0x22, portColor, m_currentSpeed, m_currentBrightness, false);
            }
            QThread::msleep(50);
            success = true;
        }
    }

    if (success) {
        DEBUG_LOG("✓ Successfully applied effect:", m_currentEffect);
    } else {
        DEBUG_LOG("✗ Failed to apply effect:", m_currentEffect);
    }
}

void LightingPage::onDeviceConnected()
{
    DEBUG_LOG("Lian Li device connected");
    // FIX: Re-apply saved settings to hardware on connect.
    // The hub loses its state on every power cycle so we must push
    // the saved effect back out whenever the device becomes available.
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

    int portToUpdate = buttonIndex;
    int colorIndex = 0;

    if (m_currentEffect == "Static" || m_currentEffect == "Breathing" ||
        m_currentEffect == "Groove" || m_currentEffect == "Meteor" ||
        m_currentEffect == "Mixing" || m_currentEffect == "Neon" ||
        m_currentEffect == "Runway" || m_currentEffect == "Stack") {
        portToUpdate = buttonIndex;
        colorIndex = 0;
    } else if (m_currentEffect == "Tunnel") {
        colorIndex = buttonIndex;
        portToUpdate = (m_selectedPort >= 0 && m_selectedPort < 4) ? m_selectedPort : 0;
    } else if (m_currentEffect == "Staggered" || m_currentEffect == "Tide") {
        colorIndex = buttonIndex;
        portToUpdate = (m_selectedPort >= 0 && m_selectedPort < 4) ? m_selectedPort : 0;
    }

    QColor currentColor = m_portColors[portToUpdate][colorIndex];

    QString label;
    if (m_currentEffect == "Static" || m_currentEffect == "Breathing" ||
        m_currentEffect == "Groove" || m_currentEffect == "Meteor" ||
        m_currentEffect == "Mixing" || m_currentEffect == "Neon" ||
        m_currentEffect == "Runway" || m_currentEffect == "Stack") {
        label = QString("Select Color for Port %1").arg(portToUpdate + 1);
    } else {
        label = QString("Select Color %1").arg(colorIndex + 1);
    }
    QColor newColor = QColorDialog::getColor(currentColor, this, label);

    if (newColor.isValid()) {
        if (m_currentEffect == "Static" || m_currentEffect == "Breathing" ||
            m_currentEffect == "Groove" || m_currentEffect == "Meteor" ||
            m_currentEffect == "Mixing" || m_currentEffect == "Neon" ||
            m_currentEffect == "Runway" || m_currentEffect == "Stack") {
            m_portColors[portToUpdate][colorIndex] = newColor;
            updateColorButton(buttonIndex);
        } else {
            if (m_selectedPort >= 0 && m_selectedPort < 4) {
                m_portColors[m_selectedPort][colorIndex] = newColor;
                updateColorButton(buttonIndex);
            } else {
                for (int port = 0; port < 4; port++) {
                    m_portColors[port][colorIndex] = newColor;
                }
                updateColorButton(buttonIndex);
            }
        }

        applyCurrentEffect();
        saveLightingSettings();
    }
}

void LightingPage::updateColorButton(int buttonIndex)
{
    if (buttonIndex < 0 || buttonIndex >= 4) return;

    QColor color;

    if (m_currentEffect == "Static" || m_currentEffect == "Breathing" ||
        m_currentEffect == "Groove" || m_currentEffect == "Meteor" ||
        m_currentEffect == "Mixing" || m_currentEffect == "Neon" ||
        m_currentEffect == "Runway" || m_currentEffect == "Stack") {
        color = m_portColors[buttonIndex][0];
    } else {
        int port = (m_selectedPort >= 0 && m_selectedPort < 4) ? m_selectedPort : 0;
        color = m_portColors[port][buttonIndex];
    }

    QString style = QString("QPushButton { background-color: %1; border: 2px solid #555555; border-radius: 4px; }")
                   .arg(color.name());
    m_colorButtons[buttonIndex]->setStyleSheet(style);
}

void LightingPage::saveLightingSettings()
{
    // Do not save while we are in the middle of loading
    if (m_isLoading) return;

    QSettings settings("LConnect3", "Lighting");

    settings.setValue("Effect", m_currentEffect);
    settings.setValue("Speed", m_currentSpeed);
    settings.setValue("Brightness", m_currentBrightness);
    settings.setValue("DirectionLeft", m_directionLeft);

    saveEffectColors(m_currentEffect);

    // Legacy "PortColors" for backward compatibility
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

    settings.setValue("SelectedPort", m_selectedPort);

    DEBUG_LOG("Saved lighting settings: Effect=", m_currentEffect,
             "Speed=", m_currentSpeed,
             "Brightness=", m_currentBrightness);
}

void LightingPage::loadLightingSettings()
{
    // Set guard: prevent any signal handler from saving over the data we are
    // about to load, and prevent onEffectChanged from running mid-load.
    m_isLoading = true;

    QSettings settings("LConnect3", "Lighting");

    m_currentEffect   = settings.value("Effect",       "Rainbow Wave").toString();
    m_currentSpeed    = settings.value("Speed",        75).toInt();
    m_currentBrightness = settings.value("Brightness", 100).toInt();
    m_directionLeft   = settings.value("DirectionLeft", false).toBool();

    // Load colors for the saved effect
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
                int port     = i / 4;
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

    // --- Update UI without triggering signals ---

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

    for (int i = 0; i < 4; ++i) updateColorButton(i);

    // Lift guard before updating effect UI so visibility helpers work normally
    m_isLoading = false;

    // Update control visibility/labels for the loaded effect (no save, no apply)
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

    m_currentEffect     = "Rainbow Wave";
    m_currentSpeed      = 75;
    m_currentBrightness = 100;
    m_directionLeft     = false;
    m_selectedPort      = -1;

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

    for (int i = 0; i < 4; ++i) updateColorButton(i);

    updateEffectUI();
    applyCurrentEffect();
}

void LightingPage::loadFanConfiguration()
{
    QSettings settings("LianLi", "LConnect3");
    m_portEnabled[0] = settings.value("FanConfig/Port1", true).toBool();
    m_portEnabled[1] = settings.value("FanConfig/Port2", true).toBool();
    m_portEnabled[2] = settings.value("FanConfig/Port3", true).toBool();
    m_portEnabled[3] = settings.value("FanConfig/Port4", true).toBool();
}

void LightingPage::updatePortButtonStates()
{
    for (int i = 0; i < 4; ++i) {
        if (m_colorButtons[i]) {
            bool enabled = m_portEnabled[i];
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

void LightingPage::clearOldEffectSettings(const QString &oldEffect, const QString &newEffect)
{
    int oldNumColors = 0;
    int newNumColors = 0;

    if (oldEffect == "Static" || oldEffect == "Breathing" || oldEffect == "Tunnel") {
        oldNumColors = 4;
    } else if (oldEffect == "Staggered" || oldEffect == "Tide") {
        oldNumColors = 2;
    } else if (oldEffect == "Meteor" || oldEffect == "Mixing" || oldEffect == "Neon" ||
               oldEffect == "Runway" || oldEffect == "Stack" || oldEffect == "Groove") {
        oldNumColors = 1;
    }

    if (newEffect == "Static" || newEffect == "Breathing" || newEffect == "Tunnel") {
        newNumColors = 4;
    } else if (newEffect == "Staggered" || newEffect == "Tide") {
        newNumColors = 2;
    } else if (newEffect == "Meteor" || newEffect == "Mixing" || newEffect == "Neon" ||
               newEffect == "Runway" || newEffect == "Stack" || newEffect == "Groove") {
        newNumColors = 1;
    }

    if (newNumColors < oldNumColors || newNumColors < 4) {
        QColor defaultColor(255, 255, 255);
        for (int port = 0; port < 4; port++) {
            for (int colorIdx = newNumColors; colorIdx < 4; colorIdx++) {
                m_portColors[port][colorIdx] = defaultColor;
            }
        }
    }
}

void LightingPage::saveEffectColors(const QString &effectName)
{
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
    QSettings settings("LConnect3", "Lighting");

    QString effectKey = QString("EffectColors/%1").arg(effectName);
    int size = settings.beginReadArray(effectKey);

    if (size > 0) {
        for (int i = 0; i < size && i < 16; ++i) {
            settings.setArrayIndex(i);
            int port     = i / 4;
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
        QColor defaultColor(255, 255, 255);
        for (int port = 0; port < 4; port++) {
            for (int colorIdx = 0; colorIdx < 4; colorIdx++) {
                m_portColors[port][colorIdx] = defaultColor;
            }
        }
        DEBUG_LOG("No saved colors for effect:", effectName, "- using defaults");
    }
    settings.endArray();

    for (int i = 0; i < 4; ++i) {
        updateColorButton(i);
    }
}
