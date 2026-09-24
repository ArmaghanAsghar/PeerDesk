#pragma once

#include "SessionWorker.hpp"
#include "peerdesk/map.hpp"

#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QStackedWidget>
#include <QtWidgets/QWidget>

class QThread;

namespace peerdesk {

class VideoSurface : public QWidget {
    Q_OBJECT
public:
    explicit VideoSurface(QWidget* parent = nullptr);
    void setFrame(const QImage& img);
    void setHostSize(int w, int h);

signals:
    void mappedMouse(quint8 action, quint8 button, quint16 x, quint16 y, qint16 wheel);
    void mappedKey(quint8 down, quint32 keysym);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;

private:
    void emit_mouse(MouseAction action, const QPoint& pos, quint8 button, qint16 wheel);
    QImage frame_;
    int host_w_ = 0;
    int host_h_ = 0;
};

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private slots:
    void onConnect();
    void onDisconnect();
    void showSession(int w, int h);
    void showForm(const QString& message, bool error);

private:
    QStackedWidget* stack_ = nullptr;
    QWidget* form_page_ = nullptr;
    QLineEdit* ip_ = nullptr;
    QLineEdit* port_ = nullptr;
    QLineEdit* user_ = nullptr;
    QLineEdit* pass_ = nullptr;
    QLabel* form_status_ = nullptr;
    QPushButton* connect_btn_ = nullptr;
    VideoSurface* video_ = nullptr;
    QLabel* session_status_ = nullptr;
    QThread* thread_ = nullptr;
    SessionWorker* worker_ = nullptr;
};

}  // namespace peerdesk
