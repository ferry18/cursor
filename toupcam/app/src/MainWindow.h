#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QHBoxLayout>
#include <memory>

class CameraController;
class CameraWidget;
class ControlPanel;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    void setupUI();
    void connectSignals();
    
private:
    std::unique_ptr<CameraController> m_cameraController;
    CameraWidget* m_cameraWidget;
    ControlPanel* m_controlPanel;
};

#endif // MAINWINDOW_H