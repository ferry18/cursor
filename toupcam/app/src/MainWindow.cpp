#include "MainWindow.h"
#include "GLCameraView.h"
#include "CameraController.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QToolButton>
#include <QWidget>
#include <QLabel>
#include <QStatusBar>
#include <QFile>
#include <QDir>
#include <QAction>
#include <QSpacerItem>
#include <QSizePolicy>

static QString iconsPrefix()
{
	return QString(":/icons/");
}

MainWindow::MainWindow(QWidget* parent)
	: QMainWindow(parent)
	, m_cameraView(nullptr)
	, m_controlsPane(nullptr)
	, m_actionShutter(nullptr)
	, m_actionRecord(nullptr)
	, m_actionGain(nullptr)
	, m_actionHighFullWell(nullptr)
	, m_actionHeating(nullptr)
	, m_actionCooling(nullptr)
	, m_camera(new CameraController(this))
	, m_isRecording(false)
	, m_heatingLevel(0)
	, m_coolingIdx(0)
{
	setWindowTitle("Toupcam Live");
	resize(2560, 1600);
	createUi();
	wireSignals();
	updateIcons();

	if (!m_camera->openAndConfigure()) {
		statusBar()->showMessage("Camera open/configure failed", 5000);
	}
	else {
		m_camera->start();
	}
}

MainWindow::~MainWindow() {}

void MainWindow::createUi()
{
	m_iconShutterIdle = QIcon(iconsPrefix() + "shutter_button/shutter_button_not_pressed.svg");
	m_iconShutterPressed = QIcon(iconsPrefix() + "shutter_button/shutter_button_pressed.svg");
	m_iconRecordIdle = QIcon(iconsPrefix() + "record_button/record_button_not_pressed.svg");
	m_iconRecordPressed = QIcon(iconsPrefix() + "record_button/record_button_pressed.svg");
	m_iconGainLCG = QIcon(iconsPrefix() + "gain_switch/gain_switch_LCG.svg");
	m_iconGainHCG = QIcon(iconsPrefix() + "gain_switch/gain_switch_HCG.svg");
	m_iconHfwOff = QIcon(iconsPrefix() + "high-full_well_button/high_full_well_0.svg");
	m_iconHfwOn = QIcon(iconsPrefix() + "high-full_well_button/high_full_well_1.svg");
	m_iconHeat0 = QIcon(iconsPrefix() + "heating_button/heating_button_level_0.svg");
	m_iconHeat1 = QIcon(iconsPrefix() + "heating_button/heating_button_level_1.svg");
	m_iconHeat2 = QIcon(iconsPrefix() + "heating_button/heating_button_level_2.svg");
	m_iconHeat3 = QIcon(iconsPrefix() + "heating_button/heating_button_level_3.svg");
	m_iconCool0 = QIcon(iconsPrefix() + "cooling_button/cooling_button_level_0.svg");
	m_iconCool1 = QIcon(iconsPrefix() + "cooling_button/cooling_button_level_1.svg");
	m_iconCool2 = QIcon(iconsPrefix() + "cooling_button/cooling_button_level_2.svg");
	m_iconCool3 = QIcon(iconsPrefix() + "cooling_button/cooling_button_level_3.svg");

	auto* central = new QWidget(this);
	setCentralWidget(central);
	auto* root = new QHBoxLayout(central);
	root->setContentsMargins(0,0,0,0);
	root->setSpacing(0);

	m_controlsPane = new QWidget(central);
	m_controlsPane->setFixedWidth(171);
	auto* controlsLayout = new QVBoxLayout(m_controlsPane);
	controlsLayout->setContentsMargins(8,8,8,8);
	controlsLayout->setSpacing(12);

	// Buttons
	auto makeButton = [&](const QIcon& icon, const QString& tooltip){
		auto* b = new QToolButton(m_controlsPane);
		b->setIcon(icon);
		b->setIconSize(QSize(48,48));
		b->setToolButtonStyle(Qt::ToolButtonIconOnly);
		b->setAutoRaise(true);
		b->setToolTip(tooltip);
		return b;
	};

	m_btnShutter = makeButton(m_iconShutterIdle, "Shutter");
	m_btnRecord = makeButton(m_iconRecordIdle, "Record");
	m_btnGain = makeButton(m_iconGainLCG, "Conversion Gain");
	m_btnHfw = makeButton(m_iconHfwOff, "High Full Well");
	m_btnHeat = makeButton(m_iconHeat0, "Heating");
	m_btnCool = makeButton(m_iconCool0, "Cooling");

	controlsLayout->addWidget(m_btnShutter);
	controlsLayout->addWidget(m_btnRecord);
	controlsLayout->addWidget(m_btnGain);
	controlsLayout->addWidget(m_btnHfw);
	controlsLayout->addWidget(m_btnHeat);
	controlsLayout->addWidget(m_btnCool);
	controlsLayout->addStretch(1);

	m_cameraView = new GLCameraView(central);
	m_cameraView->setMinimumSize(2389, 1600);
	m_cameraView->setController(m_camera);

	root->addWidget(m_controlsPane);
	root->addWidget(m_cameraView, 1);

	// Actions
	m_actionShutter = new QAction(this);
	m_actionRecord = new QAction(this);
	m_actionGain = new QAction(this);
	m_actionHighFullWell = new QAction(this);
	m_actionHeating = new QAction(this);
	m_actionCooling = new QAction(this);

	connect(m_btnShutter, &QToolButton::clicked, this, &MainWindow::onShutterClicked);
	connect(m_btnRecord, &QToolButton::clicked, this, &MainWindow::onRecordToggled);
	connect(m_btnGain, &QToolButton::clicked, this, &MainWindow::onGainToggled);
	connect(m_btnHfw, &QToolButton::clicked, this, &MainWindow::onHighFullWellToggled);
	connect(m_btnHeat, &QToolButton::clicked, this, &MainWindow::onHeatingClicked);
	connect(m_btnCool, &QToolButton::clicked, this, &MainWindow::onCoolingClicked);
}

void MainWindow::wireSignals()
{
	connect(m_camera, &CameraController::frameUpdated, m_cameraView, &GLCameraView::refresh);
	connect(m_cameraView, &GLCameraView::clickedImage, this, &MainWindow::onAeSpotRequested);
	connect(m_cameraView, &GLCameraView::doubleClicked, this, &MainWindow::onAeDefaultRequested);
	connect(m_camera, &CameraController::fpsUpdated, this, &MainWindow::onFpsUpdate);
}

void MainWindow::updateIcons()
{
	// handled per-button as states change; no global toolbar
}

void MainWindow::updateNumbering()
{
	// handled by FrameWriter inside CameraController
}

void MainWindow::onShutterClicked()
{
	m_camera->shutterSnap();
}

void MainWindow::onRecordToggled()
{
	m_isRecording = !m_isRecording;
	m_camera->setRecording(m_isRecording);
}

void MainWindow::onGainToggled()
{
	m_camera->toggleGain();
}

void MainWindow::onHighFullWellToggled()
{
	m_camera->toggleHighFullWell();
}

void MainWindow::onHeatingClicked()
{
	m_camera->cycleHeating();
}

void MainWindow::onCoolingClicked()
{
	m_camera->cycleCooling();
}

void MainWindow::onAeDefaultRequested()
{
	m_camera->setAeRoiDefault70();
	m_cameraView->clearAeSpot();
}

void MainWindow::onAeSpotRequested(int x, int y)
{
	m_camera->setSpotAeAt(x, y);
	m_cameraView->showAeSpotAt(QPoint(x, y));
}

void MainWindow::onFpsUpdate(double fps)
{
	statusBar()->showMessage(QString::number(fps, 'f', 1) + " fps");
}