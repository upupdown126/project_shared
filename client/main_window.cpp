#include "main_window.h"
#include "client_theme.h"
#include "video_decoder.h"

#include <QApplication>
#include <QCheckBox>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QMediaContent>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPixmap>
#include <QProcess>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QSplitter>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QVideoWidget>

namespace {
QString clientSettingsPath() {
    QString directory = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if (directory.isEmpty()) directory = QCoreApplication::applicationDirPath();
    QDir().mkpath(directory);
    return QDir(directory).filePath("client.ini");
}
}

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent), decoder_(new VideoDecoder), player_(this),
      publisher_(new QProcess(this)), shuttingDown_(false) {
    setWindowTitle(tr("智能家居监控客户端")); resize(1280, 820);
    QWidget* central = new QWidget(this); central->setObjectName("centralPanel");
    QVBoxLayout* root = new QVBoxLayout(central); root->setContentsMargins(16, 16, 16, 14); root->setSpacing(12);

    QHBoxLayout* server = new QHBoxLayout;
    host_ = new QLineEdit("127.0.0.1"); port_ = new QLineEdit("8000");
    username_ = new QLineEdit; password_ = new QLineEdit; password_->setEchoMode(QLineEdit::Password);
    QPushButton* connectButton = new QPushButton(tr("连接"));
    QPushButton* registerButton = new QPushButton(tr("注册"));
    QPushButton* loginButton = new QPushButton(tr("登录"));
    autoLogin_ = new QCheckBox(tr("自动连接/登录")); autoLogin_->setChecked(true);
    themeButton_ = new QPushButton; themeButton_->setObjectName("themeButton"); themeButton_->setCheckable(true);
    server->addWidget(new QLabel(tr("服务器"))); server->addWidget(host_); server->addWidget(port_);
    server->addWidget(connectButton); server->addWidget(new QLabel(tr("用户")));
    server->addWidget(username_); server->addWidget(password_);
    server->addWidget(registerButton); server->addWidget(loginButton); server->addWidget(autoLogin_);
    server->addWidget(themeButton_); root->addLayout(server);

    QSplitter* splitter = new QSplitter; QWidget* left = new QWidget;
    QVBoxLayout* leftLayout = new QVBoxLayout(left); tree_ = new QTreeWidget;
    tree_->setHeaderLabels(QStringList() << tr("设备/通道") << tr("地址"));
    QPushButton* refresh = new QPushButton(tr("刷新设备"));
    leftLayout->addWidget(tree_); leftLayout->addWidget(refresh);
    QGroupBox* device = new QGroupBox(tr("保存摄像头")); QFormLayout* form = new QFormLayout(device);
    cameraId_ = new QLineEdit; cameraType_ = new QSpinBox; cameraType_->setRange(0, 1);
    channels_ = new QSpinBox; channels_->setRange(1, 128); serial_ = new QLineEdit;
    cameraIp_ = new QLineEdit; rtsp_ = new QLineEdit; rtmp_ = new QLineEdit;
    QPushButton* save = new QPushButton(tr("保存/更新"));
    form->addRow(tr("ID"), cameraId_); form->addRow(tr("类型(0枪机/1球机)"), cameraType_);
    form->addRow(tr("通道数"), channels_); form->addRow(tr("序列号"), serial_);
    form->addRow(tr("IP"), cameraIp_); form->addRow("RTSP", rtsp_); form->addRow("RTMP", rtmp_);
    form->addRow(save); leftLayout->addWidget(device);

    QGroupBox* localPublisher = new QGroupBox(tr("本机摄像头推流"));
    QFormLayout* publisherForm = new QFormLayout(localPublisher);
    webcamDevice_ = new QLineEdit("Integrated Webcam");
    publishUrl_ = new QLineEdit;
    autoPublish_ = new QCheckBox(tr("启动客户端时自动推流")); autoPublish_->setChecked(true);
    publisherStart_ = new QPushButton(tr("启动推流")); publisherStop_ = new QPushButton(tr("停止推流"));
    QHBoxLayout* publisherActions = new QHBoxLayout; publisherActions->addWidget(publisherStart_); publisherActions->addWidget(publisherStop_);
    publisherForm->addRow(tr("设备名称"), webcamDevice_);
    publisherForm->addRow(tr("发布地址"), publishUrl_);
    publisherForm->addRow(autoPublish_);
    publisherForm->addRow(publisherActions);
    leftLayout->addWidget(localPublisher); splitter->addWidget(left);

    QWidget* right = new QWidget; QVBoxLayout* rightLayout = new QVBoxLayout(right);
    videoStack_ = new QStackedWidget; liveView_ = new QLabel(tr("等待实时视频"));
    liveView_->setAlignment(Qt::AlignCenter); liveView_->setMinimumSize(640, 360);
    liveView_->setStyleSheet("background:#111;color:#ddd;"); playbackView_ = new QVideoWidget;
    videoStack_->addWidget(liveView_); videoStack_->addWidget(playbackView_); rightLayout->addWidget(videoStack_, 1);
    QHBoxLayout* actions = new QHBoxLayout;
    QPushButton* live = new QPushButton(tr("播放实时")); QPushButton* stop = new QPushButton(tr("停止"));
    QPushButton* record = new QPushButton(tr("开始录像")); QPushButton* stopRecord = new QPushButton(tr("停止录像"));
    live->setCheckable(true); record->setCheckable(true);
    actions->addWidget(live); actions->addWidget(stop); actions->addWidget(record); actions->addWidget(stopRecord);
    rightLayout->addLayout(actions);

    QGroupBox* ptz = new QGroupBox(tr("云台控制")); QGridLayout* ptzGrid = new QGridLayout(ptz);
    const QString commands[9] = {"left_up", "up", "right_up", "left", "stop", "right",
                                 "left_down", "down", "right_down"};
    for (int i = 0; i < 9; ++i) { const QString command = commands[i];
        QPushButton* button = new QPushButton(command);
        ptzGrid->addWidget(button, i / 3, i % 3); connect(button, &QPushButton::clicked, this,
            [this, command] { sendPtz(command); }); }
    QPushButton* zoomIn = new QPushButton("zoom_in"), *zoomOut = new QPushButton("zoom_out");
    speed_ = new QSpinBox; speed_->setRange(0, 100); speed_->setValue(50);
    ptzGrid->addWidget(zoomIn, 0, 3); ptzGrid->addWidget(zoomOut, 1, 3); ptzGrid->addWidget(speed_, 2, 3);
    connect(zoomIn, &QPushButton::clicked, this, [this] { sendPtz("zoom_in"); });
    connect(zoomOut, &QPushButton::clicked, this, [this] { sendPtz("zoom_out"); });
    rightLayout->addWidget(ptz);

    QHBoxLayout* playback = new QHBoxLayout; beginMs_ = new QLineEdit; endMs_ = new QLineEdit;
    const qint64 now = QDateTime::currentMSecsSinceEpoch(); beginMs_->setText(QString::number(now - 3600000));
    endMs_->setText(QString::number(now)); QPushButton* playbackButton = new QPushButton(tr("HLS 回放"));
    playback->addWidget(new QLabel(tr("开始ms"))); playback->addWidget(beginMs_);
    playback->addWidget(new QLabel(tr("结束ms"))); playback->addWidget(endMs_);
    playback->addWidget(playbackButton); rightLayout->addLayout(playback); splitter->addWidget(right);
    splitter->setStretchFactor(1, 1); root->addWidget(splitter, 1);
    logView_ = new QPlainTextEdit; logView_->setReadOnly(true); logView_->setMaximumBlockCount(500);
    logView_->setMaximumHeight(130); root->addWidget(logView_); setCentralWidget(central);
    loadSettings();
    applyTheme(themeButton_->isChecked());
    log(tr("客户端配置：%1").arg(QDir::toNativeSeparators(clientSettingsPath())));

    decoder_->moveToThread(&decoderThread_); connect(&decoderThread_, &QThread::finished, decoder_, &QObject::deleteLater);
    connect(&protocol_, &ProtocolClient::videoMetadata, decoder_, &VideoDecoder::configure);
    connect(&protocol_, &ProtocolClient::compressedVideoPacket, decoder_, &VideoDecoder::decode);
    connect(decoder_, &VideoDecoder::frameReady, this, &MainWindow::showFrame);
    connect(decoder_, &VideoDecoder::decoderError, this, &MainWindow::log); decoderThread_.start();
    player_.setVideoOutput(playbackView_);

    connect(connectButton, &QPushButton::clicked, this, [this] {
        saveSettings();
        protocol_.connectToServer(host_->text(), quint16(port_->text().toUShort())); });
    connect(registerButton, &QPushButton::clicked, this, [this] {
        saveSettings();
        protocol_.registerUser(username_->text(), password_->text()); });
    connect(loginButton, &QPushButton::clicked, this, [this] {
        saveSettings();
        protocol_.login(username_->text(), password_->text()); });
    connect(refresh, &QPushButton::clicked, &protocol_, &ProtocolClient::requestCameras);
    connect(save, &QPushButton::clicked, this, &MainWindow::saveCamera);
    connect(live, &QPushButton::clicked, this, [this, live] { playLive(); live->setChecked(true); });
    connect(stop, &QPushButton::clicked, this, [this, live] { stopLive(); live->setChecked(false); });
    connect(record, &QPushButton::clicked, this, [this, record] { startRecording(); record->setChecked(true); });
    connect(stopRecord, &QPushButton::clicked, this, [this, record] { stopRecording(); record->setChecked(false); });
    connect(playbackButton, &QPushButton::clicked, this, &MainWindow::playRecording);
    connect(themeButton_, &QPushButton::toggled, this, &MainWindow::applyTheme);
    connect(publisherStart_, &QPushButton::clicked, this, &MainWindow::startWebcamPublisher);
    connect(publisherStop_, &QPushButton::clicked, this, &MainWindow::stopWebcamPublisher);
    connect(&protocol_, &ProtocolClient::camerasReceived, this, &MainWindow::showCameras);
    connect(&protocol_, &ProtocolClient::message, this, &MainWindow::log);
    connect(&protocol_, &ProtocolClient::protocolError, this, &MainWindow::log);
    connect(&protocol_, &ProtocolClient::connected, this, [this] {
        log(tr("服务器已连接"));
        if (autoLogin_->isChecked() && !username_->text().isEmpty() && !password_->text().isEmpty())
            protocol_.login(username_->text(), password_->text());
    });
    connect(&protocol_, &ProtocolClient::disconnected, this, [this] { log(tr("服务器已断开")); });
    connect(&protocol_, &ProtocolClient::playbackUrlReceived, this, [this](const QUrl& url) {
        videoStack_->setCurrentWidget(playbackView_); player_.setMedia(QMediaContent(url)); player_.play();
        log(tr("开始回放 %1").arg(url.toString())); });

    connect(publisher_, &QProcess::started, this, [this] {
        publisherStart_->setEnabled(false); publisherStop_->setEnabled(true);
        log(tr("本机摄像头推流已启动"));
    });
    connect(publisher_, &QProcess::readyReadStandardOutput, this, [this] {
        const QString output = QString::fromLocal8Bit(publisher_->readAllStandardOutput()).trimmed();
        if (!output.isEmpty()) log(tr("FFmpeg: %1").arg(output));
    });
    connect(publisher_, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
        [this](int code, QProcess::ExitStatus) {
            publisherStart_->setEnabled(true); publisherStop_->setEnabled(false);
            if (!shuttingDown_) log(tr("本机摄像头推流已结束，退出码 %1").arg(code));
        });
    connect(publisher_, &QProcess::errorOccurred, this, [this](QProcess::ProcessError) {
        if (!shuttingDown_) log(tr("无法启动摄像头推流：%1").arg(publisher_->errorString()));
    });
    publisherStop_->setEnabled(false);

    QTimer::singleShot(500, this, [this] {
        if (autoPublish_->isChecked()) startWebcamPublisher();
        if (autoLogin_->isChecked() && !host_->text().isEmpty())
            protocol_.connectToServer(host_->text(), quint16(port_->text().toUShort()));
    });
}

MainWindow::~MainWindow() {
    saveSettings();
    shuttingDown_ = true; stopWebcamPublisher();
    protocol_.stopStream(); decoderThread_.quit(); decoderThread_.wait();
}

void MainWindow::loadSettings() {
    QSettings settings(clientSettingsPath(), QSettings::IniFormat);
    host_->setText(settings.value("connection/host", host_->text()).toString());
    port_->setText(settings.value("connection/port", port_->text()).toString());
    username_->setText(settings.value("authentication/username").toString());
    password_->setText(settings.value("authentication/password").toString());
    autoLogin_->setChecked(settings.value("connection/autoLogin", true).toBool());
    cameraId_->setText(settings.value("camera/id", "1").toString());
    cameraType_->setValue(settings.value("camera/type", 0).toInt());
    channels_->setValue(settings.value("camera/channels", 1).toInt());
    serial_->setText(settings.value("camera/serial", "PC-WEBCAM").toString());
    cameraIp_->setText(settings.value("camera/ip", host_->text()).toString());
    rtsp_->setText(settings.value("camera/rtsp", "rtsp://mediamtx:8554/webcam").toString());
    rtmp_->setText(settings.value("camera/rtmp").toString());
    speed_->setValue(settings.value("ptz/speed", speed_->value()).toInt());
    beginMs_->setText(settings.value("playback/beginMs", beginMs_->text()).toString());
    endMs_->setText(settings.value("playback/endMs", endMs_->text()).toString());
    webcamDevice_->setText(settings.value("publisher/device", webcamDevice_->text()).toString());
    publishUrl_->setText(settings.value("publisher/url",
        QString("rtsp://%1:8554/webcam").arg(host_->text())).toString());
    autoPublish_->setChecked(settings.value("publisher/autoStart", true).toBool());
    themeButton_->setChecked(settings.value("appearance/darkTheme", false).toBool());
    const QByteArray geometry = settings.value("appearance/windowGeometry").toByteArray();
    if (!geometry.isEmpty()) restoreGeometry(geometry);
}

void MainWindow::saveSettings() const {
    QSettings settings(clientSettingsPath(), QSettings::IniFormat);
    settings.setValue("connection/host", host_->text());
    settings.setValue("connection/port", port_->text());
    settings.setValue("authentication/username", username_->text());
    settings.setValue("authentication/password", password_->text());
    settings.setValue("connection/autoLogin", autoLogin_->isChecked());
    settings.setValue("camera/id", cameraId_->text());
    settings.setValue("camera/type", cameraType_->value());
    settings.setValue("camera/channels", channels_->value());
    settings.setValue("camera/serial", serial_->text());
    settings.setValue("camera/ip", cameraIp_->text());
    settings.setValue("camera/rtsp", rtsp_->text());
    settings.setValue("camera/rtmp", rtmp_->text());
    settings.setValue("ptz/speed", speed_->value());
    settings.setValue("playback/beginMs", beginMs_->text());
    settings.setValue("playback/endMs", endMs_->text());
    settings.setValue("publisher/device", webcamDevice_->text());
    settings.setValue("publisher/url", publishUrl_->text());
    settings.setValue("publisher/autoStart", autoPublish_->isChecked());
    settings.setValue("appearance/darkTheme", themeButton_->isChecked());
    settings.setValue("appearance/windowGeometry", saveGeometry());
    QTreeWidgetItem* item = tree_->currentItem();
    if (item && item->parent()) {
        settings.setValue("selection/cameraId", item->data(0, Qt::UserRole).toUInt());
        settings.setValue("selection/channel", item->data(0, Qt::UserRole + 1).toUInt());
    }
    settings.sync();
}

void MainWindow::applyTheme(bool dark) {
    qApp->setStyleSheet(dark ? client_theme::dark() : client_theme::light());
    themeButton_->setText(dark ? tr("深色主题") : tr("浅色主题"));
    liveView_->setStyleSheet(dark
        ? QStringLiteral("background:#070B12;color:#8FA0B8;border:1px solid #303C50;border-radius:10px;")
        : QStringLiteral("background:#101620;color:#C9D3E1;border:1px solid #D8E0EC;border-radius:10px;"));
    QSettings settings(clientSettingsPath(), QSettings::IniFormat);
    settings.setValue("appearance/darkTheme", dark);
}

QString MainWindow::ffmpegExecutable() const {
    const QString bundled = QDir(QCoreApplication::applicationDirPath()).filePath("ffmpeg.exe");
    if (QFileInfo::exists(bundled)) return bundled;
    const QString workspace = QDir(QCoreApplication::applicationDirPath()).absoluteFilePath(
        "../../../ffmpeg_sdk_win64/bin/ffmpeg.exe");
    if (QFileInfo::exists(workspace)) return workspace;
    return QStandardPaths::findExecutable("ffmpeg.exe");
}

void MainWindow::startWebcamPublisher() {
    if (publisher_->state() != QProcess::NotRunning) { log(tr("本机摄像头推流已经在运行")); return; }
    const QString executable = ffmpegExecutable();
    if (executable.isEmpty()) { log(tr("未找到 ffmpeg.exe，请使用完整的客户端版本目录")); return; }
    if (webcamDevice_->text().trimmed().isEmpty() || publishUrl_->text().trimmed().isEmpty()) {
        log(tr("摄像头设备名称或发布地址为空")); return;
    }
    saveSettings();
    QStringList arguments;
    arguments << "-hide_banner" << "-loglevel" << "warning" << "-nostats"
              << "-f" << "dshow" << "-rtbufsize" << "512M"
              << "-i" << ("video=" + webcamDevice_->text().trimmed()) << "-an"
              << "-c:v" << "libx264" << "-preset" << "ultrafast" << "-tune" << "zerolatency"
              << "-pix_fmt" << "yuv420p" << "-g" << "50"
              << "-f" << "rtsp" << "-rtsp_transport" << "tcp" << publishUrl_->text().trimmed();
    publisher_->setProcessChannelMode(QProcess::MergedChannels);
    publisher_->setProgram(executable); publisher_->setArguments(arguments); publisher_->start();
}

void MainWindow::stopWebcamPublisher() {
    if (publisher_->state() == QProcess::NotRunning) return;
    publisher_->write("q\n"); publisher_->waitForBytesWritten(250);
    if (!publisher_->waitForFinished(2000)) {
        publisher_->terminate();
        if (!publisher_->waitForFinished(1000)) { publisher_->kill(); publisher_->waitForFinished(1000); }
    }
    if (!shuttingDown_) log(tr("本机摄像头推流已停止，摄像头已释放"));
}

void MainWindow::showCameras(const QVector<CameraDeviceDto>& cameras) {
    cameras_ = cameras; tree_->clear();
    QSettings settings(clientSettingsPath(), QSettings::IniFormat);
    const quint32 savedCamera = settings.value("selection/cameraId", 0).toUInt();
    const quint32 savedChannel = settings.value("selection/channel", 0).toUInt();
    QTreeWidgetItem* fallback = 0;
    for (int i = 0; i < cameras.size(); ++i) {
        QTreeWidgetItem* root = new QTreeWidgetItem(tree_, QStringList() <<
            QString("%1 [%2]").arg(cameras[i].serialNumber).arg(cameras[i].id) << cameras[i].ip);
        root->setData(0, Qt::UserRole, cameras[i].id);
        for (quint32 channel = 0; channel < cameras[i].channels; ++channel) {
            QTreeWidgetItem* item = new QTreeWidgetItem(root, QStringList() <<
                tr("通道 %1").arg(channel) << selectedStream(cameras[i]));
            item->setData(0, Qt::UserRole, cameras[i].id); item->setData(0, Qt::UserRole + 1, channel);
            if (!fallback) fallback = item;
            if (cameras[i].id == savedCamera && channel == savedChannel) tree_->setCurrentItem(item);
        }
        root->setExpanded(true);
    }
    if (!tree_->currentItem() && fallback) tree_->setCurrentItem(fallback);
    log(tr("已加载 %1 台摄像头").arg(cameras.size()));
}
bool MainWindow::selection(CameraDeviceDto& camera, quint32& channel) const {
    QTreeWidgetItem* item = tree_->currentItem();
    if (!item || !item->parent()) return false;
    const quint32 id = item->data(0, Qt::UserRole).toUInt(); channel = item->data(0, Qt::UserRole + 1).toUInt();
    for (int i = 0; i < cameras_.size(); ++i) if (cameras_[i].id == id) { camera = cameras_[i]; return true; }
    return false;
}
QString MainWindow::selectedStream(const CameraDeviceDto& camera) const {
    return !camera.rtspUrl.isEmpty() ? camera.rtspUrl : camera.rtmpUrl;
}
void MainWindow::showFrame(const QImage& image) {
    videoStack_->setCurrentWidget(liveView_);
    liveView_->setPixmap(QPixmap::fromImage(image).scaled(liveView_->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
}
void MainWindow::playLive() { CameraDeviceDto camera; quint32 channel;
    if (!selection(camera, channel)) { log(tr("请先选择一个通道")); return; }
    player_.stop(); videoStack_->setCurrentWidget(liveView_); protocol_.startStream(camera.id, selectedStream(camera)); }
void MainWindow::stopLive() { protocol_.stopStream(); QMetaObject::invokeMethod(decoder_, "reset", Qt::QueuedConnection); }
void MainWindow::saveCamera() { CameraDeviceDto camera; camera.id = cameraId_->text().toUInt();
    camera.type = cameraType_->value(); camera.channels = channels_->value(); camera.serialNumber = serial_->text();
    camera.ip = cameraIp_->text(); camera.rtspUrl = rtsp_->text(); camera.rtmpUrl = rtmp_->text();
    saveSettings(); protocol_.saveCamera(camera); }
void MainWindow::startRecording() { CameraDeviceDto camera; quint32 channel;
    if (!selection(camera, channel)) { log(tr("请先选择一个通道")); return; }
    protocol_.startRecording(camera.id, channel, selectedStream(camera)); }
void MainWindow::stopRecording() { CameraDeviceDto camera; quint32 channel;
    if (!selection(camera, channel)) { log(tr("请先选择一个通道")); return; }
    protocol_.stopRecording(camera.id, channel); }
void MainWindow::playRecording() { CameraDeviceDto camera; quint32 channel;
    if (!selection(camera, channel)) { log(tr("请先选择一个通道")); return; }
    saveSettings(); protocol_.requestPlayback(camera.id, channel, beginMs_->text().toLongLong(), endMs_->text().toLongLong()); }
void MainWindow::sendPtz(const QString& command) { CameraDeviceDto camera; quint32 channel;
    if (!selection(camera, channel)) { log(tr("请先选择一个通道")); return; } protocol_.ptz(channel, command, speed_->value()); }
void MainWindow::log(const QString& text) {
    logView_->appendPlainText(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss ") + text);
}
