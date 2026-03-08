#ifndef SYSTEMINFOPAGE_H
#define SYSTEMINFOPAGE_H

#include <QWidget>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QProgressBar>
#include <QTimer>

class MonitoringCard;

struct GPUInfo {
    int load = -1;        // GPU utilization percentage
    int temperature = -1; // GPU temperature in Celsius
    int clockRate = -1;   // GPU clock rate in MHz
    double power = -1.0;  // GPU power consumption in watts
    double voltage = -1.0; // GPU voltage in volts
    int memoryUsed = -1;  // GPU memory used in MB
    int memoryTotal = -1; // GPU total memory in MB
    QString vendor;       // GPU vendor (NVIDIA, AMD, Intel, etc.)
    QString model;        // GPU model name
};

class SystemInfoPage : public QWidget
{
    Q_OBJECT

public:
    explicit SystemInfoPage(QWidget *parent = nullptr);

private slots:
    void updateSystemInfo();

private:
    void setupUI();
    void createMonitoringCards();
    void updateCPUInfo();
    void updateCPUPowerAndVoltage();
    void updateGPUInfo();
    GPUInfo detectGPU();
    GPUInfo detectNVIDIAGPU();
    GPUInfo detectAMDGPU();
    GPUInfo detectIntelGPU();
    GPUInfo detectGenericGPU();
    void updateRAMInfo();
    void updateNetworkInfo();
    void updateStorageInfo();
    
    QVBoxLayout *m_mainLayout;
    QHBoxLayout *m_headerLayout;
    QGridLayout *m_contentGrid;
    
    // Header
    QLabel *m_titleLabel;
    QLabel *m_floatingWindowLabel;
    
    // Monitoring cards
    MonitoringCard *m_cpuLoadCard;
    MonitoringCard *m_cpuPowerCard;
    MonitoringCard *m_cpuVoltageCard;
    
    // CPU real-time data labels
    QLabel *m_cpuTempLabel;
    QLabel *m_cpuClockLabel;
    
    // GPU real-time data labels
    QLabel *m_gpuTempLabel;
    QLabel *m_gpuClockLabel;
    
    MonitoringCard *m_gpuLoadCard;
    MonitoringCard *m_gpuPowerCard;
    MonitoringCard *m_gpuMemoryCard;
    
    MonitoringCard *m_ramUsageCard;
    QLabel *m_ramDetailsLabel;
    
    MonitoringCard *m_networkCard;
    MonitoringCard *m_storageCard;
    
    // Update timer
    QTimer *m_updateTimer;
    
    // Cached GPU vendor to avoid repeated lspci calls
    QString m_cachedGPUVendor;
    bool m_gpuVendorDetected = false;
    
    // Counter for staggering heavy (process-based) updates
    int m_slowUpdateCounter = 0;
    
    // CPU power monitoring variables
    static qint64 m_prevEnergyUJ;
    static qint64 m_prevTimestamp;
};

#endif // SYSTEMINFOPAGE_H
