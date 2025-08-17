#ifndef CAMERACONTROLLER_H
#define CAMERACONTROLLER_H

#include <QObject>
#include <QTimer>
#include <QMutex>
#include <QImage>
#include <memory>
#include <atomic>
#include "toupcam.h"

class CameraController : public QObject
{
    Q_OBJECT

public:
    explicit CameraController(QObject *parent = nullptr);
    ~CameraController();

    bool initialize();
    void shutdown();
    
    // Camera control methods
    bool startCapture();
    void stopCapture();
    
    // Settings
    void setConversionGain(int mode); // 0=LCG, 1=HCG
    void setHighFullWell(bool enabled);
    void setHeating(int level); // 0-4
    void setCoolingTemperature(int index); // 0-3 for 10°C, 0°C, -15°C, -30°C
    void setAutoExposureRect(const QRect& rect);
    void resetAutoExposureRect();
    
    // Capture methods
    void capturePhoto();
    void startRecording();
    void stopRecording();
    bool isRecording() const { return m_isRecording; }
    
    // Get current frame info
    int getImageWidth() const { return m_imageWidth; }
    int getImageHeight() const { return m_imageHeight; }
    
signals:
    void frameReady(const QImage& image);
    void cameraError(const QString& error);
    void fpsUpdated(float fps);
    void temperatureUpdated(float temperature);
    
private:
    static void __stdcall eventCallback(unsigned nEvent, void* pCallbackCtx);
    void handleImageEvent();
    void handleExpoEvent();
    void handleTempTintEvent();
    void handleError();
    void handleDisconnected();
    
    void setupCameraOptions();
    void setDefaultAutoExposureRect();
    void updateFPS();
    
    void savePhoto(const unsigned char* data, int width, int height);
    void saveVideoFrame(const unsigned char* data, int width, int height);
    
private:
    HToupcam m_hcam;
    ToupcamDeviceV2 m_device;
    
    // Image data
    std::unique_ptr<unsigned char[]> m_imageBuffer;
    int m_imageWidth;
    int m_imageHeight;
    QMutex m_bufferMutex;
    
    // Recording state
    std::atomic<bool> m_isRecording;
    QString m_recordingDir;
    std::atomic<int> m_frameCount;
    
    // FPS tracking
    QTimer* m_fpsTimer;
    
    // Temperature tracking
    QTimer* m_tempTimer;
    int m_currentTempIndex;
    
    // Settings
    int m_heatingLevel;
    bool m_highFullWellEnabled;
    int m_conversionGain;
};

#endif // CAMERACONTROLLER_H