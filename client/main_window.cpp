#include "main_window.h"
#include "client_theme.h"
#include "video_decoder.h"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QDialog>
#include <QDockWidget>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMediaContent>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPixmap>
#include <QProcess>
#include <QPushButton>
#include <QResizeEvent>
#include <QSettings>
#include <QSlider>
#include <QSpinBox>
#include <QSplitter>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QStatusBar>
#include <QStyle>
#include <QTabWidget>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
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

QFrame* makeCard(QWidget* parent = 0) {
    QFrame* card = new QFrame(parent);
    card->setProperty("card", true);
    return card;
}

QLabel* makeLabel(const QString& text, const char* role, QWidget* parent = 0) {
    QLabel* label = new QLabel(text, parent);
    label->setProperty("role", role);
    return label;
}

void setStatus(QLabel* label, const QString& text, const char* tone) {
    label->setText(text);
    label->setProperty("tone", tone);
    label->style()->unpolish(label);
    label->style()->polish(label);
}
}

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent), decoder_(new VideoDecoder), player_(this),
      deviceStack_(0), monitorSplitter_(0), liveView_(0), publisher_(new QProcess(this)),
      shuttingDown_(false), layoutMaximized_(false) {
    setWindowTitle(tr("智能家居监控系统"));
    resize(1360, 820);
    setMinimumSize(1180, 700);

    host_ = new QLineEdit("127.0.0.1");
    port_ = new QLineEdit("8000");
    username_ = new QLineEdit;
    password_ = new QLineEdit;
    password_->setEchoMode(QLineEdit::Password);
    autoLogin_ = new QCheckBox(tr("自动连接 / 登录"));
    autoLogin_->setChecked(true);

    cameraId_ = new QLineEdit;
    cameraType_ = new QSpinBox;
    cameraType_->setRange(0, 1);
    channels_ = new QSpinBox;
    channels_->setRange(1, 128);
    serial_ = new QLineEdit;
    cameraIp_ = new QLineEdit;
    rtsp_ = new QLineEdit;
    rtmp_ = new QLineEdit;

    webcamDevice_ = new QLineEdit("Integrated Webcam");
    publishUrl_ = new QLineEdit;
    autoPublish_ = new QCheckBox(tr("启动客户端时自动推流"));
    autoPublish_->setChecked(true);
    publisherStart_ = new QPushButton(tr("启动推流"));
    publisherStart_->setProperty("role", "primary");
    publisherStop_ = new QPushButton(tr("停止推流"));
    publisherStop_->setProperty("role", "danger");

    beginMs_ = new QLineEdit;
    endMs_ = new QLineEdit;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    beginMs_->setText(QString::number(now - 3600000));
    endMs_->setText(QString::number(now));

    QWidget* central = new QWidget(this);
    central->setObjectName("centralPanel");
    QVBoxLayout* root = new QVBoxLayout(central);
    root->setContentsMargins(14, 14, 14, 14);
    root->setSpacing(0);
    monitorSplitter_ = new QSplitter(Qt::Horizontal);
    monitorSplitter_->setObjectName("monitorSplitter");
    monitorSplitter_->setChildrenCollapsible(false);

    devicesPanel_ = makeCard();
    devicesPanel_->setObjectName("devicesPanel");
    devicesPanel_->setMinimumWidth(280);
    QVBoxLayout* devicesLayout = new QVBoxLayout(devicesPanel_);
    devicesLayout->setContentsMargins(14, 13, 14, 14);
    devicesLayout->setSpacing(9);
    devicesLayout->addWidget(makeLabel(tr("设备列表"), "title"));
    devicesLayout->addWidget(makeLabel(tr("双击或选择通道后开始预览"), "subtitle"));
    deviceStack_ = new QStackedWidget;
    deviceStack_->setObjectName("deviceContentStack");
    QWidget* deviceEmptyState = new QWidget;
    deviceEmptyState->setObjectName("deviceEmptyState");
    QVBoxLayout* deviceEmptyLayout = new QVBoxLayout(deviceEmptyState);
    deviceEmptyLayout->setContentsMargins(22, 22, 22, 22);
    deviceEmptyLayout->setSpacing(12);
    deviceEmptyLayout->addStretch();
    QLabel* emptyDeviceIcon = makeLabel(tr("⌂"), "emptyIcon");
    emptyDeviceIcon->setAlignment(Qt::AlignCenter);
    deviceEmptyLayout->addWidget(emptyDeviceIcon);
    QLabel* emptyDeviceTitle = makeLabel(tr("暂无设备"), "emptyTitle");
    emptyDeviceTitle->setAlignment(Qt::AlignCenter);
    deviceEmptyLayout->addWidget(emptyDeviceTitle);
    QLabel* emptyDeviceHint = makeLabel(tr("连接服务器后刷新设备，\n或通过设备配置添加设备。"), "emptyHint");
    emptyDeviceHint->setAlignment(Qt::AlignCenter);
    emptyDeviceHint->setWordWrap(true);
    deviceEmptyLayout->addWidget(emptyDeviceHint);
    QPushButton* emptyRefreshButton = new QPushButton(tr("刷新设备"));
    emptyRefreshButton->setProperty("role", "softPrimary");
    deviceEmptyLayout->addWidget(emptyRefreshButton, 0, Qt::AlignHCenter);
    deviceEmptyLayout->addStretch();
    tree_ = new QTreeWidget;
    tree_->setHeaderLabels(QStringList() << tr("设备 / 通道") << tr("地址"));
    tree_->setAlternatingRowColors(true);
    tree_->setUniformRowHeights(true);
    tree_->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    tree_->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    deviceStack_->addWidget(deviceEmptyState);
    deviceStack_->addWidget(tree_);
    deviceStack_->setCurrentIndex(0);
    devicesLayout->addWidget(deviceStack_, 1);
    monitorSplitter_->addWidget(devicesPanel_);

    QFrame* videoPanel = makeCard();
    videoPanel->setObjectName("livePreviewPanel");
    videoPanel->setMinimumWidth(460);
    QVBoxLayout* videoLayout = new QVBoxLayout(videoPanel);
    videoLayout->setContentsMargins(14, 13, 14, 14);
    videoLayout->setSpacing(10);
    QHBoxLayout* videoHeader = new QHBoxLayout;
    QVBoxLayout* videoWords = new QVBoxLayout;
    videoWords->setSpacing(1);
    videoWords->addWidget(makeLabel(tr("实时监控"), "title"));
    videoWords->addWidget(makeLabel(tr("当前设备的视频预览与录像回放"), "subtitle"));
    videoHeader->addLayout(videoWords);
    videoHeader->addStretch();
    previewShrinkButton_ = new QPushButton(tr("−"));
    previewShrinkButton_->setObjectName("previewLayoutShrinkButton");
    previewShrinkButton_->setProperty("role", "layout");
    previewShrinkButton_->setToolTip(tr("缩小实时画面区域，为两侧面板留出更多空间"));
    previewShrinkButton_->setAccessibleName(tr("缩小实时画面区域"));
    QPushButton* previewDefaultButton = new QPushButton(tr("默认"));
    previewDefaultButton->setObjectName("previewLayoutDefaultButton");
    previewDefaultButton->setProperty("role", "layout");
    previewDefaultButton->setToolTip(tr("恢复设备列表、实时画面、设备控制的默认比例"));
    previewDefaultButton->setAccessibleName(tr("恢复实时监控默认布局"));
    previewExpandButton_ = new QPushButton(tr("+"));
    previewExpandButton_->setObjectName("previewLayoutExpandButton");
    previewExpandButton_->setProperty("role", "layout");
    previewExpandButton_->setToolTip(tr("放大实时画面区域，同时保留可用的两侧面板"));
    previewExpandButton_->setAccessibleName(tr("放大实时画面区域"));
    previewMaximizeButton_ = new QPushButton(tr("最大化"));
    previewMaximizeButton_->setObjectName("previewLayoutMaximizeButton");
    previewMaximizeButton_->setProperty("role", "layout");
    previewMaximizeButton_->setCheckable(true);
    previewMaximizeButton_->setToolTip(tr("在主窗口内最大化实时画面，保留菜单栏和状态栏"));
    previewMaximizeButton_->setAccessibleName(tr("最大化实时画面区域"));
    videoHeader->addWidget(previewShrinkButton_);
    videoHeader->addWidget(previewDefaultButton);
    videoHeader->addWidget(previewExpandButton_);
    videoHeader->addWidget(previewMaximizeButton_);
    videoLayout->addLayout(videoHeader);
    QFrame* videoFrame = new QFrame;
    videoFrame->setObjectName("videoFrame");
    QVBoxLayout* videoFrameLayout = new QVBoxLayout(videoFrame);
    videoFrameLayout->setContentsMargins(8, 8, 8, 8);
    videoFrameLayout->setSpacing(0);
    videoStack_ = new QStackedWidget;
    videoStack_->setObjectName("videoSurface");
    liveView_ = new QLabel(tr("◉\n\n尚未开始预览\n请选择左侧在线设备\n双击设备通道或点击“开始预览”"));
    liveView_->setObjectName("liveView");
    liveView_->setProperty("previewState", "idle");
    liveView_->setAlignment(Qt::AlignCenter);
    playbackView_ = new QVideoWidget;
    videoStack_->addWidget(liveView_);
    videoStack_->addWidget(playbackView_);
    videoFrameLayout->addWidget(videoStack_);
    videoLayout->addWidget(videoFrame, 1);
    monitorSplitter_->addWidget(videoPanel);

    controlPanel_ = makeCard();
    controlPanel_->setObjectName("controlPanel");
    controlPanel_->setMinimumWidth(340);
    QVBoxLayout* controlLayout = new QVBoxLayout(controlPanel_);
    controlLayout->setContentsMargins(14, 13, 14, 14);
    controlLayout->setSpacing(10);
    controlLayout->addWidget(makeLabel(tr("设备控制"), "title"));
    controlLayout->addWidget(makeLabel(tr("云台方向、变焦与速度"), "subtitle"));
    QGridLayout* ptzGrid = new QGridLayout;
    ptzGrid->setHorizontalSpacing(8);
    ptzGrid->setVerticalSpacing(8);
    const QString commands[9] = {"left_up", "up", "right_up", "left", "stop", "right",
                                 "left_down", "down", "right_down"};
    const QString directionLabels[9] = {tr("左上 ↖"), tr("向上 ↑"), tr("右上 ↗"), tr("向左 ←"),
                                        tr("停止"), tr("向右 →"), tr("左下 ↙"), tr("向下 ↓"), tr("右下 ↘")};
    for (int i = 0; i < 9; ++i) {
        const QString command = commands[i];
        QPushButton* button = new QPushButton(directionLabels[i]);
        button->setProperty("role", "ptz");
        button->setProperty("tone", command == QStringLiteral("stop") ? "stop" : "normal");
        ptzGrid->addWidget(button, i / 3, i % 3);
        connect(button, &QPushButton::clicked, this, [this, command] { sendPtz(command); });
    }
    controlLayout->addLayout(ptzGrid);
    QHBoxLayout* zoomLayout = new QHBoxLayout;
    QPushButton* zoomOut = new QPushButton(tr("缩小 −"));
    QPushButton* zoomIn = new QPushButton(tr("放大 +"));
    zoomLayout->addWidget(zoomOut);
    zoomLayout->addWidget(zoomIn);
    controlLayout->addLayout(zoomLayout);
    controlLayout->addWidget(makeLabel(tr("云台速度"), "field"));
    QHBoxLayout* speedLayout = new QHBoxLayout;
    speedLayout->setSpacing(9);
    speedLayout->addWidget(makeLabel(tr("慢"), "sliderEdge"));
    speed_ = new QSlider(Qt::Horizontal);
    speed_->setObjectName("ptzSpeedSlider");
    speed_->setRange(0, 100);
    speed_->setValue(50);
    speedLayout->addWidget(speed_, 1);
    speedLayout->addWidget(makeLabel(tr("快"), "sliderEdge"));
    QLabel* speedValue = makeLabel(QString::number(speed_->value()), "sliderValue");
    speedValue->setAlignment(Qt::AlignCenter);
    speedValue->setMinimumWidth(48);
    speedLayout->addWidget(speedValue);
    controlLayout->addLayout(speedLayout);
    controlLayout->addStretch();
    connect(speed_, &QSlider::valueChanged, speedValue,
            [speedValue](int value) { speedValue->setText(QString::number(value)); });
    connect(zoomIn, &QPushButton::clicked, this, [this] { sendPtz("zoom_in"); });
    connect(zoomOut, &QPushButton::clicked, this, [this] { sendPtz("zoom_out"); });
    monitorSplitter_->addWidget(controlPanel_);
    monitorSplitter_->setStretchFactor(0, 0);
    monitorSplitter_->setStretchFactor(1, 1);
    monitorSplitter_->setStretchFactor(2, 0);
    root->addWidget(monitorSplitter_, 1);
    setCentralWidget(central);

    QDialog* connectionDialog = new QDialog(this);
    connectionDialog->setWindowTitle(tr("连接与账户"));
    connectionDialog->setAttribute(Qt::WA_QuitOnClose, false);
    connectionDialog->resize(640, 420);
    QVBoxLayout* connectionLayout = new QVBoxLayout(connectionDialog);
    connectionLayout->setContentsMargins(20, 18, 20, 18);
    connectionLayout->setSpacing(12);
    connectionLayout->addWidget(makeLabel(tr("连接与账户"), "appTitle"));
    connectionLayout->addWidget(makeLabel(tr("服务器配置和账户验证仅在需要时打开"), "subtitle"));
    QFormLayout* connectionForm = new QFormLayout;
    connectionForm->setHorizontalSpacing(12);
    connectionForm->setVerticalSpacing(9);
    connectionForm->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    connectionForm->addRow(tr("服务器地址"), host_);
    connectionForm->addRow(tr("端口"), port_);
    connectionForm->addRow(tr("用户名"), username_);
    connectionForm->addRow(tr("密码"), password_);
    connectionLayout->addLayout(connectionForm);
    connectionLayout->addWidget(autoLogin_);
    QPushButton* connectButton = new QPushButton(tr("连接服务器"));
    connectButton->setProperty("role", "primary");
    QPushButton* registerButton = new QPushButton(tr("注册账号"));
    QPushButton* loginButton = new QPushButton(tr("登录"));
    loginButton->setProperty("role", "primary");
    QHBoxLayout* connectionButtons = new QHBoxLayout;
    connectionButtons->addWidget(connectButton);
    connectionButtons->addStretch();
    connectionButtons->addWidget(registerButton);
    connectionButtons->addWidget(loginButton);
    connectionLayout->addLayout(connectionButtons);

    QDialog* deviceDialog = new QDialog(this);
    deviceDialog->setWindowTitle(tr("设备配置"));
    deviceDialog->setAttribute(Qt::WA_QuitOnClose, false);
    deviceDialog->resize(700, 600);
    QVBoxLayout* deviceLayout = new QVBoxLayout(deviceDialog);
    deviceLayout->setContentsMargins(20, 18, 20, 18);
    deviceLayout->setSpacing(10);
    deviceLayout->addWidget(makeLabel(tr("摄像头配置"), "appTitle"));
    deviceLayout->addWidget(makeLabel(tr("添加或更新服务端摄像头信息"), "subtitle"));
    QFormLayout* deviceForm = new QFormLayout;
    deviceForm->setHorizontalSpacing(12);
    deviceForm->setVerticalSpacing(8);
    deviceForm->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    deviceForm->addRow(tr("ID"), cameraId_);
    deviceForm->addRow(tr("设备类型（0 枪机 / 1 球机）"), cameraType_);
    deviceForm->addRow(tr("通道数"), channels_);
    deviceForm->addRow(tr("序列号"), serial_);
    deviceForm->addRow(tr("IP 地址"), cameraIp_);
    deviceForm->addRow(tr("RTSP"), rtsp_);
    deviceForm->addRow(tr("RTMP"), rtmp_);
    deviceLayout->addLayout(deviceForm);
    QPushButton* save = new QPushButton(tr("保存 / 更新设备"));
    save->setProperty("role", "primary");
    deviceLayout->addWidget(save);

    QDialog* publisherDialog = new QDialog(this);
    publisherDialog->setWindowTitle(tr("本机摄像头推流"));
    publisherDialog->setAttribute(Qt::WA_QuitOnClose, false);
    publisherDialog->resize(680, 380);
    QVBoxLayout* publisherLayout = new QVBoxLayout(publisherDialog);
    publisherLayout->setContentsMargins(20, 18, 20, 18);
    publisherLayout->setSpacing(11);
    publisherLayout->addWidget(makeLabel(tr("本机摄像头推流"), "appTitle"));
    publisherLayout->addWidget(makeLabel(tr("使用 FFmpeg 发布本机摄像头画面"), "subtitle"));
    QFormLayout* publisherForm = new QFormLayout;
    publisherForm->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    publisherForm->setHorizontalSpacing(12);
    publisherForm->setVerticalSpacing(9);
    publisherForm->addRow(tr("设备名称"), webcamDevice_);
    publisherForm->addRow(tr("发布地址"), publishUrl_);
    publisherLayout->addLayout(publisherForm);
    publisherLayout->addWidget(autoPublish_);
    QHBoxLayout* publisherButtons = new QHBoxLayout;
    publisherButtons->addWidget(publisherStart_);
    publisherButtons->addWidget(publisherStop_);
    publisherLayout->addLayout(publisherButtons);
    publisherLayout->addStretch();

    QDialog* playbackDialog = new QDialog(this);
    playbackDialog->setWindowTitle(tr("录像回放"));
    playbackDialog->setAttribute(Qt::WA_QuitOnClose, false);
    playbackDialog->resize(640, 340);
    QVBoxLayout* playbackLayout = new QVBoxLayout(playbackDialog);
    playbackLayout->setContentsMargins(20, 18, 20, 18);
    playbackLayout->setSpacing(11);
    playbackLayout->addWidget(makeLabel(tr("录像回放"), "appTitle"));
    playbackLayout->addWidget(makeLabel(tr("选择设备通道后按时间范围请求 HLS 录像"), "subtitle"));
    QFormLayout* playbackForm = new QFormLayout;
    playbackForm->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    playbackForm->addRow(tr("开始时间戳（ms）"), beginMs_);
    playbackForm->addRow(tr("结束时间戳（ms）"), endMs_);
    playbackLayout->addLayout(playbackForm);
    QPushButton* playbackButton = new QPushButton(tr("播放录像"));
    playbackButton->setProperty("role", "primary");
    playbackLayout->addWidget(playbackButton);

    QDockWidget* logDock = new QDockWidget(tr("运行日志"), this);
    logDock->setObjectName("logDock");
    logDock->setAllowedAreas(Qt::BottomDockWidgetArea | Qt::TopDockWidgetArea);
    logView_ = new QPlainTextEdit;
    logView_->setObjectName("logView");
    logView_->setReadOnly(true);
    logView_->setMaximumBlockCount(500);
    logDock->setWidget(logView_);
    addDockWidget(Qt::BottomDockWidgetArea, logDock);
    logDock->hide();

    QMenu* fileMenu = menuBar()->addMenu(tr("文件"));
    QAction* connectionAction = fileMenu->addAction(tr("连接与账户..."));
    connectionAction->setShortcut(QKeySequence("Ctrl+L"));
    fileMenu->addSeparator();
    QAction* exitAction = fileMenu->addAction(tr("退出程序"));

    QMenu* deviceMenu = menuBar()->addMenu(tr("设备"));
    QAction* refreshAction = deviceMenu->addAction(tr("刷新设备"));
    refreshAction->setShortcut(QKeySequence("F5"));
    QAction* deviceAction = deviceMenu->addAction(tr("设备配置..."));

    QMenu* monitorMenu = menuBar()->addMenu(tr("监控"));
    QAction* previewAction = monitorMenu->addAction(tr("开始预览"));
    previewAction->setCheckable(true);
    QAction* stopPreviewAction = monitorMenu->addAction(tr("停止预览"));
    monitorMenu->addSeparator();
    QAction* recordAction = monitorMenu->addAction(tr("开始录像"));
    recordAction->setCheckable(true);
    QAction* stopRecordAction = monitorMenu->addAction(tr("停止录像"));
    QAction* playbackAction = monitorMenu->addAction(tr("录像回放..."));

    QMenu* viewMenu = menuBar()->addMenu(tr("视图"));
    viewMenu->addAction(logDock->toggleViewAction());
    themeAction_ = viewMenu->addAction(tr("深色模式"));
    themeAction_->setCheckable(true);

    QMenu* toolsMenu = menuBar()->addMenu(tr("工具"));
    QAction* publisherAction = toolsMenu->addAction(tr("本机摄像头推流..."));

    QMenu* infoMenu = menuBar()->addMenu(tr("信息"));
    QAction* runtimeInfoAction = infoMenu->addAction(tr("连接与运行信息..."));
    QAction* aboutAction = infoMenu->addAction(tr("关于"));
    connectionInfoAction_ = menuBar()->addAction(tr("● 未连接"));
    connectionInfoAction_->setToolTip(tr("查看连接与运行信息"));

    QToolBar* toolBar = addToolBar(tr("常用操作"));
    toolBar->setObjectName("mainToolBar");
    toolBar->setMovable(false);
    toolBar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    toolBar->addAction(connectionAction);
    toolBar->addAction(refreshAction);
    toolBar->addSeparator();
    toolBar->addAction(previewAction);
    toolBar->addAction(stopPreviewAction);
    toolBar->addSeparator();
    toolBar->addAction(recordAction);
    toolBar->addAction(stopRecordAction);
    if (QToolButton* previewToolButton = qobject_cast<QToolButton*>(toolBar->widgetForAction(previewAction)))
        previewToolButton->setProperty("role", "primary");

    connectionStatus_ = makeLabel(tr("服务器：未连接"), "badge");
    connectionStatus_->setProperty("tone", "neutral");
    videoStatus_ = makeLabel(tr("画面：未预览"), "badge");
    videoStatus_->setProperty("tone", "neutral");
    publisherStatus_ = makeLabel(tr("推流：已停止"), "badge");
    publisherStatus_->setProperty("tone", "neutral");
    deviceCount_ = makeLabel(tr("设备：0 台"), "badge");
    deviceCount_->setProperty("tone", "neutral");
    statusBar()->addWidget(connectionStatus_);
    statusBar()->addWidget(videoStatus_);
    statusBar()->addPermanentWidget(deviceCount_);
    statusBar()->addPermanentWidget(publisherStatus_);

    loadSettings();
    applyTheme(themeAction_->isChecked());
    QTimer::singleShot(0, this, [this] {
        QSettings settings(clientSettingsPath(), QSettings::IniFormat);
        const QByteArray state = settings.value("appearance/monitorSplitterState").toByteArray();
        if (state.isEmpty() || !monitorSplitter_->restoreState(state))
            restoreDefaultMonitorLayout();
        updatePreviewLayoutButtons();
    });
    log(tr("客户端配置：%1").arg(QDir::toNativeSeparators(clientSettingsPath())));

    decoder_->moveToThread(&decoderThread_); connect(&decoderThread_, &QThread::finished, decoder_, &QObject::deleteLater);
    connect(&protocol_, &ProtocolClient::videoMetadata, decoder_, &VideoDecoder::configure);
    connect(&protocol_, &ProtocolClient::compressedVideoPacket, decoder_, &VideoDecoder::decode);
    connect(decoder_, &VideoDecoder::frameReady, this, &MainWindow::showFrame);
    connect(decoder_, &VideoDecoder::decoderError, this, &MainWindow::log); decoderThread_.start();
    player_.setVideoOutput(playbackView_);

    connect(connectButton, &QPushButton::clicked, this, [this] {
        saveSettings();
        setStatus(connectionStatus_, tr("服务器：连接中"), "warning");
        protocol_.connectToServer(host_->text(), quint16(port_->text().toUShort())); });
    connect(registerButton, &QPushButton::clicked, this, [this] {
        saveSettings();
        protocol_.registerUser(username_->text(), password_->text()); });
    connect(loginButton, &QPushButton::clicked, this, [this] {
        saveSettings();
        protocol_.login(username_->text(), password_->text()); });
    connect(refreshAction, &QAction::triggered, &protocol_, &ProtocolClient::requestCameras);
    connect(emptyRefreshButton, &QPushButton::clicked, &protocol_, &ProtocolClient::requestCameras);
    connect(save, &QPushButton::clicked, this, &MainWindow::saveCamera);
    connect(previewAction, &QAction::triggered, this, [this, previewAction] {
        playLive(); previewAction->setChecked(tree_->currentItem() && tree_->currentItem()->parent()); });
    connect(stopPreviewAction, &QAction::triggered, this, [this, previewAction] {
        stopLive(); previewAction->setChecked(false); });
    connect(recordAction, &QAction::triggered, this, [this, recordAction] {
        startRecording(); recordAction->setChecked(tree_->currentItem() && tree_->currentItem()->parent()); });
    connect(stopRecordAction, &QAction::triggered, this, [this, recordAction] {
        stopRecording(); recordAction->setChecked(false); });
    connect(playbackButton, &QPushButton::clicked, this, &MainWindow::playRecording);
    connect(themeAction_, &QAction::toggled, this, &MainWindow::applyTheme);
    connect(previewShrinkButton_, &QPushButton::clicked, this, [this] { adjustPreviewWidth(-1); });
    connect(previewDefaultButton, &QPushButton::clicked, this, &MainWindow::restoreDefaultMonitorLayout);
    connect(previewExpandButton_, &QPushButton::clicked, this, [this] { adjustPreviewWidth(1); });
    connect(previewMaximizeButton_, &QPushButton::clicked, this, &MainWindow::togglePreviewMaximized);
    connect(monitorSplitter_, &QSplitter::splitterMoved, this, [this] { updatePreviewLayoutButtons(); updateLivePixmap(); });
    connect(publisherStart_, &QPushButton::clicked, this, &MainWindow::startWebcamPublisher);
    connect(publisherStop_, &QPushButton::clicked, this, &MainWindow::stopWebcamPublisher);
    connect(connectionAction, &QAction::triggered, connectionDialog, [connectionDialog] {
        connectionDialog->show(); connectionDialog->raise(); connectionDialog->activateWindow(); });
    connect(deviceAction, &QAction::triggered, deviceDialog, [deviceDialog] {
        deviceDialog->show(); deviceDialog->raise(); deviceDialog->activateWindow(); });
    connect(publisherAction, &QAction::triggered, publisherDialog, [publisherDialog] {
        publisherDialog->show(); publisherDialog->raise(); publisherDialog->activateWindow(); });
    connect(playbackAction, &QAction::triggered, playbackDialog, [playbackDialog] {
        playbackDialog->show(); playbackDialog->raise(); playbackDialog->activateWindow(); });
    connect(exitAction, &QAction::triggered, this, &QWidget::close);
    connect(tree_, &QTreeWidget::itemDoubleClicked, this, [this, previewAction](QTreeWidgetItem* item, int) {
        if (item && item->parent()) { playLive(); previewAction->setChecked(true); }
    });
    QDialog* runtimeInfoDialog = new QDialog(this);
    runtimeInfoDialog->setWindowTitle(tr("连接与运行信息"));
    runtimeInfoDialog->setObjectName("runtimeInfoDialog");
    runtimeInfoDialog->setAttribute(Qt::WA_QuitOnClose, false);
    runtimeInfoDialog->resize(640, 460);
    QVBoxLayout* runtimeInfoLayout = new QVBoxLayout(runtimeInfoDialog);
    runtimeInfoLayout->setContentsMargins(22, 20, 22, 20);
    runtimeInfoLayout->setSpacing(14);
    runtimeInfoLayout->addWidget(makeLabel(tr("连接与运行信息"), "appTitle"));
    runtimeInfoLayout->addWidget(makeLabel(tr("查看当前客户端连接、画面和推流状态"), "subtitle"));
    QGridLayout* runtimeInfoGrid = new QGridLayout;
    runtimeInfoGrid->setHorizontalSpacing(24);
    runtimeInfoGrid->setVerticalSpacing(13);
    QLabel* infoServerValue = makeLabel(QString(), "infoValue");
    QLabel* infoConnectionValue = makeLabel(QString(), "infoValue");
    QLabel* infoUserValue = makeLabel(QString(), "infoValue");
    QLabel* infoDeviceValue = makeLabel(QString(), "infoValue");
    QLabel* infoVideoValue = makeLabel(QString(), "infoValue");
    QLabel* infoPublisherValue = makeLabel(QString(), "infoValue");
    const QString infoNames[6] = {tr("服务器"), tr("连接状态"), tr("当前用户"), tr("设备数量"), tr("视频状态"), tr("本机推流")};
    QLabel* infoValues[6] = {infoServerValue, infoConnectionValue, infoUserValue,
                             infoDeviceValue, infoVideoValue, infoPublisherValue};
    for (int row = 0; row < 6; ++row) {
        runtimeInfoGrid->addWidget(makeLabel(infoNames[row], "field"), row, 0);
        runtimeInfoGrid->addWidget(infoValues[row], row, 1);
    }
    runtimeInfoGrid->setColumnStretch(1, 1);
    runtimeInfoLayout->addLayout(runtimeInfoGrid);
    runtimeInfoLayout->addStretch();
    QPushButton* closeRuntimeInfo = new QPushButton(tr("关闭"));
    runtimeInfoLayout->addWidget(closeRuntimeInfo, 0, Qt::AlignRight);
    connect(closeRuntimeInfo, &QPushButton::clicked, runtimeInfoDialog, &QDialog::close);
    const auto showRuntimeInfo = [this, runtimeInfoDialog, infoServerValue, infoConnectionValue,
                                  infoUserValue, infoDeviceValue, infoVideoValue, infoPublisherValue] {
        infoServerValue->setText(QString("%1:%2").arg(host_->text(), port_->text()));
        const bool connected = connectionStatus_->text().contains(tr("已连接"));
        const bool connecting = connectionStatus_->text().contains(tr("连接中"));
        setStatus(infoConnectionValue, connectionStatus_->text(), connected ? "success" : (connecting ? "warning" : "neutral"));
        infoUserValue->setText(username_->text().isEmpty() ? tr("未登录") : username_->text());
        infoDeviceValue->setText(tr("%1 台").arg(cameras_.size()));
        const bool videoActive = videoStatus_->text().contains(tr("实时预览")) || videoStatus_->text().contains(tr("录像回放"));
        const bool videoPending = videoStatus_->text().contains(tr("请求"));
        setStatus(infoVideoValue, videoStatus_->text(), videoActive ? "success" : (videoPending ? "warning" : "neutral"));
        const bool publishing = publisherStatus_->text().contains(tr("运行中"));
        const bool publishWarning = publisherStatus_->text().contains(tr("失败")) || publisherStatus_->text().contains(tr("缺少"));
        setStatus(infoPublisherValue, publisherStatus_->text(), publishing ? "success" : (publishWarning ? "error" : "neutral"));
        runtimeInfoDialog->show();
        runtimeInfoDialog->raise();
        runtimeInfoDialog->activateWindow();
    };
    connect(runtimeInfoAction, &QAction::triggered, this, showRuntimeInfo);
    connect(connectionInfoAction_, &QAction::triggered, this, showRuntimeInfo);
    connect(aboutAction, &QAction::triggered, this, [this] {
        QMessageBox::about(this, tr("关于"), tr("智能家居监控系统\nQt Widgets 客户端 v2"));
    });
    connect(&protocol_, &ProtocolClient::camerasReceived, this, &MainWindow::showCameras);
    connect(&protocol_, &ProtocolClient::message, this, &MainWindow::log);
    connect(&protocol_, &ProtocolClient::protocolError, this, &MainWindow::log);
    connect(&protocol_, &ProtocolClient::registrationFinished, this, [this] {
        statusBar()->showMessage(tr("注册成功，正在自动登录"), 5000);
        QMessageBox::information(this, tr("注册成功"),
            tr("账号注册成功。点击确定后，客户端将自动登录。"));
        protocol_.login(username_->text(), password_->text());
    });
    connect(&protocol_, &ProtocolClient::authenticated, this, [this] {
        statusBar()->showMessage(tr("登录成功，身份认证已完成"), 5000);
        QMessageBox::information(this, tr("登录成功"),
            tr("身份认证成功，现在可以刷新设备并开始预览。"));
    });
    connect(&protocol_, &ProtocolClient::connected, this, [this] {
        setStatus(connectionStatus_, tr("服务器：已连接"), "success");
        connectionInfoAction_->setText(tr("● 已连接"));
        log(tr("服务器已连接"));
        if (autoLogin_->isChecked() && !username_->text().isEmpty() && !password_->text().isEmpty())
            protocol_.login(username_->text(), password_->text());
    });
    connect(&protocol_, &ProtocolClient::disconnected, this, [this] {
        setStatus(connectionStatus_, tr("服务器：未连接"), "neutral");
        connectionInfoAction_->setText(tr("● 未连接"));
        log(tr("服务器已断开")); });
    connect(&protocol_, &ProtocolClient::playbackUrlReceived, this, [this](const QUrl& url) {
        videoStack_->setCurrentWidget(playbackView_); player_.setMedia(QMediaContent(url)); player_.play();
        setStatus(videoStatus_, tr("画面：录像回放"), "info");
        log(tr("开始回放 %1").arg(url.toString())); });

    connect(publisher_, &QProcess::started, this, [this] {
        publisherStart_->setEnabled(false); publisherStop_->setEnabled(true);
        setStatus(publisherStatus_, tr("推流：运行中"), "success");
        log(tr("本机摄像头推流已启动"));
    });
    connect(publisher_, &QProcess::readyReadStandardOutput, this, [this] {
        const QString output = QString::fromLocal8Bit(publisher_->readAllStandardOutput()).trimmed();
        if (!output.isEmpty()) log(tr("FFmpeg: %1").arg(output));
    });
    connect(publisher_, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
        [this](int code, QProcess::ExitStatus) {
            publisherStart_->setEnabled(true); publisherStop_->setEnabled(false);
            setStatus(publisherStatus_, tr("推流：已停止"), "neutral");
            if (!shuttingDown_) log(tr("本机摄像头推流已结束，退出码 %1").arg(code));
        });
    connect(publisher_, &QProcess::errorOccurred, this, [this](QProcess::ProcessError) {
        setStatus(publisherStatus_, tr("推流：启动失败"), "warning");
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
    themeAction_->setChecked(settings.value("appearance/darkTheme", false).toBool());
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
    settings.setValue("appearance/darkTheme", themeAction_->isChecked());
    settings.setValue("appearance/windowGeometry", saveGeometry());
    if (monitorSplitter_ && !layoutMaximized_)
        settings.setValue("appearance/monitorSplitterState", monitorSplitter_->saveState());
    QTreeWidgetItem* item = tree_->currentItem();
    if (item && item->parent()) {
        settings.setValue("selection/cameraId", item->data(0, Qt::UserRole).toUInt());
        settings.setValue("selection/channel", item->data(0, Qt::UserRole + 1).toUInt());
    }
    settings.sync();
}

void MainWindow::applyTheme(bool dark) {
    qApp->setStyleSheet(dark ? client_theme::dark() : client_theme::light());
    QSettings settings(clientSettingsPath(), QSettings::IniFormat);
    settings.setValue("appearance/darkTheme", dark);
}

void MainWindow::restoreDefaultMonitorLayout() {
    if (!monitorSplitter_) return;
    if (layoutMaximized_) {
        devicesPanel_->show();
        controlPanel_->show();
        layoutMaximized_ = false;
        previewMaximizeButton_->setChecked(false);
        previewMaximizeButton_->setText(tr("最大化"));
        previewMaximizeButton_->setAccessibleName(tr("最大化实时画面区域"));
    }
    const int total = qMax(monitorSplitter_->width(), 960);
    monitorSplitter_->setSizes(QList<int>() << total * 22 / 100
                                             << total * 50 / 100
                                             << total * 28 / 100);
    preMaximizeSizes_.clear();
    QSettings settings(clientSettingsPath(), QSettings::IniFormat);
    settings.remove("appearance/monitorSplitterState");
    updatePreviewLayoutButtons();
    updateLivePixmap();
}

void MainWindow::adjustPreviewWidth(int direction) {
    if (!monitorSplitter_ || layoutMaximized_ || direction == 0) return;
    QList<int> sizes = monitorSplitter_->sizes();
    if (sizes.size() != 3) return;
    const int total = sizes[0] + sizes[1] + sizes[2];
    const int step = qMax(56, total * 9 / 100);
    const int leftMinimum = devicesPanel_->minimumWidth();
    const int previewMinimum = monitorSplitter_->widget(1)->minimumWidth();
    const int rightMinimum = controlPanel_->minimumWidth();

    if (direction > 0) {
        const int leftAvailable = qMax(0, sizes[0] - leftMinimum);
        const int rightAvailable = qMax(0, sizes[2] - rightMinimum);
        const int amount = qMin(step, leftAvailable + rightAvailable);
        int takeLeft = qMin(leftAvailable, amount / 2);
        int takeRight = qMin(rightAvailable, amount - takeLeft);
        if (takeLeft + takeRight < amount)
            takeLeft += qMin(leftAvailable - takeLeft, amount - takeLeft - takeRight);
        sizes[0] -= takeLeft;
        sizes[1] += takeLeft + takeRight;
        sizes[2] -= takeRight;
    } else {
        const int amount = qMin(step, qMax(0, sizes[1] - previewMinimum));
        const int addLeft = amount * 44 / 100;
        sizes[0] += addLeft;
        sizes[1] -= amount;
        sizes[2] += amount - addLeft;
    }
    monitorSplitter_->setSizes(sizes);
    updatePreviewLayoutButtons();
    updateLivePixmap();
}

void MainWindow::togglePreviewMaximized() {
    if (!monitorSplitter_) return;
    if (!layoutMaximized_) {
        preMaximizeSizes_ = monitorSplitter_->sizes();
        devicesPanel_->hide();
        controlPanel_->hide();
        layoutMaximized_ = true;
        previewMaximizeButton_->setChecked(true);
        previewMaximizeButton_->setText(tr("退出最大化"));
        previewMaximizeButton_->setToolTip(tr("恢复最大化前的三栏布局"));
        previewMaximizeButton_->setAccessibleName(tr("退出实时画面区域最大化"));
    } else {
        devicesPanel_->show();
        controlPanel_->show();
        layoutMaximized_ = false;
        previewMaximizeButton_->setChecked(false);
        previewMaximizeButton_->setText(tr("最大化"));
        previewMaximizeButton_->setToolTip(tr("在主窗口内最大化实时画面，保留菜单栏和状态栏"));
        previewMaximizeButton_->setAccessibleName(tr("最大化实时画面区域"));
        const QList<int> restoreSizes = preMaximizeSizes_;
        QTimer::singleShot(0, this, [this, restoreSizes] {
            if (restoreSizes.size() == 3) monitorSplitter_->setSizes(restoreSizes);
            else restoreDefaultMonitorLayout();
            updatePreviewLayoutButtons();
            updateLivePixmap();
        });
    }
    updatePreviewLayoutButtons();
    updateLivePixmap();
}

void MainWindow::updatePreviewLayoutButtons() {
    if (!monitorSplitter_ || !previewShrinkButton_ || !previewExpandButton_) return;
    if (layoutMaximized_) {
        previewShrinkButton_->setEnabled(false);
        previewExpandButton_->setEnabled(false);
        return;
    }
    const QList<int> sizes = monitorSplitter_->sizes();
    if (sizes.size() != 3) return;
    previewShrinkButton_->setEnabled(sizes[1] > monitorSplitter_->widget(1)->minimumWidth());
    previewExpandButton_->setEnabled(sizes[0] > devicesPanel_->minimumWidth() ||
                                     sizes[2] > controlPanel_->minimumWidth());
}

void MainWindow::updateLivePixmap() {
    if (!liveView_ || lastFrame_.isNull()) return;
    liveView_->setPixmap(QPixmap::fromImage(lastFrame_).scaled(
        liveView_->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void MainWindow::resizeEvent(QResizeEvent* event) {
    QMainWindow::resizeEvent(event);
    updateLivePixmap();
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
    if (executable.isEmpty()) {
        setStatus(publisherStatus_, tr("推流：缺少 FFmpeg"), "warning");
        log(tr("未找到 ffmpeg.exe，请使用完整的客户端版本目录")); return;
    }
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
    deviceStack_->setCurrentIndex(cameras.isEmpty() ? 0 : 1);
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
    deviceCount_->setText(tr("设备：%1 台").arg(cameras.size()));
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
    lastFrame_ = image;
    if (liveView_->property("previewState").toString() != QStringLiteral("active")) {
        liveView_->setProperty("previewState", "active");
        liveView_->style()->unpolish(liveView_);
        liveView_->style()->polish(liveView_);
    }
    liveView_->setText(QString());
    updateLivePixmap();
}
void MainWindow::playLive() { CameraDeviceDto camera; quint32 channel;
    if (!selection(camera, channel)) { log(tr("请先选择一个通道")); return; }
    player_.stop(); videoStack_->setCurrentWidget(liveView_);
    setStatus(videoStatus_, tr("画面：实时预览"), "success");
    protocol_.startStream(camera.id, selectedStream(camera)); }
void MainWindow::stopLive() { protocol_.stopStream(); QMetaObject::invokeMethod(decoder_, "reset", Qt::QueuedConnection);
    lastFrame_ = QImage(); liveView_->clear(); liveView_->setProperty("previewState", "idle");
    liveView_->style()->unpolish(liveView_); liveView_->style()->polish(liveView_);
    liveView_->setText(tr("◉\n\n尚未开始预览\n请选择左侧在线设备\n双击设备通道或点击“开始预览”"));
    setStatus(videoStatus_, tr("画面：未预览"), "neutral"); }
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
    saveSettings(); setStatus(videoStatus_, tr("画面：请求回放"), "warning");
    protocol_.requestPlayback(camera.id, channel, beginMs_->text().toLongLong(), endMs_->text().toLongLong()); }
void MainWindow::sendPtz(const QString& command) { CameraDeviceDto camera; quint32 channel;
    if (!selection(camera, channel)) { log(tr("请先选择一个通道")); return; } protocol_.ptz(channel, command, speed_->value()); }
void MainWindow::log(const QString& text) {
    logView_->appendPlainText(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss ") + text);
}
