#ifndef IMAGE_VIEW_H
#define IMAGE_VIEW_H

#include "utils.h"
#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLTexture>
#include <QOpenGLBuffer>
#include <QOpenGLVertexArrayObject>

class ImageView : public QOpenGLWidget, protected QOpenGLFunctions {
    Q_OBJECT
    
public:
    explicit ImageView(QWidget* parent = nullptr);
    ~ImageView();
    
    // Update image data (RAW8 monochrome)
    void updateImage(const uint8_t* data, int width, int height);
    
    // Zoom controls
    void setZoom(double zoom);
    double zoom() const { return m_zoom; }
    void zoomToPoint(const QPoint& screenPos, double zoomDelta);
    void resetZoom();
    
    // Pan controls
    void setPan(const QPointF& pan);
    QPointF pan() const { return m_pan; }
    void panBy(const QPointF& delta);
    
    // Convert between screen and image coordinates
    QPointF screenToImage(const QPoint& screenPos) const;
    QPoint imageToScreen(const QPointF& imagePos) const;
    
    // Auto exposure ROI
    void showAEIcon(const QPoint& screenPos);
    
signals:
    void clicked(const QPointF& imagePos);
    void doubleClicked(const QPointF& imagePos);
    void zoomChanged(double zoom);
    
protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;
    
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    
private:
    void initShaders();
    void updateViewMatrix();
    void drawImage();
    void drawOverlays();
    void drawMiniMap();
    void drawAEIcon();
    void constrainPan();
    
private:
    // OpenGL resources
    std::unique_ptr<QOpenGLShaderProgram> m_program;
    std::unique_ptr<QOpenGLTexture> m_texture;
    QOpenGLBuffer m_vbo;
    QOpenGLVertexArrayObject m_vao;
    
    // Image data
    int m_imageWidth = CAMERA_WIDTH;
    int m_imageHeight = CAMERA_HEIGHT;
    bool m_textureNeedsUpdate = false;
    std::vector<uint8_t> m_imageData;
    
    // View state
    double m_zoom = 1.0;
    QPointF m_pan{0.0, 0.0};
    QMatrix4x4 m_viewMatrix;
    QMatrix4x4 m_projMatrix;
    
    // Interaction
    bool m_panning = false;
    QPoint m_lastMousePos;
    
    // AE icon animation
    struct AEIconState {
        bool visible = false;
        QPointF position;
        std::chrono::steady_clock::time_point startTime;
        QPropertyAnimation* fadeAnimation = nullptr;
        double opacity = 0.0;
    } m_aeIcon;
    
    // Mini-map
    static constexpr int MINIMAP_WIDTH = 200;
    static constexpr int MINIMAP_HEIGHT = 134; // Maintain aspect ratio
    static constexpr int MINIMAP_MARGIN = 10;
    static constexpr int MINIMAP_BORDER = 2;
};

#endif // IMAGE_VIEW_H