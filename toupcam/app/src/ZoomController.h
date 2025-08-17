#ifndef ZOOMCONTROLLER_H
#define ZOOMCONTROLLER_H

#include <QObject>
#include <QPointF>
#include <QRectF>
#include <QSize>

class ZoomController : public QObject
{
    Q_OBJECT

public:
    explicit ZoomController(QObject *parent = nullptr);

    // Zoom control
    void zoomIn(const QPointF& cursorPos = QPointF(), bool toCursor = false);
    void zoomOut(const QPointF& cursorPos = QPointF(), bool toCursor = false);
    void setZoom(float level, const QPointF& cursorPos = QPointF(), bool toCursor = false);
    void resetZoom();
    
    // Pan control
    void pan(const QPointF& delta);
    void setPanPosition(const QPointF& pos);
    
    // Getters
    float getZoomLevel() const { return m_zoomLevel; }
    QPointF getPanPosition() const { return m_panPosition; }
    QRectF getVisibleRect() const;
    QRectF getVisibleRectInImage() const;
    
    // Set image dimensions
    void setImageSize(const QSize& size);
    void setViewportSize(const QSize& size);
    
    // Transform points between viewport and image coordinates
    QPointF viewportToImage(const QPointF& viewportPos) const;
    QPointF imageToViewport(const QPointF& imagePos) const;
    
    // Check if zoomed in
    bool isZoomedIn() const { return m_zoomLevel > 1.0f; }
    
    // Get minimap rectangle (normalized 0-1)
    QRectF getMinimapRect() const;

signals:
    void zoomChanged(float level);
    void panChanged(const QPointF& position);
    void viewChanged();

private:
    void updatePanLimits();
    void clampPan();
    float clampZoom(float zoom) const;
    
private:
    static constexpr float MIN_ZOOM = 1.0f;
    static constexpr float MAX_ZOOM = 8.0f;
    static constexpr float ZOOM_STEP = 1.2f;
    
    float m_zoomLevel;
    QPointF m_panPosition;  // Center of visible area in image coordinates
    QSize m_imageSize;
    QSize m_viewportSize;
    
    QPointF m_minPan;
    QPointF m_maxPan;
};

#endif // ZOOMCONTROLLER_H