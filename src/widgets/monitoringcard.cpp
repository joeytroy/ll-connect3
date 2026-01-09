#include "monitoringcard.h"
#include <QPainter>
#include <QPainterPath>
#include <QFont>
#include <QFontMetrics>
#include <QSizePolicy>
#include <QtGlobal>

MonitoringCard::MonitoringCard(CardType type, const QString &title, QWidget *parent)
    : QFrame(parent)
    , m_type(type)
    , m_layout(nullptr)
    , m_headerLayout(nullptr)
    , m_iconLabel(nullptr)
    , m_titleLabel(nullptr)
    , m_valueLabel(nullptr)
    , m_subValueLabel(nullptr)
    , m_progressBar(nullptr)
    , m_progressCanvas(nullptr)
    , m_cardColor(QColor(45, 166, 255))
    , m_progressValue(0.0)
    , m_progressAnimation(nullptr)
{
    setupUI();
    m_titleLabel->setText(title);
    m_valueText = "--";
    m_subValueText.clear();
    m_valueLabel->setText(m_valueText);
    m_subValueLabel->setText(m_subValueText);
}

void MonitoringCard::setupUI()
{
    setObjectName("monitoringCard");
    setAttribute(Qt::WA_StyledBackground, true);
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setAttribute(Qt::WA_NoSystemBackground, false);
    
    // Set proper size policies based on card type - circular progress should be fixed size on left
    switch (m_type) {
    case CircularProgress:
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        setFixedSize(180, 180);
        // Remove background for circular progress - just show the circle
        setStyleSheet("background: transparent; border: none;");
        break;
    case TemperatureBar:
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        setMinimumSize(120, 80);
        setMaximumHeight(100);
        break;
    default:
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        setMinimumSize(100, 80);
        setMaximumHeight(120); // Increased to accommodate longer text
        break;
    }

    m_layout = new QVBoxLayout(this);
    if (m_type == CircularProgress) {
        m_layout->setContentsMargins(0, 0, 0, 0);
        m_layout->setSpacing(0);
    } else {
        m_layout->setContentsMargins(12, 10, 12, 12);
        m_layout->setSpacing(8);
    }

    // Header with icon and title
    m_headerLayout = new QHBoxLayout();
    m_headerLayout->setContentsMargins(0, 0, 0, 0);
    m_headerLayout->setSpacing(8);

    m_iconLabel = new QLabel();
    m_iconLabel->setObjectName("cardIcon");
    m_iconLabel->setFixedSize(24, 24);
    m_iconLabel->setAlignment(Qt::AlignCenter);
    m_iconLabel->setVisible(false); // Hide the icon for all card types

    m_titleLabel = new QLabel();
    m_titleLabel->setObjectName("cardTitle");
    m_titleLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    m_headerLayout->addWidget(m_iconLabel);
    m_headerLayout->addWidget(m_titleLabel, 1);
    m_headerLayout->addStretch();

    m_layout->addLayout(m_headerLayout);

    // Value display
    m_valueLabel = new QLabel();
    m_valueLabel->setObjectName("cardValue");
    m_valueLabel->setAlignment(Qt::AlignCenter);

    m_subValueLabel = new QLabel();
    m_subValueLabel->setObjectName("cardSubValue");
    m_subValueLabel->setAlignment(Qt::AlignCenter);

    if (m_type == CircularProgress) {
        m_valueLabel->setVisible(false);
        m_subValueLabel->setVisible(false);
        m_titleLabel->setVisible(false); // Hide the title for circular progress

        m_progressCanvas = new QWidget();
        m_progressCanvas->setObjectName("circularCanvas");
        m_progressCanvas->setFixedSize(162, 162);
        m_progressCanvas->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        m_layout->addWidget(m_progressCanvas, 1);
        m_layout->setAlignment(m_progressCanvas, Qt::AlignCenter);
    } else if (m_type == TemperatureBar) {
        m_progressBar = new QProgressBar();
        m_progressBar->setObjectName("temperatureBar");
        m_progressBar->setRange(0, 100);
        m_progressBar->setValue(0);
        m_progressBar->setTextVisible(false);
        m_progressBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        m_progressBar->setMinimumHeight(12);
        m_progressBar->setMaximumHeight(20);

        QHBoxLayout *tempLayout = new QHBoxLayout();
        tempLayout->setContentsMargins(0, 0, 0, 0);
        tempLayout->setSpacing(12);

        QLabel *tempLabel = new QLabel("Temperature");
        tempLabel->setObjectName("tempLabel");

        tempLayout->addWidget(tempLabel);
        tempLayout->addWidget(m_progressBar, 1);
        m_valueLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        tempLayout->addWidget(m_valueLabel);
        tempLayout->addStretch();

        m_layout->addLayout(tempLayout);
        m_layout->addWidget(m_subValueLabel);
    } else {
        m_valueLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        m_subValueLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        m_layout->addWidget(m_valueLabel);
        m_layout->addWidget(m_subValueLabel);
    }

    // Initialize progress animation
    m_progressAnimation = new QPropertyAnimation(this, "progressValue");
    m_progressAnimation->setDuration(450);
    m_progressAnimation->setEasingCurve(QEasingCurve::OutCubic);

    // Base styling - different for circular progress vs other types
    if (m_type == CircularProgress) {
        setStyleSheet(R"(
            #monitoringCard {
                background: transparent;
                border: none;
                border-radius: 0px;
            }

        #cardIcon {
            background-color: rgba(255, 255, 255, 0.05);
            border-radius: 12px;
            color: rgba(255, 255, 255, 0.85);
            font-size: 12px;
        }

        #cardTitle {
            color: rgba(255, 255, 255, 0.6);
            font-size: 10px;
            font-weight: 600;
            letter-spacing: 1px;
            text-transform: uppercase;
        }

        #cardValue {
            color: #ffffff;
            font-size: 20px;
            font-weight: 600;
        }

        #cardSubValue {
            color: rgba(255, 255, 255, 0.5);
            font-size: 10px;
        }

        #tempLabel {
            color: rgba(255, 255, 255, 0.6);
            font-size: 10px;
            min-width: 80px;
        }

        #temperatureBar {
            background: rgba(255, 255, 255, 0.08);
            border: none;
            border-radius: 6px;
        }

        #temperatureBar::chunk {
            border-radius: 6px;
        }
    )");
    } else {
        setStyleSheet(R"(
            #monitoringCard {
                background: rgba(9, 22, 48, 0.9);
                border-radius: 20px;
                border: 1px solid rgba(255, 255, 255, 0.06);
            }

            #cardIcon {
                background-color: rgba(255, 255, 255, 0.05);
                border-radius: 12px;
                color: rgba(255, 255, 255, 0.85);
                font-size: 12px;
            }

            #cardTitle {
                color: rgba(255, 255, 255, 0.6);
                font-size: 10px;
                font-weight: 600;
                letter-spacing: 1px;
                text-transform: uppercase;
            }

            #cardValue {
                color: #ffffff;
                font-size: 20px;
                font-weight: 600;
            }

            #cardSubValue {
                color: rgba(255, 255, 255, 0.5);
                font-size: 10px;
            }

            #tempLabel {
                color: rgba(255, 255, 255, 0.6);
                font-size: 10px;
                min-width: 80px;
            }

            #temperatureBar {
                background: rgba(255, 255, 255, 0.08);
                border: none;
                border-radius: 6px;
            }

            #temperatureBar::chunk {
                border-radius: 6px;
            }
        )");
    }
}

void MonitoringCard::setValue(const QString &value)
{
    m_valueText = value;
    m_valueLabel->setText(value);
    if (m_type == CircularProgress) {
        update();
    }
}

void MonitoringCard::setSubValue(const QString &subValue)
{
    m_subValueText = subValue;
    m_subValueLabel->setText(subValue);
    if (m_type == CircularProgress) {
        update();
    }
}

void MonitoringCard::setProgress(int percentage)
{
    if (m_progressAnimation) {
        m_progressAnimation->stop();
        m_progressAnimation->setStartValue(m_progressValue);
        m_progressAnimation->setEndValue(percentage);
        m_progressAnimation->start();
    } else {
        setProgressValue(percentage);
    }
}

void MonitoringCard::setColor(const QColor &color)
{
    m_cardColor = color;
    
    const QString accentHex = color.name(QColor::HexRgb);
    QColor softColor = color;
    softColor.setAlphaF(0.22);

    QString iconStyle = QStringLiteral(
        "background-color: %1;"
        "color: %2;"
        "border-radius: 12px;"
        "font-weight: 600;")
        .arg(softColor.name(QColor::HexArgb))
        .arg(accentHex);
    m_iconLabel->setStyleSheet(iconStyle);

    if (m_progressBar && m_type == TemperatureBar) {
        QString barStyle = QStringLiteral(
            "QProgressBar {"
            "background: rgba(255, 255, 255, 0.08);"
            "border: none;"
            "border-radius: 6px;"
            "}"
            "QProgressBar::chunk {"
            "background-color: %1;"
            "border-radius: 6px;"
            "}")
            .arg(accentHex);
        m_progressBar->setStyleSheet(barStyle);
    }

    if (m_valueLabel && m_type != CircularProgress) {
        QString valueStyle = QStringLiteral(
            "color: %1;"
            "font-weight: 600;")
            .arg(accentHex);
        m_valueLabel->setStyleSheet(valueStyle);
    }

    if (m_subValueLabel && m_type != CircularProgress) {
        m_subValueLabel->setStyleSheet(QStringLiteral(
            "color: rgba(255, 255, 255, 0.55);"
            "font-size: 12px;"));
    }

    update();
}

void MonitoringCard::setIcon(const QString &iconText)
{
    m_iconLabel->setText(iconText);
}

void MonitoringCard::setCircularSize(int widgetSize, int canvasSize)
{
    if (m_type != CircularProgress || !m_progressCanvas) {
        return;
    }

    setFixedSize(widgetSize, widgetSize);
    m_progressCanvas->setFixedSize(canvasSize, canvasSize);
    int margin = qMax(0, (widgetSize - canvasSize) / 2);
    m_layout->setContentsMargins(margin, margin, margin, margin);
    m_layout->setSpacing(0);
    updateGeometry();
    update();
}

void MonitoringCard::setProgressValue(qreal value)
{
    m_progressValue = qBound<qreal>(0.0, value, 100.0);
    if (m_progressBar) {
        m_progressBar->setValue(static_cast<int>(m_progressValue));
    }
    update();
}

void MonitoringCard::paintEvent(QPaintEvent *event)
{
    QFrame::paintEvent(event);
    
    if (m_type == CircularProgress && m_progressCanvas) {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setRenderHint(QPainter::SmoothPixmapTransform);
        
        // Handle High DPI scaling
        qreal devicePixelRatio = painter.device()->devicePixelRatioF();
        painter.scale(1.0 / devicePixelRatio, 1.0 / devicePixelRatio);
        
        QRectF canvasRect = QRectF(m_progressCanvas->pos(), QSizeF(m_progressCanvas->width(), m_progressCanvas->height()));
        if (canvasRect.width() < 1.0 || canvasRect.height() < 1.0)
            return;

        qreal diameter = qMin(canvasRect.width(), canvasRect.height());
        if (diameter <= 0.0)
            return;

        qreal padding = qMax<qreal>(12.0, diameter * 0.08);
        diameter = qMax<qreal>(0.0, diameter - padding * 2.0);
        if (diameter <= 0.0)
            return;

        QPointF center = canvasRect.center();
        QRectF baseRect(center.x() - diameter / 2.0,
                        center.y() - diameter / 2.0,
                        diameter,
                        diameter);

        qreal strokeWidth = qBound<qreal>(3.0, diameter * 0.02, diameter * 0.04);
        QRectF ringRect = baseRect.adjusted(strokeWidth / 2.0,
                                            strokeWidth / 2.0,
                                            -strokeWidth / 2.0,
                                            -strokeWidth / 2.0);

        // Background ring
        painter.setPen(QPen(QColor(26, 45, 86, 220), strokeWidth, Qt::SolidLine, Qt::RoundCap));
        painter.drawArc(ringRect, 0, 360 * 16);

        // Progress arc
        painter.setPen(QPen(m_cardColor, strokeWidth, Qt::SolidLine, Qt::RoundCap));
        int startAngle = 90 * 16;
        int spanAngle = static_cast<int>(-m_progressValue * 3.6 * 16);
        painter.drawArc(ringRect, startAngle, spanAngle);

        // Inner fill
        QRectF innerRect = ringRect.adjusted(strokeWidth * 0.6,
                                             strokeWidth * 0.6,
                                             -strokeWidth * 0.6,
                                             -strokeWidth * 0.6);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(7, 15, 36, 230));
        painter.drawEllipse(innerRect);

        // Value text - scaled down for High DPI
        QString valueText = m_valueText.isEmpty() ? QStringLiteral("--") : m_valueText;
        painter.setPen(Qt::white);
        QFont valueFont = painter.font();
        valueFont.setPointSizeF(qBound<qreal>(12.0, innerRect.height() * 0.24, 28.0));
        valueFont.setWeight(QFont::DemiBold);
        painter.setFont(valueFont);

        if (!m_subValueText.isEmpty()) {
            QFontMetricsF valueMetrics(valueFont);
            qreal valueHeight = valueMetrics.height();

            QFont subFont = valueFont;
            subFont.setPointSizeF(qBound<qreal>(7.0, valueFont.pointSizeF() * 0.45, 14.0));
            subFont.setWeight(QFont::Medium);
            QFontMetricsF subMetrics(subFont);
            qreal subHeight = subMetrics.height();

            qreal spacing = qBound<qreal>(2.0, valueHeight * 0.25, 8.0);
            qreal totalHeight = valueHeight + spacing + subHeight;
            qreal top = innerRect.center().y() - totalHeight / 2.0;

            QRectF valueRect(innerRect.left(), top, innerRect.width(), valueHeight);
            painter.drawText(valueRect, Qt::AlignCenter, valueText);

            painter.setFont(subFont);
            painter.setPen(QColor(195, 208, 255, 210));
            QRectF subRect(innerRect.left(), valueRect.bottom() + spacing, innerRect.width(), subHeight);
            painter.drawText(subRect, Qt::AlignCenter, m_subValueText);
        } else {
            painter.drawText(innerRect, Qt::AlignCenter, valueText);
        }
    }
}
