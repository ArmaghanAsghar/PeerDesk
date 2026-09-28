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

// Shows the remote frame letterboxed, and converts local mouse and keyboard
// input into host-pixel events.
class VideoSurface : public QWidget {
    Q_OBJECT
public:
    explicit VideoSurface(QWidget* parent = nullptr);
    // Replace the displayed frame and schedule a repaint.
    void setFrame(const QImage& img);
    // Set the host screen size used for letterboxing and coordinate mapping.
    void setHostSize(int w, int h);

signals:
    // Mouse input in host pixels. `action` is a MouseAction value, `button` is
    // 1/2/3 = left/middle/right, and `wheel` is +1/-1 for Wheel actions.
    void mappedMouse(quint8 action, quint8 button, quint16 x, quint16 y, qint16 wheel);
    // Key press (down = 1) or release (down = 0) as an X11 keysym.
    void mappedKey(quint8 down, quint32 keysym);

protected:
    // Draw the frame letterboxed, or a placeholder until the first frame arrives.
    void paintEvent(QPaintEvent* event) override;
    // The mouse handlers forward events inside the image area as mappedMouse.
    // A press also grabs keyboard focus.
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    // Forward mapped keys as mappedKey. Auto-repeat and unmapped keys are dropped.
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;

private:
    // Map `pos` through the letterbox and emit mappedMouse. Does nothing if the
    // host size is unknown or `pos` falls in the letterbox bars.
    void emit_mouse(MouseAction action, const QPoint& pos, quint8 button, qint16 wheel);
    QImage frame_;
    int host_w_ = 0;
    int host_h_ = 0;
};

// Top-level client window. It switches between the connect form and the live
// session view, and owns the SessionWorker and its thread.
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    // Build both pages, start the worker thread, and wire up the signals.
    explicit MainWindow(QWidget* parent = nullptr);
    // Disconnect any session and stop the worker thread (waits up to 3 s).
    ~MainWindow() override;

private slots:
    // Validate the form and ask the worker to connect (queued to its thread).
    void onConnect();
    // Ask the worker to end the current session.
    void onDisconnect();
    // Switch to the session page for a host of w x h pixels.
    void showSession(int w, int h);
    // Return to the connect form and show `message`, in red if `error`.
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
