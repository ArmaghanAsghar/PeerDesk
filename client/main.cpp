#include "MainWindow.hpp"

#include <QtWidgets/QApplication>

// PeerDesk viewer entry point: show the connect window and run the Qt event loop.
int main(int argc, char** argv) {
    QApplication app(argc, argv);
    app.setApplicationName("PeerDesk");
    app.setOrganizationName("PeerDesk");
    peerdesk::MainWindow w;
    w.show();
    return app.exec();
}
