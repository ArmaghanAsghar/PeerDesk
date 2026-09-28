// Client-side tests: keymap, VideoSurface input mapping, SessionWorker against an
// in-process synthetic host, and MainWindow form/session flow.

#include "MainWindow.hpp"
#include "SessionWorker.hpp"
#include "keymap.hpp"

#include "host_server.hpp"

#include <QtCore/QThread>
#include <QtGui/QKeyEvent>
#include <QtGui/QMouseEvent>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>
#include <QtWidgets/QApplication>

#include <csignal>
#include <filesystem>
#include <memory>
#include <thread>
#include <unistd.h>

using namespace peerdesk;

namespace {

// Synthetic host on an ephemeral loopback port, running on its own thread.
class TestHost {
public:
    TestHost() {
        dir_ = std::filesystem::temp_directory_path() /
               ("peerdesk-client-test-" + std::to_string(getpid()));
        std::filesystem::remove_all(dir_);
        HostServer::Config cfg;
        cfg.port = 0;
        cfg.bind = "127.0.0.1";
        cfg.synthetic = true;
        cfg.inject = false;
        cfg.data_dir = dir_;
        cfg.bootstrap_user = "jordan";
        cfg.bootstrap_password = "peerdesk";
        cfg.fps = 10;
        server_ = std::make_unique<HostServer>(cfg);
        std::string err;
        ok_ = server_->setup(err) && server_->is_listening();
        if (ok_) thread_ = std::thread([this] { server_->run(); });
    }
    ~TestHost() {
        server_->request_stop();
        if (thread_.joinable()) thread_.join();
        std::filesystem::remove_all(dir_);
    }
    bool ok() const { return ok_; }
    quint16 port() const { return server_->port(); }

private:
    std::filesystem::path dir_;
    std::unique_ptr<HostServer> server_;
    std::thread thread_;
    bool ok_ = false;
};

// Mirrors how MainWindow owns the worker: moved to a QThread, driven by queued calls.
struct WorkerRig {
    QThread thread;
    SessionWorker* worker = new SessionWorker;
    WorkerRig() {
        worker->moveToThread(&thread);
        thread.start();
    }
    ~WorkerRig() {
        worker->disconnectSession();
        thread.quit();
        thread.wait(5000);
        delete worker;
    }
    void connectTo(quint16 port, const QString& user, const QString& pass) {
        QMetaObject::invokeMethod(worker, "connectToHost", Qt::QueuedConnection,
                                  Q_ARG(QString, "127.0.0.1"), Q_ARG(quint16, port),
                                  Q_ARG(QString, user), Q_ARG(QString, pass));
    }
};

// Deliver a synthetic mouse event of `type` at widget-local `pos` directly to `w`.
void sendMouse(QWidget* w, QEvent::Type type, QPoint pos, Qt::MouseButton button) {
    QMouseEvent ev(type, QPointF(pos), QPointF(w->mapToGlobal(pos)), button,
                   type == QEvent::MouseButtonRelease ? Qt::NoButton : Qt::MouseButtons(button),
                   Qt::NoModifier);
    QCoreApplication::sendEvent(w, &ev);
}

// First descendant of `root` of type T whose text() equals `text`, or null.
template <typename T>
T* findByText(QWidget* root, const QString& text) {
    for (auto* c : root->findChildren<T*>()) {
        if (c->text() == text) return c;
    }
    return nullptr;
}

// First QLineEdit under `root` with the given placeholder text, or null.
QLineEdit* findField(QWidget* root, const QString& placeholder) {
    for (auto* e : root->findChildren<QLineEdit*>()) {
        if (e->placeholderText() == placeholder) return e;
    }
    return nullptr;
}

}  // namespace

class ClientTest : public QObject {
    Q_OBJECT

private slots:
    // --- keymap -------------------------------------------------------------
    void keymap_special_keys() {
        QCOMPARE(qt_to_xkeysym(Qt::Key_Return, ""), 0xff0du);
        QCOMPARE(qt_to_xkeysym(Qt::Key_Enter, ""), 0xff0du);
        QCOMPARE(qt_to_xkeysym(Qt::Key_Backspace, ""), 0xff08u);
        QCOMPARE(qt_to_xkeysym(Qt::Key_Tab, ""), 0xff09u);
        QCOMPARE(qt_to_xkeysym(Qt::Key_Escape, ""), 0xff1bu);
        QCOMPARE(qt_to_xkeysym(Qt::Key_Left, ""), 0xff51u);
        QCOMPARE(qt_to_xkeysym(Qt::Key_Up, ""), 0xff52u);
        QCOMPARE(qt_to_xkeysym(Qt::Key_Right, ""), 0xff53u);
        QCOMPARE(qt_to_xkeysym(Qt::Key_Down, ""), 0xff54u);
        QCOMPARE(qt_to_xkeysym(Qt::Key_Delete, ""), 0xffffu);
        QCOMPARE(qt_to_xkeysym(Qt::Key_Shift, ""), 0xffe1u);
        QCOMPARE(qt_to_xkeysym(Qt::Key_Control, ""), 0xffe3u);
        QCOMPARE(qt_to_xkeysym(Qt::Key_Alt, ""), 0xffe9u);
        QCOMPARE(qt_to_xkeysym(Qt::Key_Meta, ""), 0xffebu);
        QCOMPARE(qt_to_xkeysym(Qt::Key_Space, " "), 0x20u);
    }

    void keymap_printable_text() {
        QCOMPARE(qt_to_xkeysym(Qt::Key_A, "a"), static_cast<uint32_t>('a'));
        QCOMPARE(qt_to_xkeysym(Qt::Key_A, "A"), static_cast<uint32_t>('A'));
        QCOMPARE(qt_to_xkeysym(Qt::Key_1, "1"), static_cast<uint32_t>('1'));
        QCOMPARE(qt_to_xkeysym(Qt::Key_Exclam, "!"), static_cast<uint32_t>('!'));
        QCOMPARE(qt_to_xkeysym(0, QString(QChar(0xe9))), 0xe9u);  // é (Latin-1)
    }

    void keymap_letter_fallback_without_text() {
        // e.g. Ctrl+C delivers a control char as text; fall back to the key code.
        QCOMPARE(qt_to_xkeysym(Qt::Key_C, QString(QChar(0x03))), static_cast<uint32_t>('c'));
        QCOMPARE(qt_to_xkeysym(Qt::Key_Z, ""), static_cast<uint32_t>('z'));
    }

    void keymap_unknown_is_zero() {
        QCOMPARE(qt_to_xkeysym(Qt::Key_F13, ""), 0u);
        QCOMPARE(qt_to_xkeysym(0, QString(QChar(0x4e2d))), 0u);  // outside Latin-1
    }

    // --- VideoSurface -------------------------------------------------------
    void surface_ignores_mouse_before_host_size() {
        VideoSurface s;
        s.resize(800, 450);
        QSignalSpy spy(&s, &VideoSurface::mappedMouse);
        sendMouse(&s, QEvent::MouseMove, {100, 100}, Qt::NoButton);
        QCOMPARE(spy.count(), 0);
    }

    void surface_maps_mouse_to_host_pixels() {
        VideoSurface s;
        s.resize(800, 450);
        s.setHostSize(1600, 900);
        QSignalSpy spy(&s, &VideoSurface::mappedMouse);

        sendMouse(&s, QEvent::MouseMove, {400, 225}, Qt::NoButton);
        sendMouse(&s, QEvent::MouseButtonPress, {0, 0}, Qt::LeftButton);
        sendMouse(&s, QEvent::MouseButtonRelease, {10, 20}, Qt::RightButton);
        sendMouse(&s, QEvent::MouseButtonPress, {10, 20}, Qt::MiddleButton);
        QCOMPARE(spy.count(), 4);

        auto check = [&](int i, MouseAction a, int button, int x, int y) {
            const auto args = spy.at(i);
            QCOMPARE(args.at(0).value<quint8>(), static_cast<quint8>(a));
            QCOMPARE(args.at(1).value<quint8>(), static_cast<quint8>(button));
            QCOMPARE(args.at(2).value<quint16>(), static_cast<quint16>(x));
            QCOMPARE(args.at(3).value<quint16>(), static_cast<quint16>(y));
        };
        check(0, MouseAction::Move, 0, 800, 450);
        check(1, MouseAction::Down, 1, 0, 0);
        check(2, MouseAction::Up, 3, 20, 40);
        check(3, MouseAction::Down, 2, 20, 40);
    }

    void surface_drops_clicks_in_letterbox_bars() {
        VideoSurface s;
        s.resize(1000, 450);  // wider than 16:9 -> 100px bars left and right
        s.setHostSize(1600, 900);
        QSignalSpy spy(&s, &VideoSurface::mappedMouse);
        sendMouse(&s, QEvent::MouseMove, {50, 200}, Qt::NoButton);
        sendMouse(&s, QEvent::MouseMove, {950, 200}, Qt::NoButton);
        QCOMPARE(spy.count(), 0);
        sendMouse(&s, QEvent::MouseMove, {100, 0}, Qt::NoButton);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(2).value<quint16>(), 0);
    }

    void surface_keys_emit_and_skip_autorepeat() {
        VideoSurface s;
        QSignalSpy spy(&s, &VideoSurface::mappedKey);
        QKeyEvent press(QEvent::KeyPress, Qt::Key_A, Qt::NoModifier, "a");
        QKeyEvent repeat(QEvent::KeyPress, Qt::Key_A, Qt::NoModifier, "a", true);
        QKeyEvent release(QEvent::KeyRelease, Qt::Key_A, Qt::NoModifier, "a");
        QKeyEvent unknown(QEvent::KeyPress, Qt::Key_F13, Qt::NoModifier);
        QCoreApplication::sendEvent(&s, &press);
        QCoreApplication::sendEvent(&s, &repeat);
        QCoreApplication::sendEvent(&s, &release);
        QCoreApplication::sendEvent(&s, &unknown);
        QCOMPARE(spy.count(), 2);
        QCOMPARE(spy.at(0).at(0).value<quint8>(), 1);
        QCOMPARE(spy.at(0).at(1).value<quint32>(), static_cast<quint32>('a'));
        QCOMPARE(spy.at(1).at(0).value<quint8>(), 0);
    }

    // --- SessionWorker against a live host -----------------------------------
    void worker_unreachable_host() {
        WorkerRig rig;
        QSignalSpy unreachable(rig.worker, &SessionWorker::unreachable);
        rig.connectTo(1, "jordan", "peerdesk");  // nothing listens on port 1
        QVERIFY(unreachable.wait(10000));
    }

    void worker_bad_password() {
        TestHost host;
        QVERIFY(host.ok());
        WorkerRig rig;
        QSignalSpy failed(rig.worker, &SessionWorker::authFailed);
        QSignalSpy ready(rig.worker, &SessionWorker::sessionReady);
        rig.connectTo(host.port(), "jordan", "wrong");
        QVERIFY(failed.wait(20000));
        QCOMPARE(ready.count(), 0);
    }

    void worker_session_frames_input_disconnect_reconnect() {
        TestHost host;
        QVERIFY(host.ok());
        WorkerRig rig;
        QSignalSpy ready(rig.worker, &SessionWorker::sessionReady);
        QSignalSpy frames(rig.worker, &SessionWorker::frameArrived);
        QSignalSpy ended(rig.worker, &SessionWorker::sessionEnded);

        rig.connectTo(host.port(), "jordan", "peerdesk");
        QVERIFY(ready.wait(20000));
        QVERIFY(ready.at(0).at(0).toInt() >= 320);
        QVERIFY(ready.at(0).at(1).toInt() >= 200);

        QVERIFY(frames.wait(5000));
        const auto img = frames.at(0).at(0).value<QImage>();
        QVERIFY(!img.isNull());
        QCOMPARE(img.width(), ready.at(0).at(0).toInt());

        // Input is queued from the UI thread while pump() runs; session must stay up.
        rig.worker->enqueueMouse(static_cast<quint8>(MouseAction::Move), 0, 10, 10, 0);
        rig.worker->enqueueKey(1, 'a');
        rig.worker->enqueueKey(0, 'a');
        const int before = frames.count();
        QTRY_VERIFY_WITH_TIMEOUT(frames.count() > before + 1, 5000);
        QCOMPARE(ended.count(), 0);

        rig.worker->disconnectSession();
        QVERIFY(ended.wait(5000));
        QCOMPARE(ended.at(0).at(0).toString(), QString("Disconnected"));

        // Same worker, same host process: reconnect works.
        ready.clear();
        QTest::qWait(300);
        rig.connectTo(host.port(), "jordan", "peerdesk");
        QVERIFY(ready.wait(20000));
    }

    void worker_second_viewer_rejected_as_busy() {
        TestHost host;
        QVERIFY(host.ok());
        WorkerRig first;
        QSignalSpy ready(first.worker, &SessionWorker::sessionReady);
        first.connectTo(host.port(), "jordan", "peerdesk");
        QVERIFY(ready.wait(20000));

        WorkerRig second;
        QSignalSpy failed(second.worker, &SessionWorker::authFailed);
        second.connectTo(host.port(), "jordan", "peerdesk");
        QVERIFY(failed.wait(20000));
    }

    // --- MainWindow ---------------------------------------------------------
    void window_rejects_invalid_port() {
        MainWindow w;
        auto* port = findField(&w, "Port");
        auto* btn = findByText<QPushButton>(&w, "Connect");
        QVERIFY(port && btn);
        port->setText("not-a-port");
        btn->click();
        QVERIFY(btn->isEnabled());
        QVERIFY(findByText<QLabel>(&w, "Enter IP and a valid port.") != nullptr);
        QCOMPARE(w.findChild<QStackedWidget*>()->currentIndex(), 0);
    }

    void window_connects_and_disconnects() {
        TestHost host;
        QVERIFY(host.ok());
        MainWindow w;
        w.show();
        auto* stack = w.findChild<QStackedWidget*>();
        auto* btn = findByText<QPushButton>(&w, "Connect");
        findField(&w, "Port")->setText(QString::number(host.port()));
        btn->click();
        QVERIFY(!btn->isEnabled());
        QTRY_COMPARE_WITH_TIMEOUT(stack->currentIndex(), 1, 20000);

        findByText<QPushButton>(&w, "Disconnect")->click();
        QTRY_COMPARE_WITH_TIMEOUT(stack->currentIndex(), 0, 5000);
        QVERIFY(btn->isEnabled());
        QVERIFY(findByText<QLabel>(&w, "Disconnected") != nullptr);
    }

    void window_shows_auth_failure() {
        TestHost host;
        QVERIFY(host.ok());
        MainWindow w;
        auto* btn = findByText<QPushButton>(&w, "Connect");
        findField(&w, "Port")->setText(QString::number(host.port()));
        findField(&w, "Password")->setText("wrong");
        btn->click();
        QTRY_VERIFY_WITH_TIMEOUT(btn->isEnabled(), 20000);
        QCOMPARE(w.findChild<QStackedWidget*>()->currentIndex(), 0);
    }
};

int main(int argc, char** argv) {
    std::signal(SIGPIPE, SIG_IGN);
    QApplication app(argc, argv);
    ClientTest t;
    return QTest::qExec(&t, argc, argv);
}

#include "client_test.moc"
