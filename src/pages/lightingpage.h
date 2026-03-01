#ifndef LIGHTINGPAGE_H
#define LIGHTINGPAGE_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QSlider>
#include <QGroupBox>
#include <QCheckBox>

class CustomSlider;
class LianLiQtIntegration;

class LightingPage : public QWidget
{
    Q_OBJECT

public:
    explicit LightingPage(QWidget *parent = nullptr);
    void resetToDefaults();

protected:
    void showEvent(QShowEvent *event) override;

private slots:
    void onEffectChanged();
    void onSpeedChanged(int value);
    void onBrightnessChanged(int value);
    void onDirectionChanged();
    void onApply();
    void onDeviceConnected();
    void onDeviceDisconnected();
    void onColorButtonClicked();

private:
    void setupUI();
    void setupControls();
    void updateColorButton(int portIndex);
    void saveLightingSettings();
    void loadLightingSettings();
    void saveEffectColors(const QString &effectName);
    void loadEffectColors(const QString &effectName);
    void loadFanConfiguration();
    void updatePortButtonStates();
    void applyCurrentEffect();
    void clearOldEffectSettings(const QString &oldEffect, const QString &newEffect);
    void updateEffectUI();
    
    bool isPerPortEffect() const;
    int getEffectColorCount() const;
    bool effectHasDirection() const;
    bool effectHasSpeed() const;
    
    QVBoxLayout *m_mainLayout;
    QHBoxLayout *m_contentLayout;
    QVBoxLayout *m_leftLayout;
    QVBoxLayout *m_rightLayout;
    
    // Controls
    QGroupBox *m_lightingGroup;
    QComboBox *m_effectCombo;
    CustomSlider *m_speedSlider;
    CustomSlider *m_brightnessSlider;
    
    QWidget *m_directionWidget;
    QHBoxLayout *m_directionLayout;
    QPushButton *m_leftDirectionBtn;
    QPushButton *m_rightDirectionBtn;
    
    // Static Color specific controls (also used for Breathing and Meteor)
    QWidget *m_staticColorWidget;
    QHBoxLayout *m_colorBoxLayout;
    QLabel *m_colorLabel; // Label that changes for Meteor mode
    QPushButton *m_colorButtons[4]; // One for each port/fan
    QLabel *m_colorLabels[4]; // Labels that change for Meteor mode
    
    QPushButton *m_applyBtn;
    
    
    // Current settings
    QString m_currentEffect;
    int m_currentSpeed;
    int m_currentBrightness;
    bool m_directionLeft;
    QColor m_portColors[4][4]; // [port][color_index] - supports up to 4 colors per port
    bool m_portEnabled[4];  // Which ports have fans connected
    int m_selectedPort; // -1 = none, 0-3 = port index

    bool m_isLoading;  // Guard flag to suppress signals/saves during settings load

    // Lian Li integration
    LianLiQtIntegration *m_lianLi;
};

#endif // LIGHTINGPAGE_H
