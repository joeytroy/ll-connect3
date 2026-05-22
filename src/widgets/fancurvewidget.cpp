#include "fancurvewidget.h"
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QFont>
#include <QFontMetrics>
#include <cmath>

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
    , m_selectedPoint(-1)
    , m_graphEnabled(true)
    , m_useFahrenheit(false)
    , m_backgroundColor(QColor(26, 26, 26))
    , m_gridColor(QColor(60, 60, 60))
    , m_axisColor(QColor(200, 200, 200))
    , m_curveColor(QColor(100, 150, 255))
    , m_pointColor(QColor(255, 255, 255))
    , m_currentLineColor(QColor(0, 255, 0))
    , m_selectedColor(QColor(255, 200, 0))
{
    setMinimumSize(400, 200);
    setupCurveData();
}

void FanCurveWidget::setFanSize(int maxRPM)
{
    m_displayRpmMax = maxRPM;
    // Keep m_rpmMax at 2100 so curves don't move
    update();
}

void FanCurveWidget::setUseFahrenheit(bool useF)
{
    if (m_useFahrenheit != useF) {
        m_useFahrenheit = useF;
        update();
    }
}

void FanCurveWidget::setProfile(const QString &profile)
{
    m_profile = profile;
    setupCurveData();
    m_selectedPoint = -1;
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
    // Curve was replaced wholesale (profile switch, port switch, default reset);
    // a previously selected index may now refer to a totally different point.
    m_selectedPoint = -1;
    update();
}

void FanCurveWidget::setSelectedPoint(int index)
{
    if (index < -1 || index >= m_curvePoints.size()) {
        return;
    }
    if (m_selectedPoint != index) {
        m_selectedPoint = index;
        update();
    }
}

void FanCurveWidget::updatePoint(int index, double temp, double rpm)
{
    if (index < 0 || index >= m_curvePoints.size()) {
        return;
    }

    // Keep curve monotonic in temperature
    if (index > 0) {
        temp = qMax(temp, m_curvePoints[index - 1].x() + 1.0);
    }
    if (index < m_curvePoints.size() - 1) {
        temp = qMin(temp, m_curvePoints[index + 1].x() - 1.0);
    }

    // Match drag clamping: first point can drop to 120, others to 840
    double minRPM = (index == 0) ? 120.0 : 840.0;
    rpm = qBound(minRPM, rpm, m_rpmMax);
    temp = qBound(m_tempMin, temp, m_tempMax);

    m_curvePoints[index] = QPointF(temp, rpm);
    update();
}

void FanCurveWidget::setupCurveData()
{
    m_curvePoints.clear();
    
    if (m_profile == "Quiet") {
        m_curvePoints << QPointF(0, 120);
        m_curvePoints << QPointF(25, 420);
        m_curvePoints << QPointF(45, 840);
        m_curvePoints << QPointF(65, 1050);
        m_curvePoints << QPointF(80, 1680);
        m_curvePoints << QPointF(90, 2100);
        m_curvePoints << QPointF(100, 2100);
    } else if (m_profile == "Standard") {
        m_curvePoints << QPointF(0, 120);
        m_curvePoints << QPointF(25, 420);
        m_curvePoints << QPointF(40, 1050);
        m_curvePoints << QPointF(55, 1260);
        m_curvePoints << QPointF(70, 1680);
        m_curvePoints << QPointF(90, 2100);
        m_curvePoints << QPointF(100, 2100);
    } else if (m_profile == "High Speed") {
        m_curvePoints << QPointF(0, 120);
        m_curvePoints << QPointF(25, 910);
        m_curvePoints << QPointF(35, 1140);
        m_curvePoints << QPointF(50, 1470);
        m_curvePoints << QPointF(70, 1800);
        m_curvePoints << QPointF(85, 2100);
        m_curvePoints << QPointF(100, 2100);
    } else if (m_profile == "Full Speed") {
        m_curvePoints << QPointF(0, 120);
        m_curvePoints << QPointF(25, 2100);
        m_curvePoints << QPointF(40, 2100);
        m_curvePoints << QPointF(55, 2100);
        m_curvePoints << QPointF(70, 2100);
        m_curvePoints << QPointF(90, 2100);
        m_curvePoints << QPointF(100, 2100);
    } else {
        m_curvePoints << QPointF(0, 120);
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
        double scaleFactor = (double)rpm / m_displayRpmMax;
        double positionRpm = scaleFactor * 2100.0;
        int y = graphRect.bottom() - (positionRpm - m_rpmMin) / (m_rpmMax - m_rpmMin) * graphRect.height();
        painter.drawLine(graphRect.left(), y, graphRect.right(), y);
    }
    
    // Minor horizontal grid lines
    painter.setPen(QPen(m_gridColor, 0.5));
    for (int rpm = minorStep; rpm < m_displayRpmMax; rpm += rpmStep) {
        double scaleFactor = (double)rpm / m_displayRpmMax;
        double positionRpm = scaleFactor * 2100.0;
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
    
    // X-axis labels (Temperature) — show °C or °F per user setting
    for (int temp = 0; temp <= 100; temp += 10) {
        int x = graphRect.left() + (temp - m_tempMin) / (m_tempMax - m_tempMin) * graphRect.width();
        QString label = m_useFahrenheit
            ? (QString::number(32 + temp * 9 / 5) + "°F")
            : (QString::number(temp) + "°C");
        painter.drawText(x - 18, graphRect.bottom() + 15, 36, 15, Qt::AlignCenter, label);
    }
    
    // Y-axis labels (RPM)
    int yRpmStep = 420; // Default for 120mm fans (2100 / 5)
    if (m_displayRpmMax == 1600) {
        yRpmStep = 400; // For 140mm fans (1600 / 4)
    }
    
    for (int rpm = 0; rpm <= m_displayRpmMax; rpm += yRpmStep) {
        double scaleFactor = (double)rpm / m_displayRpmMax;
        double positionRpm = scaleFactor * 2100.0;
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

        if (i == m_selectedPoint) {
            // Larger, gold-outlined point with white fill to indicate selection
            painter.setPen(QPen(m_selectedColor, 3));
            painter.setBrush(QBrush(m_pointColor));
            painter.drawEllipse(point, 7, 7);
        } else {
            // Draw white circle with blue outline
            painter.setPen(QPen(m_curveColor, 2));
            painter.setBrush(QBrush(m_pointColor));
            painter.drawEllipse(point, 4, 4);
        }
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
    if (event->button() != Qt::LeftButton) {
        return;
    }

    QPointF clickPoint = event->pos();

    // Find nearest point within the hit radius
    int hitIndex = -1;
    for (int i = 0; i < m_curvePoints.size(); ++i) {
        QPointF point = dataToPixel(m_curvePoints[i]);
        if (QLineF(clickPoint, point).length() < 12) {
            hitIndex = i;
            break;
        }
    }

    if (hitIndex >= 0) {
        m_dragging = true;
        m_draggedPoint = hitIndex;
        m_selectedPoint = hitIndex;
        update();
        emit pointSelected(hitIndex,
                           m_curvePoints[hitIndex].x(),
                           m_curvePoints[hitIndex].y());
    } else if (m_selectedPoint != -1) {
        // Clicking empty graph area clears selection
        m_selectedPoint = -1;
        update();
        emit pointSelected(-1, 0.0, 0.0);
    }
}

void FanCurveWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragging && m_draggedPoint >= 0) {
        QPointF clickPoint = event->pos();
        QPointF dataPoint = pixelToData(clickPoint);

        // Clamp temperature to valid range
        dataPoint.setX(qMax(m_tempMin, qMin(m_tempMax, dataPoint.x())));

        // Clamp RPM with special handling:
        // - First point (0°C idle): can be as low as 120 RPM
        // - All other points: minimum 840 RPM to prevent fan shutdown
        double minRPM = (m_draggedPoint == 0) ? 120.0 : 840.0;
        dataPoint.setY(qMax(minRPM, qMin((double)m_rpmMax, dataPoint.y())));

        m_curvePoints[m_draggedPoint] = dataPoint;
        update();

        // Live-update the numeric editor while dragging
        emit pointSelected(m_draggedPoint, dataPoint.x(), dataPoint.y());
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
    // Note: m_selectedPoint persists across release for sticky selection.
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
