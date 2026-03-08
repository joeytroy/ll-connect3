#include "systeminfopage.h"
#include "widgets/monitoringcard.h"
#include <QFont>
#include <QSettings>
#include <QProcess>
#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QRegularExpression>
#include <QtMath>
#include <cmath>
#include <algorithm>
#include <QTimer>
#include <QDateTime>

// Initialize static variables
qint64 SystemInfoPage::m_prevEnergyUJ = 0;
qint64 SystemInfoPage::m_prevTimestamp = 0;

SystemInfoPage::SystemInfoPage(QWidget *parent)
    : QWidget(parent)
{
    setupUI();
    createMonitoringCards();
    
    // Start update timer
    m_updateTimer = new QTimer(this);
    connect(m_updateTimer, &QTimer::timeout, this, &SystemInfoPage::updateSystemInfo);
    m_updateTimer->start(1000); // Update every second
    
    // Initial update
    updateSystemInfo();
}

void SystemInfoPage::setupUI()
{
    // Set proper size policy for the page
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(18, 16, 18, 10); // Reduced bottom margin to bring up bottom of window
    m_mainLayout->setSpacing(14);

    // Header
    m_headerLayout = new QHBoxLayout();
    m_headerLayout->setContentsMargins(0, 0, 0, 0);
    m_headerLayout->setSpacing(12);

    m_titleLabel = new QLabel("System Resource");
    m_titleLabel->setObjectName("pageTitle");
    m_titleLabel->hide(); // Tab handles the title

    m_floatingWindowLabel = new QLabel("Displays a floating system information window");
    m_floatingWindowLabel->setObjectName("floatingWindowLabel");
    m_floatingWindowLabel->hide();

    m_headerLayout->addWidget(m_titleLabel);
    m_headerLayout->addStretch();
    m_headerLayout->addWidget(m_floatingWindowLabel);

    m_mainLayout->addLayout(m_headerLayout);

    // Content layout: 2 columns, CPU/GPU top row, RAM/Network/Storage bottom row
    m_contentGrid = new QGridLayout();
    m_contentGrid->setHorizontalSpacing(8);
    m_contentGrid->setVerticalSpacing(8);
    m_contentGrid->setColumnStretch(0, 1);
    m_contentGrid->setColumnStretch(1, 2);
    m_contentGrid->setRowStretch(0, 3); // Top row with CPU, GPU
    m_contentGrid->setRowStretch(1, 0); // Divider
    m_contentGrid->setRowStretch(2, 2); // Bottom row with RAM, Network, Storage
    m_contentGrid->setRowMinimumHeight(2, 230); // Reduced to bring up bottom of window
    m_contentGrid->setSizeConstraint(QLayout::SetDefaultConstraint);

    m_mainLayout->addLayout(m_contentGrid);

    // Apply styles - Updated to match LL-Connect 3 reference
    setStyleSheet(R"(
        SystemInfoPage {
            background-color: transparent;
        }
        
        #pageTitle {
            color: rgba(255, 255, 255, 0.8);
            font-size: 22px;
            font-weight: 600;
            letter-spacing: 1px;
        }
        
        #floatingWindowLabel {
            color: rgba(255, 255, 255, 0.55);
            font-size: 14px;
        }
        
        #contentDivider {
            background-color: rgba(255, 255, 255, 0.12);
            border: none;
        }
        
        QFrame#infoPanel {
            background: transparent;
            border: none;
            border-radius: 0px;
            padding: 0;
        }

        QLabel#cpuHeading {
            color: #2ca8ff;
            font-weight: 600;
            font-size: 14px;
            letter-spacing: 1px;
        }

        QLabel#gpuHeading {
            color: #3edb8d;
            font-weight: 600;
            font-size: 14px;
            letter-spacing: 1px;
        }

        QLabel#ramHeading {
            color: #ffc431;
            font-weight: 600;
            font-size: 14px;
            letter-spacing: 1px;
        }

        QLabel#networkHeading {
            color: #ff66d0;
            font-weight: 600;
            font-size: 14px;
            letter-spacing: 1px;
        }

        QLabel#storageHeading {
            color: #ff8c3a;
            font-weight: 600;
            font-size: 14px;
            letter-spacing: 1px;
        }
    )");
}

void SystemInfoPage::createMonitoringCards()
{
    // CPU panel with proper size policy
    QFrame *cpuPanel = new QFrame();
    cpuPanel->setObjectName("infoPanel");
    cpuPanel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    QVBoxLayout *cpuPanelLayout = new QVBoxLayout(cpuPanel);
    cpuPanelLayout->setContentsMargins(0, -18, 0, 6);
    cpuPanelLayout->setSpacing(2);

    // CPU main content: strict 2x2 grid - all spacing must match GPU exactly
    // Header will be moved below the circle for stability
    QGridLayout *cpuGrid = new QGridLayout();
    cpuGrid->setHorizontalSpacing(14);
    cpuGrid->setVerticalSpacing(20); // Match GPU exactly
    cpuGrid->setContentsMargins(8, -92, 0, 0);

    // Left column: CPU circle
    m_cpuLoadCard = new MonitoringCard(MonitoringCard::CircularProgress, "CPU LOAD");
    m_cpuLoadCard->setColor(QColor(45, 166, 255));
    m_cpuLoadCard->setProgress(3);
    m_cpuLoadCard->setValue("3%");
    cpuGrid->addWidget(m_cpuLoadCard, 0, 0, Qt::AlignLeft | Qt::AlignTop);

    // Below circle: header label and metrics (temp + clock) - exact same positioning as GPU
    QVBoxLayout *cpuMetricsColumn = new QVBoxLayout();
    cpuMetricsColumn->setSpacing(0); // No spacing between elements
    cpuMetricsColumn->setContentsMargins(0, 30, 0, 0); // Increased top margin to push text lower below circle
    
    // CPU header label below circle
    QLabel *cpuHeader = new QLabel("CPU");
    cpuHeader->setObjectName("cpuHeading");
    cpuMetricsColumn->addWidget(cpuHeader);
    
    // Temperature line - simple label and value
    QHBoxLayout *tempLine = new QHBoxLayout();
    tempLine->setSpacing(8);
    
    QLabel *tempLabel = new QLabel("Temperature");
    tempLabel->setStyleSheet("color: rgba(255, 255, 255, 0.6); font-size: 9px;");
    tempLine->addWidget(tempLabel);
    
    m_cpuTempLabel = new QLabel("-- °C");
    m_cpuTempLabel->setStyleSheet("color: #ffffff; font-size: 11px; font-weight: 600;");
    tempLine->addWidget(m_cpuTempLabel);
    
    tempLine->addStretch(); // Push content to the left
    
    cpuMetricsColumn->addLayout(tempLine);
    
    // Clock rate line
    QHBoxLayout *clockLine = new QHBoxLayout();
    clockLine->setSpacing(8);
    
    QLabel *clockLabel = new QLabel("Clock rate");
    clockLabel->setStyleSheet("color: rgba(255, 255, 255, 0.6); font-size: 9px;");
    clockLine->addWidget(clockLabel);
    
    m_cpuClockLabel = new QLabel("-- MHz");
    m_cpuClockLabel->setStyleSheet("color: #ffffff; font-size: 11px; font-weight: 600;");
    clockLine->addWidget(m_cpuClockLabel);
    
    clockLine->addStretch(); // Push content to the left
    
    cpuMetricsColumn->addLayout(clockLine);
    cpuGrid->addLayout(cpuMetricsColumn, 1, 0, Qt::AlignLeft | Qt::AlignTop);
    cpuGrid->setColumnMinimumWidth(1, 120); // Ensure enough space for metrics
    cpuGrid->setRowMinimumHeight(0, 180); // Match GPU row 0 height for consistent circle positioning
    cpuGrid->setRowMinimumHeight(1, 60); // Ensure enough vertical space for metrics

    // Right column: power over voltage - must match GPU card sizes exactly
    m_cpuPowerCard = new MonitoringCard(MonitoringCard::RectangularValue, "CPU POWERS");
    m_cpuPowerCard->setColor(QColor(45, 166, 255));
    m_cpuPowerCard->setValue("-- W");
    m_cpuPowerCard->setMinimumWidth(150);
    m_cpuPowerCard->setMinimumHeight(90); // Match GPU power card for alignment
    m_cpuPowerCard->setMaximumHeight(120); // Match GPU power card for alignment
    cpuGrid->addWidget(m_cpuPowerCard, 0, 1, Qt::AlignTop);

    m_cpuVoltageCard = new MonitoringCard(MonitoringCard::RectangularValue, "CPU VOLTAGES");
    m_cpuVoltageCard->setColor(QColor(45, 166, 255));
    m_cpuVoltageCard->setValue("-- V");
    m_cpuVoltageCard->setMinimumWidth(150);
    m_cpuVoltageCard->setMinimumHeight(90); // Match GPU memory card height for alignment
    m_cpuVoltageCard->setMaximumHeight(120); // Match GPU memory card height for alignment
    cpuGrid->addWidget(m_cpuVoltageCard, 1, 1, Qt::AlignTop);

    cpuGrid->setColumnStretch(0, 2);
    cpuGrid->setColumnStretch(1, 1);
    cpuPanelLayout->addLayout(cpuGrid);
    m_contentGrid->addWidget(cpuPanel, 0, 0);

    // GPU panel with proper size policy
    QFrame *gpuPanel = new QFrame();
    gpuPanel->setObjectName("infoPanel");
    gpuPanel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    QVBoxLayout *gpuPanelLayout = new QVBoxLayout(gpuPanel);
    gpuPanelLayout->setContentsMargins(0, -18, 0, 6);
    gpuPanelLayout->setSpacing(2);

    // GPU main content: strict 2x2 grid - all spacing must match CPU exactly
    // Header will be moved below the circle for stability
    QGridLayout *gpuGrid = new QGridLayout();
    gpuGrid->setHorizontalSpacing(14);
    gpuGrid->setVerticalSpacing(20); // Match CPU exactly
    gpuGrid->setContentsMargins(8, -92, 0, 0);

    // Left column: GPU circle
    m_gpuLoadCard = new MonitoringCard(MonitoringCard::CircularProgress, "GPU LOAD");
    m_gpuLoadCard->setColor(QColor(62, 219, 141));
    gpuGrid->addWidget(m_gpuLoadCard, 0, 0, Qt::AlignLeft | Qt::AlignTop);

    // Below circle: header label and GPU metrics - exact same positioning as CPU
    QVBoxLayout *gpuMetricsColumn = new QVBoxLayout();
    gpuMetricsColumn->setSpacing(0); // No spacing between elements
    gpuMetricsColumn->setContentsMargins(0, 30, 0, 0); // Increased top margin to push text lower below circle

    // GPU header label below circle
    QLabel *gpuHeader = new QLabel("GPU");
    gpuHeader->setObjectName("gpuHeading");
    gpuMetricsColumn->addWidget(gpuHeader);

    QHBoxLayout *gpuTempLine = new QHBoxLayout();
    gpuTempLine->setSpacing(8);
    QLabel *gpuTempLabel = new QLabel("Temperature");
    gpuTempLabel->setStyleSheet("color: rgba(255, 255, 255, 0.6); font-size: 9px;");
    gpuTempLine->addWidget(gpuTempLabel);
    m_gpuTempLabel = new QLabel("-- °C");
    m_gpuTempLabel->setStyleSheet("color: #ffffff; font-size: 11px; font-weight: 600;");
    gpuTempLine->addWidget(m_gpuTempLabel);
    gpuTempLine->addStretch();
    gpuMetricsColumn->addLayout(gpuTempLine);

    QHBoxLayout *gpuClockLine = new QHBoxLayout();
    gpuClockLine->setSpacing(8);
    QLabel *gpuClockLabel = new QLabel("Clock rate");
    gpuClockLabel->setStyleSheet("color: rgba(255, 255, 255, 0.6); font-size: 9px;");
    gpuClockLine->addWidget(gpuClockLabel);
    m_gpuClockLabel = new QLabel("-- MHz");
    m_gpuClockLabel->setStyleSheet("color: #ffffff; font-size: 11px; font-weight: 600;");
    gpuClockLine->addWidget(m_gpuClockLabel);
    gpuClockLine->addStretch();
    gpuMetricsColumn->addLayout(gpuClockLine);

    gpuGrid->addLayout(gpuMetricsColumn, 1, 0, Qt::AlignLeft | Qt::AlignTop);
    gpuGrid->setColumnMinimumWidth(1, 120); // Ensure enough space for metrics
    gpuGrid->setRowMinimumHeight(0, 180); // Match CPU row 0 height for consistent circle positioning
    gpuGrid->setRowMinimumHeight(1, 60); // Ensure enough vertical space for metrics - must match CPU exactly

    // Right column: power over voltage - must match CPU card sizes exactly
    m_gpuPowerCard = new MonitoringCard(MonitoringCard::RectangularValue, "GPU Powers");
    m_gpuPowerCard->setColor(QColor(62, 219, 141));
    m_gpuPowerCard->setMinimumWidth(150);
    m_gpuPowerCard->setMinimumHeight(90); // Match CPU power card for alignment
    m_gpuPowerCard->setMaximumHeight(120); // Match CPU power card for alignment
    gpuGrid->addWidget(m_gpuPowerCard, 0, 1, Qt::AlignTop);

    m_gpuMemoryCard = new MonitoringCard(MonitoringCard::RectangularValue, "GPU MEMORY");
    m_gpuMemoryCard->setColor(QColor(62, 219, 141));
    m_gpuMemoryCard->setMinimumWidth(150);
    m_gpuMemoryCard->setMinimumHeight(90); // Increased height for better text display
    m_gpuMemoryCard->setMaximumHeight(120); // Allow more height for the memory text
    gpuGrid->addWidget(m_gpuMemoryCard, 1, 1, Qt::AlignTop);

    gpuGrid->setColumnStretch(0, 2);
    gpuGrid->setColumnStretch(1, 1);
    gpuPanelLayout->addLayout(gpuGrid);
    m_contentGrid->addWidget(gpuPanel, 0, 1);

    QFrame *contentDivider = new QFrame();
    contentDivider->setObjectName("contentDivider");
    contentDivider->setFrameShape(QFrame::HLine);
    contentDivider->setFrameShadow(QFrame::Plain);
    contentDivider->setFixedHeight(5);
    m_contentGrid->addWidget(contentDivider, 1, 0, 1, 2);

    // Bottom row: RAM, Network, Storage
    QWidget *bottomContainer = new QWidget();
    bottomContainer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    QHBoxLayout *bottomLayout = new QHBoxLayout(bottomContainer);
    bottomLayout->setContentsMargins(0, 0, 0, 0);
    bottomLayout->setSpacing(8);

    // RAM Panel - matching CPU/GPU structure, but in bottom row
    QFrame *ramPanel = new QFrame();
    ramPanel->setObjectName("infoPanel");
    ramPanel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    QVBoxLayout *ramPanelLayout = new QVBoxLayout(ramPanel);
    ramPanelLayout->setContentsMargins(0, -18, 0, 20); // Increased bottom margin to prevent clipping
    ramPanelLayout->setSpacing(2);

    // RAM grid - ensure text is clearly below circle  
    QGridLayout *ramGrid = new QGridLayout();
    ramGrid->setHorizontalSpacing(14);
    ramGrid->setVerticalSpacing(20); // Match CPU/GPU spacing
    ramGrid->setContentsMargins(8, -92, 0, 15);

    // Circle in row 0
    m_ramUsageCard = new MonitoringCard(MonitoringCard::CircularProgress, "RAM");
    m_ramUsageCard->setColor(QColor(255, 196, 49));
    ramGrid->addWidget(m_ramUsageCard, 0, 0, Qt::AlignLeft | Qt::AlignTop);

    // Below circle: header label and RAM details - matching CPU/GPU pattern
    QVBoxLayout *ramMetricsColumn = new QVBoxLayout();
    ramMetricsColumn->setSpacing(0);
    ramMetricsColumn->setContentsMargins(0, 30, 0, 0); // Increased top margin to push text lower below circle

    // RAM header label below circle
    QLabel *ramHeader = new QLabel("RAM");
    ramHeader->setObjectName("ramHeading");
    ramMetricsColumn->addWidget(ramHeader);

    // RAM details text
    m_ramDetailsLabel = new QLabel("-- / -- RAM");
    m_ramDetailsLabel->setAlignment(Qt::AlignLeft);
    m_ramDetailsLabel->setStyleSheet("color: rgba(255, 255, 255, 0.75); font-size: 11px; font-weight: 600;");
    ramMetricsColumn->addWidget(m_ramDetailsLabel);

    ramGrid->addLayout(ramMetricsColumn, 1, 0, Qt::AlignLeft | Qt::AlignTop);
    ramGrid->setRowMinimumHeight(0, 180); // Match CPU/GPU row 0 height
    ramGrid->setRowMinimumHeight(1, 60);  // Match CPU/GPU row 1 height
    ramGrid->setColumnStretch(0, 1); // Allow column to expand

    ramPanelLayout->addLayout(ramGrid);
    ramPanelLayout->addStretch(); // Add stretch to push content up and prevent clipping
    bottomLayout->addWidget(ramPanel);

    // Network Panel
    QFrame *networkPanel = new QFrame();
    networkPanel->setObjectName("infoPanel");
    networkPanel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    networkPanel->setMinimumWidth(180);
    QVBoxLayout *networkLayout = new QVBoxLayout(networkPanel);
    networkLayout->setContentsMargins(0, -14, 0, 6);
    networkLayout->setSpacing(4);

    QLabel *networkHeader = new QLabel("Network");
    networkHeader->setObjectName("networkHeading");
    networkLayout->addWidget(networkHeader);

    m_networkCard = new MonitoringCard(MonitoringCard::NetworkSpeed, "Network");
    m_networkCard->setColor(QColor(255, 102, 208));

    QVBoxLayout *networkContentLayout = new QVBoxLayout();
    networkContentLayout->setContentsMargins(12, -36, 12, 0);
    networkContentLayout->setSpacing(0);
    networkContentLayout->addWidget(m_networkCard, 0, Qt::AlignTop);
    networkContentLayout->addStretch();

    networkLayout->addLayout(networkContentLayout);
    bottomLayout->addWidget(networkPanel);

    // Storage Panel
    QFrame *storagePanel = new QFrame();
    storagePanel->setObjectName("infoPanel");
    storagePanel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    storagePanel->setMinimumWidth(180);
    QVBoxLayout *storageLayout = new QVBoxLayout(storagePanel);
    storageLayout->setContentsMargins(0, -14, 0, 6);
    storageLayout->setSpacing(4);

    QLabel *storageHeader = new QLabel("Storage");
    storageHeader->setObjectName("storageHeading");
    storageLayout->addWidget(storageHeader);

    m_storageCard = new MonitoringCard(MonitoringCard::StorageInfo, "Storage");
    m_storageCard->setColor(QColor(255, 140, 58));

    QVBoxLayout *storageContentLayout = new QVBoxLayout();
    storageContentLayout->setContentsMargins(12, -36, 12, 0);
    storageContentLayout->setSpacing(0);
    storageContentLayout->addWidget(m_storageCard, 0, Qt::AlignTop);
    storageContentLayout->addStretch();

    storageLayout->addLayout(storageContentLayout);
    bottomLayout->addWidget(storagePanel);

    m_contentGrid->addWidget(bottomContainer, 2, 0, 1, 2); // Span both columns
}

void SystemInfoPage::updateSystemInfo()
{
    // Fast file-based updates every tick (instant, no QProcess)
    updateCPUInfo();
    updateRAMInfo();
    updateNetworkInfo();
    
    // GPU info uses external processes (nvidia-smi etc.) — update every 3 seconds
    if (m_slowUpdateCounter % 3 == 0) {
        updateGPUInfo();
    }
    
    // Storage uses df — update every 10 seconds
    if (m_slowUpdateCounter % 10 == 0) {
        updateStorageInfo();
    }
    
    m_slowUpdateCounter++;
}

void SystemInfoPage::updateCPUInfo()
{
    // CPU Load from /proc/stat (real-time CPU usage)
    static long prevIdle = 0, prevTotal = 0;
    
    QFile statFile("/proc/stat");
    if (statFile.open(QIODevice::ReadOnly)) {
        QTextStream stream(&statFile);
        QString line = stream.readLine();
        
        if (line.startsWith("cpu ")) {
            QStringList parts = line.split(' ', Qt::SkipEmptyParts);
            if (parts.size() >= 8) {
                // Parse CPU times: user, nice, system, idle, iowait, irq, softirq, steal
                long user = parts[1].toLong();
                long nice = parts[2].toLong();
                long system = parts[3].toLong();
                long idle = parts[4].toLong();
                long iowait = parts[5].toLong();
                long irq = parts[6].toLong();
                long softirq = parts[7].toLong();
                long steal = parts.size() > 8 ? parts[8].toLong() : 0;
                
                long totalIdle = idle + iowait;
                long total = user + nice + system + totalIdle + irq + softirq + steal;
                
                if (prevTotal > 0) {
                    long totalDiff = total - prevTotal;
                    long idleDiff = totalIdle - prevIdle;
                    
                    if (totalDiff > 0) {
                        int cpuLoad = static_cast<int>(((totalDiff - idleDiff) * 100) / totalDiff);
                        cpuLoad = std::max(0, std::min(100, cpuLoad)); // Clamp between 0-100
                        
                        m_cpuLoadCard->setProgress(cpuLoad);
                        m_cpuLoadCard->setValue(QString::number(cpuLoad) + "%");
                        m_cpuLoadCard->setSubValue("CPU LOAD");
                    }
                }
                
                prevIdle = totalIdle;
                prevTotal = total;
            }
        }
        statFile.close();
    }
    
    // CPU Temperature from hwmon sysfs (instant, no QProcess needed)
    int maxTemp = 0;
    {
        QDir hwmonDir("/sys/class/hwmon");
        QStringList hwmonDirs = hwmonDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        
        for (const QString &hwmon : hwmonDirs) {
            QFile nameFile("/sys/class/hwmon/" + hwmon + "/name");
            if (nameFile.open(QIODevice::ReadOnly)) {
                QTextStream stream(&nameFile);
                QString name = stream.readLine().trimmed();
                nameFile.close();
                
                // Check for CPU temperature sensors
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
    
    // Fallback: try thermal zones if hwmon didn't work
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
    
    // Update CPU temperature label (respect Fan temperature unit: °C or °F)
    if (maxTemp > 0) {
        bool useF = QSettings("LianLi", "LConnect3").value("FanTemperatureUnit", "C").toString() == "F";
        if (useF) {
            int f = static_cast<int>(std::round(maxTemp * 9.0 / 5.0 + 32));
            m_cpuTempLabel->setText(QString::number(f) + " °F");
        } else {
            m_cpuTempLabel->setText(QString::number(maxTemp) + " °C");
        }
    } else {
        bool useF = QSettings("LianLi", "LConnect3").value("FanTemperatureUnit", "C").toString() == "F";
        m_cpuTempLabel->setText(useF ? "-- °F" : "-- °C");
    }
    
    // CPU Clock from /proc/cpuinfo
    QFile cpuFile("/proc/cpuinfo");
    if (cpuFile.open(QIODevice::ReadOnly)) {
        QTextStream stream(&cpuFile);
        QString line;
        double maxClock = 0.0;
        while (stream.readLineInto(&line)) {
            if (line.startsWith("cpu MHz")) {
                QStringList parts = line.split(':');
                if (parts.size() == 2) {
                    double mhz = parts[1].trimmed().toDouble();
                    if (mhz > maxClock) maxClock = mhz;
                }
            }
        }
        cpuFile.close();
        
        // Update CPU clock label
        if (maxClock > 0) {
            m_cpuClockLabel->setText(QString::number(static_cast<int>(maxClock)) + " MHz");
        } else {
            m_cpuClockLabel->setText("-- MHz");
        }
    } else {
        m_cpuClockLabel->setText("-- MHz");
    }
    
    // CPU Power and Voltage - try to get from various sources
    updateCPUPowerAndVoltage();
}

void SystemInfoPage::updateCPUPowerAndVoltage()
{
    // Try to get CPU power from RAPL (Running Average Power Limit)
    QStringList raplPaths = {
        "/sys/class/powercap/intel-rapl/intel-rapl:0/energy_uj",
        "/sys/class/powercap/intel-rapl/intel-rapl:1/energy_uj",
        "/sys/class/powercap/intel-rapl/intel-rapl:0:0/energy_uj"
    };
    
    bool foundRAPL = false;
    for (const QString &path : raplPaths) {
        QFile raplFile(path);
        if (raplFile.open(QIODevice::ReadOnly)) {
            QTextStream stream(&raplFile);
            QString energyStr = stream.readLine();
            if (!energyStr.isEmpty()) {
                qint64 currentEnergyUJ = energyStr.toLongLong();
                qint64 currentTimestamp = QDateTime::currentMSecsSinceEpoch();
                
                if (m_prevTimestamp > 0 && m_prevEnergyUJ > 0) {
                    qint64 timeDiff = currentTimestamp - m_prevTimestamp; // milliseconds
                    qint64 energyDiff = currentEnergyUJ - m_prevEnergyUJ; // microjoules
                    
                    if (timeDiff > 0 && energyDiff >= 0) {
                        // Convert to watts: (microjoules / 1000000) / (milliseconds / 1000)
                        double powerW = (energyDiff / 1000000.0) / (timeDiff / 1000.0);
                        // Clamp to reasonable range (0-500W)
                        if (powerW >= 0 && powerW <= 500) {
                            m_cpuPowerCard->setValue(QString::number(powerW, 'f', 1) + " W");
                        } else {
                            m_cpuPowerCard->setValue("-- W");
                        }
                    } else {
                        m_cpuPowerCard->setValue("-- W");
                    }
                } else {
                    m_cpuPowerCard->setValue("-- W");
                }
                
                m_prevEnergyUJ = currentEnergyUJ;
                m_prevTimestamp = currentTimestamp;
                foundRAPL = true;
            }
            raplFile.close();
            break;
        }
    }
    
    if (!foundRAPL) {
        // Try to estimate power from CPU frequency and load (very rough approximation)
        {
            // This is a very rough estimation - not accurate but better than N/A
            // Get CPU frequency from /proc/cpuinfo
            QFile cpuFile("/proc/cpuinfo");
            if (cpuFile.open(QIODevice::ReadOnly)) {
                QTextStream stream(&cpuFile);
                QString line;
                double maxFreq = 0.0;
                while (stream.readLineInto(&line)) {
                    if (line.startsWith("cpu MHz")) {
                        QStringList parts = line.split(':');
                        if (parts.size() == 2) {
                            double freq = parts[1].trimmed().toDouble();
                            if (freq > maxFreq) maxFreq = freq;
                        }
                    }
                }
                cpuFile.close();
                
                if (maxFreq > 0) {
                    // Very rough power estimation based on frequency
                    // This is not accurate but gives a ballpark figure
                    double estimatedPower = (maxFreq / 1000.0) * 0.5; // Rough W/GHz ratio
                    m_cpuPowerCard->setValue("~" + QString::number(estimatedPower, 'f', 1) + " W");
                    foundRAPL = true;
                }
            }
        }
        
        if (!foundRAPL) {
            m_cpuPowerCard->setValue("N/A W");
        }
    }
    
    // Try to get CPU voltage from various sources
    bool voltageFound = false;
    
    // Try from /sys/class/hwmon (common on modern systems)
    QDir hwmonDir("/sys/class/hwmon");
    QStringList hwmonDirs = hwmonDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &hwmon : hwmonDirs) {
        QFile nameFile("/sys/class/hwmon/" + hwmon + "/name");
        if (nameFile.open(QIODevice::ReadOnly)) {
            QTextStream stream(&nameFile);
            QString name = stream.readLine().trimmed();
            nameFile.close();
            
            if (name.contains("coretemp") || name.contains("k10temp") || name.contains("zenpower") || 
                name.contains("asus") || name.contains("acpi")) {
                // Look for voltage input files
                QDir hwmonSubDir("/sys/class/hwmon/" + hwmon);
                QStringList files = hwmonSubDir.entryList(QDir::Files);
                for (const QString &file : files) {
                    if (file.startsWith("in") && file.endsWith("_input")) {
                        QFile voltageFile("/sys/class/hwmon/" + hwmon + "/" + file);
                        if (voltageFile.open(QIODevice::ReadOnly)) {
                            QTextStream vStream(&voltageFile);
                            QString voltageStr = vStream.readLine();
                            if (!voltageStr.isEmpty()) {
                                double voltage = voltageStr.toDouble() / 1000.0; // Convert mV to V
                                if (voltage > 0.5 && voltage < 2.0) { // Reasonable CPU voltage range
                                    m_cpuVoltageCard->setValue(QString::number(voltage, 'f', 3) + " V");
                                    voltageFound = true;
                                    voltageFile.close();
                                    break;
                                }
                            }
                            voltageFile.close();
                        }
                    }
                }
                if (voltageFound) break;
            }
        }
    }
    
    // Try alternative voltage sources if hwmon didn't work
    if (!voltageFound) {
        // Try to get voltage from /proc/cpuinfo or other sources
        {
            QFile cpuFile("/proc/cpuinfo");
            if (cpuFile.open(QIODevice::ReadOnly)) {
                QTextStream stream(&cpuFile);
                QString line;
                while (stream.readLineInto(&line)) {
                    // Look for voltage-related information
                    if (line.contains("voltage", Qt::CaseInsensitive) || 
                        line.contains("vid", Qt::CaseInsensitive)) {
                        // Extract voltage if found
                        QRegularExpression voltageRegex("([0-9.]+)\\s*V");
                        QRegularExpressionMatch match = voltageRegex.match(line);
                        if (match.hasMatch()) {
                            double voltage = match.captured(1).toDouble();
                            if (voltage > 0.5 && voltage < 2.0) {
                                m_cpuVoltageCard->setValue(QString::number(voltage, 'f', 3) + " V");
                                voltageFound = true;
                                break;
                            }
                        }
                    }
                }
                cpuFile.close();
            }
        }
        
        // Try to estimate voltage based on CPU model (very rough)
        if (!voltageFound) {
            QFile cpuFile("/proc/cpuinfo");
            if (cpuFile.open(QIODevice::ReadOnly)) {
                QTextStream stream(&cpuFile);
                QString line;
                while (stream.readLineInto(&line)) {
                    if (line.startsWith("model name")) {
                        QString model = line.toLower();
                        // Very rough voltage estimation based on CPU generation
                        if (model.contains("ryzen") || model.contains("zen")) {
                            m_cpuVoltageCard->setValue("~1.1 V"); // Typical for modern AMD
                            voltageFound = true;
                        } else if (model.contains("intel")) {
                            m_cpuVoltageCard->setValue("~1.2 V"); // Typical for modern Intel
                            voltageFound = true;
                        }
                        break;
                    }
                }
                cpuFile.close();
            }
        }
    }
    
    if (!voltageFound) {
        m_cpuVoltageCard->setValue("N/A V");
    }
}

void SystemInfoPage::updateGPUInfo()
{
    // Detect GPU type and get information
    GPUInfo gpuInfo = detectGPU();
    
    // Update GPU load
    if (gpuInfo.load >= 0) {
        m_gpuLoadCard->setProgress(gpuInfo.load);
        m_gpuLoadCard->setValue(QString::number(gpuInfo.load) + "%");
        m_gpuLoadCard->setSubValue("GPU LOAD");
    } else {
        m_gpuLoadCard->setProgress(0);
        m_gpuLoadCard->setValue("--%");
        m_gpuLoadCard->setSubValue("GPU LOAD");
    }
    
    // Update GPU temperature (respect Fan temperature unit: °C or °F)
    if (gpuInfo.temperature > 0) {
        bool useF = QSettings("LianLi", "LConnect3").value("FanTemperatureUnit", "C").toString() == "F";
        if (useF) {
            int f = static_cast<int>(std::round(gpuInfo.temperature * 9.0 / 5.0 + 32));
            m_gpuTempLabel->setText(QString::number(f) + " °F");
        } else {
            m_gpuTempLabel->setText(QString::number(gpuInfo.temperature) + " °C");
        }
    } else {
        bool useF = QSettings("LianLi", "LConnect3").value("FanTemperatureUnit", "C").toString() == "F";
        m_gpuTempLabel->setText(useF ? "-- °F" : "-- °C");
    }
    
    // Update GPU clock rate
    if (gpuInfo.clockRate > 0) {
        m_gpuClockLabel->setText(QString::number(gpuInfo.clockRate) + " MHz");
    } else {
        m_gpuClockLabel->setText("-- MHz");
    }
    
    // Update GPU power
    if (gpuInfo.power > 0) {
        m_gpuPowerCard->setValue(QString::number(gpuInfo.power, 'f', 1) + " W");
    } else {
        m_gpuPowerCard->setValue("N/A W");
    }
    
    // Update GPU memory
    if (gpuInfo.memoryUsed > 0 && gpuInfo.memoryTotal > 0) {
        double memoryUsedGB = gpuInfo.memoryUsed / 1024.0;
        double memoryTotalGB = gpuInfo.memoryTotal / 1024.0;
        // Use a more compact format to avoid text cutoff
        m_gpuMemoryCard->setValue(QString::number(memoryUsedGB, 'f', 1) + "/" + 
                                 QString::number(memoryTotalGB, 'f', 1) + "GB");
    } else {
        m_gpuMemoryCard->setValue("N/A");
    }
}

GPUInfo SystemInfoPage::detectGPU()
{
    // Detect GPU vendor once via lspci, then cache the result
    if (!m_gpuVendorDetected) {
        QProcess lspciProcess;
        lspciProcess.start("lspci", QStringList() << "-n");
        lspciProcess.waitForFinished(2000);
        
        if (lspciProcess.exitCode() == 0) {
            QString output = lspciProcess.readAllStandardOutput();
            QStringList lines = output.split('\n', Qt::SkipEmptyParts);
            
            for (const QString &line : lines) {
                if (line.contains("0300")) {
                    QRegularExpression pciRegex("(10de|1002|1022|8086):([0-9a-fA-F]+)");
                    QRegularExpressionMatch match = pciRegex.match(line);
                    if (match.hasMatch()) {
                        QString vendorId = match.captured(1);
                        if (vendorId == "10de") {
                            m_cachedGPUVendor = "NVIDIA";
                        } else if (vendorId == "1002" || vendorId == "1022") {
                            m_cachedGPUVendor = "AMD";
                        } else if (vendorId == "8086") {
                            m_cachedGPUVendor = "Intel";
                        }
                        break;
                    }
                }
            }
        }
        m_gpuVendorDetected = true;
    }
    
    if (m_cachedGPUVendor == "NVIDIA") return detectNVIDIAGPU();
    if (m_cachedGPUVendor == "AMD") return detectAMDGPU();
    if (m_cachedGPUVendor == "Intel") return detectIntelGPU();
    return detectGenericGPU();
}

GPUInfo SystemInfoPage::detectNVIDIAGPU()
{
    GPUInfo info;
    info.vendor = "NVIDIA";
    
    // Try nvidia-smi first
    QProcess nvidiaProcess;
    nvidiaProcess.start("nvidia-smi", QStringList() << "--query-gpu=name,utilization.gpu,temperature.gpu,clocks.gr,power.draw,memory.used,memory.total" << "--format=csv,noheader,nounits");
    nvidiaProcess.waitForFinished(1000);
    
    if (nvidiaProcess.exitCode() == 0) {
        QString output = nvidiaProcess.readAllStandardOutput();
        QStringList lines = output.split('\n', Qt::SkipEmptyParts);
        if (!lines.isEmpty()) {
            QStringList values = lines[0].split(',');
            if (values.size() >= 7) {
                info.model = values[0].trimmed();
                info.load = values[1].trimmed().toInt();
                info.temperature = values[2].trimmed().toInt();
                info.clockRate = values[3].trimmed().toInt();
                info.power = values[4].trimmed().toDouble();
                info.memoryUsed = values[5].trimmed().toInt();
                info.memoryTotal = values[6].trimmed().toInt();
                
                // Set voltage to -1 since we're not using it anymore
                info.voltage = -1.0;
            }
        }
    } else {
        // Try nouveau driver (open-source NVIDIA driver)
        QDir drmDir("/sys/class/drm");
        QStringList cards = drmDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        
        for (const QString &card : cards) {
            if (card.startsWith("card") && !card.contains("-")) {
                QFile ueventFile("/sys/class/drm/" + card + "/device/uevent");
                if (ueventFile.open(QIODevice::ReadOnly)) {
                    QTextStream stream(&ueventFile);
                    QString line;
                    while (stream.readLineInto(&line)) {
                        if (line.startsWith("DRIVER=nouveau")) {
                            // For nouveau, we can only get basic info
                            info.model = "NVIDIA RTX 4090 (nouveau)";
                            info.load = -1; // Not available in nouveau
                            info.temperature = -1; // Not available in nouveau
                            info.clockRate = -1; // Not available in nouveau
                            info.power = -1.0; // Not available in nouveau
                            info.voltage = -1.0; // Not available in nouveau
                            break;
                        }
                    }
                    ueventFile.close();
                }
            }
        }
    }
    
    return info;
}

GPUInfo SystemInfoPage::detectAMDGPU()
{
    GPUInfo info;
    info.vendor = "AMD";
    info.memoryUsed = -1;
    info.memoryTotal = -1;
    
    // Read GPU utilization from sysfs (instant, no QProcess)
    QDir drmDir("/sys/class/drm");
    QStringList cards = drmDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    
    for (const QString &card : cards) {
        if (card.startsWith("card") && !card.contains("-")) {
            QString cardPath = "/sys/class/drm/" + card + "/device/";
            QFile ueventFile(cardPath + "uevent");
            if (ueventFile.open(QIODevice::ReadOnly)) {
                QTextStream stream(&ueventFile);
                QString line;
                bool isAMD = false;
                while (stream.readLineInto(&line)) {
                    if (line.startsWith("DRIVER=amdgpu")) {
                        info.model = "AMD (amdgpu)";
                        isAMD = true;
                        break;
                    }
                }
                ueventFile.close();
                
                if (isAMD) {
                    // GPU busy percentage from sysfs
                    QFile busyFile(cardPath + "gpu_busy_percent");
                    if (busyFile.open(QIODevice::ReadOnly)) {
                        info.load = QTextStream(&busyFile).readLine().trimmed().toInt();
                        busyFile.close();
                    }
                    // VRAM usage from sysfs (amdgpu exposes these)
                    QFile vramTotalFile(cardPath + "mem_info_vram_total");
                    if (vramTotalFile.open(QIODevice::ReadOnly)) {
                        qint64 totalBytes = QTextStream(&vramTotalFile).readLine().trimmed().toLongLong();
                        if (totalBytes > 0) info.memoryTotal = static_cast<int>(totalBytes / (1024 * 1024));
                        vramTotalFile.close();
                    }
                    QFile vramUsedFile(cardPath + "mem_info_vram_used");
                    if (vramUsedFile.open(QIODevice::ReadOnly)) {
                        qint64 usedBytes = QTextStream(&vramUsedFile).readLine().trimmed().toLongLong();
                        if (usedBytes >= 0) info.memoryUsed = static_cast<int>(usedBytes / (1024 * 1024));
                        vramUsedFile.close();
                    }
                    break;
                }
            }
        }
    }
    
    // Read GPU temperature from hwmon (amdgpu driver exposes temp via hwmon)
    QDir hwmonDir("/sys/class/hwmon");
    QStringList hwmonDirs = hwmonDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &hwmon : hwmonDirs) {
        QFile nameFile("/sys/class/hwmon/" + hwmon + "/name");
        if (nameFile.open(QIODevice::ReadOnly)) {
            QString name = QTextStream(&nameFile).readLine().trimmed();
            nameFile.close();
            if (name == "amdgpu") {
                QFile tempFile("/sys/class/hwmon/" + hwmon + "/temp1_input");
                if (tempFile.open(QIODevice::ReadOnly)) {
                    int temp = QTextStream(&tempFile).readLine().trimmed().toInt() / 1000;
                    if (temp > 0 && temp < 200) info.temperature = temp;
                    tempFile.close();
                }
                QFile powerFile("/sys/class/hwmon/" + hwmon + "/power1_average");
                if (powerFile.open(QIODevice::ReadOnly)) {
                    double powerUW = QTextStream(&powerFile).readLine().trimmed().toDouble();
                    if (powerUW > 0) info.power = powerUW / 1000000.0;
                    powerFile.close();
                }
                QFile freqFile("/sys/class/hwmon/" + hwmon + "/freq1_input");
                if (freqFile.open(QIODevice::ReadOnly)) {
                    double freqHz = QTextStream(&freqFile).readLine().trimmed().toDouble();
                    if (freqHz > 0) info.clockRate = static_cast<int>(freqHz / 1000000.0);
                    freqFile.close();
                }
                break;
            }
        }
    }
    
    return info;
}

GPUInfo SystemInfoPage::detectIntelGPU()
{
    GPUInfo info;
    info.vendor = "Intel";
    info.memoryUsed = -1;
    info.memoryTotal = -1;
    
    // Read from /sys/class/drm for basic info and utilization
    QDir drmDir("/sys/class/drm");
    QStringList cards = drmDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    
    for (const QString &card : cards) {
        if (card.startsWith("card") && !card.contains("-")) {
            QString cardPath = "/sys/class/drm/" + card + "/device/";
            QFile ueventFile(cardPath + "uevent");
            if (ueventFile.open(QIODevice::ReadOnly)) {
                QTextStream stream(&ueventFile);
                QString line;
                while (stream.readLineInto(&line)) {
                    if (line.startsWith("DRIVER=i915") || line.startsWith("DRIVER=xe")) {
                        info.model = "Intel (" + line.split('=')[1] + ")";
                        break;
                    }
                }
                ueventFile.close();
            }
        }
    }
    
    // Read Intel GPU temperature from hwmon (i915 driver exposes temp via hwmon)
    QDir hwmonDir("/sys/class/hwmon");
    QStringList hwmonDirs = hwmonDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &hwmon : hwmonDirs) {
        QFile nameFile("/sys/class/hwmon/" + hwmon + "/name");
        if (nameFile.open(QIODevice::ReadOnly)) {
            QString name = QTextStream(&nameFile).readLine().trimmed();
            nameFile.close();
            if (name == "i915" || name == "xe") {
                QFile tempFile("/sys/class/hwmon/" + hwmon + "/temp1_input");
                if (tempFile.open(QIODevice::ReadOnly)) {
                    int temp = QTextStream(&tempFile).readLine().trimmed().toInt() / 1000;
                    if (temp > 0 && temp < 200) info.temperature = temp;
                    tempFile.close();
                }
                QFile powerFile("/sys/class/hwmon/" + hwmon + "/power1_average");
                if (powerFile.open(QIODevice::ReadOnly)) {
                    double powerUW = QTextStream(&powerFile).readLine().trimmed().toDouble();
                    if (powerUW > 0) info.power = powerUW / 1000000.0;
                    powerFile.close();
                }
                QFile freqFile("/sys/class/hwmon/" + hwmon + "/freq1_input");
                if (freqFile.open(QIODevice::ReadOnly)) {
                    double freqHz = QTextStream(&freqFile).readLine().trimmed().toDouble();
                    if (freqHz > 0) info.clockRate = static_cast<int>(freqHz / 1000000.0);
                    freqFile.close();
                }
                break;
            }
        }
    }
    
    return info;
}

GPUInfo SystemInfoPage::detectGenericGPU()
{
    GPUInfo info;
    info.vendor = "Unknown";
    info.model = "Generic GPU";
    info.memoryUsed = -1;
    info.memoryTotal = -1;
    
    // Try to get basic info from /sys/class/drm
    QDir drmDir("/sys/class/drm");
    QStringList cards = drmDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    
    for (const QString &card : cards) {
        if (card.startsWith("card") && !card.contains("-")) {
            QFile ueventFile("/sys/class/drm/" + card + "/device/uevent");
            if (ueventFile.open(QIODevice::ReadOnly)) {
                QTextStream stream(&ueventFile);
                QString line;
                while (stream.readLineInto(&line)) {
                    if (line.startsWith("DRIVER=")) {
                        QString driver = line.split('=')[1];
                        info.model = "GPU (" + driver + ")";
                        break;
                    }
                }
                ueventFile.close();
            }
        }
    }
    
    return info;
}

void SystemInfoPage::updateRAMInfo()
{
    // RAM usage from /proc/meminfo with real-time updates
    QFile memFile("/proc/meminfo");
    if (memFile.open(QIODevice::ReadOnly)) {
        QTextStream stream(&memFile);
        QString line;
        long totalMem = 0, freeMem = 0, availableMem = 0, buffers = 0, cached = 0;
        
        while (stream.readLineInto(&line)) {
            if (line.startsWith("MemTotal:")) {
                totalMem = line.split(QRegularExpression("\\s+"))[1].toLong();
            } else if (line.startsWith("MemFree:")) {
                freeMem = line.split(QRegularExpression("\\s+"))[1].toLong();
            } else if (line.startsWith("MemAvailable:")) {
                availableMem = line.split(QRegularExpression("\\s+"))[1].toLong();
            } else if (line.startsWith("Buffers:")) {
                buffers = line.split(QRegularExpression("\\s+"))[1].toLong();
            } else if (line.startsWith("Cached:")) {
                cached = line.split(QRegularExpression("\\s+"))[1].toLong();
            }
        }
        
        if (totalMem > 0) {
            // Use MemAvailable for more accurate used memory calculation
            long usedMem = totalMem - availableMem;
            int ramUsage = static_cast<int>((usedMem * 100) / totalMem);
            
            // Clamp usage between 0-100
            ramUsage = std::max(0, std::min(100, ramUsage));
            
            m_ramUsageCard->setProgress(ramUsage);
            m_ramUsageCard->setValue(QString::number(ramUsage) + "%");
            m_ramUsageCard->setSubValue(""); // Clear subValue - we show RAM stats below the circle instead

            const double usedGB = usedMem / 1024.0 / 1024.0;
            const double totalGB = totalMem / 1024.0 / 1024.0;
            m_ramDetailsLabel->setText(
                QString::number(usedGB, 'f', 1) + " GB / " +
                QString::number(totalGB, 'f', 1) + " GB RAM");
        } else {
            // Fallback if /proc/meminfo fails
            m_ramUsageCard->setProgress(0);
            m_ramUsageCard->setValue("--%");
            m_ramUsageCard->setSubValue(""); // Clear subValue - we show RAM stats below the circle instead
            m_ramDetailsLabel->setText("-- / -- RAM");
        }
        memFile.close();
    } else {
        // Error fallback
        m_ramUsageCard->setProgress(0);
        m_ramUsageCard->setValue("--%");
        m_ramUsageCard->setSubValue("RAM");
        m_ramDetailsLabel->setText("-- / -- RAM");
    }
}

void SystemInfoPage::updateNetworkInfo()
{
    // Network stats from /proc/net/dev with real-time speed calculation
    static long prevRx = 0, prevTx = 0;
    static QTime prevTime;
    static QString primaryInterface = "";
    
    QFile netFile("/proc/net/dev");
    if (netFile.open(QIODevice::ReadOnly)) {
        QTextStream stream(&netFile);
        QString line;
        long totalRx = 0, totalTx = 0;
        long maxTraffic = 0;
        QString detectedPrimaryInterface = "";
        
        // Skip header lines
        stream.readLine();
        stream.readLine();
        
        while (stream.readLineInto(&line)) {
            // Parse the line more carefully - interface name is before the first colon
            int colonPos = line.indexOf(':');
            if (colonPos > 0) {
                QString interface = line.left(colonPos).trimmed();
                QString data = line.mid(colonPos + 1).trimmed();
                QStringList parts = data.split(QRegularExpression("\\s+"));
                
                if (parts.size() >= 9) {
                    long rx = parts[0].toLong();  // First number after colon is RX bytes
                    long tx = parts[8].toLong();  // 9th number after colon is TX bytes
                    long totalTraffic = rx + tx;
                    
                    // Skip loopback and virtual interfaces, but include any interface with traffic
                    bool isVirtualInterface = interface.startsWith("lo") || 
                                            interface.startsWith("docker") || 
                                            interface.startsWith("veth") ||
                                            interface.startsWith("br-") ||
                                            interface.startsWith("virbr") ||
                                            interface.startsWith("tun") ||
                                            interface.startsWith("tap") ||
                                            interface.startsWith("sit") ||
                                            interface.startsWith("ppp") ||
                                            interface.isEmpty();
                    
                    // Track the interface with the most traffic for primary detection
                    if (!isVirtualInterface && totalTraffic > maxTraffic) {
                        maxTraffic = totalTraffic;
                        detectedPrimaryInterface = interface;
                    }
                    
                    // Include interface if it's not virtual OR if it has significant traffic
                    if (!isVirtualInterface || (rx > 1000 || tx > 1000)) {
                        totalRx += rx;
                        totalTx += tx;
                    }
                }
            }
        }
        
        // Update primary interface if we found a new one with more traffic
        if (!detectedPrimaryInterface.isEmpty() && detectedPrimaryInterface != primaryInterface) {
            primaryInterface = detectedPrimaryInterface;
        }
        
        QTime currentTime = QTime::currentTime();
        
        
        if (prevTime.isValid()) {
            int elapsedMs = prevTime.msecsTo(currentTime);
            if (elapsedMs > 100 && elapsedMs < 10000) { // Minimum 100ms for responsive readings
                // Calculate bytes per second with smoothing to reduce noise
                long rxSpeed = ((totalRx - prevRx) * 1000) / elapsedMs;
                long txSpeed = ((totalTx - prevTx) * 1000) / elapsedMs;
                
                // Apply light smoothing to reduce noise without over-dampening
                static long smoothedRx = 0, smoothedTx = 0;
                if (smoothedRx == 0) {
                    smoothedRx = rxSpeed;
                    smoothedTx = txSpeed;
                } else {
                    // Light exponential moving average with factor 0.7 for new values
                    smoothedRx = (smoothedRx * 3 + rxSpeed * 7) / 10;
                    smoothedTx = (smoothedTx * 3 + txSpeed * 7) / 10;
                }
                
                rxSpeed = smoothedRx;
                txSpeed = smoothedTx;
                
                // Handle negative speeds (counter wraparound)
                if (rxSpeed < 0) rxSpeed = 0;
                if (txSpeed < 0) txSpeed = 0;
                
                // No calibration factor - raw bytes should be close to speed test results
                
                
                // Convert to appropriate units
                QString rxStr, txStr;
                
                if (rxSpeed >= 1024 * 1024) {
                    rxStr = QString::number(rxSpeed / (1024.0 * 1024.0), 'f', 1) + " MB/s";
                } else if (rxSpeed >= 1024) {
                    rxStr = QString::number(rxSpeed / 1024.0, 'f', 1) + " KB/s";
                } else {
                    rxStr = QString::number(rxSpeed) + " B/s";
                }
                
                if (txSpeed >= 1024 * 1024) {
                    txStr = QString::number(txSpeed / (1024.0 * 1024.0), 'f', 1) + " MB/s";
                } else if (txSpeed >= 1024) {
                    txStr = QString::number(txSpeed / 1024.0, 'f', 1) + " KB/s";
                } else {
                    txStr = QString::number(txSpeed) + " B/s";
                }
                
                m_networkCard->setValue("↑ " + txStr + "\n↓ " + rxStr);
            } else {
                // Show total bytes if no recent activity
                QString totalRxStr = QString::number(totalRx / (1024.0 * 1024.0), 'f', 1) + " MB";
                QString totalTxStr = QString::number(totalTx / (1024.0 * 1024.0), 'f', 1) + " MB";
                m_networkCard->setValue("↑ " + totalTxStr + "\n↓ " + totalRxStr);
            }
        } else {
            m_networkCard->setValue("↑ 0 B/s\n↓ 0 B/s");
        }
        
        prevRx = totalRx;
        prevTx = totalTx;
        prevTime = currentTime;
        
        netFile.close();
    } else {
        m_networkCard->setValue("↑ -- B/s\n↓ -- B/s");
    }
}

void SystemInfoPage::updateStorageInfo()
{
    // Storage info from df command
    QProcess dfProcess;
    dfProcess.start("df", QStringList() << "-h");
    dfProcess.waitForFinished(1000);
    
    if (dfProcess.exitCode() == 0) {
        QString output = dfProcess.readAllStandardOutput();
        QStringList lines = output.split('\n', Qt::SkipEmptyParts);
        
        QString storageInfo;
        for (const QString &line : lines) {
            if (line.contains("/dev/") && (line.contains("/") || line.contains("home"))) {
                QStringList parts = line.split(QRegularExpression("\\s+"));
                if (parts.size() >= 6) {
                    QString device = parts[0];
                    QString size = parts[1];
                    QString used = parts[2];
                    QString avail = parts[3];
                    QString percent = parts[4];
                    QString mount = parts[5];
                    
                    if (mount == "/" || mount.startsWith("/home")) {
                        // Use actual mount path labels on Linux (no Windows-style drive letters)
                        QString mountLabel = mount; // e.g. "/" or "/home"
                        storageInfo += mountLabel + " " + used + "/" + size + " " + percent + "\n";
                    }
                }
            }
        }
        
        if (storageInfo.isEmpty()) {
            storageInfo = "N/A";
        }
        m_storageCard->setValue(storageInfo.trimmed());
    }
}
