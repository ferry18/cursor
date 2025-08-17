#ifndef CAMERA_CONTROLLER_H
#define CAMERA_CONTROLLER_H

#include "utils.h"
#include "frame_buffer.h"

class CameraController : public QObject {
    Q_OBJECT
    
public:
    enum class ConversionGain { LCG, HCG };
    enum class CoolingLevel { Level10C, Level0C, LevelMinus15C, LevelMinus30C };
    enum class HeatingLevel { Level0, Level2, Level3, Level4 };
    
    explicit CameraController(QObject* parent = nullptr);
    ~CameraController();
    
    // Camera lifecycle
    bool openCamera();
    void closeCamera();
    bool isOpen() const { return m_hcam != nullptr; }
    
    // Frame access
    FrameBuffer* frameBuffer() { return m_frameBuffer.get(); }
    
    // Camera controls
    void setConversionGain(ConversionGain gain);
    ConversionGain conversionGain() const { return m_conversionGain; }
    
    void setHighFullWell(bool enabled);
    bool highFullWell() const { return m_highFullWell; }
    
    void setCoolingLevel(CoolingLevel level);
    CoolingLevel coolingLevel() const { return m_coolingLevel; }
    
    void setHeatingLevel(HeatingLevel level);
    HeatingLevel heatingLevel() const { return m_heatingLevel; }
    
    // Auto exposure ROI
    void setAutoExposureROI(const QRect& roi);
    void resetAutoExposureROI();
    QRect autoExposureROI() const { return m_aeROI; }
    
    // Capture
    void captureStill();
    
    // Recording
    void startRecording();
    void stopRecording();
    bool isRecording() const { return m_recording; }
    
    // Frame rate
    double frameRate() const { return m_frameRate; }
    
signals:
    void cameraOpened();
    void cameraClosed();
    void frameReady();
    void stillCaptured(const QString& filename);
    void error(const QString& message);
    void frameRateUpdated(double fps);
    
private:
    // Camera event callback
    static void __stdcall eventCallback(unsigned nEvent, void* pCallbackCtx);
    void handleEvent(unsigned nEvent);
    
    // Initialize camera with all settings
    bool initializeCamera();
    
    // Frame handling
    void pullFrame(bool isStill = false);
    
    // Recording
    void writeFrameToFile(const FrameBuffer::Frame* frame);
    
private:
    HToupcam m_hcam = nullptr;
    ToupcamDeviceV2 m_device;
    std::unique_ptr<FrameBuffer> m_frameBuffer;
    
    // Camera state
    ConversionGain m_conversionGain = ConversionGain::LCG;
    bool m_highFullWell = false;
    CoolingLevel m_coolingLevel = CoolingLevel::Level10C;
    HeatingLevel m_heatingLevel = HeatingLevel::Level0;
    QRect m_aeROI;
    
    // Recording
    std::atomic<bool> m_recording{false};
    std::unique_ptr<std::thread> m_recordingThread;
    std::mutex m_recordingMutex;
    std::condition_variable m_recordingCv;
    std::deque<std::unique_ptr<FrameBuffer::Frame>> m_recordingQueue;
    
    // Stats
    std::atomic<double> m_frameRate{0.0};
    QTimer* m_frameRateTimer;
    
    // Frame counting for FPS
    unsigned m_frameCount = 0;
    std::chrono::steady_clock::time_point m_lastFpsTime;
};

#endif // CAMERA_CONTROLLER_H