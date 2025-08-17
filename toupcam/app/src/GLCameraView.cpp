#include "GLCameraView.h"
#include "CameraController.h"

#include <QMouseEvent>
#include <QWheelEvent>
#include <QPainter>
#include <QtMath>
#include <QOpenGLFunctions>
#include <QOpenGLContext>

GLCameraView::GLCameraView(QWidget* parent)
	: QOpenGLWidget(parent)
	, m_ctrl(nullptr)
	, m_texId(0)
	, m_texW(0)
	, m_texH(0)
	, m_zoom(1.0f)
	, m_pan(0,0)
	, m_middleDragging(false)
	, m_lastMouse(0,0)
	, m_reticleImagePt(0,0)
	, m_reticleShownMs(0)
	, m_reticleVisible(false)
{
	setMouseTracking(true);
	setFocusPolicy(Qt::StrongFocus);
	m_drawTimer.start();
	m_reticleTimer.start();
}

GLCameraView::~GLCameraView()
{
	makeCurrent();
	if (m_texId) {
		glDeleteTextures(1, &m_texId);
		m_texId = 0;
	}
	doneCurrent();
}

void GLCameraView::setController(CameraController* ctrl)
{
	m_ctrl = ctrl;
}

void GLCameraView::setTargetDisplaySize(int, int)
{
	// Reserved for layout hints if needed
}

void GLCameraView::refresh()
{
	update();
}

void GLCameraView::showAeSpotAt(const QPoint& imagePt)
{
	m_reticleImagePt = imagePt;
	m_reticleVisible = true;
	m_reticleTimer.restart();
	update();
}

void GLCameraView::clearAeSpot()
{
	m_reticleVisible = false;
	update();
}

void GLCameraView::initializeGL()
{
	initializeOpenGLFunctions();
	if (!context() || !context()->isValid()) return;
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_BLEND);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	m_prog = new QOpenGLShaderProgram(this);
	m_prog->addShaderFromSourceCode(QOpenGLShader::Vertex,
		"#version 300 es\n"
		"layout(location=0) in vec2 aPos;\n"
		"layout(location=1) in vec2 aTex;\n"
		"out vec2 vTex;\n"
		"uniform vec4 uUv;\n" // u0,v0,u1,v1
		"void main(){\n"
		"  vTex = mix(uUv.xy, uUv.zw, aTex);\n"
		"  gl_Position = vec4(aPos, 0.0, 1.0);\n"
		"}\n");
	m_prog->addShaderFromSourceCode(QOpenGLShader::Fragment,
		"#version 300 es\n"
		"precision mediump float;\n"
		"in vec2 vTex;\n"
		"uniform sampler2D uTex;\n"
		"out vec4 frag;\n"
		"void main(){\n"
		"  float g = texture(uTex, vTex).r;\n"
		"  frag = vec4(g, g, g, 1.0);\n"
		"}\n");
	m_prog->link();
	m_uniformSampler = m_prog->uniformLocation("uTex");
	m_uniformUv = m_prog->uniformLocation("uUv");
	m_vbo.create();
	m_vao.create();
}

void GLCameraView::resizeGL(int, int)
{
}

void GLCameraView::uploadTextureIfNeeded(int w, int h)
{
	if (m_texId == 0 || m_texW != w || m_texH != h) {
		if (m_texId) {
			glDeleteTextures(1, &m_texId);
			m_texId = 0;
		}
		glGenTextures(1, &m_texId);
		glBindTexture(GL_TEXTURE_2D, m_texId);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, w, h, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);
		m_texW = w;
		m_texH = h;
	}
}

QRectF GLCameraView::imageRectOnWidget() const
{
	// Preserve aspect ratio fit into widget rect
	if (!m_ctrl) return QRectF();
	QSize fs = m_ctrl->finalSize();
	if (fs.isEmpty()) return QRectF();
	double widgetW = width();
	double widgetH = height();
	double imgW = fs.width();
	double imgH = fs.height();
	double scale = qMin(widgetW / imgW, widgetH / imgH);
	double w = imgW * scale;
	double h = imgH * scale;
	double x = (widgetW - w) * 0.5;
	double y = (widgetH - h) * 0.5;
	return QRectF(x, y, w, h);
}

QPointF GLCameraView::widgetToImage(const QPointF& p) const
{
	QRectF r = imageRectOnWidget();
	QSize fs = m_ctrl ? m_ctrl->finalSize() : QSize();
	if (fs.isEmpty() || r.width() <= 0 || r.height() <= 0) return QPointF();
	QPointF clamped = p - r.topLeft();
	clamped.setX(qBound(0.0, clamped.x(), r.width()));
	clamped.setY(qBound(0.0, clamped.y(), r.height()));
	double sx = fs.width() / r.width();
	double sy = fs.height() / r.height();
	QPointF img(clamped.x() * sx, clamped.y() * sy);
	// apply zoom/pan viewport transform
	QPointF vp = img;
	vp.setX(m_pan.x() + vp.x() / m_zoom);
	vp.setY(m_pan.y() + vp.y() / m_zoom);
	return vp;
}

QPointF GLCameraView::imageToWidget(const QPointF& p) const
{
	QRectF r = imageRectOnWidget();
	QSize fs = m_ctrl ? m_ctrl->finalSize() : QSize();
	if (fs.isEmpty() || r.width() <= 0 || r.height() <= 0) return QPointF();
	QPointF img = p;
	img.setX((img.x() - m_pan.x()) * m_zoom);
	img.setY((img.y() - m_pan.y()) * m_zoom);
	double sx = r.width() / fs.width();
	double sy = r.height() / fs.height();
	QPointF wpt(r.left() + img.x() * sx, r.top() + img.y() * sy);
	return wpt;
}

void GLCameraView::paintGL()
{
	glViewport(0, 0, width(), height());
	glClearColor(0, 0, 0, 1);
	glClear(GL_COLOR_BUFFER_BIT);
	if (!m_ctrl) return;

	const uchar* data = nullptr;
	int w = 0, h = 0; qint64 ts = 0;
	if (!m_ctrl->getLatestFrame(data, w, h, ts) || data == nullptr || w == 0 || h == 0) {
		return;
	}
	uploadTextureIfNeeded(w, h);

	glBindTexture(GL_TEXTURE_2D, m_texId);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RED, GL_UNSIGNED_BYTE, data);

	QRectF dst = imageRectOnWidget();
	if (dst.isEmpty()) return;

	// Compute viewport from zoom/pan
	float vx = m_pan.x();
	float vy = m_pan.y();
	float vw = w / m_zoom;
	float vh = h / m_zoom;
	vx = qBound(0.0f, vx, float(w - vw));
	vy = qBound(0.0f, vy, float(h - vh));

	// Draw textured quad with nearest sampling and viewport selection using texture coords
	float u0 = vx / float(w);
	float v0 = vy / float(h);
	float u1 = (vx + vw) / float(w);
	float v1 = (vy + vh) / float(h);

	// Modern GL draw using shader and VBO/VAO
	m_vao.bind();
	m_vbo.bind();
	// quad covering dst, but we draw in NDC then let painter overlays handle other transforms
	float x0 = -1.0f + 2.0f * dst.left() / width();
	float y0 =  1.0f - 2.0f * dst.top() / height();
	float x1 = -1.0f + 2.0f * dst.right() / width();
	float y1 =  1.0f - 2.0f * dst.bottom() / height();
	float verts[] = {
		x0, y0, 0.f, 0.f,
		x1, y0, 1.f, 0.f,
		x1, y1, 1.f, 1.f,
		x0, y1, 0.f, 1.f
	};
	m_vbo.allocate(verts, sizeof(verts));
	m_prog->bind();
	m_prog->setUniformValue(m_uniformSampler, 0);
	m_prog->setUniformValue(m_uniformUv, QVector4D(u0, v0, u1, v1));
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, m_texId);
	m_vao.bind();
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
	glDisableVertexAttribArray(0);
	glDisableVertexAttribArray(1);
	m_prog->release();
	m_vbo.release();
	m_vao.release();

	// Overlays with QPainter
	QPainter p(this);
	p.setRenderHint(QPainter::Antialiasing, false);

	// Mini-map when zoomed in
	if (m_zoom > 1.0f) {
		QRect viewport = dst.toRect();
		drawMiniMap(p, viewport);
	}

	// AE reticle
	if (m_reticleVisible) {
		drawAeReticle(p);
	}
}

void GLCameraView::drawMiniMap(QPainter& p, const QRect& viewport)
{
	// Bottom-right mini-map, 20% of shorter side
	int size = qMin(viewport.width(), viewport.height()) * 0.25;
	QRect mini(viewport.right() - size - 12, viewport.bottom() - size - 12, size, size);
	p.setPen(QPen(Qt::white, 2));
	p.drawRect(mini);

	// Inner rect scales with 1/z and position reflects m_pan
	QSize fs = m_ctrl->finalSize();
	if (fs.isEmpty()) return;
	float z = m_zoom;
	if (z <= 1.0f) return;
	float innerW = mini.width() / z;
	float innerH = mini.height() / z;
	float panXRatio = m_pan.x() / float(fs.width());
	float panYRatio = m_pan.y() / float(fs.height());
	float maxPanX = 1.0f - (1.0f / z);
	float maxPanY = 1.0f - (1.0f / z);
	float relX = (maxPanX > 0.f) ? panXRatio / maxPanX : 0.f;
	float relY = (maxPanY > 0.f) ? panYRatio / maxPanY : 0.f;
	int innerX = mini.left() + int(relX * (mini.width() - innerW));
	int innerY = mini.top() + int(relY * (mini.height() - innerH));
	QRect inner(innerX, innerY, int(innerW), int(innerH));
	p.drawRect(inner);
}

void GLCameraView::drawAeReticle(QPainter& p)
{
	qint64 ms = m_reticleTimer.elapsed();
	if (ms > 200 + 5000 + 500) {
		m_reticleVisible = false;
		return;
	}

	// Alpha animation: ease-in 200ms, hold, fade 500ms
	float alpha = 1.0f;
	if (ms < 200) {
		alpha = float(ms) / 200.0f;
	}
	else if (ms > 200 + 5000) {
		float f = float(ms - 5200) / 500.0f;
		alpha = qBound(0.0f, 1.0f - f, 1.0f);
	}

	p.setOpacity(alpha);
	QPointF wpt = imageToWidget(m_reticleImagePt);
	const int size = 80;
	QRect r(int(wpt.x()) - size/2, int(wpt.y()) - size/2, size, size);
	p.setPen(QPen(Qt::yellow, 2));
	p.drawRect(r);
	p.drawLine(r.center().x() - 10, r.center().y(), r.center().x() + 10, r.center().y());
	p.drawLine(r.center().x(), r.center().y() - 10, r.center().x(), r.center().y() + 10);
	p.setOpacity(1.0);
}

void GLCameraView::mousePressEvent(QMouseEvent* e)
{
	if (e->button() == Qt::MiddleButton) {
		m_middleDragging = true;
		m_lastMouse = e->pos();
		return;
	}
	if (e->button() == Qt::LeftButton) {
		QPointF img = widgetToImage(e->pos());
		emit clickedImage(int(img.x()), int(img.y()));
	}
}

void GLCameraView::mouseDoubleClickEvent(QMouseEvent* e)
{
	if (e->button() == Qt::LeftButton) {
		emit doubleClicked();
	}
}

void GLCameraView::mouseMoveEvent(QMouseEvent* e)
{
	if (m_middleDragging) {
		QPoint delta = e->pos() - m_lastMouse;
		m_lastMouse = e->pos();
		// Convert delta in widget space to image-space pan delta
		QRectF r = imageRectOnWidget();
		QSize fs = m_ctrl ? m_ctrl->finalSize() : QSize();
		if (!fs.isEmpty() && r.width() > 0 && r.height() > 0) {
			double sx = fs.width() / r.width();
			double sy = fs.height() / r.height();
			m_pan.setX(qBound(0.0, m_pan.x() - delta.x() * sx / m_zoom, double(fs.width() - fs.width() / m_zoom)));
			m_pan.setY(qBound(0.0, m_pan.y() - delta.y() * sy / m_zoom, double(fs.height() - fs.height() / m_zoom)));
			update();
		}
	}
}

void GLCameraView::wheelEvent(QWheelEvent* e)
{
	// Ctrl+wheel zoom to cursor
	if (e->modifiers() & Qt::ControlModifier) {
		float numDegrees = e->angleDelta().y() / 8.0f;
		float numSteps = numDegrees / 15.0f;
		float factor = std::pow(1.1f, numSteps);
		QPointF cursorImg = widgetToImage(e->position());
		float newZoom = qMax(1.0f, m_zoom * factor);
		// Keep cursor point stable
		QSize fs = m_ctrl ? m_ctrl->finalSize() : QSize();
		if (!fs.isEmpty()) {
			QPointF vp = m_pan;
			vp.setX(cursorImg.x() - (cursorImg.x() - m_pan.x()) * (m_zoom / newZoom));
			vp.setY(cursorImg.y() - (cursorImg.y() - m_pan.y()) * (m_zoom / newZoom));
			float vw = fs.width() / newZoom;
			float vh = fs.height() / newZoom;
			vp.setX(qBound(0.0, vp.x(), double(fs.width() - vw)));
			vp.setY(qBound(0.0, vp.y(), double(fs.height() - vh)));
			m_pan = vp;
		}
		m_zoom = newZoom;
		update();
		return;
	}
	// Wheel without modifier: vertical pan
	QPoint numPixels = e->pixelDelta();
	QPoint numDegrees = e->angleDelta();
	int dy = numPixels.y() ? numPixels.y() : (numDegrees.y() / 8 * 1.5);
	QRectF r = imageRectOnWidget();
	QSize fs = m_ctrl ? m_ctrl->finalSize() : QSize();
	if (!fs.isEmpty() && r.width() > 0 && r.height() > 0) {
		double sy = fs.height() / r.height();
		m_pan.setY(qBound(0.0, m_pan.y() - dy * sy / m_zoom, double(fs.height() - fs.height() / m_zoom)));
		update();
	}
}