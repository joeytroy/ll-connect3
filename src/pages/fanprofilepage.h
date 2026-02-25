#ifndef FANPROFILEPAGE_H
#define FANPROFILEPAGE_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QSlider>
#include <QSpinBox>
#include <QTableWidget>
#include <QGroupBox>
#include <QRadioButton>
#include <QCheckBox>
#include <QWidget>
#include "widgets/fancurvewidget.h"
#include "usb/lian_li_sl_infinity_controller.h"

class FanProfilePage : public QWidget
{
    Q_OBJECT

public:
    explicit FanProfilePage(QWidget *parent = nullptr);

private slots:
    void onProfileChanged();
    void onDefaultClicked();
    void onApplyToAllClicked();
    void updateFanData();
    void onCurvePointsChanged(const QVector<QPointF> &points);
    void onPortSelectionChanged();
    void onFanSizeChanged(int port);
    void onRenameCustomProfile(int profileNum);
    void onSensorSourceChanged(int index);
    void onTargetTemperatureChanged(int value);
    void onTargetRPMChanged(int value);
    void onPortNameEdited(QTableWidgetItem *item);

private:
    void setupUI();
    void setupFanTable();
    void setupFanCurve();
    void setupControls();
    void updateFanCurve();
    void updateTemperature();
    void updateFanRPMs();
    void updateCPULoad();
    void updateGPULoad();
    int calculateRPMForTemperature(int temperature);
    int calculateRPMForLoad(int temperature, int cpuLoad, int gpuLoad);
    int getRealCPUTemperature();
    int getRealCPULoad();
    int getRealGPULoad();
    int getRealGPUTemperature();
    QVector<int> getRealFanRPMs();
    int getRealFanRPM(int port);
    int convertPercentageToRPM(int percentage);
    void controlFanSpeeds();
    void setFanSpeed(int port, int speedPercent);
    void updateFanTable();
    bool isPortConnected(int port);
    QColor getTemperatureColor(int temperature);
    void saveCustomCurves();
    void loadCustomCurves();
    void saveCustomProfiles();
    void loadCustomProfiles();
    void savePortProfiles();
    void loadPortProfiles();
    void savePortTargets();
    void loadPortTargets();
    void savePortNames();
    void loadPortNames();
    QVector<QPointF> getDefaultCurveForProfile(const QString &profile);
    int calculateRPMForCustomCurve(int port, int temperature);
    QString getCurrentProfile();
    QString getInternalProfileName(const QString &displayName);
    bool useFahrenheit() const;
    // Fan detection functions removed - configuration is now in Settings
    
    QVBoxLayout *m_mainLayout;
    QVBoxLayout *m_contentLayout;
    QVBoxLayout *m_leftLayout;
    QVBoxLayout *m_rightLayout;
    
    // Fan table
    QTableWidget *m_fanTable;
    QVector<QComboBox*> m_fanSizeComboBoxes; // Size dropdown for each port
    
    // Fan curve
    FanCurveWidget *m_fanCurveWidget;
    
    // Controls
    QGroupBox *m_profileGroup;
    QRadioButton *m_quietRadio;
    QRadioButton *m_stdSpRadio;
    QRadioButton *m_highSpRadio;
    QRadioButton *m_fullSpRadio;
    QRadioButton *m_custom1Radio;
    QRadioButton *m_custom2Radio;
    QRadioButton *m_custom3Radio;
    
    QPushButton *m_applyToAllButton;
    QPushButton *m_defaultButton;
    
    // New control section: sensor source + numeric temp/RPM editor
    QComboBox *m_sensorSourceCombo;
    QSpinBox  *m_targetTempSpin;
    QSpinBox  *m_targetRpmSpin;
    
    // Current selected port (1-4)
    int m_selectedPort;
    
    // Per-port custom curves (port 1-4 -> curve points)
    QMap<int, QVector<QPointF>> m_customCurves;
    
    // Per-port profile names (port 1-4 -> profile name like "Quiet", "StdSP", etc.)
    QMap<int, QString> m_portProfiles;
    
    // Per-port target temperature/RPM (for numeric editor)
    QMap<int, int> m_portTargetTemp;
    QMap<int, int> m_portTargetRpm;

    // Per-port custom display names (e.g. "CPU Rad", "GPU Rad")
    QMap<int, QString> m_portNames;
    
    // Custom profile names and base curves
    QMap<int, QString> m_customProfileNames; // Profile 1-3 -> custom name
    QMap<int, QVector<QPointF>> m_customProfileCurves; // Profile 1-3 -> base curve
    
    // Update timers
    QTimer *m_updateTimer;
    QTimer *m_tempUpdateTimer;
    QTimer *m_fanRPMTimer;
    QTimer *m_cpuLoadTimer;
    QTimer *m_gpuLoadTimer;
    
    // Cached temperature for real-time updates
    int m_cachedTemperature;
    int m_temperatureCounter;
    
    // Cached CPU and GPU load
    int m_cachedCPULoad;
    int m_cachedGPULoad;
    
    // Cached fan RPMs
    QVector<int> m_cachedFanRPMs;
    
    // Port detection
    QVector<bool> m_portConnected;
    QVector<int> m_activePorts;
    
    // Fan size per port (120mm = 2100 RPM, 140mm = 1600 RPM)
    QMap<int, int> m_fanSizeMaxRPM;
    
    // HID controller for fan control
    LianLiSLInfinityController *m_hidController;
};

#endif // FANPROFILEPAGE_H
