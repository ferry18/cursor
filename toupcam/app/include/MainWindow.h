#pragma once

#include <QMainWindow>
#include <QScopedPointer>
#include <QTimer>
#include <QPointer>
#include <QIcon>
#include <QQueue>
#include <QElapsedTimer>

class QToolButton;
class GLCameraView;
class CameraController;

class MainWindow : public QMainWindow {
	Q_OBJECT
public:
	explicit MainWindow(QWidget* parent = nullptr);
	~MainWindow() override;

private slots:
	void onShutterClicked();
	void onRecordToggled();
	void onGainToggled();
	void onHighFullWellToggled();
	void onHeatingClicked();
	void onCoolingClicked();
	void onAeDefaultRequested();
	void onAeSpotRequested(int x, int y); // image-space click
	void onFpsUpdate(double fps);

private:
	void createUi();
	void wireSignals();
	void updateIcons();
	void updateNumbering();

private:
	GLCameraView* m_cameraView;
	QWidget* m_controlsPane;

	QAction* m_actionShutter;
	QAction* m_actionRecord;
	QAction* m_actionGain;
	QAction* m_actionHighFullWell;
	QAction* m_actionHeating;
	QAction* m_actionCooling;

	QToolButton* m_btnShutter;
	QToolButton* m_btnRecord;
	QToolButton* m_btnGain;
	QToolButton* m_btnHfw;
	QToolButton* m_btnHeat;
	QToolButton* m_btnCool;

	QIcon m_iconShutterIdle;
	QIcon m_iconShutterPressed;
	QIcon m_iconRecordIdle;
	QIcon m_iconRecordPressed;
	QIcon m_iconGainLCG;
	QIcon m_iconGainHCG;
	QIcon m_iconHfwOff;
	QIcon m_iconHfwOn;
	QIcon m_iconHeat0;
	QIcon m_iconHeat1;
	QIcon m_iconHeat2;
	QIcon m_iconHeat3;
	QIcon m_iconCool0;
	QIcon m_iconCool1;
	QIcon m_iconCool2;
	QIcon m_iconCool3;

	CameraController* m_camera;
	bool m_isRecording;
	int m_heatingLevel; // 0,2,3,4 -> mapped to 0..3 icons
	int m_coolingIdx; // 0..3 mapping to 10C,0C,-15C,-30C
};