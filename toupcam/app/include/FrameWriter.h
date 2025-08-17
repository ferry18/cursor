#pragma once

#include <QObject>
#include <QDir>
#include <QAtomicInteger>
#include <QMutex>
#include <QQueue>
#include <QThread>

class FrameWriter : public QObject {
	Q_OBJECT
public:
	explicit FrameWriter(const QString& photosDir, const QString& recordingsDir, QObject* parent = nullptr);
	~FrameWriter() override;

	bool init();
	int nextIndex() const;
	void setIndex(int n);

	void saveStill(const QByteArray& raw, const QByteArray& json);
	void setRecording(bool enabled);
	void queueFrame(const QByteArray& raw, const QByteArray& json);

signals:
	void errorRaised(const QString& err);

private slots:
	void writerLoop();

private:
	QString m_photosDir;
	QString m_recordingsDir;
	QAtomicInteger<int> m_index;
	QMutex m_queueMutex;
	QQueue<QPair<QByteArray,QByteArray>> m_queue;
	bool m_recording;
	QThread m_thread;
};