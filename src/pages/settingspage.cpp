#include "settingspage.h"
#include "lightingpage.h"
#include <QSettings>
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <QMessageBox>

SettingsPage::SettingsPage(QWidget *parent)
    : QWidget(parent)
    , m_lightingPage(nullptr)
{
    setupUI();
    setupBehaviorSettings();
    setupFanConfiguration();
    setupDebugSettings();
    loadFanConfiguration();
}

void SettingsPage::setLightingPage(LightingPage *lightingPage)
{
    m_lightingPage = lightingPage;
}

void SettingsPage::setupUI()
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(20, 20, 20, 20);
    m_mainLayout->setSpacing(20);
    
    // Header
    m_headerLayout = new QHBoxLayout();
    m_headerLayout->setContentsMargins(0, 0, 0, 0);
    
    m_titleLabel = new QLabel("Settings");
    m_titleLabel->setObjectName("pageTitle");
    
    m_headerLayout->addWidget(m_titleLabel);
    m_headerLayout->addStretch();
    
    m_mainLayout->addLayout(m_headerLayout);
    
    // Content layout
    m_contentLayout = new QHBoxLayout();
    m_contentLayout->setSpacing(20);
    
    m_leftLayout = new QVBoxLayout();
    m_rightLayout = new QVBoxLayout();
    
    m_contentLayout->addLayout(m_leftLayout, 1);
    m_contentLayout->addLayout(m_rightLayout, 1);
    
    m_mainLayout->addLayout(m_contentLayout);
    
    // Apply styles
    setStyleSheet(R"(
        #pageTitle {
            color: #ffffff;
            font-size: 24px;
            font-weight: bold;
        }
        
        #settingsGroup {
            color: #ffffff;
            font-size: 14px;
            font-weight: bold;
            border: 1px solid #404040;
            border-radius: 8px;
            padding: 20px;
        }
        
        #settingsCheck {
            color: #cccccc;
            font-size: 12px;
        }
        
        #settingsLabel {
            color: #cccccc;
            font-size: 12px;
            min-width: 120px;
        }
        
        #actionButton {
            background-color: #2a82da;
            color: #ffffff;
            border: none;
            padding: 10px 20px;
            border-radius: 4px;
            font-size: 14px;
            font-weight: bold;
            margin: 5px;
        }
        
        #actionButton:hover {
            background-color: #1e6bb8;
        }
    )");
}

void SettingsPage::setupBehaviorSettings()
{
    m_behaviorGroup = new QGroupBox("Startup & Display");
    m_behaviorGroup->setObjectName("settingsGroup");

    QVBoxLayout *behaviorLayout = new QVBoxLayout(m_behaviorGroup);
    behaviorLayout->setSpacing(12);

    QLabel *descLabel = new QLabel("Control how LL-Connect 3 starts:");
    descLabel->setObjectName("settingsLabel");
    descLabel->setWordWrap(true);
    behaviorLayout->addWidget(descLabel);

    QSettings settings("LianLi", "LConnect3");
    bool minimizeOnStartup = settings.value("Startup/MinimizeOnStartup", false).toBool();

    m_minimizeOnStartupCheck = new QCheckBox("Minimize window on startup");
    m_minimizeOnStartupCheck->setObjectName("settingsCheck");
    m_minimizeOnStartupCheck->setChecked(minimizeOnStartup);
    connect(m_minimizeOnStartupCheck, &QCheckBox::toggled, this, [this](bool checked) {
        QSettings settings("LianLi", "LConnect3");
        settings.setValue("Startup/MinimizeOnStartup", checked);
        settings.sync();
        emit minimizeOnStartupChanged(checked);
    });
    behaviorLayout->addWidget(m_minimizeOnStartupCheck);

    m_leftLayout->addWidget(m_behaviorGroup);
}

void SettingsPage::onResetAll()
{
    // Show confirmation dialog with information about what will be reset
    QMessageBox msgBox(this);
    msgBox.setWindowTitle("Reset All Settings");
    msgBox.setText("Reset All Settings");
    msgBox.setInformativeText(
        "This will reset ALL application settings to their default values:\n\n"
        "• Fan Port Configuration (all ports enabled)\n"
        "• Debug Mode (disabled)\n"
        "• Lighting Effects (Rainbow Wave)\n"
        "• Lighting Colors (Red, Blue, Green, Yellow per port)\n"
        "• Lighting Speed (75)\n"
        "• Lighting Brightness (100)\n"
        "• Fan Profiles (all ports reset to Quiet)\n"
        "• Fan Curves (default profiles)\n"
        "• Custom Profiles (cleared)\n\n"
        "This action cannot be undone. Continue?"
    );
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    msgBox.setDefaultButton(QMessageBox::No);
    msgBox.setIcon(QMessageBox::Question);
    
    int ret = msgBox.exec();
    if (ret != QMessageBox::Yes) {
        return; // User cancelled
    }
    
    // Clear all QSettings groups
    {
        // Clear main application settings
        QSettings mainSettings("LianLi", "LConnect3");
        mainSettings.clear();
        mainSettings.sync();
    }
    
    {
        // Clear lighting settings
        QSettings lightingSettings("LConnect3", "Lighting");
        lightingSettings.clear();
        lightingSettings.sync();
    }
    
    {
        // Clear fan profile settings
        QSettings fanProfileSettings("LConnect3", "FanProfile");
        fanProfileSettings.clear();
        fanProfileSettings.sync();
    }
    
    {
        // Clear fan curve settings
        QSettings fanCurveSettings("LConnect3", "FanCurves");
        fanCurveSettings.clear();
        fanCurveSettings.sync();
    }
    
    {
        // Clear custom profiles
        QSettings customProfileSettings("LConnect3", "CustomProfiles");
        customProfileSettings.clear();
        customProfileSettings.sync();
    }
    
    {
        // Clear port profiles
        QSettings portProfileSettings("LConnect3", "PortProfiles");
        portProfileSettings.clear();
        portProfileSettings.sync();
    }
    
    // Reset UI to default values
    m_debugModeCheck->setChecked(false);
    m_debugFanSpeedsCheck->setChecked(false);
    m_debugFanLightsCheck->setChecked(false);
    m_kernelLoggingCheck->setChecked(false);
    
    // Reset fan configuration to all enabled
    m_fanPort1Check->setChecked(true);
    m_fanPort2Check->setChecked(true);
    m_fanPort3Check->setChecked(true);
    m_fanPort4Check->setChecked(true);
    
    // Save the reset values
    saveFanConfiguration();
    
    // Reset lighting page colors and settings if available
    if (m_lightingPage) {
        m_lightingPage->resetToDefaults();
    }
    
    // Show success message
    QMessageBox::information(this, "Reset Complete", 
        "All settings have been reset to their default values.\n\n"
        "You may need to navigate to the Lighting page to see the changes applied.");
    
    qDebug() << "All settings and colors have been reset to defaults";
}

void SettingsPage::setupFanConfiguration()
{
    m_fanConfigGroup = new QGroupBox("Fan Port Configuration");
    m_fanConfigGroup->setObjectName("settingsGroup");
    
    QVBoxLayout *fanLayout = new QVBoxLayout(m_fanConfigGroup);
    fanLayout->setSpacing(15);
    
    // Description label
    QLabel *descLabel = new QLabel("Select which ports have fans connected:");
    descLabel->setObjectName("settingsLabel");
    descLabel->setWordWrap(true);
    fanLayout->addWidget(descLabel);
    
    // Port checkboxes
    m_fanPort1Check = new QCheckBox("Port 1");
    m_fanPort1Check->setObjectName("settingsCheck");
    m_fanPort1Check->setChecked(true);
    connect(m_fanPort1Check, &QCheckBox::toggled, [this](bool checked) {
        onFanPortToggled(1, checked);
    });
    
    m_fanPort2Check = new QCheckBox("Port 2");
    m_fanPort2Check->setObjectName("settingsCheck");
    m_fanPort2Check->setChecked(true);
    connect(m_fanPort2Check, &QCheckBox::toggled, [this](bool checked) {
        onFanPortToggled(2, checked);
    });
    
    m_fanPort3Check = new QCheckBox("Port 3");
    m_fanPort3Check->setObjectName("settingsCheck");
    m_fanPort3Check->setChecked(true);
    connect(m_fanPort3Check, &QCheckBox::toggled, [this](bool checked) {
        onFanPortToggled(3, checked);
    });
    
    m_fanPort4Check = new QCheckBox("Port 4");
    m_fanPort4Check->setObjectName("settingsCheck");
    m_fanPort4Check->setChecked(true);
    connect(m_fanPort4Check, &QCheckBox::toggled, [this](bool checked) {
        onFanPortToggled(4, checked);
    });
    
    fanLayout->addWidget(m_fanPort1Check);
    fanLayout->addWidget(m_fanPort2Check);
    fanLayout->addWidget(m_fanPort3Check);
    fanLayout->addWidget(m_fanPort4Check);
    
    // Info label
    QLabel *infoLabel = new QLabel("Note: The hardware cannot auto-detect fans. Please configure manually.");
    infoLabel->setObjectName("infoLabel");
    infoLabel->setWordWrap(true);
    infoLabel->setStyleSheet("color: #888888; font-size: 11px; font-style: italic;");
    fanLayout->addWidget(infoLabel);
    
    m_rightLayout->addWidget(m_fanConfigGroup);
    
    // Reset All button
    m_resetAllBtn = new QPushButton("Reset All");
    m_resetAllBtn->setObjectName("actionButton");
    connect(m_resetAllBtn, &QPushButton::clicked, this, &SettingsPage::onResetAll);
    
    m_rightLayout->addWidget(m_resetAllBtn);
    m_rightLayout->addStretch();
}

void SettingsPage::onFanPortToggled(int port, bool enabled)
{
    // Write to kernel driver immediately
    QString procPath = QString("/proc/Lian_li_SL_INFINITY/Port_%1/fan_config").arg(port);
    QFile file(procPath);
    
    if (file.open(QIODevice::WriteOnly)) {
        QTextStream stream(&file);
        stream << (enabled ? "1" : "0");
        file.close();
        qDebug() << "Fan port" << port << "set to" << (enabled ? "enabled" : "disabled");
    } else {
        qWarning() << "Failed to configure fan port" << port << ":" << file.errorString();
    }
    
    // Save to settings for persistence
    saveFanConfiguration();
}

void SettingsPage::loadFanConfiguration()
{
    // Load from QSettings
    QSettings settings("LianLi", "LConnect3");
    
    // Load saved configuration, default to all enabled
    bool port1 = settings.value("FanConfig/Port1", true).toBool();
    bool port2 = settings.value("FanConfig/Port2", true).toBool();
    bool port3 = settings.value("FanConfig/Port3", true).toBool();
    bool port4 = settings.value("FanConfig/Port4", true).toBool();
    
    // Block signals while setting initial state to avoid triggering writes
    m_fanPort1Check->blockSignals(true);
    m_fanPort2Check->blockSignals(true);
    m_fanPort3Check->blockSignals(true);
    m_fanPort4Check->blockSignals(true);
    
    m_fanPort1Check->setChecked(port1);
    m_fanPort2Check->setChecked(port2);
    m_fanPort3Check->setChecked(port3);
    m_fanPort4Check->setChecked(port4);
    
    m_fanPort1Check->blockSignals(false);
    m_fanPort2Check->blockSignals(false);
    m_fanPort3Check->blockSignals(false);
    m_fanPort4Check->blockSignals(false);
    
    // Apply to kernel driver
    for (int port = 1; port <= 4; ++port) {
        bool enabled = false;
        switch (port) {
            case 1: enabled = port1; break;
            case 2: enabled = port2; break;
            case 3: enabled = port3; break;
            case 4: enabled = port4; break;
        }
        
        QString procPath = QString("/proc/Lian_li_SL_INFINITY/Port_%1/fan_config").arg(port);
        QFile file(procPath);
        
        if (file.open(QIODevice::WriteOnly)) {
            QTextStream stream(&file);
            stream << (enabled ? "1" : "0");
            file.close();
        }
    }
}

void SettingsPage::saveFanConfiguration()
{
    QSettings settings("LianLi", "LConnect3");
    
    settings.setValue("FanConfig/Port1", m_fanPort1Check->isChecked());
    settings.setValue("FanConfig/Port2", m_fanPort2Check->isChecked());
    settings.setValue("FanConfig/Port3", m_fanPort3Check->isChecked());
    settings.setValue("FanConfig/Port4", m_fanPort4Check->isChecked());
    
    settings.sync();
}

void SettingsPage::writeKernelLoggingFlag(bool enabled)
{
    QString procPath = "/proc/Lian_li_SL_INFINITY/logging_enabled";
    QFile file(procPath);

    if (file.open(QIODevice::WriteOnly)) {
        QTextStream stream(&file);
        stream << (enabled ? "1" : "0");
        file.close();
    } else {
        qWarning() << "Failed to set kernel logging state:" << file.errorString();
    }
}

void SettingsPage::setupDebugSettings()
{
    m_debugGroup = new QGroupBox("Developer Settings");
    m_debugGroup->setObjectName("settingsGroup");
    
    QVBoxLayout *debugLayout = new QVBoxLayout(m_debugGroup);
    debugLayout->setSpacing(15);
    
    // Description label
    QLabel *descLabel = new QLabel("Debug and diagnostic options:");
    descLabel->setObjectName("settingsLabel");
    descLabel->setWordWrap(true);
    debugLayout->addWidget(descLabel);
    
    // Load current settings
    QSettings settings("LianLi", "LConnect3");
    bool debugEnabled = settings.value("Debug/Enabled", false).toBool();
    bool debugFanSpeeds = settings.value("Debug/FanSpeeds", false).toBool();
    bool debugFanLights = settings.value("Debug/FanLights", false).toBool();
    bool kernelLogs = settings.value("Debug/KernelLogs", false).toBool();
    
    // Debug mode checkbox (master control)
    m_debugModeCheck = new QCheckBox("Enable Debug Mode");
    m_debugModeCheck->setObjectName("settingsCheck");
    m_debugModeCheck->setChecked(debugEnabled);
    
    connect(m_debugModeCheck, &QCheckBox::toggled, [](bool checked) {
        QSettings settings("LianLi", "LConnect3");
        settings.setValue("Debug/Enabled", checked);
        settings.sync();
        
        if (checked) {
            qDebug() << "Debug mode enabled - verbose logging active";
        } else {
            qDebug() << "Debug mode disabled - minimal logging";
        }
    });
    
    debugLayout->addWidget(m_debugModeCheck);
    
    // Fan speeds debug checkbox
    m_debugFanSpeedsCheck = new QCheckBox("Debug Fan Speeds");
    m_debugFanSpeedsCheck->setObjectName("settingsCheck");
    m_debugFanSpeedsCheck->setChecked(debugFanSpeeds);
    m_debugFanSpeedsCheck->setEnabled(debugEnabled);
    
    connect(m_debugFanSpeedsCheck, &QCheckBox::toggled, [](bool checked) {
        QSettings settings("LianLi", "LConnect3");
        settings.setValue("Debug/FanSpeeds", checked);
        settings.sync();
    });
    
    connect(m_debugModeCheck, &QCheckBox::toggled, m_debugFanSpeedsCheck, &QCheckBox::setEnabled);
    
    debugLayout->addWidget(m_debugFanSpeedsCheck);
    
    // Fan lights debug checkbox
    m_debugFanLightsCheck = new QCheckBox("Debug Fan Lights (RGB)");
    m_debugFanLightsCheck->setObjectName("settingsCheck");
    m_debugFanLightsCheck->setChecked(debugFanLights);
    m_debugFanLightsCheck->setEnabled(debugEnabled);
    
    connect(m_debugFanLightsCheck, &QCheckBox::toggled, [](bool checked) {
        QSettings settings("LianLi", "LConnect3");
        settings.setValue("Debug/FanLights", checked);
        settings.sync();
    });
    
    connect(m_debugModeCheck, &QCheckBox::toggled, m_debugFanLightsCheck, &QCheckBox::setEnabled);
    
    debugLayout->addWidget(m_debugFanLightsCheck);

    // Kernel driver logging
    m_kernelLoggingCheck = new QCheckBox("Enable Kernel Driver Logging");
    m_kernelLoggingCheck->setObjectName("settingsCheck");
    m_kernelLoggingCheck->setChecked(kernelLogs);
    m_kernelLoggingCheck->setEnabled(debugEnabled);
    
    connect(m_kernelLoggingCheck, &QCheckBox::toggled, [this](bool checked) {
        QSettings settings("LianLi", "LConnect3");
        settings.setValue("Debug/KernelLogs", checked);
        settings.sync();
        writeKernelLoggingFlag(checked);
    });
    
    connect(m_debugModeCheck, &QCheckBox::toggled, this, [this](bool checked) {
        m_kernelLoggingCheck->setEnabled(checked);
        if (checked && m_kernelLoggingCheck->isChecked()) {
            writeKernelLoggingFlag(true);
        }
        if (!checked && m_kernelLoggingCheck->isChecked()) {
            m_kernelLoggingCheck->setChecked(false);
        }
    });
    
    debugLayout->addWidget(m_kernelLoggingCheck);
    
    // Info label
    QLabel *infoLabel = new QLabel("When enabled, detailed diagnostic information will be printed to the console. You can enable specific debug categories below. Kernel driver logging writes to dmesg and is off by default.");
    infoLabel->setObjectName("infoLabel");
    infoLabel->setWordWrap(true);
    infoLabel->setStyleSheet("color: #888888; font-size: 11px; font-style: italic;");
    debugLayout->addWidget(infoLabel);
    
    m_leftLayout->addWidget(m_debugGroup);

    // Push the persisted kernel logging preference down to the driver on startup
    writeKernelLoggingFlag(kernelLogs && debugEnabled);
}
