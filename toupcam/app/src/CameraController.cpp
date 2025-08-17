#include "CameraController.h"
#include <QDateTime>
#include <QDir>
#include <QDebug>
#include <QRect>
#include <QThread>
#include <QFile>
#include <sched.h>

CameraController::CameraController(QObject *parent)
    : QObject(parent)
    , m_hcam(nullptr)
    , m_imageWidth(3104)
    , m_imageHeight(2084)
    , m_isRecording(false)
    , m_frameCount(0)
    , m_currentTempIndex(0)
    , m_heatingLevel(0)
    , m_highFullWellEnabled(false)
    , m_conversionGain(0)
{
    m_fpsTimer = new QTimer(this);
    m_fpsTimer->setInterval(1000); // Update FPS every second
    connect(m_fpsTimer, &QTimer::timeout, this, &CameraController::updateFPS);
    
    m_tempTimer = new QTimer(this);
    m_tempTimer->setInterval(2000); // Check temperature every 2 seconds
    connect(m_tempTimer, &QTimer::timeout, this, [this]() {
        if (m_hcam) {
            short temp;
            if (SUCCEEDED(Toupcam_get_Temperature(m_hcam, &temp))) {
                emit temperatureUpdated(temp / 10.0f);
            }
        }
    });
}

CameraController::~CameraController()
{
    shutdown();
}

bool CameraController::initialize()
{
    // Enumerate cameras
    ToupcamDeviceV2 arr[TOUPCAM_MAX];
    unsigned count = Toupcam_EnumV2(arr);
    if (count == 0) {
        emit cameraError("No camera found");
        return false;
    }
    
    // Store device info
    m_device = arr[0];
    
    // Open camera
    m_hcam = Toupcam_Open(m_device.id);
    if (!m_hcam) {
        emit cameraError("Failed to open camera");
        return false;
    }
    
    // Set resolution to index 1 (3104x2084)
    HRESULT hr = Toupcam_put_eSize(m_hcam, 1);
    if (FAILED(hr)) {
        emit cameraError("Failed to set resolution");
        Toupcam_Close(m_hcam);
        m_hcam = nullptr;
        return false;
    }
    
    // Get actual resolution
    Toupcam_get_Size(m_hcam, &m_imageWidth, &m_imageHeight);
    
    // Allocate image buffer
    m_imageBuffer.reset(new unsigned char[m_imageWidth * m_imageHeight]);
    
    // Configure camera options
    setupCameraOptions();
    
    // Set default auto-exposure rectangle
    setDefaultAutoExposureRect();
    
    return true;
}

void CameraController::setupCameraOptions()
{
    if (!m_hcam) return;
    
    // IWR Readout Mode
    Toupcam_put_Option(m_hcam, TOUPCAM_OPTION_READOUT_MODE, 0);
    
    // High priority thread (Linux SCHED_FIFO)
    int priority = (SCHED_FIFO << 16) | sched_get_priority_max(SCHED_FIFO);
    Toupcam_put_Option(m_hcam, TOUPCAM_OPTION_THREAD_PRIORITY, priority);
    
    // Auto exposure damping
    Toupcam_put_Option(m_hcam, TOUPCAM_OPTION_AUTOEXP_EXPOTIME_DAMP, 0);
    
    // Disable tail light
    Toupcam_put_Option(m_hcam, TOUPCAM_OPTION_TAILLIGHT, 0);
    
    // High full well mode (default off)
    Toupcam_put_Option(m_hcam, TOUPCAM_OPTION_HIGH_FULLWELL, 0);
    
    // Process mode (not RAW)
    Toupcam_put_Option(m_hcam, TOUPCAM_OPTION_RAW, 0);
    
    // 8-bit mode
    Toupcam_put_Option(m_hcam, TOUPCAM_OPTION_BITDEPTH, 0);
    
    // RGB mode for grayscale
    Toupcam_put_Option(m_hcam, TOUPCAM_OPTION_RGB, 3); // 8 Bits Grey
    
    // LCG mode (default)
    Toupcam_put_Option(m_hcam, TOUPCAM_OPTION_CG, 0);
    
    // Heating off
    Toupcam_put_Option(m_hcam, TOUPCAM_OPTION_HEAT, 0);
    
    // Enable TEC and set to 10°C default
    Toupcam_put_Option(m_hcam, TOUPCAM_OPTION_TEC, 1);
    Toupcam_put_Option(m_hcam, TOUPCAM_OPTION_TECTARGET, 100);
    
    // Enable auto exposure by default
    Toupcam_put_AutoExpoEnable(m_hcam, 1);
}

void CameraController::setDefaultAutoExposureRect()
{
    if (!m_hcam) return;
    
    // 70% of 3104x2084 = 2172x1458
    RECT aeRect;
    aeRect.left = (m_imageWidth - (m_imageWidth * 70 / 100)) / 2;
    aeRect.top = (m_imageHeight - (m_imageHeight * 70 / 100)) / 2;
    aeRect.right = aeRect.left + (m_imageWidth * 70 / 100);
    aeRect.bottom = aeRect.top + (m_imageHeight * 70 / 100);
    
    Toupcam_put_AEAuxRect(m_hcam, &aeRect);
}

bool CameraController::startCapture()
{
    if (!m_hcam) return false;
    
    HRESULT hr = Toupcam_StartPullModeWithCallback(m_hcam, eventCallback, this);
    if (SUCCEEDED(hr)) {
        m_fpsTimer->start();
        m_tempTimer->start();
        return true;
    }
    
    emit cameraError("Failed to start capture");
    return false;
}

void CameraController::stopCapture()
{
    if (m_hcam) {
        Toupcam_Stop(m_hcam);
        m_fpsTimer->stop();
        m_tempTimer->stop();
    }
}

void CameraController::shutdown()
{
    stopCapture();
    
    if (m_hcam) {
        Toupcam_Close(m_hcam);
        m_hcam = nullptr;
    }
}

void CameraController::eventCallback(unsigned nEvent, void* pCallbackCtx)
{
    CameraController* pThis = static_cast<CameraController*>(pCallbackCtx);
    
    switch (nEvent) {
    case TOUPCAM_EVENT_IMAGE:
        pThis->handleImageEvent();
        break;
    case TOUPCAM_EVENT_EXPOSURE:
        pThis->handleExpoEvent();
        break;
    case TOUPCAM_EVENT_TEMPTINT:
        pThis->handleTempTintEvent();
        break;
    case TOUPCAM_EVENT_ERROR:
        pThis->handleError();
        break;
    case TOUPCAM_EVENT_DISCONNECTED:
        pThis->handleDisconnected();
        break;
    }
}

void CameraController::handleImageEvent()
{
    ToupcamFrameInfoV4 info = {0};
    
    QMutexLocker locker(&m_bufferMutex);
    
    HRESULT hr = Toupcam_PullImageV4(m_hcam, m_imageBuffer.get(), 0, 8, 0, &info);
    if (SUCCEEDED(hr)) {
        // Create QImage from the buffer (grayscale 8-bit)
        QImage image(m_imageBuffer.get(), info.v3.width, info.v3.height, 
                     info.v3.width, QImage::Format_Grayscale8);
        
        // Make a deep copy for thread safety
        QImage imageCopy = image.copy();
        
        // Save frame if recording
        if (m_isRecording) {
            saveVideoFrame(m_imageBuffer.get(), info.v3.width, info.v3.height);
        }
        
        emit frameReady(imageCopy);
    }
}

void CameraController::handleExpoEvent()
{
    // Exposure changed event
}

void CameraController::handleTempTintEvent()
{
    // Temperature/tint changed event
}

void CameraController::handleError()
{
    emit cameraError("Camera error occurred");
}

void CameraController::handleDisconnected()
{
    emit cameraError("Camera disconnected");
    shutdown();
}

void CameraController::updateFPS()
{
    if (!m_hcam) return;
    
    unsigned nFrame, nTime, nTotalFrame;
    if (SUCCEEDED(Toupcam_get_FrameRate(m_hcam, &nFrame, &nTime, &nTotalFrame))) {
        if (nTime > 0) {
            float fps = static_cast<float>(nFrame) * 1000.0f / nTime;
            emit fpsUpdated(fps);
        }
    }
}

void CameraController::setConversionGain(int mode)
{
    if (m_hcam && mode != m_conversionGain) {
        m_conversionGain = mode;
        Toupcam_put_Option(m_hcam, TOUPCAM_OPTION_CG, mode);
    }
}

void CameraController::setHighFullWell(bool enabled)
{
    if (m_hcam && enabled != m_highFullWellEnabled) {
        m_highFullWellEnabled = enabled;
        Toupcam_put_Option(m_hcam, TOUPCAM_OPTION_HIGH_FULLWELL, enabled ? 1 : 0);
    }
}

void CameraController::setHeating(int level)
{
    if (m_hcam && level >= 0 && level <= 4) {
        m_heatingLevel = level;
        Toupcam_put_Option(m_hcam, TOUPCAM_OPTION_HEAT, level);
    }
}

void CameraController::setCoolingTemperature(int index)
{
    if (!m_hcam || index < 0 || index > 3) return;
    
    const int tempTargets[] = {100, 0, -150, -300}; // in 0.1°C units
    m_currentTempIndex = index;
    
    // Enable TEC if not already enabled
    Toupcam_put_Option(m_hcam, TOUPCAM_OPTION_TEC, 1);
    
    // Set target temperature
    Toupcam_put_Option(m_hcam, TOUPCAM_OPTION_TECTARGET, tempTargets[index]);
}

void CameraController::setAutoExposureRect(const QRect& rect)
{
    if (!m_hcam) return;
    
    RECT aeRect;
    aeRect.left = rect.left();
    aeRect.top = rect.top();
    aeRect.right = rect.right();
    aeRect.bottom = rect.bottom();
    
    Toupcam_put_AEAuxRect(m_hcam, &aeRect);
}

void CameraController::resetAutoExposureRect()
{
    setDefaultAutoExposureRect();
}

void CameraController::capturePhoto()
{
    QMutexLocker locker(&m_bufferMutex);
    if (m_imageBuffer) {
        savePhoto(m_imageBuffer.get(), m_imageWidth, m_imageHeight);
    }
}

void CameraController::startRecording()
{
    if (!m_isRecording) {
        // Create recording directory
        QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
        m_recordingDir = QString("recordings/rec_%1").arg(timestamp);
        QDir().mkpath(m_recordingDir);
        
        m_frameCount = 0;
        m_isRecording = true;
    }
}

void CameraController::stopRecording()
{
    if (m_isRecording) {
        m_isRecording = false;
        
        // Save metadata
        QString metadataPath = m_recordingDir + "/metadata.json";
        QFile metadataFile(metadataPath);
        if (metadataFile.open(QIODevice::WriteOnly)) {
            QString metadata = QString(
                "{\n"
                "  \"width\": %1,\n"
                "  \"height\": %2,\n"
                "  \"frames\": %3,\n"
                "  \"format\": \"PNG\"\n"
                "}\n"
            ).arg(m_imageWidth).arg(m_imageHeight).arg(m_frameCount.load());
            
            metadataFile.write(metadata.toUtf8());
            metadataFile.close();
        }
    }
}

void CameraController::savePhoto(const unsigned char* data, int width, int height)
{
    QImage image(data, width, height, width, QImage::Format_Grayscale8);
    
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    QString filename = QString("photos/capture_%1.png").arg(timestamp);
    
    // Ensure directory exists
    QDir().mkpath("photos");
    
    image.save(filename, "PNG", 100);
}

void CameraController::saveVideoFrame(const unsigned char* data, int width, int height)
{
    // Run in separate thread to avoid blocking
    QThread* thread = QThread::create([this, width, height, frameNum = m_frameCount.load()]() {
        QImage image(m_imageBuffer.get(), width, height, width, QImage::Format_Grayscale8);
        
        QString filename = QString("%1/frame_%2.png")
            .arg(m_recordingDir)
            .arg(frameNum, 5, 10, QChar('0'));
        
        image.save(filename, "PNG", 100);
    });
    
    thread->start();
    m_frameCount++;
}