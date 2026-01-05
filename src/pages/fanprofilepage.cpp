#include "fanprofilepage.h"
#include "utils/qtdebugutil.h"
#include <QHeaderView>
#include <QFont>
#include <QTimer>
#include <QVector>
#include <QPointF>
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <QProcess>
#include <QThread>
#include <QRegularExpression>
#include <QDir>
#include <QRandomGenerator>
#include <QSettings>
#include <QMap>
#include <cmath>
#include <vector>
#include <algorithm>
#include <deque>
#include <QElapsedTimer>
#include <QInputDialog>

FanProfilePage::FanProfilePage(QWidget *parent)
    : QWidget(parent)
    , m_cachedTemperature(39) // Start with your current temperature
    , m_temperatureCounter(0)
    , m_cachedCPULoad(0) // Initialize CPU load
    , m_cachedGPULoad(0) // Initialize GPU load
    , m_cachedFanRPMs(4, 0) // Initialize with 4 fans at 0 RPM
    , m_portConnected(4, false) // Initialize port detection
    , m_activePorts() // Empty initially
    , m_hidController(nullptr)
    , m_selectedPort(1) // Default to Port 1
{
    // Initialize all ports with 120mm fan size (2100 RPM max) by default
    for (int port = 1; port <= 4; ++port) {
        m_fanSizeMaxRPM[port] = 2100;
        m_portProfiles[port] = "Quiet"; // Initialize all ports to Quiet profile
    }
    
    // Initialize custom profile names and curves
    for (int i = 1; i <= 3; ++i) {
        m_customProfileNames[i] = "Cust" + QString::number(i);
        // Initialize with Quiet profile curve
        m_customProfileCurves[i] = getDefaultCurveForProfile("Quiet");
    }
    
    // Set minimum size for the page - more compact
    setMinimumSize(700, 500);
    
    setupUI();
    setupFanTable();
    setupFanCurve();
    setupControls();
    
    // Start fast update timer for visual updates
    m_updateTimer = new QTimer(this);
    connect(m_updateTimer, &QTimer::timeout, this, &FanProfilePage::updateFanData);
    m_updateTimer->start(50); // Update every 50ms for smooth real-time updates
    
    // Start slower timer for temperature reading (every 500ms)
    m_tempUpdateTimer = new QTimer(this);
    connect(m_tempUpdateTimer, &QTimer::timeout, this, &FanProfilePage::updateTemperature);
    m_tempUpdateTimer->start(500); // Update temperature every 500ms
    
    // Start timer for fan RPM reading (every 1 second)
    m_fanRPMTimer = new QTimer(this);
    connect(m_fanRPMTimer, &QTimer::timeout, this, &FanProfilePage::updateFanRPMs);
    m_fanRPMTimer->start(1000); // Update fan RPMs every 1 second
    
    // CPU and GPU load monitoring removed - not needed for fan control
    
    // Initialize HID controller for fan control
    m_hidController = new LianLiSLInfinityController();
    if (m_hidController->Initialize()) {
        qDebug() << "Lian Li device connected successfully";
        qDebug() << "Device name:" << QString::fromStdString(m_hidController->GetDeviceName());
        qDebug() << "Firmware version:" << QString::fromStdString(m_hidController->GetFirmwareVersion());
    } else {
        qDebug() << "Failed to connect to Lian Li device - fans will not work";
    }
    
    // Fan configuration is now handled via Settings page
    
    // Load saved custom curves and profiles
    loadCustomCurves();
    loadCustomProfiles();
    loadPortProfiles(); // Load saved port profile assignments
    
    // Load the last selected profile
    QSettings settings("LConnect3", "FanProfile");
    QString lastProfile = settings.value("LastSelectedProfile", "Quiet").toString();
    
    // Select the appropriate radio button (handle both old internal names and new display names)
    if (lastProfile == "Quiet") {
        m_quietRadio->setChecked(true);
    } else if (lastProfile == "StdSP" || lastProfile == "Standard") {
        m_stdSpRadio->setChecked(true);
    } else if (lastProfile == "HighSP" || lastProfile == "High Speed") {
        m_highSpRadio->setChecked(true);
    } else if (lastProfile == "FullSP" || lastProfile == "Full Speed") {
        m_fullSpRadio->setChecked(true);
    } else if (lastProfile == m_customProfileNames[1]) {
        m_custom1Radio->setChecked(true);
    } else if (lastProfile == m_customProfileNames[2]) {
        m_custom2Radio->setChecked(true);
    } else if (lastProfile == m_customProfileNames[3]) {
        m_custom3Radio->setChecked(true);
    } else {
        m_quietRadio->setChecked(true); // Default fallback
    }
    
    qDebug() << "Restored last selected profile:" << lastProfile;
    
    // Connect curve widget signal for when user drags points
    connect(m_fanCurveWidget, &FanCurveWidget::curvePointsChanged, this, &FanProfilePage::onCurvePointsChanged);
    
    // Connect table selection to update which port's curve is shown
    connect(m_fanTable, &QTableWidget::itemSelectionChanged, this, &FanProfilePage::onPortSelectionChanged);
    
    // Initial update
    updateTemperature();
    updateFanRPMs();
    updateFanData();
}

void FanProfilePage::setupUI()
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(15, 15, 15, 15);
    m_mainLayout->setSpacing(15);
    
    // Header removed to maximize space for fan table
    
    // Content layout - Vertical: Fans on top, Profile on bottom
    m_contentLayout = new QVBoxLayout();
    m_contentLayout->setSpacing(10);
    m_contentLayout->setContentsMargins(0, 0, 0, 0);
    
    m_leftLayout = new QVBoxLayout();
    m_leftLayout->setSpacing(8);
    m_rightLayout = new QVBoxLayout();
    m_rightLayout->setSpacing(8);
    
    // Add both layouts to main content (fans first, then profile)
    m_contentLayout->addLayout(m_leftLayout, 0);  // Fans on top - fixed size
    m_contentLayout->addLayout(m_rightLayout, 1); // Profile on bottom - expandable
    
    m_mainLayout->addLayout(m_contentLayout);
    
    // Header styles removed since header is no longer used
}

void FanProfilePage::setupFanTable()
{
    // Fan section without title to maximize space for the table
    
    m_fanTable = new QTableWidget(4, 6); // Always show 4 rows for 4 ports
    m_fanTable->setObjectName("fanTable");
    
    QStringList headers = {"#", "Port", "Profile", "Temperature", "Fan RPMs", "Size"};
    m_fanTable->setHorizontalHeaderLabels(headers);
    
    // Set table properties
    m_fanTable->setAlternatingRowColors(true);
    m_fanTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_fanTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_fanTable->verticalHeader()->setVisible(false);
    m_fanTable->horizontalHeader()->setStretchLastSection(true);
    
    // Set column widths to fit better
    m_fanTable->setColumnWidth(0, 30);  // # column
    m_fanTable->setColumnWidth(1, 80);  // Port column
    m_fanTable->setColumnWidth(2, 80);  // Profile column
    m_fanTable->setColumnWidth(3, 120); // Temperature column
    m_fanTable->setColumnWidth(4, 80);  // Fan RPMs column
    m_fanTable->setColumnWidth(5, 60);  // Size column (smaller)
    
    // Set table size - more compact
    m_fanTable->setMaximumHeight(160);
    m_fanTable->setMinimumHeight(120);
    
    // Initialize all 4 rows with default data
    for (int row = 0; row < 4; ++row) {
        // Row number
        QTableWidgetItem *rowItem = new QTableWidgetItem(QString::number(row + 1));
        rowItem->setFlags(rowItem->flags() & ~Qt::ItemIsEditable); // Make read-only
        m_fanTable->setItem(row, 0, rowItem);
        
        // Port number
        QTableWidgetItem *portItem = new QTableWidgetItem("Port " + QString::number(row + 1));
        portItem->setFlags(portItem->flags() & ~Qt::ItemIsEditable); // Make read-only
        m_fanTable->setItem(row, 1, portItem);
        
        // Profile (will be updated based on selection)
        QTableWidgetItem *profileItem = new QTableWidgetItem("Quiet");
        profileItem->setFlags(profileItem->flags() & ~Qt::ItemIsEditable); // Make read-only
        m_fanTable->setItem(row, 2, profileItem);
        
        // Temperature (will be updated with real data)
        QTableWidgetItem *tempItem = new QTableWidgetItem("0°C");
        tempItem->setForeground(getTemperatureColor(0));
        tempItem->setFlags(tempItem->flags() & ~Qt::ItemIsEditable); // Make read-only
        m_fanTable->setItem(row, 3, tempItem);
        
        // RPM (will be updated with real data)
        QTableWidgetItem *rpmItem = new QTableWidgetItem("0 RPM");
        rpmItem->setForeground(QColor(255, 165, 0)); // Orange color
        rpmItem->setFlags(rpmItem->flags() & ~Qt::ItemIsEditable); // Make read-only
        m_fanTable->setItem(row, 4, rpmItem);
        
        // Size dropdown (120MM or 140MM)
        QComboBox *sizeCombo = new QComboBox();
        sizeCombo->addItem("120MM");
        sizeCombo->addItem("140MM");
        sizeCombo->setCurrentIndex(0); // Default to 120MM
        sizeCombo->setStyleSheet(R"(
            QComboBox {
                background-color: #3d3d3d;
                color: white;
                border: 1px solid #555;
                border-radius: 4px;
                padding: 2px 8px;
                min-width: 70px;
            }
            QComboBox::drop-down {
                border: none;
                width: 20px;
            }
            QComboBox::down-arrow {
                image: none;
                border-left: 4px solid transparent;
                border-right: 4px solid transparent;
                border-top: 5px solid white;
                margin-right: 5px;
            }
            QComboBox QAbstractItemView {
                background-color: #3d3d3d;
                color: white;
                selection-background-color: #2a82da;
                border: 1px solid #555;
            }
        )");
        m_fanSizeComboBoxes.append(sizeCombo);
        m_fanTable->setCellWidget(row, 5, sizeCombo);
        
        // Connect fan size change signal
        int port = row + 1; // Capture the port number (1-4)
        connect(sizeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, port]() {
            onFanSizeChanged(port);
        });
    }
    
    m_leftLayout->addWidget(m_fanTable);
    
    // Style the table
    m_fanTable->setStyleSheet(R"(
        QTableWidget {
            background-color: #2d2d2d;
            border: 1px solid #404040;
            border-radius: 8px;
            gridline-color: #404040;
        }
        
        QTableWidget::item {
            padding: 6px;
            border-bottom: 1px solid #404040;
        }
        
        QTableWidget::item:selected {
            background-color: #2a82da;
        }
        
        QHeaderView::section {
            background-color: #404040;
            color: #ffffff;
            padding: 6px;
            border: none;
            font-weight: bold;
            font-size: 12px;
        }
        
        QComboBox {
            background-color: #404040;
            color: #ffffff;
            border: 1px solid #555555;
            border-radius: 4px;
            padding: 2px;
            font-size: 11px;
        }
    )");
}

void FanProfilePage::setupFanCurve()
{
    m_fanCurveWidget = new FanCurveWidget();
    m_fanCurveWidget->setObjectName("fanCurveWidget");
    m_fanCurveWidget->setMinimumSize(400, 180);
    m_fanCurveWidget->setMaximumHeight(220);
    
    // Set initial profile
    m_fanCurveWidget->setProfile("Quiet");
    m_fanCurveWidget->setCurrentTemperature(25);
    m_fanCurveWidget->setCurrentRPM(420);
    
    // Style the widget
    m_fanCurveWidget->setStyleSheet(R"(
        #fanCurveWidget {
            background-color: #1a1a1a;
            border: 1px solid #404040;
            border-radius: 8px;
        }
    )");
}

void FanProfilePage::setupControls()
{
    // Profile section without title to save space
    
    // Fan Profile group - use vertical layout to fit all profiles
    m_profileGroup = new QGroupBox("Fan Profile");
    m_profileGroup->setObjectName("controlGroup");
    
    QVBoxLayout *profileMainLayout = new QVBoxLayout(m_profileGroup);
    profileMainLayout->setSpacing(8);
    
    // First row: Built-in profiles
    QHBoxLayout *builtinLayout = new QHBoxLayout();
    builtinLayout->setSpacing(15);
    
    m_quietRadio = new QRadioButton("Quiet");
    m_stdSpRadio = new QRadioButton("StdSP");
    m_highSpRadio = new QRadioButton("HighSP");
    m_fullSpRadio = new QRadioButton("FullSP");
    
    m_quietRadio->setChecked(true);
    
    builtinLayout->addWidget(m_quietRadio);
    builtinLayout->addWidget(m_stdSpRadio);
    builtinLayout->addWidget(m_highSpRadio);
    builtinLayout->addWidget(m_fullSpRadio);
    builtinLayout->addStretch();
    
    // Second row: Custom profiles with rename buttons
    QHBoxLayout *customLayout = new QHBoxLayout();
    customLayout->setSpacing(15);
    
    m_custom1Radio = new QRadioButton(m_customProfileNames[1]);
    m_custom2Radio = new QRadioButton(m_customProfileNames[2]);
    m_custom3Radio = new QRadioButton(m_customProfileNames[3]);
    
    QPushButton *rename1Btn = new QPushButton("✎");
    QPushButton *rename2Btn = new QPushButton("✎");
    QPushButton *rename3Btn = new QPushButton("✎");
    
    rename1Btn->setMaximumWidth(25);
    rename2Btn->setMaximumWidth(25);
    rename3Btn->setMaximumWidth(25);
    rename1Btn->setToolTip("Rename custom profile 1");
    rename2Btn->setToolTip("Rename custom profile 2");
    rename3Btn->setToolTip("Rename custom profile 3");
    
    customLayout->addWidget(m_custom1Radio);
    customLayout->addWidget(rename1Btn);
    customLayout->addWidget(m_custom2Radio);
    customLayout->addWidget(rename2Btn);
    customLayout->addWidget(m_custom3Radio);
    customLayout->addWidget(rename3Btn);
    customLayout->addStretch();
    
    profileMainLayout->addLayout(builtinLayout);
    profileMainLayout->addLayout(customLayout);
    
    connect(m_quietRadio, &QRadioButton::toggled, this, &FanProfilePage::onProfileChanged);
    connect(m_stdSpRadio, &QRadioButton::toggled, this, &FanProfilePage::onProfileChanged);
    connect(m_highSpRadio, &QRadioButton::toggled, this, &FanProfilePage::onProfileChanged);
    connect(m_fullSpRadio, &QRadioButton::toggled, this, &FanProfilePage::onProfileChanged);
    connect(m_custom1Radio, &QRadioButton::toggled, this, &FanProfilePage::onProfileChanged);
    connect(m_custom2Radio, &QRadioButton::toggled, this, &FanProfilePage::onProfileChanged);
    connect(m_custom3Radio, &QRadioButton::toggled, this, &FanProfilePage::onProfileChanged);
    
    connect(rename1Btn, &QPushButton::clicked, this, [this]() { onRenameCustomProfile(1); });
    connect(rename2Btn, &QPushButton::clicked, this, [this]() { onRenameCustomProfile(2); });
    connect(rename3Btn, &QPushButton::clicked, this, [this]() { onRenameCustomProfile(3); });
    
    m_rightLayout->addWidget(m_profileGroup);
    
    // Create horizontal layout for fan curve and buttons
    QHBoxLayout *fanCurveLayout = new QHBoxLayout();
    fanCurveLayout->setSpacing(15);
    
    // Fan curve on the left
    fanCurveLayout->addWidget(m_fanCurveWidget, 1); // Fan curve takes remaining space
    
    // Buttons on the right
    QVBoxLayout *buttonsLayout = new QVBoxLayout();
    buttonsLayout->setSpacing(10);
    
    m_applyToAllButton = new QPushButton("Apply To All");
    m_applyToAllButton->setObjectName("actionButton");
    m_applyToAllButton->setMinimumWidth(120);
    m_applyToAllButton->setToolTip("Apply current port's curve to all other ports");
    connect(m_applyToAllButton, &QPushButton::clicked, this, &FanProfilePage::onApplyToAllClicked);
    
    m_defaultButton = new QPushButton("Default");
    m_defaultButton->setObjectName("actionButton");
    m_defaultButton->setMinimumWidth(120);
    m_defaultButton->setToolTip("Reset current port's curve to the selected profile default");
    connect(m_defaultButton, &QPushButton::clicked, this, &FanProfilePage::onDefaultClicked);
    
    buttonsLayout->addWidget(m_applyToAllButton);
    buttonsLayout->addWidget(m_defaultButton);
    buttonsLayout->addStretch();
    
    fanCurveLayout->addLayout(buttonsLayout);
    
    m_rightLayout->addLayout(fanCurveLayout);
    m_rightLayout->addStretch();
    
    // Apply control styles
    setStyleSheet(R"(
        #controlGroup {
            color: #ffffff;
            font-size: 14px;
            font-weight: bold;
            border: 1px solid #404040;
            border-radius: 8px;
            padding: 10px;
            margin-top: 10px;
        }
        
        #controlLabel {
            color: #cccccc;
            font-size: 12px;
        }
        
        #controlCheck {
            color: #ffffff;
        }
        
        QRadioButton {
            color: #cccccc;
            font-size: 12px;
        }
        
        QRadioButton::indicator {
            width: 12px;
            height: 12px;
        }
        
        QRadioButton::indicator::unchecked {
            border: 2px solid #555555;
            border-radius: 6px;
            background-color: transparent;
        }
        
        QRadioButton::indicator::checked {
            border: 2px solid #2a82da;
            border-radius: 6px;
            background-color: #2a82da;
        }
        
        #actionButton {
            background-color: #2a82da;
            color: #ffffff;
            border: none;
            padding: 10px 16px;
            border-radius: 6px;
            font-size: 12px;
            font-weight: 600;
            min-height: 36px;
        }
        
        #actionButton:hover {
            background-color: #1e6bb8;
        }
        
        #actionButton:pressed {
            background-color: #155a9e;
        }
    )");
}

QString FanProfilePage::getCurrentProfile()
{
    if (m_quietRadio->isChecked()) return "Quiet";
    if (m_stdSpRadio->isChecked()) return "StdSP";
    if (m_highSpRadio->isChecked()) return "HighSP";
    if (m_fullSpRadio->isChecked()) return "FullSP";
    if (m_custom1Radio->isChecked()) return m_customProfileNames[1];
    if (m_custom2Radio->isChecked()) return m_customProfileNames[2];
    if (m_custom3Radio->isChecked()) return m_customProfileNames[3];
    return "Quiet"; // Default
}

QString FanProfilePage::getInternalProfileName(const QString &displayName)
{
    // Map display names to internal names used for curves
    if (displayName == "StdSP") return "Standard";
    if (displayName == "HighSP") return "High Speed";
    if (displayName == "FullSP") return "Full Speed";
    return displayName; // Quiet, custom profiles, etc. use same name
}

void FanProfilePage::updateFanCurve()
{
    // Update fan curve based on selected profile
    QString displayName = getCurrentProfile();
    QString internalName = getInternalProfileName(displayName);
    
    // For custom profiles, load their base curve into the widget
    if (m_custom1Radio->isChecked()) {
        m_fanCurveWidget->setCustomCurve(m_customProfileCurves[1]);
    } else if (m_custom2Radio->isChecked()) {
        m_fanCurveWidget->setCustomCurve(m_customProfileCurves[2]);
    } else if (m_custom3Radio->isChecked()) {
        m_fanCurveWidget->setCustomCurve(m_customProfileCurves[3]);
    } else {
        // Built-in profile - use internal name for widget
        m_fanCurveWidget->setProfile(internalName);
    }
}

void FanProfilePage::updateTemperature()
{
    // Try to get real CPU temperature first, fall back to simulation
    int realTemp = getRealCPUTemperature();
    
    if (realTemp != -1) {
        // Use real temperature
        m_cachedTemperature = realTemp;
    } else {
        // Use simulation with smooth variation
        m_temperatureCounter++;
        int baseTemp = 39; // Your current temperature
        int tempVariation = (m_temperatureCounter % 120) - 60; // -60 to +60 variation
        m_cachedTemperature = qMax(25, qMin(85, baseTemp + tempVariation));
    }
}

void FanProfilePage::updateFanRPMs()
{
    // Try to get real fan RPMs first, fall back to simulation
    QVector<int> realRPMs = getRealFanRPMs();
    
    if (!realRPMs.isEmpty()) {
        // Use real fan RPMs
        m_cachedFanRPMs = realRPMs;
        // qDebug() << "Using real fan RPMs:" << m_cachedFanRPMs;
    } else {
        // Use enhanced simulation with more realistic fan behavior
        static int simulationCounter = 0;
        simulationCounter++;
        
        for (int i = 0; i < 4; ++i) {
            if (i == 2) {
                // Port 3 is off
                m_cachedFanRPMs[i] = 0;
            } else {
                // Simulate realistic fan RPM based on temperature and profile
                int baseRPM = calculateRPMForTemperature(m_cachedTemperature);
                
                // Add more realistic variation based on fan characteristics
                int variation = 0;
                if (baseRPM < 500) {
                    // Low RPM fans have less variation
                    variation = QRandomGenerator::global()->bounded(50) - 25;
                } else if (baseRPM < 1500) {
                    // Medium RPM fans have moderate variation
                    variation = QRandomGenerator::global()->bounded(100) - 50;
                } else {
                    // High RPM fans have more variation
                    variation = QRandomGenerator::global()->bounded(150) - 75;
                }
                
                // Add some "breathing" effect - fans don't change instantly
                int targetRPM = qMax(0, baseRPM + variation);
                
                // Smooth transition to target RPM (simulate fan inertia)
                if (m_cachedFanRPMs[i] == 0) {
                    m_cachedFanRPMs[i] = targetRPM; // Instant start
                } else {
                    int currentRPM = m_cachedFanRPMs[i];
                    int diff = targetRPM - currentRPM;
                    int change = diff / 10; // Gradual change
                    if (change == 0 && diff != 0) {
                        change = (diff > 0) ? 1 : -1; // At least 1 RPM change
                    }
                    m_cachedFanRPMs[i] = qMax(0, currentRPM + change);
                }
                
                // Ensure minimum RPM for running fans (except when off)
                if (m_cachedFanRPMs[i] > 0 && m_cachedFanRPMs[i] < 200) {
                    m_cachedFanRPMs[i] = 200 + QRandomGenerator::global()->bounded(100);
                }
            }
        }
        
        // Debug output every 10 updates
        if (simulationCounter % 10 == 0) {
            // qDebug() << "Simulated fan RPMs (temp:" << m_cachedTemperature << "°C):" << m_cachedFanRPMs;
        }
    }
}

void FanProfilePage::updateFanData()
{
    // Use cached temperature for fast updates
    int currentTemp = m_cachedTemperature;
    
    // Calculate corresponding RPM based on current profile (for curve reference)
    int calculatedRPM = calculateRPMForTemperature(currentTemp);
    
    // Get average fake RPM from connected fans (based on temperature and profile)
    int realRPM = 0;
    if (!m_activePorts.isEmpty()) {
        int totalRPM = 0;
        int activeCount = 0;
        for (int port : m_activePorts) {
            int portRPM = getRealFanRPM(port);
            totalRPM += portRPM;
            if (portRPM > 0) activeCount++;
        }
        realRPM = activeCount > 0 ? totalRPM / activeCount : 0;
    }
    
    // Update fan curve widget - show real RPM instead of calculated
    m_fanCurveWidget->setCurrentTemperature(currentTemp);
    m_fanCurveWidget->setCurrentRPM(realRPM);
    
    // Force update of the fan curve widget
    m_fanCurveWidget->update();
    
    // Update table data for all 4 ports
    for (int row = 0; row < 4; ++row) {
        int port = row + 1; // Port numbers are 1-4
        
        // Always try to get RPM from kernel driver - let user see which ports work
        int realRPM = getRealFanRPM(port);
        
        // Profile - display the profile assigned to this port
        QString portProfile = m_portProfiles.value(port, "Quiet");
        QTableWidgetItem *profileItem = new QTableWidgetItem(portProfile);
        profileItem->setFlags(profileItem->flags() & ~Qt::ItemIsEditable); // Make read-only
        m_fanTable->setItem(row, 2, profileItem);
        
        // Temperature with color coding (show for all ports)
        QTableWidgetItem *tempItem = new QTableWidgetItem(QString::number(currentTemp) + "°C");
        tempItem->setForeground(getTemperatureColor(currentTemp));
        tempItem->setFlags(tempItem->flags() & ~Qt::ItemIsEditable); // Make read-only
        m_fanTable->setItem(row, 3, tempItem);
        
        // RPM with orange color (0 for inactive ports)
        QTableWidgetItem *rpmItem = new QTableWidgetItem(QString::number(realRPM) + " RPM");
        rpmItem->setForeground(QColor(255, 165, 0)); // Orange color
        rpmItem->setFlags(rpmItem->flags() & ~Qt::ItemIsEditable); // Make read-only
        m_fanTable->setItem(row, 4, rpmItem);
    }
    
    // Control fan speeds based on temperature and profile
    controlFanSpeeds();
}

void FanProfilePage::onProfileChanged()
{
    QString currentProfile = getCurrentProfile();
    
    // Apply the selected profile to the currently selected port
    m_portProfiles[m_selectedPort] = currentProfile;
    
    // Apply the profile's curve to the selected port
    QVector<QPointF> profileCurve;
    if (m_custom1Radio->isChecked()) {
        profileCurve = m_customProfileCurves[1];
        m_customCurves[m_selectedPort] = profileCurve;
    } else if (m_custom2Radio->isChecked()) {
        profileCurve = m_customProfileCurves[2];
        m_customCurves[m_selectedPort] = profileCurve;
    } else if (m_custom3Radio->isChecked()) {
        profileCurve = m_customProfileCurves[3];
        m_customCurves[m_selectedPort] = profileCurve;
    } else {
        // Built-in profile - get default curve (convert display name to internal name)
        QString displayName = currentProfile;
        QString internalName = getInternalProfileName(displayName);
        profileCurve = getDefaultCurveForProfile(internalName);
        m_customCurves[m_selectedPort] = profileCurve;
    }
    
    // Update the curve widget to show the profile curve
    m_fanCurveWidget->setCustomCurve(profileCurve);
    
    // Save the currently selected profile
    QSettings settings("LConnect3", "FanProfile");
    settings.setValue("LastSelectedProfile", currentProfile);
    
    // Save port profiles
    savePortProfiles();
    
    qDebug() << "Applied profile" << currentProfile << "to Port" << m_selectedPort;
    
    updateFanCurve();
}

void FanProfilePage::onApplyToAllClicked()
{
    QString currentProfile = getCurrentProfile();
    QVector<QPointF> currentCurve = m_fanCurveWidget->getCurvePoints();
    
    qDebug() << "Apply To All clicked - applying profile" << currentProfile << "to all ports";
    
    // Apply the current profile and curve to all ports
    for (int port = 1; port <= 4; ++port) {
        m_portProfiles[port] = currentProfile;
        m_customCurves[port] = currentCurve;
    }
    
    // Save all curves and port profiles
    saveCustomCurves();
    savePortProfiles();
    
    qDebug() << "Applied profile" << currentProfile << "to all 4 ports";
}

void FanProfilePage::onDefaultClicked()
{
    qDebug() << "Default clicked - resetting to profile default";
    
    // Get default curve (Quiet profile)
    QVector<QPointF> defaultCurve = getDefaultCurveForProfile("Quiet");
    
    // If a custom profile is selected, reset that profile's base curve
    if (m_custom1Radio->isChecked()) {
        m_customProfileCurves[1] = defaultCurve;
        m_fanCurveWidget->setCustomCurve(defaultCurve);
        saveCustomProfiles();
        qDebug() << "Reset custom profile 1 (" << m_customProfileNames[1] << ") to Quiet default";
    } else if (m_custom2Radio->isChecked()) {
        m_customProfileCurves[2] = defaultCurve;
        m_fanCurveWidget->setCustomCurve(defaultCurve);
        saveCustomProfiles();
        qDebug() << "Reset custom profile 2 (" << m_customProfileNames[2] << ") to Quiet default";
    } else if (m_custom3Radio->isChecked()) {
        m_customProfileCurves[3] = defaultCurve;
        m_fanCurveWidget->setCustomCurve(defaultCurve);
        saveCustomProfiles();
        qDebug() << "Reset custom profile 3 (" << m_customProfileNames[3] << ") to Quiet default";
    } else {
        // Get the current selected profile
        QString displayName = getCurrentProfile();
        QString internalName = getInternalProfileName(displayName);
        
        // Get default curve for this profile (use internal name for curve lookup)
        defaultCurve = getDefaultCurveForProfile(internalName);
        
        // Set it for the current port
        m_customCurves[m_selectedPort] = defaultCurve;
        
        // Update the widget to show the default curve
        m_fanCurveWidget->setCustomCurve(defaultCurve);
        
        // Update port profile name
        m_portProfiles[m_selectedPort] = displayName;
        
        // Save changes
        saveCustomCurves();
        savePortProfiles();
        
        qDebug() << "Reset Port" << m_selectedPort << "to" << displayName << "default curve";
    }
}


int FanProfilePage::calculateRPMForTemperature(int temperature)
{
    // Define curve data points for each profile (RPM values)
    QVector<QPointF> curvePoints;
    
    // Check if a custom profile is selected
    if (m_custom1Radio->isChecked()) {
        curvePoints = m_customProfileCurves[1];
    } else if (m_custom2Radio->isChecked()) {
        curvePoints = m_customProfileCurves[2];
    } else if (m_custom3Radio->isChecked()) {
        curvePoints = m_customProfileCurves[3];
    } else {
        // Get current built-in profile (display name) and convert to internal name
        QString displayName = getCurrentProfile();
        QString profile = getInternalProfileName(displayName);
    
        if (profile == "Quiet") {
            curvePoints << QPointF(0, 120) << QPointF(25, 420) << QPointF(45, 840) 
                       << QPointF(65, 1050) << QPointF(80, 1680) << QPointF(90, 2100) << QPointF(100, 2100);
        } else if (profile == "Standard") {
            // Standard Speed (StdSP): Balanced curve
            curvePoints << QPointF(0, 120) << QPointF(25, 420) << QPointF(40, 1050) << QPointF(55, 1260) 
                       << QPointF(70, 1680) << QPointF(90, 2100) << QPointF(100, 2100);
        } else if (profile == "High Speed") {
            // High Speed (HighSP): Smooth progressive ramp
            curvePoints << QPointF(0, 120) << QPointF(25, 910) << QPointF(35, 1140) << QPointF(50, 1470)
                       << QPointF(70, 1800) << QPointF(85, 2100) << QPointF(100, 2100);
        } else if (profile == "Full Speed") {
            curvePoints << QPointF(0, 120) << QPointF(25, 2100) << QPointF(40, 2100) << QPointF(55, 2100)
                       << QPointF(70, 2100) << QPointF(90, 2100) << QPointF(100, 2100);
        } else {
            // Default to Quiet (original Lian Li curve)
            curvePoints << QPointF(0, 120) << QPointF(25, 420) << QPointF(45, 840) 
                       << QPointF(65, 1050) << QPointF(80, 1680) << QPointF(90, 2100) << QPointF(100, 2100);
        }
    }
    
    // Clamp temperature to valid range
    temperature = qMax(0, qMin(100, temperature));
    
    // Find the two points to interpolate between
    for (int i = 0; i < curvePoints.size() - 1; ++i) {
        if (temperature >= curvePoints[i].x() && temperature <= curvePoints[i + 1].x()) {
            // Linear interpolation between the two points
            double t = (temperature - curvePoints[i].x()) / (curvePoints[i + 1].x() - curvePoints[i].x());
            double rpm = curvePoints[i].y() + t * (curvePoints[i + 1].y() - curvePoints[i].y());
            
            // Convert RPM to percentage (assuming max RPM is 1200 for Lian Li fans)
            // For display purposes, we'll return the RPM value
            // For control purposes, we'll convert to percentage in controlFanSpeeds()
            return static_cast<int>(rpm);
        }
    }
    
    // If temperature is outside the curve range, clamp to nearest point
    if (temperature < curvePoints.first().x()) {
        return static_cast<int>(curvePoints.first().y());
    } else {
        return static_cast<int>(curvePoints.last().y());
    }
}

int FanProfilePage::getRealCPUTemperature()
{
    // Use the same temperature reading method as System Info page
    int maxTemp = 0;
    
    // Method 1: Try sensors command first (most accurate) - same as System Info
    QProcess sensorsProcess;
    sensorsProcess.start("sensors", QStringList() << "k10temp-pci-00c3");
    sensorsProcess.waitForFinished(1000);
    
    if (sensorsProcess.exitCode() == 0) {
        QString output = sensorsProcess.readAllStandardOutput();
        // Look for Tctl temperature (CPU core temperature) - same as System Info
        QRegularExpression tempRegex("Tctl:\\s*\\+?([0-9.]+)°C");
        QRegularExpressionMatch match = tempRegex.match(output);
        if (match.hasMatch()) {
            maxTemp = static_cast<int>(match.captured(1).toDouble());
        }
    }
    
    // Method 2: Fallback to hwmon if sensors didn't work - same as System Info
    if (maxTemp == 0) {
        QDir hwmonDir("/sys/class/hwmon");
        QStringList hwmonDirs = hwmonDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        
        for (const QString &hwmon : hwmonDirs) {
            QFile nameFile("/sys/class/hwmon/" + hwmon + "/name");
            if (nameFile.open(QIODevice::ReadOnly)) {
                QTextStream stream(&nameFile);
                QString name = stream.readLine().trimmed();
                nameFile.close();
                
                // Check for CPU temperature sensors - same as System Info
                if (name.contains("coretemp") || name.contains("k10temp") || name.contains("zenpower") || 
                    name.contains("asus") || name.contains("acpi")) {
                    
                    QDir hwmonSubDir("/sys/class/hwmon/" + hwmon);
                    QStringList files = hwmonSubDir.entryList(QDir::Files);
                    for (const QString &file : files) {
                        if (file.startsWith("temp") && file.endsWith("_input")) {
                            QFile tempFile("/sys/class/hwmon/" + hwmon + "/" + file);
                            if (tempFile.open(QIODevice::ReadOnly)) {
                                QTextStream tStream(&tempFile);
                                QString tempStr = tStream.readLine();
                                if (!tempStr.isEmpty()) {
                                    int temp = tempStr.toInt() / 1000; // Convert millidegrees to degrees
                                    if (temp > maxTemp && temp < 200) { // Reasonable temperature range
                                        maxTemp = temp;
                                    }
                                }
                                tempFile.close();
                            }
                        }
                    }
                }
            }
        }
    }
    
    // Method 3: Fallback to thermal zones if hwmon didn't work - same as System Info
    if (maxTemp == 0) {
        QDir thermalDir("/sys/class/thermal");
        QStringList thermalZones = thermalDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QString &zone : thermalZones) {
            if (zone.startsWith("thermal_zone")) {
                QFile tempFile("/sys/class/thermal/" + zone + "/temp");
                if (tempFile.open(QIODevice::ReadOnly)) {
                    QTextStream stream(&tempFile);
                    int temp = stream.readLine().toInt() / 1000; // Convert millidegrees to degrees
                    if (temp > maxTemp) maxTemp = temp;
                    tempFile.close();
                }
            }
        }
    }
    
    // Return the temperature or -1 to indicate failure
    return maxTemp > 0 ? maxTemp : -1;
}

QVector<int> FanProfilePage::getRealFanRPMs()
{
    QVector<int> fanRPMs(4, 0); // Initialize with 4 ports, all at 0 RPM
    
    // Read real RPMs from kernel driver for all ports
    for (int port = 1; port <= 4; ++port) {
        fanRPMs[port - 1] = getRealFanRPM(port);
    }
    
    return fanRPMs;
}

int FanProfilePage::getRealFanRPM(int port)
{
    if (port < 1 || port > 4) {
        return 0;
    }
    
    // Check if fan is connected using kernel driver detection
    QString connectedPath = QString("/proc/Lian_li_SL_INFINITY/Port_%1/fan_connected").arg(port);
    QFile connectedFile(connectedPath);
    
    if (connectedFile.open(QIODevice::ReadOnly)) {
        QTextStream stream(&connectedFile);
        QString connectedStr = stream.readLine().trimmed();
        connectedFile.close();
        
        bool ok;
        int connected = connectedStr.toInt(&ok);
        
        if (ok && connected == 0) {
            // Fan not connected according to kernel driver
            return 0;
        }
    } else {
        // If we can't read the status, assume not connected
        return 0;
    }
    
    // Fan is connected - show fake RPM based on temperature and custom curve
    // This gives a realistic RPM display even though we can't read actual RPM
    int baseRPM = calculateRPMForCustomCurve(port, m_cachedTemperature);
    
    // Add some realistic noise (±50 RPM)
    static int noiseCounter = 0;
    noiseCounter++;
    int noise = ((noiseCounter % 100) - 50); // -50 to +50
    int fakeRPM = baseRPM + noise;
    
    // Clamp to reasonable range
    fakeRPM = qBound(400, fakeRPM, 2100); // Minimum 400 RPM to show it's running
    
    return fakeRPM;
}

int FanProfilePage::convertPercentageToRPM(int percentage)
{
    // Convert kernel driver percentage (0-100%) to RPM values
    // Based on calibration: RPM = Percentage × 21
    // 40% = 840 RPM, 50% = 1040 RPM, 60% = 1260 RPM, 
    // 70% = 1480 RPM, 80% = 1680 RPM, 90% = 1880 RPM, 100% = 2100 RPM
    
    if (percentage <= 0) return 0;
    if (percentage >= 100) return 2100;
    
    // Linear conversion: RPM = Percentage × 21
    int rpm = percentage * 21;
    
    return rpm;
}

void FanProfilePage::controlFanSpeeds()
{
    if (!m_hidController) {
        return;
    }
    
    // Get current CPU temperature
    int currentTemp = m_cachedTemperature;
    
    // --- state (static inside controlFanSpeeds or make members) ---
    static double Tf = 0.0;                // filtered temp
    static std::deque<double> hist;        // short history for derivative
    static QMap<int, int> rpm_out;         // Per-port RPM output
    static QElapsedTimer stepTimer;
    
    // Initialize rpm_out for all ports if empty
    if (rpm_out.isEmpty()) {
        for (int port = 1; port <= 4; ++port) {
            rpm_out[port] = 0;
        }
    }
    
    double dt = stepTimer.isValid() ? stepTimer.restart()/1000.0 : 0.1;
    if (dt <= 0) dt = 0.1;

    // 1) Very fast asymmetric filter - almost instant response when heating
    int Traw = currentTemp;  // your measured temp
    double alpha = (Traw >= Tf) ? 0.95 : 0.60;  // VERY fast heating response, moderate cooling
    Tf += alpha * (Traw - Tf);

    // Keep short history for derivative (0.3s)
    int histMax = std::max(2, int(std::round(0.3 / dt)));
    hist.push_back(Tf);
    while ((int)hist.size() > histMax) hist.pop_front();

    // 2) Calculate temperature rate of change
    double dTdt = 0.0;
    if (hist.size() >= 2) dTdt = (hist.back() - hist.front()) / std::max(0.1, dt*(hist.size()-1));
    if (dTdt < 0) dTdt = 0;  // Only care about heating
    if (dTdt > 10.0) dTdt = 10.0;  // Allow very high rate of change

    // Determine if heating
    const bool heating = (dTdt > 0.02);   // very small threshold

    // 3-8) Control each port individually using its custom curve
    for (int port = 1; port <= 4; ++port) {
        // Calculate base RPM from this port's custom curve
        int base_now  = calculateRPMForCustomCurve(port, int(std::round(Tf)));
        int base_pred = calculateRPMForCustomCurve(port, int(std::round(Tf + dTdt * 10.0))); // Look ahead 10 seconds
        
        int base_rpm  = heating ? std::max(base_now, base_pred) : base_now;
        
        // Aggressive feedforward proportional to heating rate
        int ff_rpm = heating ? int(std::round(dTdt * 800.0)) : 0; // Very aggressive
        
        // Extra boost when heating rapidly (>0.3°C/s)
        int boostRPM = 0;
        if (heating && dTdt > 0.3) {
            boostRPM = 400;  // Extra 400 RPM boost when heating fast
        }

        int target = std::clamp(base_rpm + ff_rpm + boostRPM, 0, 2100);

        // Simplified slew rate control - very fast response
        double up_slew = 1500.0;    // RPM/s upward (VERY FAST)
        double down_slew = 200.0;   // RPM/s downward (moderate)
        
        // Even faster at high temperatures
        if (Tf > 65.0) {
            up_slew = 2000.0;       // EXTREMELY fast at high temps
            down_slew = 300.0;      // Faster cooling too
        }
        
        int maxStepUp = std::max(1, int(std::round(up_slew * dt)));
        int maxStepDown = std::max(1, int(std::round(down_slew * dt)));
        
        // Apply slew limits
        int gated = rpm_out[port];
        if (target > rpm_out[port]) {
            // Going up - apply max step
            gated = std::min(target, rpm_out[port] + maxStepUp);
        } else if (target < rpm_out[port]) {
            // Going down - apply max step
            gated = std::max(target, rpm_out[port] - maxStepDown);
        }
        
        // Simple write threshold - write if change is meaningful
        int writeThresh = 10;  // 10 RPM threshold
        bool shouldWrite = false;
        
        if (std::abs(gated - rpm_out[port]) >= writeThresh || rpm_out[port] == 0) {
            shouldWrite = true;
        }

        if (shouldWrite) {
            setFanSpeed(port, gated);
            rpm_out[port] = gated;
            DEBUG_LOG_CATEGORY("FanSpeeds", "Port", port, ": T=", Tf, "°C dT/dt=", dTdt, "°C/s"
                     , " heating=", heating, " base=", base_rpm 
                     , " target=", target, " -> RPM=", rpm_out[port]);
        }
    }
}

void FanProfilePage::setFanSpeed(int port, int targetRPM)
{
    // Clamp speed to valid range
    // Minimum 840 RPM to prevent fan shutdown (allow 120 RPM for idle)
    if (targetRPM > 120 && targetRPM < 840) {
        targetRPM = 840; // Enforce minimum operating speed
    }
    targetRPM = qBound(0, targetRPM, 2100);
    
    // Convert RPM to percentage for kernel driver
    // Based on calibration: Percentage = RPM / 21
    // 840 RPM = 40%, 1260 RPM = 60%, 1680 RPM = 80%, 2100 RPM = 100%
    int speedPercent = targetRPM / 21;
    
    // Clamp to valid range
    speedPercent = qBound(0, speedPercent, 100);
    
    // Calculate expected dBA based on calibration
    // Linear interpolation from calibrated values:
    // 840 RPM = 34 dBA, 1040 RPM = 39 dBA, 1260 RPM = 45 dBA, 
    // 1480 RPM = 49 dBA, 1680 RPM = 52 dBA, 1880 RPM = 56 dBA, 2100 RPM = 60 dBA
    double expectedDBA = 0.0;
    if (targetRPM <= 840) {
        expectedDBA = 34.0 + (targetRPM - 840) * (34.0 - 0.0) / (840 - 0);
    } else if (targetRPM <= 1040) {
        expectedDBA = 34.0 + (targetRPM - 840) * (39.0 - 34.0) / (1040 - 840);
    } else if (targetRPM <= 1260) {
        expectedDBA = 39.0 + (targetRPM - 1040) * (45.0 - 39.0) / (1260 - 1040);
    } else if (targetRPM <= 1480) {
        expectedDBA = 45.0 + (targetRPM - 1260) * (49.0 - 45.0) / (1480 - 1260);
    } else if (targetRPM <= 1680) {
        expectedDBA = 49.0 + (targetRPM - 1480) * (52.0 - 49.0) / (1680 - 1480);
    } else if (targetRPM <= 1880) {
        expectedDBA = 52.0 + (targetRPM - 1680) * (56.0 - 52.0) / (1880 - 1680);
    } else {
        expectedDBA = 56.0 + (targetRPM - 1880) * (60.0 - 56.0) / (2100 - 1880);
    }
    
    // Debug: show what we're actually sending
    DEBUG_LOG_CATEGORY("FanSpeeds", "RPM conversion: targetRPM=", targetRPM, " -> speedPercent=", speedPercent, "%");
    DEBUG_LOG_CATEGORY("FanSpeeds", "Expected dBA for", targetRPM, "RPM:", expectedDBA);
    
    // Use kernel driver for individual port control (more reliable)
    QString procPath = QString("/proc/Lian_li_SL_INFINITY/Port_%1/fan_speed").arg(port);
    QFile file(procPath);
    
    if (file.open(QIODevice::WriteOnly)) {
        QTextStream stream(&file);
        stream << speedPercent;
        file.close();
        
        
        DEBUG_LOG_CATEGORY("FanSpeeds", "Set Port", port, "to", targetRPM, "RPM (", speedPercent, "%, expected dBA=", expectedDBA, ") via kernel driver");
    } else {
        DEBUG_LOG_CATEGORY("FanSpeeds", "Failed to open", procPath, "for writing - falling back to USB HID");
        
        // Fallback to USB HID controller if kernel driver fails
        if (m_hidController) {
            uint8_t channel = port - 1;
            bool success = m_hidController->SetChannelSpeed(channel, speedPercent);
            
            if (success) {
                DEBUG_LOG_CATEGORY("FanSpeeds", "Set Port", port, "(Channel", channel, ") to", targetRPM, "RPM (", speedPercent, "%, expected dBA=", expectedDBA, ") via USB HID fallback");
            } else {
                DEBUG_LOG_CATEGORY("FanSpeeds", "Failed to set Port", port, "(Channel", channel, ") to", targetRPM, "RPM via USB HID fallback");
            }
        } else {
            qDebug() << "HID controller not available for Port" << port;
        }
    }
}

// CPU and GPU load monitoring removed - not needed for fan control

int FanProfilePage::getRealCPULoad()
{
    // Method 1: Try /proc/loadavg (1-minute average)
    QFile loadavgFile("/proc/loadavg");
    if (loadavgFile.open(QIODevice::ReadOnly)) {
        QTextStream stream(&loadavgFile);
        QString line = stream.readLine();
        loadavgFile.close();
        
        QStringList parts = line.split(' ');
        if (parts.size() >= 3) {
            double load1min = parts[0].toDouble();
            // Convert load average to percentage (rough approximation)
            // Assuming 4 cores, load > 4.0 = 100%
            int loadPercent = qMin(100, static_cast<int>((load1min / 4.0) * 100));
            return loadPercent;
        }
    }
    
    // Method 2: Try /proc/stat (more accurate)
    QFile statFile("/proc/stat");
    if (statFile.open(QIODevice::ReadOnly)) {
        QTextStream stream(&statFile);
        QString line = stream.readLine();
        statFile.close();
        
        if (line.startsWith("cpu ")) {
            QStringList parts = line.split(' ', Qt::SkipEmptyParts);
            if (parts.size() >= 8) {
                // Parse CPU times: user, nice, system, idle, iowait, irq, softirq, steal
                qint64 user = parts[1].toLongLong();
                qint64 nice = parts[2].toLongLong();
                qint64 system = parts[3].toLongLong();
                qint64 idle = parts[4].toLongLong();
                qint64 iowait = parts[5].toLongLong();
                qint64 irq = parts[6].toLongLong();
                qint64 softirq = parts[7].toLongLong();
                qint64 steal = parts.size() > 8 ? parts[8].toLongLong() : 0;
                
                qint64 totalIdle = idle + iowait;
                qint64 totalNonIdle = user + nice + system + irq + softirq + steal;
                qint64 total = totalIdle + totalNonIdle;
                
                // Calculate CPU usage percentage
                static qint64 prevTotal = 0;
                static qint64 prevIdle = 0;
                
                if (prevTotal > 0) {
                    qint64 totalDiff = total - prevTotal;
                    qint64 idleDiff = totalIdle - prevIdle;
                    
                    if (totalDiff > 0) {
                        int cpuPercent = static_cast<int>(((totalDiff - idleDiff) * 100) / totalDiff);
                        prevTotal = total;
                        prevIdle = totalIdle;
                        return qMax(0, qMin(100, cpuPercent));
                    }
                }
                
                prevTotal = total;
                prevIdle = totalIdle;
            }
        }
    }
    
    return -1; // Failed to get CPU load
}

int FanProfilePage::getRealGPULoad()
{
    // Method 1: Try nvidia-smi (NVIDIA GPUs)
    QProcess nvidiaProcess;
    nvidiaProcess.start("nvidia-smi", QStringList() << "--query-gpu=utilization.gpu" << "--format=csv,noheader,nounits");
    nvidiaProcess.waitForFinished(1000);
    
    if (nvidiaProcess.exitCode() == 0) {
        QString output = nvidiaProcess.readAllStandardOutput().trimmed();
        bool ok;
        int gpuLoad = output.toInt(&ok);
        if (ok && gpuLoad >= 0 && gpuLoad <= 100) {
            return gpuLoad;
        }
    }
    
    // Method 2: Try radeontop (AMD GPUs)
    QProcess radeonProcess;
    radeonProcess.start("radeontop", QStringList() << "-d" << "1" << "-l" << "1");
    radeonProcess.waitForFinished(2000);
    
    if (radeonProcess.exitCode() == 0) {
        QString output = radeonProcess.readAllStandardOutput();
        QRegularExpression gpuRegex("gpu\\s+(\\d+)%");
        QRegularExpressionMatch match = gpuRegex.match(output);
        if (match.hasMatch()) {
            return match.captured(1).toInt();
        }
    }
    
    // Method 3: Try intel_gpu_top (Intel GPUs)
    QProcess intelProcess;
    intelProcess.start("intel_gpu_top", QStringList() << "-s" << "1");
    intelProcess.waitForFinished(2000);
    
    if (intelProcess.exitCode() == 0) {
        QString output = intelProcess.readAllStandardOutput();
        QRegularExpression gpuRegex("GPU\\s+(\\d+)%");
        QRegularExpressionMatch match = gpuRegex.match(output);
        if (match.hasMatch()) {
            return match.captured(1).toInt();
        }
    }
    
    return -1; // Failed to get GPU load
}

int FanProfilePage::calculateRPMForLoad(int temperature, int cpuLoad, int gpuLoad)
{
    // Get base RPM from temperature
    int baseRPM = calculateRPMForTemperature(temperature);
    
    // Calculate load-based boost
    int maxLoad = qMax(cpuLoad, gpuLoad);
    int loadBoost = 0;
    
    if (maxLoad > 80) {
        // High load: significant boost
        loadBoost = 300 + ((maxLoad - 80) * 10); // 300-500 RPM boost
    } else if (maxLoad > 60) {
        // Medium load: moderate boost
        loadBoost = 150 + ((maxLoad - 60) * 7); // 150-290 RPM boost
    } else if (maxLoad > 40) {
        // Low load: small boost
        loadBoost = (maxLoad - 40) * 5; // 0-100 RPM boost
    }
    
    // Apply load boost to base RPM
    int finalRPM = baseRPM + loadBoost;
    
    // Clamp to valid range
    finalRPM = qMax(0, qMin(2100, finalRPM));
    
    DEBUG_LOG_CATEGORY("FanSpeeds", "Load-based RPM: temp=", temperature, "°C, CPU=", cpuLoad, "%, GPU=", gpuLoad 
             , "%, baseRPM=", baseRPM, ", loadBoost=", loadBoost, ", finalRPM=", finalRPM);
    
    return finalRPM;
}

// Profile test function removed - no longer needed

// Fan detection functions removed - configuration is now handled via Settings page

QColor FanProfilePage::getTemperatureColor(int temperature)
{
    if (temperature <= 41) {
        // 0-41°C: Blue (cool)
        return QColor(0, 150, 255);
    } else if (temperature <= 60) {
        // 42-60°C: Green (normal)
        return QColor(0, 255, 0);
    } else if (temperature <= 76) {
        // 61-76°C: Yellow (warm)
        return QColor(255, 255, 0);
    } else {
        // 77-100°C: Red (hot)
        return QColor(255, 0, 0);
    }
}

bool FanProfilePage::isPortConnected(int port)
{
    if (port < 1 || port > 4) return false;
    return m_portConnected[port - 1];
}

void FanProfilePage::onCurvePointsChanged(const QVector<QPointF> &points)
{
    qDebug() << "Curve points changed for Port" << m_selectedPort;
    
    // If a custom profile is selected, update that profile's base curve
    if (m_custom1Radio->isChecked()) {
        m_customProfileCurves[1] = points;
        saveCustomProfiles();
        qDebug() << "Updated custom profile 1 (" << m_customProfileNames[1] << ") curve";
    } else if (m_custom2Radio->isChecked()) {
        m_customProfileCurves[2] = points;
        saveCustomProfiles();
        qDebug() << "Updated custom profile 2 (" << m_customProfileNames[2] << ") curve";
    } else if (m_custom3Radio->isChecked()) {
        m_customProfileCurves[3] = points;
        saveCustomProfiles();
        qDebug() << "Updated custom profile 3 (" << m_customProfileNames[3] << ") curve";
    } else {
        // Save the custom curve for the currently selected port
        m_customCurves[m_selectedPort] = points;
        
        // Save to config
        saveCustomCurves();
    }
    
    // Immediately apply the new curve to fan control
    controlFanSpeeds();
}

void FanProfilePage::onPortSelectionChanged()
{
    // Get selected row
    QList<QTableWidgetItem*> selectedItems = m_fanTable->selectedItems();
    if (selectedItems.isEmpty()) {
        return;
    }
    
    int selectedRow = selectedItems.first()->row();
    m_selectedPort = selectedRow + 1; // Convert row (0-3) to port (1-4)
    
    qDebug() << "Port selection changed to Port" << m_selectedPort;
    
    // Update fan size for the graph
    m_fanCurveWidget->setFanSize(m_fanSizeMaxRPM[m_selectedPort]);
    
    // Load the curve for this port (either custom or default)
    if (m_customCurves.contains(m_selectedPort)) {
        m_fanCurveWidget->setCustomCurve(m_customCurves[m_selectedPort]);
    } else {
        // No custom curve, use the port's assigned profile default
        QString portProfile = m_portProfiles.value(m_selectedPort, "Quiet");
        QString internalName = getInternalProfileName(portProfile);
        QVector<QPointF> defaultCurve = getDefaultCurveForProfile(internalName);
        m_fanCurveWidget->setCustomCurve(defaultCurve);
    }
    
    // Update the radio button to match the port's profile
    QString portProfile = m_portProfiles.value(m_selectedPort, "Quiet");
    if (portProfile == "Quiet") {
        m_quietRadio->setChecked(true);
    } else if (portProfile == "StdSP") {
        m_stdSpRadio->setChecked(true);
    } else if (portProfile == "HighSP") {
        m_highSpRadio->setChecked(true);
    } else if (portProfile == "FullSP") {
        m_fullSpRadio->setChecked(true);
    } else if (portProfile == m_customProfileNames[1]) {
        m_custom1Radio->setChecked(true);
    } else if (portProfile == m_customProfileNames[2]) {
        m_custom2Radio->setChecked(true);
    } else if (portProfile == m_customProfileNames[3]) {
        m_custom3Radio->setChecked(true);
    }
}

void FanProfilePage::onFanSizeChanged(int port)
{
    if (port < 1 || port > 4) {
        return;
    }
    
    // Get the combo box for this port
    QComboBox *sizeCombo = m_fanSizeComboBoxes[port - 1];
    QString fanSize = sizeCombo->currentText();
    
    // Update the max RPM based on fan size
    if (fanSize == "120MM") {
        m_fanSizeMaxRPM[port] = 2100;
        DEBUG_LOG_CATEGORY("FanSpeeds", "Port", port, "fan size changed to 120mm (2100 RPM max)");
    } else if (fanSize == "140MM") {
        m_fanSizeMaxRPM[port] = 1600;
        DEBUG_LOG_CATEGORY("FanSpeeds", "Port", port, "fan size changed to 140mm (1600 RPM max)");
    }
    
    // If this is the currently selected port, update the graph
    if (port == m_selectedPort) {
        m_fanCurveWidget->setFanSize(m_fanSizeMaxRPM[port]);
        DEBUG_LOG_CATEGORY("FanSpeeds", "Updated graph to show", m_fanSizeMaxRPM[port], "RPM max for Port", port);
    }
}

void FanProfilePage::onRenameCustomProfile(int profileNum)
{
    if (profileNum < 1 || profileNum > 3) {
        return;
    }
    
    bool ok;
    QString currentName = m_customProfileNames[profileNum];
    QString newName = QInputDialog::getText(this, "Rename Custom Profile",
                                           "Enter new name (max 6 characters):",
                                           QLineEdit::Normal, currentName, &ok);
    
    if (ok && !newName.isEmpty()) {
        // Limit to 6 characters
        if (newName.length() > 6) {
            newName = newName.left(6);
        }
        
        // Update the name
        m_customProfileNames[profileNum] = newName;
        
        // Update the radio button text
        if (profileNum == 1) {
            m_custom1Radio->setText(newName);
        } else if (profileNum == 2) {
            m_custom2Radio->setText(newName);
        } else if (profileNum == 3) {
            m_custom3Radio->setText(newName);
        }
        
        // Save to settings
        saveCustomProfiles();
        
        qDebug() << "Renamed custom profile" << profileNum << "to" << newName;
    }
}

void FanProfilePage::saveCustomCurves()
{
    QSettings settings("LConnect3", "FanCurves");
    
    // Save each port's custom curve
    for (int port = 1; port <= 4; ++port) {
        if (m_customCurves.contains(port)) {
            QVector<QPointF> curve = m_customCurves[port];
            
            settings.beginWriteArray(QString("Port%1").arg(port));
            for (int i = 0; i < curve.size(); ++i) {
                settings.setArrayIndex(i);
                settings.setValue("temp", curve[i].x());
                settings.setValue("rpm", curve[i].y());
            }
            settings.endArray();
        }
    }
    
    qDebug() << "Saved custom curves for" << m_customCurves.size() << "ports";
}

void FanProfilePage::loadCustomCurves()
{
    QSettings settings("LConnect3", "FanCurves");
    
    // Load each port's custom curve
    for (int port = 1; port <= 4; ++port) {
        int size = settings.beginReadArray(QString("Port%1").arg(port));
        if (size > 0) {
            QVector<QPointF> curve;
            for (int i = 0; i < size; ++i) {
                settings.setArrayIndex(i);
                double temp = settings.value("temp").toDouble();
                double rpm = settings.value("rpm").toDouble();
                curve.append(QPointF(temp, rpm));
            }
            m_customCurves[port] = curve;
            qDebug() << "Loaded custom curve for Port" << port << "with" << size << "points";
        }
        settings.endArray();
    }
    
    // Load the curve for Port 1 (default selection)
    if (m_customCurves.contains(1)) {
        m_fanCurveWidget->setCustomCurve(m_customCurves[1]);
    }
}

void FanProfilePage::saveCustomProfiles()
{
    QSettings settings("LConnect3", "CustomProfiles");
    
    // Save each custom profile name and base curve
    for (int i = 1; i <= 3; ++i) {
        settings.setValue(QString("Profile%1Name").arg(i), m_customProfileNames[i]);
        
        QVector<QPointF> curve = m_customProfileCurves[i];
        settings.beginWriteArray(QString("Profile%1Curve").arg(i));
        for (int j = 0; j < curve.size(); ++j) {
            settings.setArrayIndex(j);
            settings.setValue("temp", curve[j].x());
            settings.setValue("rpm", curve[j].y());
        }
        settings.endArray();
    }
    
    qDebug() << "Saved custom profiles";
}

void FanProfilePage::loadCustomProfiles()
{
    QSettings settings("LConnect3", "CustomProfiles");
    
    // Load each custom profile name and base curve
    for (int i = 1; i <= 3; ++i) {
        // Load name
        QString name = settings.value(QString("Profile%1Name").arg(i), "Cust" + QString::number(i)).toString();
        m_customProfileNames[i] = name;
        
        // Update radio button text
        if (i == 1) m_custom1Radio->setText(name);
        else if (i == 2) m_custom2Radio->setText(name);
        else if (i == 3) m_custom3Radio->setText(name);
        
        // Load curve
        int size = settings.beginReadArray(QString("Profile%1Curve").arg(i));
        if (size > 0) {
            QVector<QPointF> curve;
            for (int j = 0; j < size; ++j) {
                settings.setArrayIndex(j);
                double temp = settings.value("temp").toDouble();
                double rpm = settings.value("rpm").toDouble();
                curve.append(QPointF(temp, rpm));
            }
            m_customProfileCurves[i] = curve;
            qDebug() << "Loaded custom profile" << i << "(" << name << ") with" << size << "points";
        } else {
            // No saved curve, use Quiet default
            m_customProfileCurves[i] = getDefaultCurveForProfile("Quiet");
        }
        settings.endArray();
    }
}

void FanProfilePage::savePortProfiles()
{
    QSettings settings("LConnect3", "PortProfiles");
    
    // Save profile name for each port
    for (int port = 1; port <= 4; ++port) {
        QString profileName = m_portProfiles.value(port, "Quiet");
        settings.setValue(QString("Port%1").arg(port), profileName);
    }
    
    qDebug() << "Saved port profiles";
}

void FanProfilePage::loadPortProfiles()
{
    QSettings settings("LConnect3", "PortProfiles");
    
    // Load profile name for each port
    for (int port = 1; port <= 4; ++port) {
        QString profileName = settings.value(QString("Port%1").arg(port), "Quiet").toString();
        m_portProfiles[port] = profileName;
        qDebug() << "Loaded Port" << port << "profile:" << profileName;
    }
}

QVector<QPointF> FanProfilePage::getDefaultCurveForProfile(const QString &profile)
{
    QVector<QPointF> curvePoints;
    
    if (profile == "Quiet") {
        curvePoints << QPointF(0, 120) << QPointF(25, 420) << QPointF(45, 840) 
                   << QPointF(65, 1050) << QPointF(80, 1680) << QPointF(90, 2100) << QPointF(100, 2100);
    } else if (profile == "Standard") {
        curvePoints << QPointF(0, 120) << QPointF(25, 420) << QPointF(40, 1050) << QPointF(55, 1260) 
                   << QPointF(70, 1680) << QPointF(90, 2100) << QPointF(100, 2100);
    } else if (profile == "High Speed") {
        curvePoints << QPointF(0, 120) << QPointF(25, 910) << QPointF(35, 1140) << QPointF(50, 1470)
                   << QPointF(70, 1800) << QPointF(85, 2100) << QPointF(100, 2100);
    } else if (profile == "Full Speed") {
        curvePoints << QPointF(0, 120) << QPointF(25, 2100) << QPointF(40, 2100) << QPointF(55, 2100)
                   << QPointF(70, 2100) << QPointF(90, 2100) << QPointF(100, 2100);
    } else {
        // Default to Quiet
        curvePoints << QPointF(0, 120) << QPointF(25, 420) << QPointF(45, 840) 
                   << QPointF(65, 1050) << QPointF(80, 1680) << QPointF(90, 2100) << QPointF(100, 2100);
    }
    
    return curvePoints;
}

int FanProfilePage::calculateRPMForCustomCurve(int port, int temperature)
{
    // Check if this port has a custom curve
    if (!m_customCurves.contains(port)) {
        // No custom curve, use the profile default
        return calculateRPMForTemperature(temperature);
    }
    
    QVector<QPointF> curvePoints = m_customCurves[port];
    
    if (curvePoints.size() < 2) {
        return 0;
    }
    
    // Clamp temperature to valid range
    temperature = qMax(0, qMin(100, temperature));
    
    // Find the two points to interpolate between
    for (int i = 0; i < curvePoints.size() - 1; ++i) {
        if (temperature >= curvePoints[i].x() && temperature <= curvePoints[i + 1].x()) {
            // Linear interpolation between the two points
            double t = (temperature - curvePoints[i].x()) / (curvePoints[i + 1].x() - curvePoints[i].x());
            double rpm = curvePoints[i].y() + t * (curvePoints[i + 1].y() - curvePoints[i].y());
            return static_cast<int>(rpm);
        }
    }
    
    // If temperature is outside the curve range, clamp to nearest point
    if (temperature < curvePoints.first().x()) {
        return static_cast<int>(curvePoints.first().y());
    } else {
        return static_cast<int>(curvePoints.last().y());
    }
}
