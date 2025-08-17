#include "app_main_window.h"
#include <QApplication>
#include <QSurfaceFormat>

int main(int argc, char* argv[]) {
    // Set OpenGL format
    QSurfaceFormat format;
    format.setVersion(3, 3);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
    format.setSwapInterval(1); // Enable VSync
    QSurfaceFormat::setDefaultFormat(format);
    
    // Create Qt application
    QApplication app(argc, argv);
    app.setApplicationName("Toupcam ATR2600M");
    app.setOrganizationName("ToupcamApp");
    
    // High DPI support is enabled by default in Qt6
    // No need to set AA_EnableHighDpiScaling or AA_UseHighDpiPixmaps
    
    // Create and show main window
    AppMainWindow window;
    window.show();
    
    return app.exec();
}