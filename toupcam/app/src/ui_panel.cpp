#include "ui_panel.h"
#include <QVBoxLayout>
#include <QLabel>

UiPanel::UiPanel(CameraController* controller, QWidget* parent)
    : QWidget(parent)
    , m_controller(controller) {
    
    // Create vertical layout
    auto* layout = new QVBoxLayout(this);
    layout->setAlignment(Qt::AlignTop);
    layout->setSpacing(15);
    layout->setContentsMargins(10, 10, 10, 10);
    
    // Title
    auto* titleLabel = new QLabel("Camera Controls");
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet("font-weight: bold; font-size: 14px; padding: 10px;");
    layout->addWidget(titleLabel);
    
    // Create buttons
    m_shutterButton = createIconButton(":/icons/shutter_not_pressed", "Capture RAW Still");
    connect(m_shutterButton, &QToolButton::clicked, this, &UiPanel::onShutterClicked);
    layout->addWidget(m_shutterButton);
    
    m_recordButton = createIconButton(":/icons/record_not_pressed", "Start/Stop Recording");
    connect(m_recordButton, &QToolButton::clicked, this, &UiPanel::onRecordToggled);
    layout->addWidget(m_recordButton);
    
    // Separator
    auto* sep1 = new QFrame();
    sep1->setFrameShape(QFrame::HLine);
    sep1->setFrameShadow(QFrame::Sunken);
    layout->addWidget(sep1);
    
    m_gainButton = createIconButton(":/icons/gain_lcg", "Conversion Gain (LCG/HCG)");
    connect(m_gainButton, &QToolButton::clicked, this, &UiPanel::onGainToggled);
    layout->addWidget(m_gainButton);
    
    m_highFullWellButton = createIconButton(":/icons/high_full_well_0", "High Full Well Mode");
    connect(m_highFullWellButton, &QToolButton::clicked, this, &UiPanel::onHighFullWellToggled);
    layout->addWidget(m_highFullWellButton);
    
    // Separator
    auto* sep2 = new QFrame();
    sep2->setFrameShape(QFrame::HLine);
    sep2->setFrameShadow(QFrame::Sunken);
    layout->addWidget(sep2);
    
    m_coolingButton = createIconButton(":/icons/cooling_0", "Cooling (10°C)");
    connect(m_coolingButton, &QToolButton::clicked, this, &UiPanel::onCoolingCycled);
    layout->addWidget(m_coolingButton);
    
    m_heatingButton = createIconButton(":/icons/heating_0", "Heating (Off)");
    connect(m_heatingButton, &QToolButton::clicked, this, &UiPanel::onHeatingCycled);
    layout->addWidget(m_heatingButton);
    
    // Add stretch at bottom
    layout->addStretch();
    
    // Set fixed width
    setFixedWidth(DEFAULT_LEFT_PANEL_WIDTH);
    
    // Style the panel
    setStyleSheet(R"(
        UiPanel {
            background-color: #2b2b2b;
            border-right: 1px solid #555;
        }
        QToolButton {
            background-color: transparent;
            border: none;
            padding: 10px;
        }
        QToolButton:hover {
            background-color: #3a3a3a;
            border-radius: 5px;
        }
        QToolButton:pressed {
            background-color: #4a4a4a;
        }
    )");
    
    // Update initial state
    updateCameraState();
}

QToolButton* UiPanel::createIconButton(const QString& iconPath, const QString& tooltip) {
    auto* button = new QToolButton();
    button->setIcon(QIcon(iconPath));
    button->setIconSize(QSize(48, 48));
    button->setToolTip(tooltip);
    button->setCursor(Qt::PointingHandCursor);
    return button;
}

void UiPanel::updateButtonIcon(QToolButton* button, const QString& iconPath) {
    button->setIcon(QIcon(iconPath));
}

void UiPanel::onShutterClicked() {
    if (!m_controller->isOpen()) return;
    
    // Visual feedback
    updateButtonIcon(m_shutterButton, ":/icons/shutter_pressed");
    QTimer::singleShot(200, this, [this]() {
        updateButtonIcon(m_shutterButton, ":/icons/shutter_not_pressed");
    });
    
    // Capture still
    m_controller->captureStill();
    emit shutterClicked();
}

void UiPanel::onRecordToggled() {
    if (!m_controller->isOpen()) return;
    
    if (m_recording) {
        m_controller->stopRecording();
        m_recording = false;
        updateButtonIcon(m_recordButton, ":/icons/record_not_pressed");
        m_recordButton->setToolTip("Start Recording");
    } else {
        m_controller->startRecording();
        m_recording = true;
        updateButtonIcon(m_recordButton, ":/icons/record_pressed");
        m_recordButton->setToolTip("Stop Recording");
    }
}

void UiPanel::onGainToggled() {
    if (!m_controller->isOpen()) return;
    
    auto gain = m_controller->conversionGain();
    if (gain == CameraController::ConversionGain::LCG) {
        m_controller->setConversionGain(CameraController::ConversionGain::HCG);
        updateButtonIcon(m_gainButton, ":/icons/gain_hcg");
        m_gainButton->setToolTip("Conversion Gain (HCG)");
    } else {
        m_controller->setConversionGain(CameraController::ConversionGain::LCG);
        updateButtonIcon(m_gainButton, ":/icons/gain_lcg");
        m_gainButton->setToolTip("Conversion Gain (LCG)");
    }
}

void UiPanel::onHighFullWellToggled() {
    if (!m_controller->isOpen()) return;
    
    bool enabled = !m_controller->highFullWell();
    m_controller->setHighFullWell(enabled);
    updateButtonIcon(m_highFullWellButton, enabled ? ":/icons/high_full_well_1" : ":/icons/high_full_well_0");
    m_highFullWellButton->setToolTip(enabled ? "High Full Well Mode (On)" : "High Full Well Mode (Off)");
}

void UiPanel::onCoolingCycled() {
    if (!m_controller->isOpen()) return;
    
    auto level = m_controller->coolingLevel();
    CameraController::CoolingLevel nextLevel;
    QString iconPath;
    QString tooltip;
    
    switch (level) {
        case CameraController::CoolingLevel::Level10C:
            nextLevel = CameraController::CoolingLevel::Level0C;
            iconPath = ":/icons/cooling_1";
            tooltip = "Cooling (0°C)";
            break;
        case CameraController::CoolingLevel::Level0C:
            nextLevel = CameraController::CoolingLevel::LevelMinus15C;
            iconPath = ":/icons/cooling_2";
            tooltip = "Cooling (-15°C)";
            break;
        case CameraController::CoolingLevel::LevelMinus15C:
            nextLevel = CameraController::CoolingLevel::LevelMinus30C;
            iconPath = ":/icons/cooling_3";
            tooltip = "Cooling (-30°C)";
            break;
        case CameraController::CoolingLevel::LevelMinus30C:
            nextLevel = CameraController::CoolingLevel::Level10C;
            iconPath = ":/icons/cooling_0";
            tooltip = "Cooling (10°C)";
            break;
    }
    
    m_controller->setCoolingLevel(nextLevel);
    updateButtonIcon(m_coolingButton, iconPath);
    m_coolingButton->setToolTip(tooltip);
}

void UiPanel::onHeatingCycled() {
    if (!m_controller->isOpen()) return;
    
    auto level = m_controller->heatingLevel();
    CameraController::HeatingLevel nextLevel;
    QString iconPath;
    QString tooltip;
    
    switch (level) {
        case CameraController::HeatingLevel::Level0:
            nextLevel = CameraController::HeatingLevel::Level2;
            iconPath = ":/icons/heating_1";
            tooltip = "Heating (Level 2)";
            break;
        case CameraController::HeatingLevel::Level2:
            nextLevel = CameraController::HeatingLevel::Level3;
            iconPath = ":/icons/heating_2";
            tooltip = "Heating (Level 3)";
            break;
        case CameraController::HeatingLevel::Level3:
            nextLevel = CameraController::HeatingLevel::Level4;
            iconPath = ":/icons/heating_3";
            tooltip = "Heating (Level 4)";
            break;
        case CameraController::HeatingLevel::Level4:
            nextLevel = CameraController::HeatingLevel::Level0;
            iconPath = ":/icons/heating_0";
            tooltip = "Heating (Off)";
            break;
    }
    
    m_controller->setHeatingLevel(nextLevel);
    updateButtonIcon(m_heatingButton, iconPath);
    m_heatingButton->setToolTip(tooltip);
}

void UiPanel::updateRecordingState(bool recording) {
    m_recording = recording;
    updateButtonIcon(m_recordButton, recording ? ":/icons/record_pressed" : ":/icons/record_not_pressed");
    m_recordButton->setToolTip(recording ? "Stop Recording" : "Start Recording");
}

void UiPanel::updateCameraState() {
    if (!m_controller->isOpen()) {
        // Disable all buttons
        m_shutterButton->setEnabled(false);
        m_recordButton->setEnabled(false);
        m_gainButton->setEnabled(false);
        m_highFullWellButton->setEnabled(false);
        m_coolingButton->setEnabled(false);
        m_heatingButton->setEnabled(false);
    } else {
        // Enable all buttons
        m_shutterButton->setEnabled(true);
        m_recordButton->setEnabled(true);
        m_gainButton->setEnabled(true);
        m_highFullWellButton->setEnabled(true);
        m_coolingButton->setEnabled(true);
        m_heatingButton->setEnabled(true);
        
        // Update icons based on current state
        auto gain = m_controller->conversionGain();
        updateButtonIcon(m_gainButton, gain == CameraController::ConversionGain::LCG ? 
            ":/icons/gain_lcg" : ":/icons/gain_hcg");
        
        bool hfw = m_controller->highFullWell();
        updateButtonIcon(m_highFullWellButton, hfw ? ":/icons/high_full_well_1" : ":/icons/high_full_well_0");
        
        // Update cooling icon
        auto cooling = m_controller->coolingLevel();
        QString coolingIcon;
        switch (cooling) {
            case CameraController::CoolingLevel::Level10C: coolingIcon = ":/icons/cooling_0"; break;
            case CameraController::CoolingLevel::Level0C: coolingIcon = ":/icons/cooling_1"; break;
            case CameraController::CoolingLevel::LevelMinus15C: coolingIcon = ":/icons/cooling_2"; break;
            case CameraController::CoolingLevel::LevelMinus30C: coolingIcon = ":/icons/cooling_3"; break;
        }
        updateButtonIcon(m_coolingButton, coolingIcon);
        
        // Update heating icon
        auto heating = m_controller->heatingLevel();
        QString heatingIcon;
        switch (heating) {
            case CameraController::HeatingLevel::Level0: heatingIcon = ":/icons/heating_0"; break;
            case CameraController::HeatingLevel::Level2: heatingIcon = ":/icons/heating_1"; break;
            case CameraController::HeatingLevel::Level3: heatingIcon = ":/icons/heating_2"; break;
            case CameraController::HeatingLevel::Level4: heatingIcon = ":/icons/heating_3"; break;
        }
        updateButtonIcon(m_heatingButton, heatingIcon);
    }
}