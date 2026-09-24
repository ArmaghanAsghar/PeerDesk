#include "MainWindow.hpp"

#include <QtWidgets/QApplication>

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    app.setApplicationName("PeerDesk");
    app.setOrganizationName("PeerDesk");
    peerdesk::MainWindow w;
    w.show();
    return app.exec();
}
