#include "ControlPanel.h"
#include "CameraController.h"
#include <QLabel>

ControlPanel::ControlPanel(CameraController* controller, QWidget *parent)
    : QWidget(parent)
    , m_controller(controller)
{
    setupUI();
    connectSignals();
}

void ControlPanel::setupUI()
{
    setFixedWidth(171);
    setStyleSheet("background-color: #2B2B2B;");
    
    m_layout = new QVBoxLayout(this);
    m_layout->setSpacing(10);
    m_layout->setContentsMargins(10, 10, 10, 10);
    
    // Title
    QLabel* title = new QLabel("Controls");
    title->setStyleSheet("color: white; font-size: 16px; font-weight: bold;");
    title->setAlignment(Qt::AlignCenter);
    m_layout->addWidget(title);
    
    m_layout->addSpacing(20);
    
    // Gain switch button (LCG/HCG)
    m_gainSwitchBtn = new IconButton(this);
    m_gainSwitchBtn->setStateCount(2);
    m_gainSwitchBtn->addStateIcon(0, "resources/icons/gain_switch/gain_switch_LCG.svg");
    m_gainSwitchBtn->addStateIcon(1, "resources/icons/gain_switch/gain_switch_HCG.svg");
    m_gainSwitchBtn->setToolTip("Conversion Gain: LCG/HCG");
    m_layout->addWidget(m_gainSwitchBtn, 0, Qt::AlignCenter);
    
    // High Full Well button
    m_highFullWellBtn = new IconButton(this);
    m_highFullWellBtn->setStateCount(2);
    m_highFullWellBtn->addStateIcon(0, "resources/icons/high-full_well_button/high_full_well_0.svg");
    m_highFullWellBtn->addStateIcon(1, "resources/icons/high-full_well_button/high_full_well_1.svg");
    m_highFullWellBtn->setToolTip("High Full Well Mode");
    m_layout->addWidget(m_highFullWellBtn, 0, Qt::AlignCenter);
    
    // Heating button
    m_heatingBtn = new IconButton(this);
    m_heatingBtn->setStateCount(4);
    m_heatingBtn->addStateIcon(0, "resources/icons/heating_button/heating_button_level_0.svg");
    m_heatingBtn->addStateIcon(1, "resources/icons/heating_button/heating_button_level_1.svg");
    m_heatingBtn->addStateIcon(2, "resources/icons/heating_button/heating_button_level_2.svg");
    m_heatingBtn->addStateIcon(3, "resources/icons/heating_button/heating_button_level_3.svg");
    m_heatingBtn->setToolTip("Heating Level");
    m_layout->addWidget(m_heatingBtn, 0, Qt::AlignCenter);
    
    // Cooling button
    m_coolingBtn = new IconButton(this);
    m_coolingBtn->setStateCount(4);
    m_coolingBtn->addStateIcon(0, "resources/icons/cooling_button/cooling_button_level_0.svg");
    m_coolingBtn->addStateIcon(1, "resources/icons/cooling_button/cooling_button_level_1.svg");
    m_coolingBtn->addStateIcon(2, "resources/icons/cooling_button/cooling_button_level_2.svg");
    m_coolingBtn->addStateIcon(3, "resources/icons/cooling_button/cooling_button_level_3.svg");
    m_coolingBtn->setToolTip("Cooling: 10°C / 0°C / -15°C / -30°C");
    m_layout->addWidget(m_coolingBtn, 0, Qt::AlignCenter);
    
    // Denoise button (placeholder)
    m_denoiseBtn = new IconButton(this);
    m_denoiseBtn->setStateCount(4);
    m_denoiseBtn->addStateIcon(0, "resources/icons/denoise_button/denoise_button_0.svg");
    m_denoiseBtn->addStateIcon(1, "resources/icons/denoise_button/denoise_button_25.svg");
    m_denoiseBtn->addStateIcon(2, "resources/icons/denoise_button/denoise_button_50.svg");
    m_denoiseBtn->addStateIcon(3, "resources/icons/denoise_button/denoise_button_75.svg");
    m_denoiseBtn->setToolTip("Denoise Level");
    m_denoiseBtn->setEnabled(false); // Not implemented yet
    m_layout->addWidget(m_denoiseBtn, 0, Qt::AlignCenter);
    
    // Low noise mode button (placeholder)
    m_lowNoiseBtn = new IconButton(this);
    m_lowNoiseBtn->setStateCount(2);
    m_lowNoiseBtn->addStateIcon(0, "resources/icons/low_noise_mode_button/low_noise_mode_button_0.svg");
    m_lowNoiseBtn->addStateIcon(1, "resources/icons/low_noise_mode_button/low_noise_mode_button_1.svg");
    m_lowNoiseBtn->setToolTip("Low Noise Mode");
    m_lowNoiseBtn->setEnabled(false); // Not implemented yet
    m_layout->addWidget(m_lowNoiseBtn, 0, Qt::AlignCenter);
    
    m_layout->addSpacing(20);
    
    // Shutter button
    m_shutterBtn = new IconButton(this);
    m_shutterBtn->setStateCount(1); // Single state, no cycling
    m_shutterBtn->addStateIcon(0, "resources/icons/shutter_button/shutter_button_not_pressed.svg");
    m_shutterBtn->setToolTip("Capture Photo");
    m_layout->addWidget(m_shutterBtn, 0, Qt::AlignCenter);
    
    // Record button
    m_recordBtn = new IconButton(this);
    m_recordBtn->setStateCount(2);
    m_recordBtn->addStateIcon(0, "resources/icons/record_button/record_button_not_pressed.svg");
    m_recordBtn->addStateIcon(1, "resources/icons/record_button/record_button_pressed.svg");
    m_recordBtn->setToolTip("Record Video");
    m_layout->addWidget(m_recordBtn, 0, Qt::AlignCenter);
    
    m_layout->addStretch();
}

void ControlPanel::connectSignals()
{
    // Gain switch
    connect(m_gainSwitchBtn, &IconButton::stateChanged, [this](int state) {
        m_controller->setConversionGain(state);
    });
    
    // High Full Well
    connect(m_highFullWellBtn, &IconButton::stateChanged, [this](int state) {
        m_controller->setHighFullWell(state == 1);
    });
    
    // Heating
    connect(m_heatingBtn, &IconButton::stateChanged, [this](int state) {
        // State 0 = off, 1 = level 2, 2 = level 3, 3 = level 4
        int heatingLevel = (state == 0) ? 0 : state + 1;
        m_controller->setHeating(heatingLevel);
    });
    
    // Cooling
    connect(m_coolingBtn, &IconButton::stateChanged, [this](int state) {
        m_controller->setCoolingTemperature(state);
    });
    
    // Shutter (photo capture)
    connect(m_shutterBtn, &IconButton::clicked, [this]() {
        m_controller->capturePhoto();
    });
    
    // Record
    connect(m_recordBtn, &IconButton::stateChanged, [this](int state) {
        if (state == 1) {
            m_controller->startRecording();
        } else {
            m_controller->stopRecording();
        }
    });
}