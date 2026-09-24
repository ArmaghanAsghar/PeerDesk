#include "MainWindow.hpp"

#include "keymap.hpp"

#include <QtCore/QThread>
#include <QtGui/QMouseEvent>
#include <QtGui/QPainter>
#include <QtGui/QWheelEvent>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QVBoxLayout>

namespace peerdesk {

VideoSurface::VideoSurface(QWidget* parent) : QWidget(parent) {
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setMinimumSize(480, 270);
}

void VideoSurface::setFrame(const QImage& img) {
    frame_ = img;
    update();
}

void VideoSurface::setHostSize(int w, int h) {
    host_w_ = w;
    host_h_ = h;
}

void VideoSurface::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.fillRect(rect(), QColor(10, 12, 18));
    if (frame_.isNull() || host_w_ <= 0 || host_h_ <= 0) {
        p.setPen(QColor(180, 190, 210));
        p.drawText(rect(), Qt::AlignCenter, "Waiting for first frame…");
        return;
    }
    const auto box = fit_letterbox(width(), height(), host_w_, host_h_);
    const QRect dest(box.dest_x, box.dest_y, box.dest_w, box.dest_h);
    p.drawImage(dest, frame_);
}

void VideoSurface::emit_mouse(MouseAction action, const QPoint& pos, quint8 button, qint16 wheel) {
    if (host_w_ <= 0 || host_h_ <= 0) return;
    const auto box = fit_letterbox(width(), height(), host_w_, host_h_);
    int hx = 0, hy = 0;
    if (!map_letterbox_point(pos.x(), pos.y(), box, host_w_, host_h_, hx, hy)) return;
    emit mappedMouse(static_cast<quint8>(action), button, static_cast<quint16>(hx),
                     static_cast<quint16>(hy), wheel);
}

void VideoSurface::mouseMoveEvent(QMouseEvent* event) {
    emit_mouse(MouseAction::Move, event->pos(), 0, 0);
}

void VideoSurface::mousePressEvent(QMouseEvent* event) {
    setFocus();
    quint8 b = 1;
    if (event->button() == Qt::MiddleButton) b = 2;
    if (event->button() == Qt::RightButton) b = 3;
    emit_mouse(MouseAction::Down, event->pos(), b, 0);
}

void VideoSurface::mouseReleaseEvent(QMouseEvent* event) {
    quint8 b = 1;
    if (event->button() == Qt::MiddleButton) b = 2;
    if (event->button() == Qt::RightButton) b = 3;
    emit_mouse(MouseAction::Up, event->pos(), b, 0);
}

void VideoSurface::wheelEvent(QWheelEvent* event) {
    const auto d = static_cast<qint16>(event->angleDelta().y() >= 0 ? 1 : -1);
    emit_mouse(MouseAction::Wheel, event->position().toPoint(), 0, d);
}

void VideoSurface::keyPressEvent(QKeyEvent* event) {
    if (event->isAutoRepeat()) return;
    const auto ks = qt_to_xkeysym(event->key(), event->text());
    if (ks) emit mappedKey(1, ks);
}

void VideoSurface::keyReleaseEvent(QKeyEvent* event) {
    if (event->isAutoRepeat()) return;
    const auto ks = qt_to_xkeysym(event->key(), event->text());
    if (ks) emit mappedKey(0, ks);
}

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("PeerDesk — connect to an Ubuntu host");
    resize(960, 640);

    auto pal = palette();
    pal.setColor(QPalette::Window, QColor(18, 22, 32));
    pal.setColor(QPalette::WindowText, QColor(230, 234, 242));
    pal.setColor(QPalette::Base, QColor(28, 34, 48));
    pal.setColor(QPalette::Text, QColor(230, 234, 242));
    pal.setColor(QPalette::Button, QColor(46, 92, 196));
    pal.setColor(QPalette::ButtonText, Qt::white);
    pal.setColor(QPalette::PlaceholderText, QColor(140, 150, 168));
    setPalette(pal);

    stack_ = new QStackedWidget(this);
    setCentralWidget(stack_);

    form_page_ = new QWidget;
    auto* form_layout = new QVBoxLayout(form_page_);
    form_layout->setContentsMargins(48, 36, 48, 36);
    form_layout->setSpacing(16);

    auto* title = new QLabel("PeerDesk");
    QFont tf = title->font();
    tf.setPointSize(28);
    tf.setBold(true);
    title->setFont(tf);

    auto* subtitle = new QLabel("LAN / VPN remote desktop — view, mouse, keyboard, reconnect.");
    subtitle->setStyleSheet("color: #9aa6bc;");

    auto* note = new QLabel("TLS is on (self-signed demo cert). Password is never sent in plaintext.");
    note->setStyleSheet("color: #7f93b0;");
    note->setWordWrap(true);

    ip_ = new QLineEdit;
    ip_->setPlaceholderText("Host IP");
    ip_->setText("127.0.0.1");
    port_ = new QLineEdit;
    port_->setPlaceholderText("Port");
    port_->setText(QString::number(kDefaultPort));
    user_ = new QLineEdit;
    user_->setPlaceholderText("Username");
    user_->setText("jordan");
    pass_ = new QLineEdit;
    pass_->setPlaceholderText("Password");
    pass_->setEchoMode(QLineEdit::Password);
    pass_->setText("peerdesk");

    auto* fields = new QFormLayout;
    fields->addRow("IP address", ip_);
    fields->addRow("Port", port_);
    fields->addRow("Username", user_);
    fields->addRow("Password", pass_);

    connect_btn_ = new QPushButton("Connect");
    connect_btn_->setDefault(true);
    connect_btn_->setMinimumHeight(36);
    connect(connect_btn_, &QPushButton::clicked, this, &MainWindow::onConnect);

    form_status_ = new QLabel;
    form_status_->setWordWrap(true);
    form_status_->setStyleSheet("color: #c5d0e0;");

    form_layout->addWidget(title);
    form_layout->addWidget(subtitle);
    form_layout->addWidget(note);
    form_layout->addSpacing(8);
    form_layout->addLayout(fields);
    form_layout->addWidget(connect_btn_);
    form_layout->addWidget(form_status_);
    form_layout->addStretch();

    auto* session = new QWidget;
    auto* sl = new QVBoxLayout(session);
    sl->setContentsMargins(8, 8, 8, 8);
    video_ = new VideoSurface;
    session_status_ = new QLabel;
    session_status_->setStyleSheet("color: #9aa6bc;");
    auto* bar = new QHBoxLayout;
    auto* disc = new QPushButton("Disconnect");
    connect(disc, &QPushButton::clicked, this, &MainWindow::onDisconnect);
    bar->addWidget(session_status_, 1);
    bar->addWidget(disc);
    sl->addWidget(video_, 1);
    sl->addLayout(bar);

    stack_->addWidget(form_page_);
    stack_->addWidget(session);

    thread_ = new QThread(this);
    worker_ = new SessionWorker;
    worker_->moveToThread(thread_);
    thread_->start();

    connect(worker_, &SessionWorker::statusChanged, this, [this](const QString& s) {
        form_status_->setText(s);
        form_status_->setStyleSheet("color: #c5d0e0;");
    });
    connect(worker_, &SessionWorker::authFailed, this, [this](const QString& s) { showForm(s, true); });
    connect(worker_, &SessionWorker::unreachable, this, [this](const QString& s) { showForm(s, true); });
    connect(worker_, &SessionWorker::sessionReady, this, &MainWindow::showSession);
    connect(worker_, &SessionWorker::frameArrived, video_, &VideoSurface::setFrame);
    connect(worker_, &SessionWorker::sessionEnded, this, [this](const QString& s) { showForm(s, false); });

    connect(video_, &VideoSurface::mappedMouse, worker_, &SessionWorker::enqueueMouse,
            Qt::DirectConnection);
    connect(video_, &VideoSurface::mappedKey, worker_, &SessionWorker::enqueueKey,
            Qt::DirectConnection);
}

MainWindow::~MainWindow() {
    worker_->disconnectSession();
    thread_->quit();
    thread_->wait(3000);
    delete worker_;
}

void MainWindow::onConnect() {
    connect_btn_->setEnabled(false);
    form_status_->setText("Connecting…");
    bool ok = false;
    const auto port = port_->text().toUShort(&ok);
    if (!ok || ip_->text().trimmed().isEmpty()) {
        showForm("Enter IP and a valid port.", true);
        return;
    }
    QMetaObject::invokeMethod(worker_, "connectToHost", Qt::QueuedConnection,
                              Q_ARG(QString, ip_->text().trimmed()), Q_ARG(quint16, port),
                              Q_ARG(QString, user_->text().trimmed()), Q_ARG(QString, pass_->text()));
}

void MainWindow::onDisconnect() {
    worker_->disconnectSession();
}

void MainWindow::showSession(int w, int h) {
    video_->setHostSize(w, h);
    session_status_->setText(QString("Connected — host %1×%2  ·  TLS  ·  input mapped to host pixels")
                                 .arg(w)
                                 .arg(h));
    stack_->setCurrentIndex(1);
    video_->setFocus();
}

void MainWindow::showForm(const QString& message, bool error) {
    connect_btn_->setEnabled(true);
    form_status_->setText(message);
    form_status_->setStyleSheet(error ? "color: #ff8b8b;" : "color: #c5d0e0;");
    stack_->setCurrentIndex(0);
}

}  // namespace peerdesk
