#ifndef AUTOEXPOSUREOVERLAY_H
#define AUTOEXPOSUREOVERLAY_H

#include <QWidget>
#include <QPropertyAnimation>
#include <QTimer>

class AutoExposureOverlay : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(qreal opacity READ opacity WRITE setOpacity)

public:
    explicit AutoExposureOverlay(QWidget *parent = nullptr);
    
    void showAt(const QPoint& position);
    
    qreal opacity() const { return m_opacity; }
    void setOpacity(qreal opacity);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void startFadeIn();
    void startFadeOut();
    
private:
    qreal m_opacity;
    QPropertyAnimation* m_fadeInAnimation;
    QPropertyAnimation* m_fadeOutAnimation;
    QTimer* m_displayTimer;
    
    static constexpr int SIZE = 50;
    static constexpr int FADE_IN_DURATION = 200;
    static constexpr int DISPLAY_DURATION = 5000;
    static constexpr int FADE_OUT_DURATION = 500;
};

#endif // AUTOEXPOSUREOVERLAY_H