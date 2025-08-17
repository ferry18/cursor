#ifndef CONTROLPANEL_H
#define CONTROLPANEL_H

#include <QWidget>
#include <QVBoxLayout>
#include "IconButton.h"

class CameraController;

class ControlPanel : public QWidget
{
    Q_OBJECT

public:
    explicit ControlPanel(CameraController* controller, QWidget *parent = nullptr);

private:
    void setupUI();
    void connectSignals();
    
private:
    CameraController* m_controller;
    
    // Control buttons
    IconButton* m_gainSwitchBtn;      // LCG/HCG
    IconButton* m_highFullWellBtn;    // High Full Well mode
    IconButton* m_heatingBtn;         // Heating control
    IconButton* m_coolingBtn;         // Cooling control
    IconButton* m_denoiseBtn;         // Denoise (not implemented yet)
    IconButton* m_lowNoiseBtn;        // Low noise mode (not implemented yet)
    IconButton* m_shutterBtn;         // Photo capture
    IconButton* m_recordBtn;          // Video recording
    
    QVBoxLayout* m_layout;
};

#endif // CONTROLPANEL_H