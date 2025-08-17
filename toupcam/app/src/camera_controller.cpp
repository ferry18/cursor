#include "camera_controller.h"

CameraController::CameraController(QObject* parent)
    : QObject(parent)
    , m_frameBuffer(std::make_unique<FrameBuffer>())
    , m_frameRateTimer(new QTimer(this))
    , m_lastFpsTime(std::chrono::steady_clock::now()) {
    
    // Enable GigE support
    Toupcam_GigeEnable(nullptr, nullptr);
    
    // Setup frame rate timer
    connect(m_frameRateTimer, &QTimer::timeout, this, [this]() {
        if (m_hcam) {
            unsigned nFrame = 0, nTime = 0, nTotalFrame = 0;
            if (SUCCEEDED(Toupcam_get_FrameRate(m_hcam, &nFrame, &nTime, &nTotalFrame)) && nTime > 0) {
                m_frameRate = nFrame * 1000.0 / nTime;
                emit frameRateUpdated(m_frameRate);
            }
        }
    });
    
    // Create directories
    ensureDirectoryExists("/photos");
    ensureDirectoryExists("/recordings");
}

CameraController::~CameraController() {
    closeCamera();
}

bool CameraController::openCamera() {
    // Enumerate devices
    ToupcamDeviceV2 devices[TOUPCAM_MAX] = {0};
    unsigned count = Toupcam_EnumV2(devices);
    
    if (count == 0) {
        emit error("No camera found");
        return false;
    }
    
    // Use first device
    m_device = devices[0];
    
    // Open camera
    m_hcam = Toupcam_Open(m_device.id);
    if (!m_hcam) {
        emit error("Failed to open camera");
        return false;
    }
    
    // Initialize camera settings
    if (!initializeCamera()) {
        closeCamera();
        return false;
    }
    
    // Start camera
    HRESULT hr = Toupcam_StartPullModeWithCallback(m_hcam, eventCallback, this);
    if (FAILED(hr)) {
        emit error(QString("Failed to start camera: 0x%1").arg(hr, 8, 16, QChar('0')));
        closeCamera();
        return false;
    }
    
    // Start frame rate monitoring
    m_frameRateTimer->start(1000);
    
    emit cameraOpened();
    return true;
}

void CameraController::closeCamera() {
    m_frameRateTimer->stop();
    
    // Stop recording if active
    if (m_recording) {
        stopRecording();
    }
    
    if (m_hcam) {
        Toupcam_Close(m_hcam);
        m_hcam = nullptr;
    }
    
    emit cameraClosed();
}

bool CameraController::initializeCamera() {
    HRESULT hr;
    
    // Set IWR readout mode
    hr = Toupcam_put_Option(m_hcam, TOUPCAM_OPTION_READOUT_MODE, 0);
    if (FAILED(hr)) {
        emit error("Failed to set IWR readout mode");
        return false;
    }
    
    // Set auto exposure time damping to 0
    hr = Toupcam_put_Option(m_hcam, TOUPCAM_OPTION_AUTOEXP_EXPOTIME_DAMP, 0);
    if (FAILED(hr)) {
        emit error("Failed to set auto exposure damping");
        return false;
    }
    
    // Disable tail light
    hr = Toupcam_put_Option(m_hcam, TOUPCAM_OPTION_TAILLIGHT, 0);
    if (FAILED(hr)) {
        emit error("Failed to disable tail light");
        return false;
    }
    
    // Set RAW8 mode
    hr = Toupcam_put_Option(m_hcam, TOUPCAM_OPTION_RAW, 1);
    if (FAILED(hr)) {
        emit error("Failed to set RAW mode");
        return false;
    }
    
    hr = Toupcam_put_Option(m_hcam, TOUPCAM_OPTION_BITDEPTH, 0);
    if (FAILED(hr)) {
        emit error("Failed to set 8-bit depth");
        return false;
    }
    
    // Set USB thread priority (Linux SCHED_FIFO with max priority)
    int priority = (1 << 16) | 99; // SCHED_FIFO = 1, max priority = 99
    hr = Toupcam_put_Option(m_hcam, TOUPCAM_OPTION_THREAD_PRIORITY, priority);
    // Not critical if this fails
    
    // Enable callback thread
    hr = Toupcam_put_Option(m_hcam, TOUPCAM_OPTION_CALLBACK_THREAD, 1);
    
    // Set pipeline buffers
    hr = Toupcam_put_Option(m_hcam, TOUPCAM_OPTION_FRONTEND_DEQUE_LENGTH, 4);
    hr = Toupcam_put_Option(m_hcam, TOUPCAM_OPTION_BACKEND_DEQUE_LENGTH, 3);
    
    // Set hardware binning to 2x2
    hr = Toupcam_put_Binning(m_hcam, "2x2", "Average");
    if (FAILED(hr)) {
        emit error("Failed to set hardware binning");
        return false;
    }
    
    // Set resolution index 1 (3104x2084)
    hr = Toupcam_put_eSize(m_hcam, 1);
    if (FAILED(hr)) {
        emit error("Failed to set resolution");
        return false;
    }
    
    // Verify final size
    int width = 0, height = 0;
    hr = Toupcam_get_FinalSize(m_hcam, &width, &height);
    if (FAILED(hr) || width != CAMERA_WIDTH || height != CAMERA_HEIGHT) {
        emit error(QString("Unexpected camera resolution: %1x%2").arg(width).arg(height));
        return false;
    }
    
    // Set initial camera controls
    setConversionGain(ConversionGain::LCG);
    setHighFullWell(false);
    setCoolingLevel(CoolingLevel::Level10C);
    setHeatingLevel(HeatingLevel::Level0);
    
    // Set default AE ROI (70% x 70%)
    resetAutoExposureROI();
    
    // Enable auto exposure
    hr = Toupcam_put_AutoExpoEnable(m_hcam, 1);
    
    // Flush any residual frames
    hr = Toupcam_put_Option(m_hcam, TOUPCAM_OPTION_FLUSH, 3);
    
    return true;
}

void CameraController::setConversionGain(ConversionGain gain) {
    if (!m_hcam) return;
    
    m_conversionGain = gain;
    int value = (gain == ConversionGain::LCG) ? 0 : 1;
    Toupcam_put_Option(m_hcam, TOUPCAM_OPTION_CG, value);
}

void CameraController::setHighFullWell(bool enabled) {
    if (!m_hcam) return;
    
    m_highFullWell = enabled;
    Toupcam_put_Option(m_hcam, TOUPCAM_OPTION_HIGH_FULLWELL, enabled ? 1 : 0);
}

void CameraController::setCoolingLevel(CoolingLevel level) {
    if (!m_hcam) return;
    
    m_coolingLevel = level;
    int temp = 0;
    switch (level) {
        case CoolingLevel::Level10C: temp = 100; break;
        case CoolingLevel::Level0C: temp = 0; break;
        case CoolingLevel::LevelMinus15C: temp = -150; break;
        case CoolingLevel::LevelMinus30C: temp = -300; break;
    }
    
    Toupcam_put_Option(m_hcam, TOUPCAM_OPTION_TECTARGET, temp);
    Toupcam_put_Option(m_hcam, TOUPCAM_OPTION_TEC, 1); // Enable TEC
}

void CameraController::setHeatingLevel(HeatingLevel level) {
    if (!m_hcam) return;
    
    m_heatingLevel = level;
    int heat = 0;
    switch (level) {
        case HeatingLevel::Level0: heat = 0; break;
        case HeatingLevel::Level2: heat = 2; break;
        case HeatingLevel::Level3: heat = 3; break;
        case HeatingLevel::Level4: heat = 4; break;
    }
    
    Toupcam_put_Option(m_hcam, TOUPCAM_OPTION_HEAT, heat);
}

void CameraController::setAutoExposureROI(const QRect& roi) {
    if (!m_hcam) return;
    
    // Ensure even dimensions
    QRect evenRoi = makeEvenRect(roi);
    
    // Convert to RECT structure
    RECT rect;
    rect.left = evenRoi.left();
    rect.top = evenRoi.top();
    rect.right = evenRoi.right();
    rect.bottom = evenRoi.bottom();
    
    HRESULT hr = Toupcam_put_AEAuxRect(m_hcam, &rect);
    if (SUCCEEDED(hr)) {
        m_aeROI = evenRoi;
    }
}

void CameraController::resetAutoExposureROI() {
    // Calculate 70% x 70% ROI centered
    int roiWidth = static_cast<int>(CAMERA_WIDTH * DEFAULT_AE_ROI_PERCENT);
    int roiHeight = static_cast<int>(CAMERA_HEIGHT * DEFAULT_AE_ROI_PERCENT);
    makeEven(roiWidth);
    makeEven(roiHeight);
    
    int x = (CAMERA_WIDTH - roiWidth) / 2;
    int y = (CAMERA_HEIGHT - roiHeight) / 2;
    makeEven(x);
    makeEven(y);
    
    setAutoExposureROI(QRect(x, y, roiWidth, roiHeight));
}

void CameraController::captureStill() {
    if (!m_hcam) return;
    
    // Use SnapR for raw capture at current resolution
    HRESULT hr = Toupcam_SnapR(m_hcam, 1, 1); // Resolution index 1, single frame
    if (FAILED(hr)) {
        emit error("Failed to capture still image");
    }
}

void CameraController::startRecording() {
    if (m_recording || !m_hcam) return;
    
    m_recording = true;
    
    // Start recording thread
    m_recordingThread = std::make_unique<std::thread>([this]() {
        while (m_recording) {
            std::unique_lock<std::mutex> lock(m_recordingMutex);
            m_recordingCv.wait(lock, [this]() { return !m_recordingQueue.empty() || !m_recording; });
            
            while (!m_recordingQueue.empty()) {
                auto frame = std::move(m_recordingQueue.front());
                m_recordingQueue.pop_front();
                lock.unlock();
                
                writeFrameToFile(frame.get());
                
                lock.lock();
            }
        }
    });
}

void CameraController::stopRecording() {
    if (!m_recording) return;
    
    m_recording = false;
    m_recordingCv.notify_all();
    
    if (m_recordingThread && m_recordingThread->joinable()) {
        m_recordingThread->join();
    }
    m_recordingThread.reset();
}

void CameraController::eventCallback(unsigned nEvent, void* pCallbackCtx) {
    CameraController* self = static_cast<CameraController*>(pCallbackCtx);
    self->handleEvent(nEvent);
}

void CameraController::handleEvent(unsigned nEvent) {
    switch (nEvent) {
        case TOUPCAM_EVENT_IMAGE:
            pullFrame(false);
            break;
            
        case TOUPCAM_EVENT_STILLIMAGE:
            pullFrame(true);
            break;
            
        case TOUPCAM_EVENT_ERROR:
            emit error("Camera error");
            break;
            
        case TOUPCAM_EVENT_DISCONNECTED:
            emit error("Camera disconnected");
            closeCamera();
            break;
            
        case TOUPCAM_EVENT_NOFRAMETIMEOUT:
            emit error("No frame timeout");
            break;
            
        case TOUPCAM_EVENT_NOPACKETTIMEOUT:
            emit error("No packet timeout");
            break;
    }
}

void CameraController::pullFrame(bool isStill) {
    if (!m_hcam) return;
    
    if (isStill) {
        // Pull still image
        auto frame = std::make_unique<FrameBuffer::Frame>();
        HRESULT hr = Toupcam_PullImageV4(m_hcam, frame->data.data(), 1, 0, 0, &frame->info);
        
        if (SUCCEEDED(hr)) {
            QString filename = getTimestampedFilename("/photos", "raw", 
                frame->info.v3.width, frame->info.v3.height);
            
            // Write raw file
            std::ofstream file(filename.toStdString(), std::ios::binary);
            if (file) {
                file.write(reinterpret_cast<const char*>(frame->data.data()), 
                    frame->data.size());
                file.close();
                emit stillCaptured(filename);
            }
        }
    } else {
        // Pull live frame
        auto frame = m_frameBuffer->getWriteFrame();
        if (!frame) {
            return; // Buffer full, dropping frame
        }
        
        HRESULT hr = Toupcam_PullImageV4(m_hcam, frame->data.data(), 0, 0, 0, &frame->info);
        
        if (SUCCEEDED(hr)) {
            frame->timestamp = std::chrono::steady_clock::now();
            
            // If recording, copy frame to recording queue
            if (m_recording) {
                auto recordFrame = std::make_unique<FrameBuffer::Frame>();
                recordFrame->data = frame->data;
                recordFrame->info = frame->info;
                recordFrame->timestamp = frame->timestamp;
                
                std::lock_guard<std::mutex> lock(m_recordingMutex);
                m_recordingQueue.push_back(std::move(recordFrame));
                m_recordingCv.notify_one();
            }
            
            m_frameBuffer->commitWriteFrame();
            emit frameReady();
            
            // Update frame count for FPS
            m_frameCount++;
        }
    }
}

void CameraController::writeFrameToFile(const FrameBuffer::Frame* frame) {
    QString filename = getTimestampedFilename("/recordings", "raw",
        frame->info.v3.width, frame->info.v3.height);
    
    std::ofstream file(filename.toStdString(), std::ios::binary);
    if (file) {
        file.write(reinterpret_cast<const char*>(frame->data.data()), 
            frame->data.size());
        file.close();
    }
}