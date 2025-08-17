#ifndef APP_MAIN_WINDOW_H
#define APP_MAIN_WINDOW_H

#include "utils.h"
#include <QMainWindow>

class CameraController;
class ImageView;
class UiPanel;

class AppMainWindow : public QMainWindow {
    Q_OBJECT
    
public:
    explicit AppMainWindow(QWidget* parent = nullptr);
    ~AppMainWindow();
    
protected:
    void closeEvent(QCloseEvent* event) override;
    
private slots:
    void onCameraOpened();
    void onCameraClosed();
    void onCameraError(const QString& error);
    void onFrameReady();
    void onImageClicked(const QPointF& imagePos);
    void onImageDoubleClicked(const QPointF& imagePos);
    void onFrameRateUpdated(double fps);
    void onStillCaptured(const QString& filename);
    
private:
    void setupUi();
    void connectSignals();
    void updateStatusBar();
    
private:
    CameraController* m_controller;
    ImageView* m_imageView;
    UiPanel* m_uiPanel;
    
    // Status bar widgets
    QLabel* m_statusLabel;
    QLabel* m_fpsLabel;
    QLabel* m_zoomLabel;
};

#endif // APP_MAIN_WINDOW_H