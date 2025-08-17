#include "app_main_window.h"
#include "camera_controller.h"
#include "image_view.h"
#include "ui_panel.h"
#include <QHBoxLayout>
#include <QStatusBar>
#include <QMessageBox>
#include <QCloseEvent>

AppMainWindow::AppMainWindow(QWidget* parent)
    : QMainWindow(parent) {
    
    // Create components
    m_controller = new CameraController(this);
    m_imageView = new ImageView(this);
    m_uiPanel = new UiPanel(m_controller, this);
    
    setupUi();
    connectSignals();
    
    // Set window properties
    setWindowTitle("Toupcam ATR2600M");
    resize(DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT);
    
    // Style
    setStyleSheet(R"(
        QMainWindow {
            background-color: #1e1e1e;
        }
        QStatusBar {
            background-color: #2b2b2b;
            color: #ffffff;
        }
        QStatusBar QLabel {
            margin: 0 10px;
        }
    )");
    
    // Open camera automatically
    QTimer::singleShot(100, this, [this]() {
        if (m_controller->openCamera()) {
            m_statusLabel->setText("Camera opened successfully");
        }
    });
}

AppMainWindow::~AppMainWindow() = default;

void AppMainWindow::setupUi() {
    // Create central widget with horizontal layout
    auto* centralWidget = new QWidget(this);
    auto* layout = new QHBoxLayout(centralWidget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    
    // Add UI panel (left) and image view (right)
    layout->addWidget(m_uiPanel);
    layout->addWidget(m_imageView, 1); // Image view gets stretch factor
    
    setCentralWidget(centralWidget);
    
    // Setup status bar
    m_statusLabel = new QLabel("Initializing...");
    m_fpsLabel = new QLabel("FPS: --");
    m_zoomLabel = new QLabel("Zoom: 100%");
    
    statusBar()->addWidget(m_statusLabel);
    statusBar()->addPermanentWidget(m_fpsLabel);
    statusBar()->addPermanentWidget(m_zoomLabel);
}

void AppMainWindow::connectSignals() {
    // Camera controller signals
    connect(m_controller, &CameraController::cameraOpened, 
            this, &AppMainWindow::onCameraOpened);
    connect(m_controller, &CameraController::cameraClosed, 
            this, &AppMainWindow::onCameraClosed);
    connect(m_controller, &CameraController::error, 
            this, &AppMainWindow::onCameraError);
    connect(m_controller, &CameraController::frameReady, 
            this, &AppMainWindow::onFrameReady);
    connect(m_controller, &CameraController::frameRateUpdated, 
            this, &AppMainWindow::onFrameRateUpdated);
    connect(m_controller, &CameraController::stillCaptured, 
            this, &AppMainWindow::onStillCaptured);
    
    // Image view signals
    connect(m_imageView, &ImageView::clicked, 
            this, &AppMainWindow::onImageClicked);
    connect(m_imageView, &ImageView::doubleClicked, 
            this, &AppMainWindow::onImageDoubleClicked);
    connect(m_imageView, &ImageView::zoomChanged, 
            this, [this](double zoom) {
        m_zoomLabel->setText(QString("Zoom: %1%").arg(int(zoom * 100)));
    });
}

void AppMainWindow::onCameraOpened() {
    m_statusLabel->setText("Camera connected");
    m_uiPanel->updateCameraState();
}

void AppMainWindow::onCameraClosed() {
    m_statusLabel->setText("Camera disconnected");
    m_fpsLabel->setText("FPS: --");
    m_uiPanel->updateCameraState();
}

void AppMainWindow::onCameraError(const QString& error) {
    m_statusLabel->setText(QString("Error: %1").arg(error));
    QMessageBox::critical(this, "Camera Error", error);
}

void AppMainWindow::onFrameReady() {
    // Get latest frame from buffer
    auto frame = m_controller->frameBuffer()->getReadFrame();
    if (frame) {
        // Update image view with RAW8 data
        m_imageView->updateImage(frame->data.data(), 
                                frame->info.v3.width, 
                                frame->info.v3.height);
        m_controller->frameBuffer()->releaseReadFrame();
    }
}

void AppMainWindow::onImageClicked(const QPointF& imagePos) {
    // Set manual AE ROI at click position
    int roiX = static_cast<int>(imagePos.x() - AE_ROI_SIZE / 2);
    int roiY = static_cast<int>(imagePos.y() - AE_ROI_SIZE / 2);
    
    // Clamp to image bounds
    roiX = std::max(0, std::min(roiX, CAMERA_WIDTH - AE_ROI_SIZE));
    roiY = std::max(0, std::min(roiY, CAMERA_HEIGHT - AE_ROI_SIZE));
    
    // Make even
    makeEven(roiX);
    makeEven(roiY);
    
    QRect aeRoi(roiX, roiY, AE_ROI_SIZE, AE_ROI_SIZE);
    m_controller->setAutoExposureROI(aeRoi);
    
    // Show AE icon animation
    m_imageView->showAEIcon(QPoint(imagePos.x(), imagePos.y()));
    
    m_statusLabel->setText(QString("AE ROI set at (%1, %2)").arg(roiX).arg(roiY));
}

void AppMainWindow::onImageDoubleClicked(const QPointF& imagePos) {
    // Reset to default AE ROI
    m_controller->resetAutoExposureROI();
    m_statusLabel->setText("AE ROI reset to default (70% x 70%)");
}

void AppMainWindow::onFrameRateUpdated(double fps) {
    m_fpsLabel->setText(QString("FPS: %1").arg(fps, 0, 'f', 1));
}

void AppMainWindow::onStillCaptured(const QString& filename) {
    m_statusLabel->setText(QString("Still captured: %1").arg(filename));
}

void AppMainWindow::closeEvent(QCloseEvent* event) {
    // Ensure camera is closed properly
    m_controller->closeCamera();
    event->accept();
}