#pragma once

#include <QObject>
#include <QByteArray>
#include <QRect>
#include <QMutex>
#include <QAtomicInteger>
#include <QSize>
#include <QElapsedTimer>
#include <memory>

extern "C" {
#include "toupcam.h"
}

class FrameWriter;

class CameraController : public QObject {
	Q_OBJECT
public:
	explicit CameraController(QObject* parent = nullptr);
	~CameraController() override;

	bool openAndConfigure();
	void close();

	bool start();
	void stop();

	QSize frameSize() const;
	QSize finalSize() const;

	// ROI in image-space
	bool setAeRoiRect(const QRect& rect);
	bool setAeRoiDefault70();

	// Buttons operations
	void toggleGain();
	void toggleHighFullWell();
	void cycleHeating();
	void cycleCooling();
	void shutterSnap();
	void setRecording(bool enabled);

	// Image-space click
	void setSpotAeAt(int x, int y);

	// Expose pointer to latest frame in RAW8 mono
	bool getLatestFrame(const uchar*& data, int& width, int& height, qint64& ts);

signals:
	void frameUpdated();
	void fpsUpdated(double fps);
	void stillCaptured(const QByteArray& data, const QByteArray& json, bool ok);
	void errorRaised(const QString& err);

private:
	bool applyStartupOptions();
	bool applyThreadPriority();
	bool setPixelFormatRaw8Mono();
	bool setResolutionIndex1();
	bool setIwrReadout();
	bool setAeDamping0();
	bool setTailLightOff();
	bool setCgLcgDefault();
	bool setHfwOffDefault();
	bool configureLowLatencyPipeline();
	bool queryFinalSize();
	void setupCallbacks();
	void teardownCallbacks();

	void onFrameCallback(const void* pData, const ToupcamFrameInfoV3* pInfo, int bSnap);
	void onEventCallback(unsigned nEvent);

private:
	HToupcam m_hcam;
	QSize m_sensorSize;
	QSize m_finalSize;
	std::unique_ptr<uchar[]> m_frameBuffer; // single latest frame
	QMutex m_frameMutex;
	qint64 m_lastTs;
	QAtomicInteger<quint64> m_frameCounter;
	QAtomicInteger<quint64> m_fpsCounter;
	QElapsedTimer m_fpsTimer;
	bool m_isOpen;
	bool m_isStreaming;

	// States
	bool m_isRecording;
	int m_heatingLevel; // 0,2,3,4 cycling
	int m_coolingIdx; // 0..3

	// Writer
	FrameWriter* m_writer;
};