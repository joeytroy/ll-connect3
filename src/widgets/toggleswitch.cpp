#include "toggleswitch.h"

#include <algorithm>
#include <QEnterEvent>
#include <QEvent>
#include <QPainter>
#include <QPropertyAnimation>

static QColor lerpColor(const QColor &a, const QColor &b, qreal t)
{
    t = std::clamp(t, 0.0, 1.0);
    QColor out;
    out.setRedF(a.redF() + (b.redF() - a.redF()) * t);
    out.setGreenF(a.greenF() + (b.greenF() - a.greenF()) * t);
    out.setBlueF(a.blueF() + (b.blueF() - a.blueF()) * t);
    out.setAlphaF(a.alphaF() + (b.alphaF() - a.alphaF()) * t);
    return out;
}

ToggleSwitch::ToggleSwitch(QWidget *parent)
    : QAbstractButton(parent)
    , m_checkedColor(palette().color(QPalette::Highlight))
    , m_uncheckedColor(palette().color(QPalette::Button))
{
    setCheckable(true);
    setFocusPolicy(Qt::StrongFocus);
    setMinimumSize(44, 24);

    m_anim = new QPropertyAnimation(this, "knobPosition", this);
    m_anim->setDuration(150);
    m_anim->setEasingCurve(QEasingCurve::InOutCubic);

    m_hoverAnim = new QPropertyAnimation(this, "hoverProgress", this);
    m_hoverAnim->setDuration(120);
    m_hoverAnim->setEasingCurve(QEasingCurve::InOutCubic);

    m_knobPos = isChecked() ? 1.0 : 0.0;
}

QSize ToggleSwitch::sizeHint() const
{
    return QSize(48, 26);
}

qreal ToggleSwitch::knobPosition() const
{
    return m_knobPos;
}

void ToggleSwitch::setKnobPosition(qreal pos)
{
    m_knobPos = std::clamp(pos, 0.0, 1.0);
    update();
}

qreal ToggleSwitch::hoverProgress() const
{
    return m_hover;
}

void ToggleSwitch::setHoverProgress(qreal p)
{
    m_hover = std::clamp(p, 0.0, 1.0);
    update();
}

QColor ToggleSwitch::checkedColor() const
{
    return m_checkedColor;
}

void ToggleSwitch::setCheckedColor(const QColor &c)
{
    m_checkedColor = c;
    update();
}

QColor ToggleSwitch::uncheckedColor() const
{
    return m_uncheckedColor;
}

void ToggleSwitch::setUncheckedColor(const QColor &c)
{
    m_uncheckedColor = c;
    update();
}

int ToggleSwitch::animationDuration() const
{
    return m_anim ? m_anim->duration() : 0;
}

void ToggleSwitch::setAnimationDuration(int ms)
{
    if (m_anim) m_anim->setDuration(ms);
}

void ToggleSwitch::paintEvent(QPaintEvent * /*event*/)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const int h = height();
    const int margin = std::max(2, h / 8);
    const int knobDiam = std::max(1, h - 2 * margin);
    const int travel = std::max(0, width() - knobDiam - 2 * margin);

    // Track
    p.setPen(Qt::NoPen);
    const QColor track = lerpColor(m_uncheckedColor, m_checkedColor, m_knobPos);
    p.setBrush(track);
    p.drawRoundedRect(rect(), h / 2.0, h / 2.0);

    // Subtle hover glow (outline) with animated intensity
    if (m_hover > 0.0) {
        QColor glow = lerpColor(QColor("#ffffff"), m_checkedColor, m_knobPos);
        glow.setAlphaF(0.18 * m_hover);
        QPen pen(glow, 2.0);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        const QRectF r = QRectF(rect()).adjusted(1.0, 1.0, -1.0, -1.0);
        p.drawRoundedRect(r, h / 2.0, h / 2.0);
        p.setPen(Qt::NoPen);
    }

    // Knob
    const int knobX = margin + static_cast<int>(travel * m_knobPos);
    p.setBrush(Qt::white);
    p.drawEllipse(knobX, margin, knobDiam, knobDiam);
}

void ToggleSwitch::checkStateSet()
{
    QAbstractButton::checkStateSet();
    if (m_userToggleInProgress) return;
    setKnobPosition(isChecked() ? 1.0 : 0.0);
}

void ToggleSwitch::startAnimation(qreal from, qreal to)
{
    if (!m_anim) {
        setKnobPosition(to);
        return;
    }
    m_anim->stop();
    m_anim->setStartValue(std::clamp(from, 0.0, 1.0));
    m_anim->setEndValue(std::clamp(to, 0.0, 1.0));
    m_anim->start();
}

void ToggleSwitch::startHoverAnimation(qreal to)
{
    if (!m_hoverAnim) {
        setHoverProgress(to);
        return;
    }
    m_hoverAnim->stop();
    m_hoverAnim->setStartValue(m_hover);
    m_hoverAnim->setEndValue(std::clamp(to, 0.0, 1.0));
    m_hoverAnim->start();
}

void ToggleSwitch::nextCheckState()
{
    const qreal startPos = m_knobPos;
    m_userToggleInProgress = true;
    QAbstractButton::nextCheckState(); // toggles checked state + emits toggled
    m_userToggleInProgress = false;

    startAnimation(startPos, isChecked() ? 1.0 : 0.0);
}

bool ToggleSwitch::hitButton(const QPoint &pos) const
{
    return rect().contains(pos);
}

void ToggleSwitch::enterEvent(QEnterEvent *event)
{
    QAbstractButton::enterEvent(event);
    startHoverAnimation(1.0);
}

void ToggleSwitch::leaveEvent(QEvent *event)
{
    QAbstractButton::leaveEvent(event);
    startHoverAnimation(0.0);
}

