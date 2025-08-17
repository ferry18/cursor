#pragma once

#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLBuffer>
#include <QOpenGLVertexArrayObject>
#include <QMatrix4x4>
#include <QTimer>
#include <QElapsedTimer>
#include <QPointF>
#include <QRectF>

class CameraController;

class GLCameraView : public QOpenGLWidget, protected QOpenGLFunctions {
	Q_OBJECT
public:
	explicit GLCameraView(QWidget* parent = nullptr);
	~GLCameraView() override;

	void setController(CameraController* ctrl);
	void setTargetDisplaySize(int width, int height);
	void refresh();
	void showAeSpotAt(const QPoint& imagePt);
	void clearAeSpot();

signals:
	void clickedImage(int x, int y);
	void doubleClicked();
	void fpsUpdated(double fps);

protected:
	void initializeGL() override;
	void resizeGL(int w, int h) override;
	void paintGL() override;

	void mousePressEvent(QMouseEvent* e) override;
	void mouseDoubleClickEvent(QMouseEvent* e) override;
	void mouseMoveEvent(QMouseEvent* e) override;
	void wheelEvent(QWheelEvent* e) override;

private:
	void uploadTextureIfNeeded(int w, int h);
	QRectF imageRectOnWidget() const;
	QPointF widgetToImage(const QPointF& p) const;
	QPointF imageToWidget(const QPointF& p) const;
	void drawMiniMap(QPainter& p, const QRect& viewport);
	void drawAeReticle(QPainter& p);

private:
	CameraController* m_ctrl;
	unsigned int m_texId;
	int m_texW;
	int m_texH;
	float m_zoom;
	QPointF m_pan;
	bool m_middleDragging;
	QPoint m_lastMouse;
	QElapsedTimer m_drawTimer;

	QOpenGLShaderProgram* m_prog;
	QOpenGLBuffer m_vbo;
	QOpenGLVertexArrayObject m_vao;
	int m_attrPos;
	int m_attrTex;
	int m_uniformSampler;
	int m_uniformUv;

	QPoint m_reticleImagePt;
	qint64 m_reticleShownMs;
	bool m_reticleVisible;
	QElapsedTimer m_reticleTimer;
};