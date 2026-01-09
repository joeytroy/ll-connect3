#include "fancurvewidget.h"
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <cmath>
#include <QFont>
#include <QFontMetrics>

FanCurveWidget::FanCurveWidget(QWidget *parent)
    : QWidget(parent)
    , m_profile("Quiet")
    , m_currentTemperature(25)
    , m_currentRPM(420)
    , m_marginLeft(50)
    , m_marginRight(20)
    , m_marginTop(20)
    , m_marginBottom(40)
    , m_tempMin(0)
    , m_tempMax(100)
    , m_rpmMin(0)
    , m_rpmMax(2100)
    , m_displayRpmMax(2100)
    , m_dragging(false)
    , m_draggedPoint(-1)
    , m_hoveredPoint(-1)
    , m_graphEnabled(true)
    , m_backgroundColor(QColor(26, 26, 26))
    , m_gridColor(QColor(60, 60, 60))
    , m_axisColor(QColor(200, 200, 200))
    , m_curveColor(QColor(100, 150, 255))
    , m_pointColor(QColor(255, 255, 255))
    , m_currentLineColor(QColor(0, 255, 0))
{
    setMinimumSize(400, 200);
    setMouseTracking(true);  // Enable mouse tracking for hover events
    setupCurveData();
}

void FanCurveWidget::setFanSize(int maxRPM)
{
    // Only change the display labels, not the actual curve scaling
    m_displayRpmMax = maxRPM;
    // Keep m_rpmMax at 2100 so curves don't move
    update();
}

void FanCurveWidget::setProfile(const QString &profile)
{
    m_profile = profile;
    setupCurveData();
    update();
}

void FanCurveWidget::setCurrentTemperature(int temperature)
{
    m_currentTemperature = temperature;
    update();
}

void FanCurveWidget::setCurrentRPM(int rpm)
{
    m_currentRPM = rpm;
    update();
}

void FanCurveWidget::setGraphEnabled(bool enabled)
{
    m_graphEnabled = enabled;
    update();
}

void FanCurveWidget::setCustomCurve(const QVector<QPointF> &points)
{
    m_curvePoints = points;
    update();
}

void FanCurveWidget::setupCurveData()
{
    m_curvePoints.clear();
    
    if (m_profile == "Quiet") {
        // Data points for 120mm fans (max 2100 RPM)
        m_curvePoints << QPointF(0, 120);     // 0°C: 120 RPM minimum
        m_curvePoints << QPointF(25, 420);
        m_curvePoints << QPointF(45, 840);
        m_curvePoints << QPointF(65, 1050);
        m_curvePoints << QPointF(80, 1680);
        m_curvePoints << QPointF(90, 2100);
        m_curvePoints << QPointF(100, 2100);
    } else if (m_profile == "Standard") {
        // Standard Speed curve (StdSP) - Balanced curve
        m_curvePoints << QPointF(0, 120);     // 0°C: 120 RPM minimum
        m_curvePoints << QPointF(25, 420);    // 25°C: 420 RPM (34 dBA)
        m_curvePoints << QPointF(40, 1050);   // 40°C: 1050 RPM (39 dBA)
        m_curvePoints << QPointF(55, 1260);   // 55°C: 1260 RPM (45 dBA)
        m_curvePoints << QPointF(70, 1680);   // 70°C: 1680 RPM (52 dBA)
        m_curvePoints << QPointF(90, 2100);   // 90°C: 2100 RPM (60 dBA)
        m_curvePoints << QPointF(100, 2100);  // 100°C: 2100 RPM (60 dBA)
    } else if (m_profile == "High Speed") {
        // High Speed curve (HighSP) - Smooth progressive ramp
        m_curvePoints << QPointF(0, 120);     // 0°C: 120 RPM minimum
        m_curvePoints << QPointF(25, 910);    // 25°C: 910 RPM (~36 dBA)
        m_curvePoints << QPointF(35, 1140);   // 35°C: 1140 RPM (~42 dBA)
        m_curvePoints << QPointF(50, 1470);   // 50°C: 1470 RPM (~48 dBA)
        m_curvePoints << QPointF(70, 1800);   // 70°C: 1800 RPM (~55 dBA)
        m_curvePoints << QPointF(85, 2100);   // 85°C: 2100 RPM (60 dBA)
        m_curvePoints << QPointF(100, 2100);  // 100°C: 2100 RPM (60 dBA)
    } else if (m_profile == "Full Speed") {
        // Maximum speed curve - instant max cooling
        m_curvePoints << QPointF(0, 120);     // 0°C: 120 RPM minimum
        m_curvePoints << QPointF(25, 2100);   // 25°C: 2100 RPM (60 dBA) - instant max
        m_curvePoints << QPointF(40, 2100);   // 40°C: 2100 RPM (60 dBA)
        m_curvePoints << QPointF(55, 2100);   // 55°C: 2100 RPM (60 dBA)
        m_curvePoints << QPointF(70, 2100);   // 70°C: 2100 RPM (60 dBA)
        m_curvePoints << QPointF(90, 2100);   // 90°C: 2100 RPM (60 dBA)
        m_curvePoints << QPointF(100, 2100);  // 100°C: 2100 RPM (60 dBA)
    } else {
        // Default to Quiet (original Lian Li curve)
        m_curvePoints << QPointF(0, 120);     // 0°C: 120 RPM minimum
        m_curvePoints << QPointF(25, 420);
        m_curvePoints << QPointF(45, 840);
        m_curvePoints << QPointF(65, 1050);
        m_curvePoints << QPointF(80, 1680);
        m_curvePoints << QPointF(90, 2100);
        m_curvePoints << QPointF(100, 2100);
    }
}

void FanCurveWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // Fill background
    painter.fillRect(rect(), m_backgroundColor);
    
    // Apply opacity if graph is disabled
    if (!m_graphEnabled) {
        painter.setOpacity(0.3);
    }
    
    // Calculate graph area
    QRect graphRect = rect().adjusted(m_marginLeft, m_marginTop, -m_marginRight, -m_marginBottom);
    
    // Draw grid
    drawGrid(painter);
    
    // Draw axes
    drawAxes(painter);
    
    // Draw curve
    drawCurve(painter);
    
    // Draw data points
    drawDataPoints(painter);
    
    // Draw current temperature line
    drawCurrentLine(painter);
    
    // Draw hover info for hovered point when hovering over a point
    if (m_hoveredPoint >= 0 && m_hoveredPoint < m_curvePoints.size()) {
        QPointF point = dataToPixel(m_curvePoints[m_hoveredPoint]);
        double temp = m_curvePoints[m_hoveredPoint].x();
        double rpm = m_curvePoints[m_hoveredPoint].y();
        drawHoverInfo(painter, point, temp, rpm);
    }
}

void FanCurveWidget::drawGrid(QPainter &painter)
{
    QRect graphRect = rect().adjusted(m_marginLeft, m_marginTop, -m_marginRight, -m_marginBottom);
    
    painter.setPen(QPen(m_gridColor, 1));
    
    // Vertical grid lines (temperature)
    for (int temp = 0; temp <= 100; temp += 10) {
        int x = graphRect.left() + (temp - m_tempMin) / (m_tempMax - m_tempMin) * graphRect.width();
        painter.drawLine(x, graphRect.top(), x, graphRect.bottom());
    }
    
    // Minor vertical grid lines (every 5°C)
    painter.setPen(QPen(m_gridColor, 0.5));
    for (int temp = 5; temp < 100; temp += 10) {
        int x = graphRect.left() + (temp - m_tempMin) / (m_tempMax - m_tempMin) * graphRect.width();
        painter.drawLine(x, graphRect.top(), x, graphRect.bottom());
    }
    
    // Horizontal grid lines (RPM) - adjust grid lines based on fan size, but position using 120mm scale
    int rpmStep = 420; // Default for 120mm fans
    int minorStep = 210; // Half of major step
    if (m_displayRpmMax == 1600) {
        rpmStep = 400; // For 140mm fans
        minorStep = 200; // Half of major step
    }
    
    painter.setPen(QPen(m_gridColor, 1));
    for (int rpm = 0; rpm <= m_displayRpmMax; rpm += rpmStep) {
        // Calculate position using 120mm scale (2100 max)
        double scaleFactor = (double)rpm / m_displayRpmMax; // 0.0 to 1.0
        double positionRpm = scaleFactor * 2100.0; // Map to 120mm scale for positioning
        int y = graphRect.bottom() - (positionRpm - m_rpmMin) / (m_rpmMax - m_rpmMin) * graphRect.height();
        painter.drawLine(graphRect.left(), y, graphRect.right(), y);
    }
    
    // Minor horizontal grid lines
    painter.setPen(QPen(m_gridColor, 0.5));
    for (int rpm = minorStep; rpm < m_displayRpmMax; rpm += rpmStep) {
        // Calculate position using 120mm scale (2100 max)
        double scaleFactor = (double)rpm / m_displayRpmMax; // 0.0 to 1.0
        double positionRpm = scaleFactor * 2100.0; // Map to 120mm scale for positioning
        int y = graphRect.bottom() - (positionRpm - m_rpmMin) / (m_rpmMax - m_rpmMin) * graphRect.height();
        painter.drawLine(graphRect.left(), y, graphRect.right(), y);
    }
}

void FanCurveWidget::drawAxes(QPainter &painter)
{
    QRect graphRect = rect().adjusted(m_marginLeft, m_marginTop, -m_marginRight, -m_marginBottom);
    
    painter.setPen(QPen(m_axisColor, 2));
    
    // Draw axes
    painter.drawLine(graphRect.left(), graphRect.bottom(), graphRect.right(), graphRect.bottom()); // X-axis
    painter.drawLine(graphRect.left(), graphRect.top(), graphRect.left(), graphRect.bottom()); // Y-axis
    
    // Draw axis labels
    painter.setPen(QPen(m_axisColor, 1));
    QFont font = painter.font();
    font.setPointSize(9);
    painter.setFont(font);
    
    // X-axis labels (Temperature)
    for (int temp = 0; temp <= 100; temp += 10) {
        int x = graphRect.left() + (temp - m_tempMin) / (m_tempMax - m_tempMin) * graphRect.width();
        painter.drawText(x - 15, graphRect.bottom() + 15, 30, 15, Qt::AlignCenter, QString::number(temp) + "°C");
    }
    
    // Y-axis labels (RPM) - adjust labels based on fan size, but position stays the same
    int rpmStep = 420; // Default for 120mm fans (2100 / 5)
    if (m_displayRpmMax == 1600) {
        rpmStep = 400; // For 140mm fans (1600 / 4)
    }
    
    for (int rpm = 0; rpm <= m_displayRpmMax; rpm += rpmStep) {
        // Calculate position using 120mm scale (2100 max), but show 140mm labels (1600 max)
        double scaleFactor = (double)rpm / m_displayRpmMax; // 0.0 to 1.0
        double positionRpm = scaleFactor * 2100.0; // Map to 120mm scale for positioning
        int y = graphRect.bottom() - (positionRpm - m_rpmMin) / (m_rpmMax - m_rpmMin) * graphRect.height();
        painter.drawText(5, y - 10, m_marginLeft - 10, 20, Qt::AlignRight | Qt::AlignVCenter, QString::number(rpm));
    }
}

void FanCurveWidget::drawCurve(QPainter &painter)
{
    if (m_curvePoints.size() < 2) return;
    
    QRect graphRect = rect().adjusted(m_marginLeft, m_marginTop, -m_marginRight, -m_marginBottom);
    
    painter.setPen(QPen(m_curveColor, 2));
    
    // Draw curve line
    QPainterPath path;
    QPointF firstPoint = dataToPixel(m_curvePoints[0]);
    path.moveTo(firstPoint);
    
    for (int i = 1; i < m_curvePoints.size(); ++i) {
        QPointF point = dataToPixel(m_curvePoints[i]);
        path.lineTo(point);
    }
    
    painter.drawPath(path);
}

void FanCurveWidget::drawDataPoints(QPainter &painter)
{
    QRect graphRect = rect().adjusted(m_marginLeft, m_marginTop, -m_marginRight, -m_marginBottom);
    
    for (int i = 0; i < m_curvePoints.size(); ++i) {
        QPointF point = dataToPixel(m_curvePoints[i]);
        
        // Draw white circle with blue outline
        painter.setPen(QPen(m_curveColor, 2));
        painter.setBrush(QBrush(m_pointColor));
        painter.drawEllipse(point, 4, 4);
    }
}

QColor FanCurveWidget::getTemperatureColor(int temperature)
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

void FanCurveWidget::drawCurrentLine(QPainter &painter)
{
    QRect graphRect = rect().adjusted(m_marginLeft, m_marginTop, -m_marginRight, -m_marginBottom);
    
    // Calculate color based on temperature
    QColor lineColor = getTemperatureColor(m_currentTemperature);
    
    // Draw vertical line at current temperature
    int x = graphRect.left() + (m_currentTemperature - m_tempMin) / (m_tempMax - m_tempMin) * graphRect.width();
    
    // Draw a thicker, more prominent line
    painter.setPen(QPen(lineColor, 3));
    painter.drawLine(x, graphRect.top(), x, graphRect.bottom());
    
    // Calculate the RPM that should be on the curve at this temperature
    int curveRPM = calculateRPMForTemperature(m_currentTemperature);
    
    // Draw a larger circle at the intersection with the curve (temperature ball)
    QPointF curvePoint = QPointF(x, graphRect.bottom() - (curveRPM - m_rpmMin) / (m_rpmMax - m_rpmMin) * graphRect.height());
    painter.setPen(QPen(lineColor, 2));
    painter.setBrush(QBrush(lineColor));
    painter.drawEllipse(curvePoint, 7, 7); // Increased from 6 to 7 (about 17% larger)
    
    // Draw a fixed orange center dot (not moving)
    QColor rpmColor = QColor(255, 165, 0); // Orange color
    painter.setPen(QPen(rpmColor, 2));
    painter.setBrush(QBrush(rpmColor));
    painter.drawEllipse(curvePoint, 4, 4); // Fixed at the same position as temperature ball
}

void FanCurveWidget::drawHoverInfo(QPainter &painter, const QPointF &point, double temp, double rpm)
{
    // Format the temperature and RPM strings with proper rounding
    QString tempStr = QString::number(static_cast<int>(std::round(temp))) + "°C";
    QString rpmStr = QString::number(static_cast<int>(std::round(rpm))) + " RPM";
    QString infoText = tempStr + " / " + rpmStr;

    // Set up font
    QFont font = painter.font();
    font.setPointSize(10);
    font.setBold(true);
    painter.setFont(font);

    // Calculate text size
    QFontMetrics fm(font);
    QRect textRect = fm.boundingRect(infoText);
    int padding = 8;
    int boxWidth = textRect.width() + padding * 2;
    int boxHeight = textRect.height() + padding * 2;

    int boxX = static_cast<int>(point.x()) + 15;
    int boxY = static_cast<int>(point.y()) - boxHeight - 15;

    // Make sure the box stays within widget bounds
    if (boxX + boxWidth > width() - m_marginRight) {
        boxX = static_cast<int>(point.x()) - boxWidth - 15; // Show to the left instead
    }
    if (boxY < m_marginTop) {
        boxY = static_cast<int>(point.y()) + 15; // Show below instead
    }

    // Draw background rectangle with rounded corners
    QRect infoRect(boxX, boxY, boxWidth, boxHeight);
    painter.setPen(QPen(QColor(100, 150, 255), 2));
    painter.setBrush(QBrush(QColor(40, 40, 40, 230))); // Semi-transparent dark background
    painter.drawRoundedRect(infoRect, 6, 6);

    // Draw text
    painter.setPen(QPen(QColor(255, 255, 255), 1));
    painter.drawText(infoRect, Qt::AlignCenter, infoText);
}

QPointF FanCurveWidget::dataToPixel(const QPointF &dataPoint)
{
    QRect graphRect = rect().adjusted(m_marginLeft, m_marginTop, -m_marginRight, -m_marginBottom);
    
    double x = graphRect.left() + (dataPoint.x() - m_tempMin) / (m_tempMax - m_tempMin) * graphRect.width();
    double y = graphRect.bottom() - (dataPoint.y() - m_rpmMin) / (m_rpmMax - m_rpmMin) * graphRect.height();
    
    return QPointF(x, y);
}

QPointF FanCurveWidget::pixelToData(const QPointF &pixelPoint)
{
    QRect graphRect = rect().adjusted(m_marginLeft, m_marginTop, -m_marginRight, -m_marginBottom);
    
    double temp = m_tempMin + (pixelPoint.x() - graphRect.left()) / graphRect.width() * (m_tempMax - m_tempMin);
    double rpm = m_rpmMin + (graphRect.bottom() - pixelPoint.y()) / graphRect.height() * (m_rpmMax - m_rpmMin);
    
    return QPointF(temp, rpm);
}

void FanCurveWidget::mousePressEvent(QMouseEvent *event)
{
    
    if (event->button() == Qt::LeftButton) {
        QPointF clickPoint = event->pos();
        QPointF dataPoint = pixelToData(clickPoint);
        
        // Check if clicking near a data point
        for (int i = 0; i < m_curvePoints.size(); ++i) {
            QPointF point = dataToPixel(m_curvePoints[i]);
            if (QLineF(clickPoint, point).length() < 10) {
                m_dragging = true;
                m_draggedPoint = i;
                break;
            }
        }
    }
}

void FanCurveWidget::mouseMoveEvent(QMouseEvent *event)
{
    // Check if mouse is over any data point for hover effect
    int hoveredPoint = -1;
    QPointF mousePos = event->pos();
    
    for (int i = 0; i < m_curvePoints.size(); ++i) {
        QPointF point = dataToPixel(m_curvePoints[i]);
        if (QLineF(mousePos, point).length() < 10) {
            hoveredPoint = i;
            break;
        }
    }
    
    // Update hover state if needed
    if (hoveredPoint != m_hoveredPoint) {
        m_hoveredPoint = hoveredPoint;
        update(); // Trigger repaint to show/hide hover info
    }
    
    if (m_dragging && m_draggedPoint >= 0) {
        QPointF clickPoint = event->pos();
        QPointF dataPoint = pixelToData(clickPoint);
        
        // Snap temperature to nearest whole degree Celsius
        dataPoint.setX(std::round(dataPoint.x()));
        
        // Clamp temperature to valid range
        dataPoint.setX(qMax(m_tempMin, qMin(m_tempMax, dataPoint.x())));
        
        // Clamp RPM with special handling
        double minRPM = (m_draggedPoint == 0);
        double rpm = qMax(minRPM, qMin((double)m_rpmMax, dataPoint.y()));
        // Snap RPM to nearest 10 RPM increment
        rpm = std::round(rpm / 10.0) * 10.0;
        dataPoint.setY(rpm);
        
        m_curvePoints[m_draggedPoint] = dataPoint;
        update();
    }
}

void FanCurveWidget::mouseReleaseEvent(QMouseEvent *event)
{
    Q_UNUSED(event)
    if (m_dragging) {
        // Emit signal that curve has changed
        emit curvePointsChanged(m_curvePoints);
    }
    m_dragging = false;
    m_draggedPoint = -1;
}

int FanCurveWidget::calculateRPMForTemperature(int temperature)
{
    // Calculate RPM based on the current curve profile
    if (m_curvePoints.size() < 2) {
        return 0;
    }
    
    // Find the two points that bracket the temperature
    for (int i = 0; i < m_curvePoints.size() - 1; ++i) {
        double temp1 = m_curvePoints[i].x();
        double rpm1 = m_curvePoints[i].y();
        double temp2 = m_curvePoints[i + 1].x();
        double rpm2 = m_curvePoints[i + 1].y();
        
        if (temperature >= temp1 && temperature <= temp2) {
            // Linear interpolation between the two points
            double ratio = (temperature - temp1) / (temp2 - temp1);
            double rpm = rpm1 + ratio * (rpm2 - rpm1);
            return static_cast<int>(rpm);
        }
    }
    
    // If temperature is outside the curve range, use the closest point
    if (temperature <= m_curvePoints.first().x()) {
        return static_cast<int>(m_curvePoints.first().y());
    } else {
        return static_cast<int>(m_curvePoints.last().y());
    }
}
