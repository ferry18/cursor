#ifndef UI_PANEL_H
#define UI_PANEL_H

#include "utils.h"
#include "camera_controller.h"
#include <QToolButton>

class UiPanel : public QWidget {
    Q_OBJECT
    
public:
    explicit UiPanel(CameraController* controller, QWidget* parent = nullptr);
    
signals:
    void shutterClicked();
    
private slots:
    void onShutterClicked();
    void onRecordToggled();
    void onGainToggled();
    void onHighFullWellToggled();
    void onCoolingCycled();
    void onHeatingCycled();
    
    void updateRecordingState(bool recording);
    void updateCameraState();
    
private:
    QToolButton* createIconButton(const QString& iconPath, const QString& tooltip);
    void updateButtonIcon(QToolButton* button, const QString& iconPath);
    
private:
    CameraController* m_controller;
    
    // Buttons
    QToolButton* m_shutterButton;
    QToolButton* m_recordButton;
    QToolButton* m_gainButton;
    QToolButton* m_highFullWellButton;
    QToolButton* m_coolingButton;
    QToolButton* m_heatingButton;
    
    // State
    bool m_recording = false;
};

#endif // UI_PANEL_H