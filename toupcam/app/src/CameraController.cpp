#include "CameraController.h"
#include "FrameWriter.h"

#include <QDir>
#include <QJsonObject>
#include <QJsonDocument>
#include <QDateTime>
#include <QDebug>
#include <cstring>

extern "C" {
#include "toupcam.h"
}

static inline QString workspaceDir()
{
	return QStringLiteral("/workspace/toupcam/app");
}

static inline QString photosDir()
{
	return workspaceDir() + "/photos";
}

static inline QString recordingsDir()
{
	return workspaceDir() + "/recordings";
}

CameraController::CameraController(QObject* parent)
	: QObject(parent)
	, m_hcam(nullptr)
	, m_sensorSize(0,0)
	, m_finalSize(0,0)
	, m_frameBuffer(nullptr)
	, m_lastTs(0)
	, m_frameCounter(0)
	, m_fpsCounter(0)
	, m_isOpen(false)
	, m_isStreaming(false)
	, m_isRecording(false)
	, m_heatingLevel(0)
	, m_coolingIdx(0)
	, m_writer(new FrameWriter(photosDir(), recordingsDir(), this))
{
	m_writer->init();
	m_fpsTimer.start();
}

CameraController::~CameraController()
{
	stop();
	close();
}

QSize CameraController::frameSize() const { return m_sensorSize; }
QSize CameraController::finalSize() const { return m_finalSize; }

bool CameraController::openAndConfigure()
{
	ToupcamDeviceV2 arr[TOUPCAM_MAX] = {};
	unsigned n = Toupcam_EnumV2(arr);
	if (n == 0) return false;
	m_hcam = Toupcam_Open(arr[0].id);
	if (!m_hcam) return false;
	m_isOpen = true;
	if (!applyStartupOptions()) return false;
	return queryFinalSize();
}

void CameraController::close()
{
	if (m_hcam) {
		Toupcam_Close(m_hcam);
		m_hcam = nullptr;
		m_isOpen = false;
	}
}

bool CameraController::start()
{
	if (!m_isOpen || m_isStreaming) return false;
	setupCallbacks();
	HRESULT hr = Toupcam_StartPushModeV4(m_hcam,
		[](const void* pData, const ToupcamFrameInfoV3* pInfo, int bSnap, void* ctxData){
			CameraController* self = reinterpret_cast<CameraController*>(ctxData);
			self->onFrameCallback(pData, pInfo, bSnap);
		},
		reinterpret_cast<void*>(this),
		[](unsigned nEvent, void* ctxEvent){
			CameraController* self = reinterpret_cast<CameraController*>(ctxEvent);
			self->onEventCallback(nEvent);
		},
		reinterpret_cast<void*>(this)
	);
	if (FAILED(hr)) {
		return false;
	}
	m_isStreaming = true;
	return true;
}

void CameraController::stop()
{
	if (m_isStreaming && m_hcam) {
		Toupcam_Stop(m_hcam);
		teardownCallbacks();
		m_isStreaming = false;
	}
}

bool CameraController::applyStartupOptions()
{
	if (!setResolutionIndex1()) return false;
	if (!setIwrReadout()) return false;
	if (!setAeDamping0()) return false;
	if (!setPixelFormatRaw8Mono()) return false;
	if (!setTailLightOff()) return false;
	if (!setCgLcgDefault()) return false;
	if (!setHfwOffDefault()) return false;
	applyThreadPriority();
	configureLowLatencyPipeline();
	setAeRoiDefault70();
	return true;
}

bool CameraController::applyThreadPriority()
{
	int sched_fifo = 1; // POSIX SCHED_FIFO policy enum per header note
	int prio = 99;
	int value = (sched_fifo << 16) | (prio & 0xffff);
	Toupcam_put_Option(m_hcam, TOUPCAM_OPTION_THREAD_PRIORITY, value);
	return true;
}

bool CameraController::setPixelFormatRaw8Mono()
{
	// Prefer PIXEL_FORMAT RAW8 if available
	HRESULT hr = Toupcam_put_Option(m_hcam, TOUPCAM_OPTION_PIXEL_FORMAT, TOUPCAM_PIXELFORMAT_RAW8);
	if (FAILED(hr)) {
		Toupcam_put_Option(m_hcam, TOUPCAM_OPTION_BITDEPTH, 0);
		Toupcam_put_Option(m_hcam, TOUPCAM_OPTION_RGB, 3); // 8-bit Grey on mono
	}
	Toupcam_put_Option(m_hcam, TOUPCAM_OPTION_RAW, 1);
	return true;
}

bool CameraController::setResolutionIndex1()
{
	HRESULT hr = Toupcam_put_eSize(m_hcam, 1);
	return SUCCEEDED(hr);
}

bool CameraController::setIwrReadout()
{
	HRESULT hr = Toupcam_put_Option(m_hcam, TOUPCAM_OPTION_READOUT_MODE, 0);
	return SUCCEEDED(hr);
}

bool CameraController::setAeDamping0()
{
	HRESULT hr = Toupcam_put_Option(m_hcam, TOUPCAM_OPTION_AUTOEXP_EXPOTIME_DAMP, 0);
	return SUCCEEDED(hr);
}

bool CameraController::setTailLightOff()
{
	HRESULT hr = Toupcam_put_Option(m_hcam, TOUPCAM_OPTION_TAILLIGHT, 0);
	return SUCCEEDED(hr);
}

bool CameraController::setCgLcgDefault()
{
	HRESULT hr = Toupcam_put_Option(m_hcam, TOUPCAM_OPTION_CG, 0);
	return SUCCEEDED(hr);
}

bool CameraController::setHfwOffDefault()
{
	HRESULT hr = Toupcam_put_Option(m_hcam, TOUPCAM_OPTION_HIGH_FULLWELL, 0);
	return SUCCEEDED(hr);
}

bool CameraController::configureLowLatencyPipeline()
{
	Toupcam_put_RealTime(m_hcam, 1);
	Toupcam_put_Option(m_hcam, TOUPCAM_OPTION_BACKEND_DEQUE_LENGTH, 3);
	Toupcam_put_Option(m_hcam, TOUPCAM_OPTION_FRONTEND_DEQUE_LENGTH, 4);
	Toupcam_put_Option(m_hcam, TOUPCAM_OPTION_CALLBACK_THREAD, 1);
	return true;
}

bool CameraController::queryFinalSize()
{
	int w = 0, h = 0;
	HRESULT hr = Toupcam_get_FinalSize(m_hcam, &w, &h);
	if (SUCCEEDED(hr) && w > 0 && h > 0) {
		m_finalSize = QSize(w, h);
		m_sensorSize = m_finalSize;
		m_frameBuffer.reset(new uchar[w * h]);
		return true;
	}
	return false;
}

bool CameraController::setAeRoiRect(const QRect& rect)
{
	RECT r; r.left = rect.left(); r.top = rect.top(); r.right = rect.right(); r.bottom = rect.bottom();
	HRESULT hr = Toupcam_put_AEAuxRect(m_hcam, &r);
	return SUCCEEDED(hr);
}

bool CameraController::setAeRoiDefault70()
{
	if (m_finalSize.isEmpty()) queryFinalSize();
	int w = m_finalSize.width();
	int h = m_finalSize.height();
	int rw = int(w * 0.7);
	int rh = int(h * 0.7);
	int left = (w - rw) / 2;
	int top = (h - rh) / 2;
	RECT r; r.left = left; r.top = top; r.right = left + rw; r.bottom = top + rh;
	HRESULT hr = Toupcam_put_AEAuxRect(m_hcam, &r);
	return SUCCEEDED(hr);
}

void CameraController::toggleGain()
{
	int cur = 0;
	Toupcam_get_Option(m_hcam, TOUPCAM_OPTION_CG, &cur);
	int next = (cur == 0) ? 1 : 0;
	Toupcam_put_Option(m_hcam, TOUPCAM_OPTION_CG, next);
}

void CameraController::toggleHighFullWell()
{
	int cur = 0;
	Toupcam_get_Option(m_hcam, TOUPCAM_OPTION_HIGH_FULLWELL, &cur);
	int next = (cur == 0) ? 1 : 0;
	Toupcam_put_Option(m_hcam, TOUPCAM_OPTION_HIGH_FULLWELL, next);
}

void CameraController::cycleHeating()
{
	// Cycle 0 -> 2 -> 3 -> 4 -> 0
	int cur = 0;
	Toupcam_get_Option(m_hcam, TOUPCAM_OPTION_HEAT, &cur);
	int next = 0;
	if (cur == 0) next = 2; else if (cur == 2) next = 3; else if (cur == 3) next = 4; else next = 0;
	Toupcam_put_Option(m_hcam, TOUPCAM_OPTION_HEAT, next);
}

void CameraController::cycleCooling()
{
	// 10C -> 0C -> -15C -> -30C -> 10C
	static const int targets[] = {100, 0, -150, -300};
	int curIdx = m_coolingIdx;
	curIdx = (curIdx + 1) % 4;
	m_coolingIdx = curIdx;
	Toupcam_put_Option(m_hcam, TOUPCAM_OPTION_TECTARGET, targets[curIdx]);
}

void CameraController::shutterSnap()
{
	Toupcam_SnapR(m_hcam, 1, 1);
}

void CameraController::setRecording(bool enabled)
{
	m_isRecording = enabled;
	if (m_writer) m_writer->setRecording(enabled);
}

void CameraController::setSpotAeAt(int x, int y)
{
	if (m_finalSize.isEmpty()) queryFinalSize();
	int w = m_finalSize.width();
	int h = m_finalSize.height();
	int rw = 300;
	int rh = 300;
	int left = qBound(0, x - rw/2, w - rw);
	int top = qBound(0, y - rh/2, h - rh);
	RECT r; r.left = left; r.top = top; r.right = left + rw; r.bottom = top + rh;
	Toupcam_put_AEAuxRect(m_hcam, &r);
}

bool CameraController::getLatestFrame(const uchar*& data, int& width, int& height, qint64& ts)
{
	QMutexLocker lock(&m_frameMutex);
	if (!m_frameBuffer) return false;
	data = m_frameBuffer.get();
	width = m_finalSize.width();
	height = m_finalSize.height();
	ts = m_lastTs;
	return true;
}

void CameraController::setupCallbacks()
{
	// already set in StartPushModeV3 lambda; nothing else here
}

void CameraController::teardownCallbacks()
{
}

void CameraController::onFrameCallback(const void* pData, const ToupcamFrameInfoV3* pInfo, int bSnap)
{
	if (!pData) return;
	int w = m_finalSize.width();
	int h = m_finalSize.height();
	{
		QMutexLocker lock(&m_frameMutex);
		if (!m_frameBuffer) m_frameBuffer.reset(new uchar[w * h]);
		std::memcpy(m_frameBuffer.get(), pData, size_t(w) * size_t(h));
		m_lastTs = QDateTime::currentMSecsSinceEpoch();
	}
	// FPS counter
	quint64 cnt = m_fpsCounter.fetchAndAddRelaxed(1) + 1;
	qint64 ms = m_fpsTimer.elapsed();
	if (ms >= 1000) {
		double fps = (cnt * 1000.0) / ms;
		m_fpsCounter.storeRelaxed(0);
		m_fpsTimer.restart();
		emit fpsUpdated(fps);
	}
	
	// Handle still capture via callback bSnap
	if (bSnap) {
		QByteArray raw;
		raw.resize(w * h);
		std::memcpy(raw.data(), pData, size_t(w) * size_t(h));
		QJsonObject meta;
		meta["width"] = w;
		meta["height"] = h;
		meta["timestamp_us"] = qint64(QDateTime::currentMSecsSinceEpoch()) * 1000;
		meta["seq"] = int(pInfo ? pInfo->seq : 0);
		meta["expotime_us"] = int(pInfo ? pInfo->expotime : 0);
		meta["expogain"] = int(pInfo ? pInfo->expogain : 0);
		meta["blacklevel"] = int(pInfo ? pInfo->blacklevel : 0);
		QByteArray json = QJsonDocument(meta).toJson(QJsonDocument::Compact);
		m_writer->saveStill(raw, json);
	}
	
	// Recording queue
	if (m_isRecording) {
		QByteArray raw;
		raw.resize(w * h);
		std::memcpy(raw.data(), pData, size_t(w) * size_t(h));
		QJsonObject meta;
		meta["width"] = w;
		meta["height"] = h;
		meta["timestamp_us"] = qint64(QDateTime::currentMSecsSinceEpoch()) * 1000;
		meta["seq"] = int(pInfo ? pInfo->seq : 0);
		meta["expotime_us"] = int(pInfo ? pInfo->expotime : 0);
		meta["expogain"] = int(pInfo ? pInfo->expogain : 0);
		meta["blacklevel"] = int(pInfo ? pInfo->blacklevel : 0);
		QByteArray json = QJsonDocument(meta).toJson(QJsonDocument::Compact);
		m_writer->queueFrame(raw, json);
	}
	
	emit frameUpdated();
}

void CameraController::onEventCallback(unsigned nEvent)
{
	// Handle still image event
	switch (nEvent) {
		case 0x01: // TOUPCAM_EVENT_IMAGE per docs
			break;
		case 0x02: // TOUPCAM_EVENT_STILLIMAGE
			// handled via bSnap in data callback
			break;
		default:
			break;
	}
}