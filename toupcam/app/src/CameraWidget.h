#ifndef CAMERAWIDGET_H
#define CAMERAWIDGET_H

#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLTexture>
#include <QOpenGLBuffer>
#include <QImage>
#include <QLabel>
#include <memory>
#include "ZoomController.h"
#include "AutoExposureOverlay.h"

class CameraWidget : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT

public:
    explicit CameraWidget(QWidget *parent = nullptr);
    ~CameraWidget();

    void updateImage(const QImage& image);
    void setFPS(float fps);
    
    ZoomController* zoomController() { return m_zoomController; }

signals:
    void autoExposureRectRequested(const QRect& rect);
    void autoExposureReset();

protected:
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int w, int h) override;
    
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    
private:
    void updateTexture();
    void drawMinimap();
    void handleAutoExposureClick(const QPoint& pos);
    
private:
    // OpenGL resources
    std::unique_ptr<QOpenGLShaderProgram> m_program;
    std::unique_ptr<QOpenGLTexture> m_texture;
    QOpenGLBuffer m_vbo;
    
    // Camera image
    QImage m_currentImage;
    bool m_imageUpdated;
    
    // Zoom and pan
    ZoomController* m_zoomController;
    bool m_isPanning;
    QPoint m_lastMousePos;
    
    // FPS display
    QLabel* m_fpsLabel;
    
    // Auto exposure overlay
    AutoExposureOverlay* m_aeOverlay;
    
    // Minimap
    static constexpr int MINIMAP_SIZE = 200;
    static constexpr int MINIMAP_MARGIN = 10;
};

#endif // CAMERAWIDGET_H