#include <QApplication>
#include <QMessageBox>
#include "MainWindow.h"
#include "toupcam.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("Toupcam Viewer");
    app.setOrganizationName("ToupcamApp");

    // Check if camera is available
    ToupcamDeviceV2 arr[TOUPCAM_MAX];
    unsigned count = Toupcam_EnumV2(arr);
    
    if (count == 0) {
        QMessageBox::critical(nullptr, "Error", "No Toupcam camera found!");
        return -1;
    }

    // Create and show main window
    MainWindow window;
    window.show();

    return app.exec();
}