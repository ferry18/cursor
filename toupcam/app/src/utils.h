#ifndef UTILS_H
#define UTILS_H

#include <QtCore>
#include <QtWidgets>
#include <QtOpenGL>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QtOpenGLWidgets>
#endif
#include <QPropertyAnimation>
#include <QEasingCurve>
#include <QGraphicsOpacityEffect>

#include <memory>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <vector>
#include <deque>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

#include "toupcam.h"

// Constants
constexpr int DEFAULT_LEFT_PANEL_WIDTH = 171;
constexpr int DEFAULT_WINDOW_WIDTH = 2560;
constexpr int DEFAULT_WINDOW_HEIGHT = 1600;
constexpr int DEFAULT_VIDEO_WIDTH = 2389;
constexpr int DEFAULT_VIDEO_HEIGHT = 1600;
constexpr int CAMERA_WIDTH = 3104;
constexpr int CAMERA_HEIGHT = 2084;
constexpr int AE_ROI_SIZE = 300;
constexpr double DEFAULT_AE_ROI_PERCENT = 0.7;
constexpr int AE_ICON_FADE_IN_MS = 200;
constexpr int AE_ICON_HOLD_MS = 5000;
constexpr int AE_ICON_FADE_OUT_MS = 500;

// Helper functions
inline QString getTimestampedFilename(const QString& prefix, const QString& extension, int width, int height) {
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S");
    ss << "_" << std::setfill('0') << std::setw(3) << ms.count();
    
    return QString("%1/%2_%3x%4.%5")
        .arg(prefix)
        .arg(QString::fromStdString(ss.str()))
        .arg(width)
        .arg(height)
        .arg(extension);
}

inline void ensureDirectoryExists(const QString& path) {
    std::filesystem::create_directories(path.toStdString());
}

// Ensure even dimensions for ROI
inline void makeEven(int& value) {
    value = (value / 2) * 2;
}

inline QRect makeEvenRect(const QRect& rect) {
    int x = (rect.x() / 2) * 2;
    int y = (rect.y() / 2) * 2;
    int w = (rect.width() / 2) * 2;
    int h = (rect.height() / 2) * 2;
    return QRect(x, y, w, h);
}

#endif // UTILS_H