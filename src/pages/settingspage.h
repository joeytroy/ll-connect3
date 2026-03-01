#ifndef SETTINGSPAGE_H
#define SETTINGSPAGE_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QCheckBox>
#include <QSpinBox>
#include <QGroupBox>
#include <QLineEdit>
#include <QNetworkAccessManager>

class LightingPage;

class SettingsPage : public QWidget
{
    Q_OBJECT

public:
    explicit SettingsPage(QWidget *parent = nullptr);
    void setLightingPage(LightingPage *lightingPage);

signals:
    void minimizeOnStartupChanged(bool enabled);

private slots:
    void onResetAll();
    void onFanPortToggled(int port, bool enabled);

private:
    void setupUI();
    void setupBehaviorSettings();
    void setupFanConfiguration();
    void setupDebugSettings();
    void loadFanConfiguration();
    void saveFanConfiguration();
    
    QVBoxLayout *m_mainLayout;
    QHBoxLayout *m_headerLayout;
    QHBoxLayout *m_contentLayout;
    QVBoxLayout *m_leftLayout;
    QVBoxLayout *m_rightLayout;
    
    // Header
    QLabel *m_titleLabel;

    // Behavior settings
    QGroupBox *m_behaviorGroup;
    QCheckBox *m_minimizeOnStartupCheck;
    QCheckBox *m_checkForUpdatesCheck;
    QComboBox *m_fanTempUnitCombo;
    
    // Fan configuration
    QGroupBox *m_fanConfigGroup;
    QCheckBox *m_fanPort1Check;
    QCheckBox *m_fanPort2Check;
    QCheckBox *m_fanPort3Check;
    QCheckBox *m_fanPort4Check;
    
    // Debug settings
    QGroupBox *m_debugGroup;
    QCheckBox *m_debugModeCheck;
    QCheckBox *m_debugFanSpeedsCheck;
    QCheckBox *m_debugFanLightsCheck;
    QCheckBox *m_kernelLoggingCheck;
    
    // Action buttons
    QPushButton *m_resetAllBtn;
    
    // Reference to lighting page for reset
    LightingPage *m_lightingPage;

    void writeKernelLoggingFlag(bool enabled);

    // Version check
    QNetworkAccessManager *m_networkManager;
    void checkForUpdates(bool silent);

public:
    void checkForUpdatesOnStartup();
};

#endif // SETTINGSPAGE_H
