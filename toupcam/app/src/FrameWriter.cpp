#include "FrameWriter.h"
#include "ImageNumbering.h"

#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <QDirIterator>

FrameWriter::FrameWriter(const QString& photosDir, const QString& recordingsDir, QObject* parent)
	: QObject(parent)
	, m_photosDir(photosDir)
	, m_recordingsDir(recordingsDir)
	, m_index(1)
	, m_recording(false)
{
}

FrameWriter::~FrameWriter()
{
	m_thread.quit();
	m_thread.wait(2000);
}

bool FrameWriter::init()
{
	QDir().mkpath(m_photosDir);
	QDir().mkpath(m_recordingsDir);
	int maxIdx = ImageNumbering::findMaxIndex(m_photosDir, m_recordingsDir);
	m_index.storeRelaxed(qMax(1, maxIdx + 1));
	moveToThread(&m_thread);
	connect(&m_thread, &QThread::started, this, &FrameWriter::writerLoop);
	m_thread.start(QThread::LowestPriority);
	return true;
}

int FrameWriter::nextIndex() const
{
	return m_index.loadRelaxed();
}

void FrameWriter::setIndex(int n)
{
	m_index.storeRelaxed(n);
}

void FrameWriter::saveStill(const QByteArray& raw, const QByteArray& json)
{
	// Save into photos directory immediately
	int idx = m_index.fetchAndAddRelaxed(1);
	QString rawPath = ImageNumbering::makeRawPath(m_photosDir, idx);
	QString jsonPath = ImageNumbering::makeJsonPath(m_photosDir, idx);
	QFile f(rawPath);
	if (f.open(QIODevice::WriteOnly)) {
		f.write(raw);
		f.close();
	}
	QFile fj(jsonPath);
	if (fj.open(QIODevice::WriteOnly)) {
		fj.write(json);
		fj.close();
	}
}

void FrameWriter::setRecording(bool enabled)
{
	m_recording = enabled;
}

void FrameWriter::queueFrame(const QByteArray& raw, const QByteArray& json)
{
	if (!m_recording) return;
	QMutexLocker lock(&m_queueMutex);
	// Bounded queue: drop oldest if > 64 items
	if (m_queue.size() > 64) m_queue.dequeue();
	m_queue.enqueue(qMakePair(raw, json));
}

void FrameWriter::writerLoop()
{
	for (;;) {
		QPair<QByteArray,QByteArray> item;
		{
			QMutexLocker lock(&m_queueMutex);
			if (!m_queue.isEmpty()) {
				item = m_queue.dequeue();
			}
		}
		if (!item.first.isEmpty()) {
			int idx = m_index.fetchAndAddRelaxed(1);
			QString rawPath = ImageNumbering::makeRawPath(m_recordingsDir, idx);
			QString jsonPath = ImageNumbering::makeJsonPath(m_recordingsDir, idx);
			QFile f(rawPath);
			if (f.open(QIODevice::WriteOnly)) {
				f.write(item.first);
				f.close();
			}
			QFile fj(jsonPath);
			if (fj.open(QIODevice::WriteOnly)) {
				fj.write(item.second);
				fj.close();
			}
		}
		QThread::msleep(1);
	}
}