#pragma once

#include <QAbstractButton>
#include <QColor>

class QPropertyAnimation;

class ToggleSwitch final : public QAbstractButton
{
    Q_OBJECT
    Q_PROPERTY(qreal knobPosition READ knobPosition WRITE setKnobPosition)
    Q_PROPERTY(qreal hoverProgress READ hoverProgress WRITE setHoverProgress)
    Q_PROPERTY(QColor checkedColor READ checkedColor WRITE setCheckedColor)
    Q_PROPERTY(QColor uncheckedColor READ uncheckedColor WRITE setUncheckedColor)
    Q_PROPERTY(int animationDuration READ animationDuration WRITE setAnimationDuration)

public:
    explicit ToggleSwitch(QWidget *parent = nullptr);

    QSize sizeHint() const override;

    qreal knobPosition() const;
    void setKnobPosition(qreal pos);

    qreal hoverProgress() const;
    void setHoverProgress(qreal p);

    QColor checkedColor() const;
    void setCheckedColor(const QColor &c);

    QColor uncheckedColor() const;
    void setUncheckedColor(const QColor &c);

    int animationDuration() const;
    void setAnimationDuration(int ms);

protected:
    void paintEvent(QPaintEvent *event) override;
    void checkStateSet() override;
    void nextCheckState() override;
    bool hitButton(const QPoint &pos) const override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    void startAnimation(qreal from, qreal to);
    void startHoverAnimation(qreal to);

    qreal m_knobPos{0.0};
    qreal m_hover{0.0};
    QColor m_checkedColor;
    QColor m_uncheckedColor;
    QPropertyAnimation *m_anim{nullptr};
    QPropertyAnimation *m_hoverAnim{nullptr};
    bool m_userToggleInProgress{false};
};

