#include "CameraWidget.h"
#include <QMouseEvent>
#include <QWheelEvent>
#include <QPainter>
#include <QOpenGLContext>
#include <QDateTime>
#include <QMatrix4x4>
#include <cmath>

// Vertex shader for texture rendering
static const char *vertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec2 position;
layout (location = 1) in vec2 texCoord;
out vec2 TexCoord;
uniform mat4 transform;
void main() {
    gl_Position = transform * vec4(position, 0.0, 1.0);
    TexCoord = texCoord;
}
)";

// Fragment shader for grayscale texture
static const char *fragmentShaderSource = R"(
#version 330 core
in vec2 TexCoord;
out vec4 FragColor;
uniform sampler2D texture1;
void main() {
    float gray = texture(texture1, TexCoord).r;
    FragColor = vec4(gray, gray, gray, 1.0);
}
)";

CameraWidget::CameraWidget(QWidget *parent)
    : QOpenGLWidget(parent)
    , m_imageUpdated(false)
    , m_isPanning(false)
{
    setMouseTracking(true);
    
    // Create zoom controller
    m_zoomController = new ZoomController(this);
    connect(m_zoomController, &ZoomController::viewChanged, this, QOverload<>::of(&QWidget::update));
    
    // Create FPS label
    m_fpsLabel = new QLabel(this);
    m_fpsLabel->setStyleSheet(
        "QLabel {"
        "   color: #00FF00;"
        "   font-size: 10px;"
        "   font-family: monospace;"
        "   background-color: rgba(0, 0, 0, 128);"
        "   padding: 2px 4px;"
        "   border-radius: 2px;"
        "}"
    );
    m_fpsLabel->move(10, 10);
    setFPS(0.0f);
    
    // Create auto exposure overlay
    m_aeOverlay = new AutoExposureOverlay(this);
    m_aeOverlay->hide();
}

CameraWidget::~CameraWidget()
{
    makeCurrent();
    m_vbo.destroy();
    doneCurrent();
}

void CameraWidget::initializeGL()
{
    initializeOpenGLFunctions();
    
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    
    // Create shader program
    m_program = std::make_unique<QOpenGLShaderProgram>();
    m_program->addShaderFromSourceCode(QOpenGLShader::Vertex, vertexShaderSource);
    m_program->addShaderFromSourceCode(QOpenGLShader::Fragment, fragmentShaderSource);
    m_program->link();
    
    // Create vertex buffer
    float vertices[] = {
        // positions   // texture coords
        -1.0f,  1.0f,  0.0f, 0.0f,
        -1.0f, -1.0f,  0.0f, 1.0f,
         1.0f, -1.0f,  1.0f, 1.0f,
         1.0f,  1.0f,  1.0f, 0.0f
    };
    
    unsigned int indices[] = {
        0, 1, 2,
        0, 2, 3
    };
    
    m_vbo.create();
    m_vbo.bind();
    m_vbo.allocate(vertices, sizeof(vertices));
    
    m_program->bind();
    m_program->enableAttributeArray(0);
    m_program->setAttributeBuffer(0, GL_FLOAT, 0, 2, 4 * sizeof(float));
    m_program->enableAttributeArray(1);
    m_program->setAttributeBuffer(1, GL_FLOAT, 2 * sizeof(float), 2, 4 * sizeof(float));
    m_program->release();
}

void CameraWidget::paintGL()
{
    glClear(GL_COLOR_BUFFER_BIT);
    
    if (!m_texture || !m_currentImage.isNull()) {
        updateTexture();
    }
    
    if (m_texture) {
        m_program->bind();
        
        // Calculate transform matrix based on zoom and pan
        QMatrix4x4 transform;
        QRectF visibleRect = m_zoomController->getVisibleRectInImage();
        
        // Convert image coordinates to normalized device coordinates
        float left = (visibleRect.left() / m_currentImage.width()) * 2.0f - 1.0f;
        float right = (visibleRect.right() / m_currentImage.width()) * 2.0f - 1.0f;
        float bottom = (visibleRect.bottom() / m_currentImage.height()) * 2.0f - 1.0f;
        float top = (visibleRect.top() / m_currentImage.height()) * 2.0f - 1.0f;
        
        transform.ortho(left, right, bottom, top, -1.0f, 1.0f);
        m_program->setUniformValue("transform", transform);
        
        m_texture->bind();
        m_vbo.bind();
        
        glDrawArrays(GL_TRIANGLES, 0, 6);
        
        m_texture->release();
        m_vbo.release();
        m_program->release();
    }
    
    // Draw minimap if zoomed in
    if (m_zoomController->isZoomedIn()) {
        drawMinimap();
    }
}

void CameraWidget::resizeGL(int w, int h)
{
    glViewport(0, 0, w, h);
    m_zoomController->setViewportSize(QSize(w, h));
}

void CameraWidget::updateImage(const QImage& image)
{
    m_currentImage = image;
    m_imageUpdated = true;
    m_zoomController->setImageSize(image.size());
    update();
}

void CameraWidget::setFPS(float fps)
{
    m_fpsLabel->setText(QString("FPS: %1").arg(fps, 0, 'f', 1));
}

void CameraWidget::updateTexture()
{
    if (m_currentImage.isNull()) return;
    
    if (!m_texture) {
        m_texture = std::make_unique<QOpenGLTexture>(QOpenGLTexture::Target2D);
    }
    
    if (m_imageUpdated) {
        m_texture->destroy();
        m_texture->create();
        m_texture->setData(m_currentImage);
        m_texture->setMinificationFilter(QOpenGLTexture::Linear);
        m_texture->setMagnificationFilter(QOpenGLTexture::Linear);
        m_texture->setWrapMode(QOpenGLTexture::ClampToEdge);
        m_imageUpdated = false;
    }
}

void CameraWidget::drawMinimap()
{
    // Save OpenGL state
    GLboolean blendEnabled;
    glGetBooleanv(GL_BLEND, &blendEnabled);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // Draw using QPainter
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    int x = width() - MINIMAP_SIZE - MINIMAP_MARGIN;
    int y = height() - MINIMAP_SIZE - MINIMAP_MARGIN;
    
    // Draw minimap background
    painter.fillRect(x, y, MINIMAP_SIZE, MINIMAP_SIZE, QColor(50, 50, 50, 200));
    
    // Draw border
    painter.setPen(QPen(Qt::white, 1));
    painter.drawRect(x, y, MINIMAP_SIZE, MINIMAP_SIZE);
    
    // Draw visible area rectangle
    QRectF visibleNorm = m_zoomController->getMinimapRect();
    
    int rectX = x + visibleNorm.x() * MINIMAP_SIZE;
    int rectY = y + visibleNorm.y() * MINIMAP_SIZE;
    int rectW = visibleNorm.width() * MINIMAP_SIZE;
    int rectH = visibleNorm.height() * MINIMAP_SIZE;
    
    painter.setPen(QPen(Qt::yellow, 2));
    painter.drawRect(rectX, rectY, rectW, rectH);
    
    painter.end();
    
    // Restore OpenGL state
    if (!blendEnabled) {
        glDisable(GL_BLEND);
    }
}

void CameraWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        // Check for double-click handled separately
        if (event->modifiers() == Qt::NoModifier) {
            handleAutoExposureClick(event->pos());
        }
    } else if (event->button() == Qt::MiddleButton) {
        m_isPanning = true;
        m_lastMousePos = event->pos();
        setCursor(Qt::ClosedHandCursor);
    }
}

void CameraWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::MiddleButton) {
        m_isPanning = false;
        setCursor(Qt::ArrowCursor);
    }
}

void CameraWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (m_isPanning) {
        QPoint delta = event->pos() - m_lastMousePos;
        m_zoomController->pan(QPointF(delta));
        m_lastMousePos = event->pos();
    }
}

void CameraWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        emit autoExposureReset();
    }
}

void CameraWidget::wheelEvent(QWheelEvent *event)
{
    float delta = event->angleDelta().y();
    bool toCursor = event->modifiers() & Qt::ControlModifier;
    
    if (delta > 0) {
        m_zoomController->zoomIn(event->position(), toCursor);
    } else if (delta < 0) {
        m_zoomController->zoomOut(event->position(), toCursor);
    }
}

void CameraWidget::handleAutoExposureClick(const QPoint& pos)
{
    // Convert click position to image coordinates
    QPointF imagePos = m_zoomController->viewportToImage(pos);
    
    // Create 300x300 rectangle centered on click
    int rectSize = 300;
    QRect aeRect(
        imagePos.x() - rectSize/2,
        imagePos.y() - rectSize/2,
        rectSize,
        rectSize
    );
    
    // Clamp to image bounds
    aeRect = aeRect.intersected(QRect(0, 0, m_currentImage.width(), m_currentImage.height()));
    
    // Show auto exposure indicator
    m_aeOverlay->showAt(mapToGlobal(pos));
    
    // Emit signal to set AE rectangle
    emit autoExposureRectRequested(aeRect);
}