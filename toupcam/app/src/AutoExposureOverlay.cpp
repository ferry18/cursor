#include "AutoExposureOverlay.h"
#include <QPainter>
#include <QPaintEvent>
#include <QEasingCurve>
#include <cmath>

AutoExposureOverlay::AutoExposureOverlay(QWidget *parent)
    : QWidget(parent)
    , m_opacity(0.0)
{
    setFixedSize(SIZE, SIZE);
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_TranslucentBackground);
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    
    // Setup fade in animation
    m_fadeInAnimation = new QPropertyAnimation(this, "opacity", this);
    m_fadeInAnimation->setDuration(FADE_IN_DURATION);
    m_fadeInAnimation->setStartValue(0.0);
    m_fadeInAnimation->setEndValue(1.0);
    m_fadeInAnimation->setEasingCurve(QEasingCurve::InOutQuad);
    
    // Setup fade out animation
    m_fadeOutAnimation = new QPropertyAnimation(this, "opacity", this);
    m_fadeOutAnimation->setDuration(FADE_OUT_DURATION);
    m_fadeOutAnimation->setStartValue(1.0);
    m_fadeOutAnimation->setEndValue(0.0);
    m_fadeOutAnimation->setEasingCurve(QEasingCurve::InOutQuad);
    
    // Setup display timer
    m_displayTimer = new QTimer(this);
    m_displayTimer->setSingleShot(true);
    m_displayTimer->setInterval(DISPLAY_DURATION);
    
    connect(m_displayTimer, &QTimer::timeout, this, &AutoExposureOverlay::startFadeOut);
    connect(m_fadeOutAnimation, &QPropertyAnimation::finished, this, &QWidget::hide);
}

void AutoExposureOverlay::showAt(const QPoint& position)
{
    // Stop any running animations
    m_fadeInAnimation->stop();
    m_fadeOutAnimation->stop();
    m_displayTimer->stop();
    
    // Position the overlay centered on the click point
    move(position - QPoint(SIZE/2, SIZE/2));
    
    // Start the fade in animation
    show();
    startFadeIn();
}

void AutoExposureOverlay::setOpacity(qreal opacity)
{
    m_opacity = opacity;
    update();
}

void AutoExposureOverlay::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // Set opacity
    painter.setOpacity(m_opacity);
    
    // Draw the exposure indicator (iPhone style)
    // Outer square
    QPen pen(QColor(255, 204, 0), 2); // Yellow/orange color
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    
    int margin = 4;
    QRect outerRect(margin, margin, SIZE - 2*margin, SIZE - 2*margin);
    painter.drawRect(outerRect);
    
    // Center sun icon
    int centerX = SIZE / 2;
    int centerY = SIZE / 2;
    int sunRadius = 8;
    
    // Draw sun circle
    painter.setBrush(QColor(255, 204, 0));
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(QPoint(centerX, centerY), sunRadius, sunRadius);
    
    // Draw sun rays
    painter.setPen(QPen(QColor(255, 204, 0), 2));
    int rayLength = 5;
    int rayOffset = sunRadius + 3;
    
    for (int i = 0; i < 8; i++) {
        double angle = i * M_PI / 4.0;
        int x1 = centerX + rayOffset * cos(angle);
        int y1 = centerY + rayOffset * sin(angle);
        int x2 = centerX + (rayOffset + rayLength) * cos(angle);
        int y2 = centerY + (rayOffset + rayLength) * sin(angle);
        painter.drawLine(x1, y1, x2, y2);
    }
}

void AutoExposureOverlay::startFadeIn()
{
    m_fadeInAnimation->start();
    
    // Start the display timer after fade in completes
    connect(m_fadeInAnimation, &QPropertyAnimation::finished, [this]() {
        m_displayTimer->start();
    });
}

void AutoExposureOverlay::startFadeOut()
{
    m_fadeOutAnimation->start();
}