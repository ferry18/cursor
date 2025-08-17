#include "MainWindow.h"
#include "CameraController.h"
#include "CameraWidget.h"
#include "ControlPanel.h"
#include <QMessageBox>
#include <QStatusBar>
#include <QHBoxLayout>
#include <QSplitter>
#include <QLabel>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    // Create camera controller
    m_cameraController = std::make_unique<CameraController>(this);
    
    // Initialize camera
    if (!m_cameraController->initialize()) {
        QMessageBox::critical(this, "Error", "Failed to initialize camera!");
        return;
    }
    
    setupUI();
    connectSignals();
    
    // Start camera capture
    if (!m_cameraController->startCapture()) {
        QMessageBox::critical(this, "Error", "Failed to start camera capture!");
    }
}

MainWindow::~MainWindow()
{
}

void MainWindow::setupUI()
{
    // Set window properties
    setWindowTitle("Toupcam Viewer - ATR2600M");
    resize(2560, 1600);
    
    // Create central widget
    QWidget* centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    
    // Create horizontal layout
    QHBoxLayout* layout = new QHBoxLayout(centralWidget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    
    // Create control panel
    m_controlPanel = new ControlPanel(m_cameraController.get(), this);
    
    // Create camera widget
    m_cameraWidget = new CameraWidget(this);
    m_cameraWidget->setMinimumSize(2389, 1600);
    
    // Add widgets to layout
    layout->addWidget(m_controlPanel);
    layout->addWidget(m_cameraWidget, 1);
    
    // Create status bar
    QStatusBar* statusBar = new QStatusBar(this);
    setStatusBar(statusBar);
    
    // Add temperature display to status bar
    QLabel* tempLabel = new QLabel("Temperature: --°C", statusBar);
    statusBar->addPermanentWidget(tempLabel);
    
    // Connect temperature updates
    connect(m_cameraController.get(), &CameraController::temperatureUpdated, 
            [tempLabel](float temp) {
        tempLabel->setText(QString("Temperature: %1°C").arg(temp, 0, 'f', 1));
    });
}

void MainWindow::connectSignals()
{
    // Connect camera frame updates
    connect(m_cameraController.get(), &CameraController::frameReady,
            m_cameraWidget, &CameraWidget::updateImage);
    
    // Connect FPS updates
    connect(m_cameraController.get(), &CameraController::fpsUpdated,
            m_cameraWidget, &CameraWidget::setFPS);
    
    // Connect auto exposure controls
    connect(m_cameraWidget, &CameraWidget::autoExposureRectRequested,
            [this](const QRect& rect) {
        m_cameraController->setAutoExposureRect(rect);
    });
    
    connect(m_cameraWidget, &CameraWidget::autoExposureReset,
            [this]() {
        m_cameraController->resetAutoExposureRect();
    });
    
    // Connect error handling
    connect(m_cameraController.get(), &CameraController::cameraError,
            [this](const QString& error) {
        QMessageBox::critical(this, "Camera Error", error);
    });
}