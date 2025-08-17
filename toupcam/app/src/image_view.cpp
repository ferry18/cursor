#include "image_view.h"
#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>

ImageView::ImageView(QWidget* parent)
    : QOpenGLWidget(parent)
    , m_vbo(QOpenGLBuffer::VertexBuffer) {
    
    // Enable mouse tracking for hover effects
    setMouseTracking(true);
    
    // Pre-allocate image buffer
    m_imageData.resize(CAMERA_WIDTH * CAMERA_HEIGHT);
}

ImageView::~ImageView() {
    makeCurrent();
    m_vao.destroy();
    m_vbo.destroy();
    doneCurrent();
}

void ImageView::initializeGL() {
    initializeOpenGLFunctions();
    
    // Set clear color
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    
    // Initialize shaders
    initShaders();
    
    // Setup vertex data for a quad
    float vertices[] = {
        // Position    // TexCoord
        -1.0f, -1.0f,  0.0f, 1.0f,
         1.0f, -1.0f,  1.0f, 1.0f,
         1.0f,  1.0f,  1.0f, 0.0f,
        -1.0f,  1.0f,  0.0f, 0.0f
    };
    
    // Create VAO and VBO
    m_vao.create();
    m_vao.bind();
    
    m_vbo.create();
    m_vbo.bind();
    m_vbo.allocate(vertices, sizeof(vertices));
    
    // Set vertex attributes
    m_program->enableAttributeArray(0);
    m_program->setAttributeBuffer(0, GL_FLOAT, 0, 2, 4 * sizeof(float));
    
    m_program->enableAttributeArray(1);
    m_program->setAttributeBuffer(1, GL_FLOAT, 2 * sizeof(float), 2, 4 * sizeof(float));
    
    m_vao.release();
    
    // Create texture
    m_texture = std::make_unique<QOpenGLTexture>(QOpenGLTexture::Target2D);
    m_texture->create();
    m_texture->setMinificationFilter(QOpenGLTexture::Linear);
    m_texture->setMagnificationFilter(QOpenGLTexture::Linear);
    m_texture->setWrapMode(QOpenGLTexture::ClampToEdge);
}

void ImageView::initShaders() {
    m_program = std::make_unique<QOpenGLShaderProgram>();
    
    // Vertex shader
    const char* vertexShader = R"(
        #version 330 core
        layout (location = 0) in vec2 position;
        layout (location = 1) in vec2 texCoord;
        
        out vec2 TexCoord;
        
        uniform mat4 view;
        uniform mat4 projection;
        
        void main() {
            gl_Position = projection * view * vec4(position, 0.0, 1.0);
            TexCoord = texCoord;
        }
    )";
    
    // Fragment shader for monochrome display
    const char* fragmentShader = R"(
        #version 330 core
        in vec2 TexCoord;
        out vec4 FragColor;
        
        uniform sampler2D texture1;
        
        void main() {
            float gray = texture(texture1, TexCoord).r;
            FragColor = vec4(gray, gray, gray, 1.0);
        }
    )";
    
    m_program->addShaderFromSourceCode(QOpenGLShader::Vertex, vertexShader);
    m_program->addShaderFromSourceCode(QOpenGLShader::Fragment, fragmentShader);
    m_program->link();
}

void ImageView::resizeGL(int w, int h) {
    glViewport(0, 0, w, h);
    
    // Update projection matrix to maintain aspect ratio
    float aspectRatio = float(w) / float(h);
    float imageAspect = float(m_imageWidth) / float(m_imageHeight);
    
    m_projMatrix.setToIdentity();
    if (aspectRatio > imageAspect) {
        float scale = imageAspect / aspectRatio;
        m_projMatrix.scale(scale, 1.0f);
    } else {
        float scale = aspectRatio / imageAspect;
        m_projMatrix.scale(1.0f, scale);
    }
    
    updateViewMatrix();
}

void ImageView::paintGL() {
    glClear(GL_COLOR_BUFFER_BIT);
    
    drawImage();
    
    // Draw overlays using QPainter
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    drawOverlays();
    
    if (m_zoom > 1.0) {
        drawMiniMap();
    }
    
    if (m_aeIcon.visible) {
        drawAEIcon();
    }
}

void ImageView::drawImage() {
    if (!m_texture || !m_program) return;
    
    m_program->bind();
    m_vao.bind();
    
    // Update texture if needed
    if (m_textureNeedsUpdate && !m_imageData.empty()) {
        m_texture->destroy();
        m_texture->create();
        m_texture->setSize(m_imageWidth, m_imageHeight);
        m_texture->setFormat(QOpenGLTexture::R8_UNorm);
        m_texture->allocateStorage();
        m_texture->setData(QOpenGLTexture::Red, QOpenGLTexture::UInt8, m_imageData.data());
        m_textureNeedsUpdate = false;
    }
    
    // Set uniforms
    m_program->setUniformValue("view", m_viewMatrix);
    m_program->setUniformValue("projection", m_projMatrix);
    m_program->setUniformValue("texture1", 0);
    
    // Bind texture
    m_texture->bind();
    
    // Draw quad
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    
    m_vao.release();
    m_program->release();
}

void ImageView::drawOverlays() {
    // Reserved for future overlays
}

void ImageView::drawMiniMap() {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // Calculate mini-map position (bottom-right corner)
    int x = width() - MINIMAP_WIDTH - MINIMAP_MARGIN;
    int y = height() - MINIMAP_HEIGHT - MINIMAP_MARGIN;
    
    // Draw mini-map background
    painter.fillRect(x, y, MINIMAP_WIDTH, MINIMAP_HEIGHT, QColor(50, 50, 50, 200));
    
    // Draw border
    painter.setPen(QPen(Qt::white, MINIMAP_BORDER));
    painter.drawRect(x, y, MINIMAP_WIDTH, MINIMAP_HEIGHT);
    
    // Calculate visible area in mini-map
    double miniMapScale = double(MINIMAP_WIDTH) / double(m_imageWidth);
    int visibleWidth = std::min(int(MINIMAP_WIDTH / m_zoom), MINIMAP_WIDTH);
    int visibleHeight = std::min(int(MINIMAP_HEIGHT / m_zoom), MINIMAP_HEIGHT);
    
    // Calculate position based on pan
    int visibleX = x + int((m_pan.x() + 1.0) * 0.5 * MINIMAP_WIDTH - visibleWidth * 0.5);
    int visibleY = y + int((1.0 - m_pan.y()) * 0.5 * MINIMAP_HEIGHT - visibleHeight * 0.5);
    
    // Constrain to mini-map bounds
    visibleX = std::max(x, std::min(visibleX, x + MINIMAP_WIDTH - visibleWidth));
    visibleY = std::max(y, std::min(visibleY, y + MINIMAP_HEIGHT - visibleHeight));
    
    // Draw visible area rectangle
    painter.setPen(QPen(Qt::yellow, 2));
    painter.drawRect(visibleX, visibleY, visibleWidth, visibleHeight);
}

void ImageView::drawAEIcon() {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // Calculate time since animation start
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - m_aeIcon.startTime).count();
    
    // Calculate opacity based on animation phase
    double opacity = 1.0;
    if (elapsed < AE_ICON_FADE_IN_MS) {
        // Fade in with ease
        double t = double(elapsed) / AE_ICON_FADE_IN_MS;
        opacity = t * t; // Ease in quadratic
    } else if (elapsed > AE_ICON_FADE_IN_MS + AE_ICON_HOLD_MS) {
        // Fade out
        double fadeOutElapsed = elapsed - AE_ICON_FADE_IN_MS - AE_ICON_HOLD_MS;
        if (fadeOutElapsed < AE_ICON_FADE_OUT_MS) {
            double t = 1.0 - double(fadeOutElapsed) / AE_ICON_FADE_OUT_MS;
            opacity = t * t; // Ease out quadratic
        } else {
            // Animation complete
            m_aeIcon.visible = false;
            return;
        }
    }
    
    // Convert image position to screen position
    QPoint screenPos = imageToScreen(m_aeIcon.position);
    
    // Draw iPhone-style AE icon (simplified)
    painter.setOpacity(opacity);
    painter.setPen(QPen(Qt::yellow, 2));
    painter.setBrush(Qt::NoBrush);
    
    // Draw rectangle with corner brackets
    int size = 60;
    int cornerLength = 15;
    QRect aeRect(screenPos.x() - size/2, screenPos.y() - size/2, size, size);
    
    // Draw corner brackets instead of full rectangle
    painter.drawLine(aeRect.topLeft(), aeRect.topLeft() + QPoint(cornerLength, 0));
    painter.drawLine(aeRect.topLeft(), aeRect.topLeft() + QPoint(0, cornerLength));
    
    painter.drawLine(aeRect.topRight(), aeRect.topRight() + QPoint(-cornerLength, 0));
    painter.drawLine(aeRect.topRight(), aeRect.topRight() + QPoint(0, cornerLength));
    
    painter.drawLine(aeRect.bottomLeft(), aeRect.bottomLeft() + QPoint(cornerLength, 0));
    painter.drawLine(aeRect.bottomLeft(), aeRect.bottomLeft() + QPoint(0, -cornerLength));
    
    painter.drawLine(aeRect.bottomRight(), aeRect.bottomRight() + QPoint(-cornerLength, 0));
    painter.drawLine(aeRect.bottomRight(), aeRect.bottomRight() + QPoint(0, -cornerLength));
    
    // Draw sun-like center indicator
    int centerSize = 8;
    painter.drawEllipse(screenPos, centerSize, centerSize);
    
    // Request update for animation
    if (m_aeIcon.visible) {
        update();
    }
}

void ImageView::updateImage(const uint8_t* data, int width, int height) {
    if (!data || width <= 0 || height <= 0) return;
    
    makeCurrent();
    
    // Update dimensions if changed
    if (width != m_imageWidth || height != m_imageHeight) {
        m_imageWidth = width;
        m_imageHeight = height;
        m_imageData.resize(width * height);
        resizeGL(this->width(), this->height());
    }
    
    // Copy image data
    std::memcpy(m_imageData.data(), data, width * height);
    m_textureNeedsUpdate = true;
    
    doneCurrent();
    update();
}

void ImageView::setZoom(double zoom) {
    m_zoom = std::max(1.0, zoom);
    updateViewMatrix();
    constrainPan();
    emit zoomChanged(m_zoom);
    update();
}

void ImageView::zoomToPoint(const QPoint& screenPos, double zoomDelta) {
    // Get image position before zoom
    QPointF imagePosBeforeZoom = screenToImage(screenPos);
    
    // Apply zoom
    double newZoom = m_zoom * (1.0 + zoomDelta);
    setZoom(newZoom);
    
    // Get image position after zoom
    QPointF imagePosAfterZoom = screenToImage(screenPos);
    
    // Adjust pan to keep the zoom point fixed
    QPointF delta = imagePosAfterZoom - imagePosBeforeZoom;
    panBy(QPointF(delta.x() * 2.0 / m_imageWidth, -delta.y() * 2.0 / m_imageHeight));
}

void ImageView::resetZoom() {
    m_zoom = 1.0;
    m_pan = QPointF(0.0, 0.0);
    updateViewMatrix();
    emit zoomChanged(m_zoom);
    update();
}

void ImageView::setPan(const QPointF& pan) {
    m_pan = pan;
    constrainPan();
    updateViewMatrix();
    update();
}

void ImageView::panBy(const QPointF& delta) {
    setPan(m_pan + delta);
}

void ImageView::updateViewMatrix() {
    m_viewMatrix.setToIdentity();
    m_viewMatrix.scale(m_zoom, m_zoom);
    m_viewMatrix.translate(m_pan.x(), m_pan.y());
}

void ImageView::constrainPan() {
    // Calculate maximum pan based on zoom
    double maxPan = std::max(0.0, (m_zoom - 1.0) / m_zoom);
    
    m_pan.setX(std::max(-maxPan, std::min(maxPan, m_pan.x())));
    m_pan.setY(std::max(-maxPan, std::min(maxPan, m_pan.y())));
}

QPointF ImageView::screenToImage(const QPoint& screenPos) const {
    // Normalize screen coordinates to [-1, 1]
    double normX = (2.0 * screenPos.x() / width()) - 1.0;
    double normY = 1.0 - (2.0 * screenPos.y() / height());
    
    // Apply inverse view transform
    QVector4D pos(normX, normY, 0.0, 1.0);
    pos = m_projMatrix.inverted() * pos;
    pos = m_viewMatrix.inverted() * pos;
    
    // Convert to image coordinates
    double imageX = (pos.x() + 1.0) * 0.5 * m_imageWidth;
    double imageY = (1.0 - pos.y()) * 0.5 * m_imageHeight;
    
    return QPointF(imageX, imageY);
}

QPoint ImageView::imageToScreen(const QPointF& imagePos) const {
    // Convert image coordinates to normalized [-1, 1]
    double normX = (2.0 * imagePos.x() / m_imageWidth) - 1.0;
    double normY = 1.0 - (2.0 * imagePos.y() / m_imageHeight);
    
    // Apply view transform
    QVector4D pos(normX, normY, 0.0, 1.0);
    pos = m_viewMatrix * pos;
    pos = m_projMatrix * pos;
    
    // Convert to screen coordinates
    int screenX = int((pos.x() + 1.0) * 0.5 * width());
    int screenY = int((1.0 - pos.y()) * 0.5 * height());
    
    return QPoint(screenX, screenY);
}

void ImageView::showAEIcon(const QPoint& screenPos) {
    m_aeIcon.visible = true;
    m_aeIcon.position = screenToImage(screenPos);
    m_aeIcon.startTime = std::chrono::steady_clock::now();
    update();
}

void ImageView::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        m_lastMousePos = event->pos();
    } else if (event->button() == Qt::MiddleButton) {
        m_panning = true;
        m_lastMousePos = event->pos();
        setCursor(Qt::ClosedHandCursor);
    }
}

void ImageView::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        // Single click - emit clicked signal
        QPointF imagePos = screenToImage(event->pos());
        emit clicked(imagePos);
    } else if (event->button() == Qt::MiddleButton) {
        m_panning = false;
        setCursor(Qt::ArrowCursor);
    }
}

void ImageView::mouseMoveEvent(QMouseEvent* event) {
    if (m_panning && event->buttons() & Qt::MiddleButton) {
        QPoint delta = event->pos() - m_lastMousePos;
        m_lastMousePos = event->pos();
        
        // Convert screen delta to normalized coordinates
        double dx = 2.0 * delta.x() / width() / m_zoom;
        double dy = -2.0 * delta.y() / height() / m_zoom;
        
        panBy(QPointF(dx, dy));
    }
}

void ImageView::mouseDoubleClickEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        QPointF imagePos = screenToImage(event->pos());
        emit doubleClicked(imagePos);
    }
}

void ImageView::wheelEvent(QWheelEvent* event) {
    if (event->modifiers() & Qt::ControlModifier) {
        // Zoom with Ctrl+wheel
        double delta = event->angleDelta().y() / 1200.0; // Smaller steps
        zoomToPoint(event->position().toPoint(), delta);
    } else {
        // Vertical scroll without Ctrl
        double scrollSpeed = 0.1 / m_zoom;
        double dy = event->angleDelta().y() / 120.0 * scrollSpeed;
        panBy(QPointF(0.0, dy));
    }
    
    event->accept();
}