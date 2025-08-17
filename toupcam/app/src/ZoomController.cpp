#include "ZoomController.h"
#include <algorithm>
#include <cmath>

ZoomController::ZoomController(QObject *parent)
    : QObject(parent)
    , m_zoomLevel(1.0f)
    , m_panPosition(0, 0)
    , m_imageSize(3104, 2084)
    , m_viewportSize(2389, 1600)
{
    updatePanLimits();
}

void ZoomController::zoomIn(const QPointF& cursorPos, bool toCursor)
{
    setZoom(m_zoomLevel * ZOOM_STEP, cursorPos, toCursor);
}

void ZoomController::zoomOut(const QPointF& cursorPos, bool toCursor)
{
    setZoom(m_zoomLevel / ZOOM_STEP, cursorPos, toCursor);
}

void ZoomController::setZoom(float level, const QPointF& cursorPos, bool toCursor)
{
    float oldZoom = m_zoomLevel;
    m_zoomLevel = clampZoom(level);
    
    if (std::abs(m_zoomLevel - oldZoom) < 0.001f) {
        return;
    }
    
    if (toCursor && !cursorPos.isNull() && m_zoomLevel > 1.0f) {
        // Calculate the image point under the cursor before zoom
        QPointF imagePoint = viewportToImage(cursorPos);
        
        // Update pan limits for new zoom level
        updatePanLimits();
        
        // Calculate new pan position to keep the same image point under cursor
        QPointF viewportCenter(m_viewportSize.width() / 2.0f, m_viewportSize.height() / 2.0f);
        QPointF delta = cursorPos - viewportCenter;
        
        m_panPosition = imagePoint - delta / m_zoomLevel;
        clampPan();
    } else {
        // Zoom to center
        updatePanLimits();
        
        if (m_zoomLevel <= 1.0f) {
            // Reset to center when at 100% or less
            m_panPosition = QPointF(m_imageSize.width() / 2.0f, m_imageSize.height() / 2.0f);
        } else {
            // Keep current center point
            clampPan();
        }
    }
    
    emit zoomChanged(m_zoomLevel);
    emit viewChanged();
}

void ZoomController::resetZoom()
{
    setZoom(1.0f);
}

void ZoomController::pan(const QPointF& delta)
{
    if (m_zoomLevel <= 1.0f) return;
    
    m_panPosition -= delta / m_zoomLevel;
    clampPan();
    
    emit panChanged(m_panPosition);
    emit viewChanged();
}

void ZoomController::setPanPosition(const QPointF& pos)
{
    m_panPosition = pos;
    clampPan();
    
    emit panChanged(m_panPosition);
    emit viewChanged();
}

void ZoomController::setImageSize(const QSize& size)
{
    m_imageSize = size;
    updatePanLimits();
    clampPan();
    emit viewChanged();
}

void ZoomController::setViewportSize(const QSize& size)
{
    m_viewportSize = size;
    updatePanLimits();
    clampPan();
    emit viewChanged();
}

QRectF ZoomController::getVisibleRect() const
{
    float visibleWidth = m_viewportSize.width() / m_zoomLevel;
    float visibleHeight = m_viewportSize.height() / m_zoomLevel;
    
    return QRectF(
        m_panPosition.x() - visibleWidth / 2.0f,
        m_panPosition.y() - visibleHeight / 2.0f,
        visibleWidth,
        visibleHeight
    );
}

QRectF ZoomController::getVisibleRectInImage() const
{
    QRectF visible = getVisibleRect();
    
    // Clamp to image bounds
    float left = std::max(0.0f, visible.left());
    float top = std::max(0.0f, visible.top());
    float right = std::min(static_cast<float>(m_imageSize.width()), visible.right());
    float bottom = std::min(static_cast<float>(m_imageSize.height()), visible.bottom());
    
    return QRectF(left, top, right - left, bottom - top);
}

QPointF ZoomController::viewportToImage(const QPointF& viewportPos) const
{
    QRectF visibleRect = getVisibleRect();
    
    float imageX = visibleRect.left() + (viewportPos.x() / m_viewportSize.width()) * visibleRect.width();
    float imageY = visibleRect.top() + (viewportPos.y() / m_viewportSize.height()) * visibleRect.height();
    
    return QPointF(imageX, imageY);
}

QPointF ZoomController::imageToViewport(const QPointF& imagePos) const
{
    QRectF visibleRect = getVisibleRect();
    
    float viewportX = ((imagePos.x() - visibleRect.left()) / visibleRect.width()) * m_viewportSize.width();
    float viewportY = ((imagePos.y() - visibleRect.top()) / visibleRect.height()) * m_viewportSize.height();
    
    return QPointF(viewportX, viewportY);
}

QRectF ZoomController::getMinimapRect() const
{
    if (m_zoomLevel <= 1.0f) {
        return QRectF(0, 0, 1, 1);
    }
    
    QRectF visible = getVisibleRectInImage();
    
    return QRectF(
        visible.left() / m_imageSize.width(),
        visible.top() / m_imageSize.height(),
        visible.width() / m_imageSize.width(),
        visible.height() / m_imageSize.height()
    );
}

void ZoomController::updatePanLimits()
{
    float halfVisibleWidth = (m_viewportSize.width() / m_zoomLevel) / 2.0f;
    float halfVisibleHeight = (m_viewportSize.height() / m_zoomLevel) / 2.0f;
    
    m_minPan = QPointF(halfVisibleWidth, halfVisibleHeight);
    m_maxPan = QPointF(
        m_imageSize.width() - halfVisibleWidth,
        m_imageSize.height() - halfVisibleHeight
    );
    
    // If zoomed out enough that viewport is larger than image
    if (m_minPan.x() > m_maxPan.x()) {
        m_minPan.setX(m_imageSize.width() / 2.0f);
        m_maxPan.setX(m_imageSize.width() / 2.0f);
    }
    if (m_minPan.y() > m_maxPan.y()) {
        m_minPan.setY(m_imageSize.height() / 2.0f);
        m_maxPan.setY(m_imageSize.height() / 2.0f);
    }
}

void ZoomController::clampPan()
{
    m_panPosition.setX(std::clamp(m_panPosition.x(), m_minPan.x(), m_maxPan.x()));
    m_panPosition.setY(std::clamp(m_panPosition.y(), m_minPan.y(), m_maxPan.y()));
}

float ZoomController::clampZoom(float zoom) const
{
    float clampedZoom = std::clamp(zoom, MIN_ZOOM, MAX_ZOOM);
    
    // Snap to 100% if close
    if (std::abs(clampedZoom - 1.0f) < 0.05f) {
        clampedZoom = 1.0f;
    }
    
    return clampedZoom;
}