#include "customslider.h"

CustomSlider::CustomSlider(const QString &label, QWidget *parent)
    : QWidget(parent)
    , m_labelText(label)
    , m_snapToIncrements(false)
    , m_increment(25)
{
    setupUI();
}

void CustomSlider::setupUI()
{
    m_layout = new QHBoxLayout(this);
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setSpacing(10);
    
    // Label
    m_label = new QLabel(m_labelText);
    m_label->setObjectName("sliderLabel");
    m_label->setFixedWidth(80);
    
    // Slider
    m_slider = new QSlider(Qt::Horizontal);
    m_slider->setObjectName("customSlider");
    m_slider->setRange(0, 4);  // 0=0%, 1=25%, 2=50%, 3=75%, 4=100%
    m_slider->setValue(3);     // Default to 75%
    
    // Value label
    m_valueLabel = new QLabel("75%");
    m_valueLabel->setObjectName("sliderValue");
    m_valueLabel->setFixedWidth(40);
    m_valueLabel->setAlignment(Qt::AlignCenter);
    
    m_layout->addWidget(m_label);
    m_layout->addWidget(m_slider);
    m_layout->addWidget(m_valueLabel);
    
    // Connect signals
    connect(m_slider, &QSlider::valueChanged, this, &CustomSlider::onSliderValueChanged);
    
    // Apply styles
    setStyleSheet(R"(
        #sliderLabel {
            color: #cccccc;
            font-size: 12px;
            font-weight: bold;
        }

        #sliderLabel:disabled {
            color: #555555;
        }
        
        #customSlider {
            background-color: transparent;
        }
        
        #customSlider::groove:horizontal {
            background-color: #404040;
            height: 6px;
            border-radius: 3px;
        }

        #customSlider::groove:horizontal:disabled {
            background-color: #2a2a2a;
        }
        
        #customSlider::handle:horizontal {
            background-color: #2a82da;
            width: 16px;
            height: 16px;
            border-radius: 8px;
            margin: -5px 0;
        }

        #customSlider::handle:horizontal:disabled {
            background-color: #555555;
        }
        
        #customSlider::sub-page:horizontal {
            background-color: #2a82da;
            border-radius: 3px;
        }

        #customSlider::sub-page:horizontal:disabled {
            background-color: #3a3a3a;
        }
        
        #sliderValue {
            color: #ffffff;
            font-size: 12px;
            font-weight: bold;
        }

        #sliderValue:disabled {
            color: #555555;
        }
    )");
}

void CustomSlider::setValue(int value)
{
    if (m_snapToIncrements && m_increment == 25) {
        // Convert percentage to slider position (0-4)
        int position = value / 25;
        position = qBound(0, position, 4);
        m_slider->setValue(position);
    } else {
        m_slider->setValue(value);
    }
}

int CustomSlider::value() const
{
    if (m_snapToIncrements && m_increment == 25) {
        // Convert slider position (0-4) to percentage
        return m_slider->value() * 25;
    } else {
        return m_slider->value();
    }
}

void CustomSlider::setRange(int min, int max)
{
    // For 25% increments, we use 0-4 range internally (0=0%, 1=25%, 2=50%, 3=75%, 4=100%)
    if (m_snapToIncrements && m_increment == 25) {
        m_slider->setRange(0, 4);
        m_slider->setSingleStep(1);  // Move one position at a time (25% steps)
        m_slider->setPageStep(1);    // Page step also moves one position
    } else {
        m_slider->setRange(min, max);
    }
}

void CustomSlider::setTickInterval(int interval)
{
    m_slider->setTickInterval(interval);
    if (m_snapToIncrements && m_increment == 25) {
        // For 25% increments, show ticks at all 5 positions (0, 25, 50, 75, 100)
        m_slider->setTickPosition(QSlider::TicksBelow);
    } else {
        m_slider->setTickPosition(QSlider::TicksBelow);
    }
}

void CustomSlider::setPageStep(int step)
{
    if (m_snapToIncrements && m_increment == 25) {
        // For 25% increments, step is always 1 position (25%)
        m_slider->setPageStep(1);
        m_slider->setSingleStep(1);
    } else {
        m_slider->setPageStep(step);
        m_slider->setSingleStep(step);
    }
}

void CustomSlider::setSnapToIncrements(bool enabled, int increment)
{
    m_snapToIncrements = enabled;
    m_increment = increment;
    
    // If enabling 25% increments, update the slider range and steps
    if (enabled && increment == 25) {
        m_slider->setRange(0, 4);
        m_slider->setSingleStep(1);  // Move one position at a time (25% steps)
        m_slider->setPageStep(1);    // Page step also moves one position
        m_slider->setTickInterval(1); // Show tick marks at each position
        m_slider->setTickPosition(QSlider::TicksBelow);
    }
}

void CustomSlider::onSliderValueChanged(int value)
{
    int finalValue = value;
    
    if (m_snapToIncrements && m_increment == 25) {
        // Convert slider position (0-4) to percentage (0, 25, 50, 75, 100)
        finalValue = value * 25;
    } else if (m_snapToIncrements) {
        // Snap to specified increments for other increment values
        int halfIncrement = m_increment / 2;
        finalValue = ((value + halfIncrement) / m_increment) * m_increment;
        finalValue = qBound(0, finalValue, 100);
        
        // If the value changed due to snapping, update the slider
        if (finalValue != value) {
            m_slider->setValue(finalValue);
            return; // This will trigger another call to this function with the snapped value
        }
    }
    
    m_valueLabel->setText(QString::number(finalValue) + "%");
    emit valueChanged(finalValue);
}
