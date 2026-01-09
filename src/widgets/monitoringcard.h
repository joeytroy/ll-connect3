#ifndef MONITORINGCARD_H
#define MONITORINGCARD_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QProgressBar>
#include <QPropertyAnimation>

class MonitoringCard : public QFrame
{
    Q_OBJECT
    Q_PROPERTY(qreal progressValue READ progressValue WRITE setProgressValue)

public:
    enum CardType {
        CircularProgress,
        RectangularValue,
        TemperatureBar,
        NetworkSpeed,
        StorageInfo
    };

    explicit MonitoringCard(CardType type, const QString &title, QWidget *parent = nullptr);
    
    void setValue(const QString &value);
    void setSubValue(const QString &subValue);
    void setProgress(int percentage);
    void setColor(const QColor &color);
    void setIcon(const QString &iconText);
    void setCircularSize(int widgetSize, int canvasSize);

    qreal progressValue() const { return m_progressValue; }
    void setProgressValue(qreal value);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void setupUI();
    CardType m_type;
    QVBoxLayout *m_layout;
    QHBoxLayout *m_headerLayout;
    
    QLabel *m_iconLabel;
    QLabel *m_titleLabel;
    QLabel *m_valueLabel;
    QLabel *m_subValueLabel;
    QProgressBar *m_progressBar;
    QWidget *m_progressCanvas;
    
    QColor m_cardColor;
    qreal m_progressValue;
    QPropertyAnimation *m_progressAnimation;
    QString m_valueText;
    QString m_subValueText;
};

#endif // MONITORINGCARD_H
