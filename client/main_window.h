#ifndef SMART_HOME_CLIENT_MAIN_WINDOW_H
#define SMART_HOME_CLIENT_MAIN_WINDOW_H

#include "protocol_client.h"

#include <QMainWindow>
#include <QMediaPlayer>
#include <QImage>
#include <QList>
#include <QThread>

class QLabel;
class QAction;
class QCheckBox;
class QLineEdit;
class QPlainTextEdit;
class QProcess;
class QPushButton;
class QSlider;
class QSpinBox;
class QStackedWidget;
class QSplitter;
class QTreeWidget;
class QVideoWidget;
class QResizeEvent;
class VideoDecoder;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = 0);
    ~MainWindow();
protected:
    void resizeEvent(QResizeEvent* event);
private slots:
    void showCameras(const QVector<CameraDeviceDto>& cameras);
    void showFrame(const QImage& image);
    void playLive();
    void stopLive();
    void saveCamera();
    void startRecording();
    void stopRecording();
    void playRecording();
    void log(const QString& text);
    void applyTheme(bool dark);
    void restoreDefaultMonitorLayout();
    void togglePreviewMaximized();
private:
    bool selection(CameraDeviceDto& camera, quint32& channel) const;
    QString selectedStream(const CameraDeviceDto& camera) const;
    void sendPtz(const QString& command);
    void loadSettings();
    void saveSettings() const;
    void adjustPreviewWidth(int direction);
    void updatePreviewLayoutButtons();
    void updateLivePixmap();
    void startWebcamPublisher();
    void stopWebcamPublisher();
    QString ffmpegExecutable() const;

    ProtocolClient protocol_;
    QVector<CameraDeviceDto> cameras_;
    QThread decoderThread_;
    VideoDecoder* decoder_;
    QMediaPlayer player_;
    QTreeWidget* tree_;
    QStackedWidget* deviceStack_;
    QSplitter* monitorSplitter_;
    QWidget *devicesPanel_, *controlPanel_;
    QLabel* liveView_;
    QLabel* connectionStatus_;
    QLabel* deviceCount_;
    QLabel* videoStatus_;
    QLabel* publisherStatus_;
    QVideoWidget* playbackView_;
    QStackedWidget* videoStack_;
    QPlainTextEdit* logView_;
    QProcess* publisher_;
    QLineEdit *host_, *port_, *username_, *password_;
    QLineEdit *cameraId_, *serial_, *cameraIp_, *rtsp_, *rtmp_;
    QLineEdit *webcamDevice_, *publishUrl_;
    QSpinBox *cameraType_, *channels_;
    QSlider* speed_;
    QLineEdit *beginMs_, *endMs_;
    QCheckBox *autoLogin_, *autoPublish_;
    QPushButton *publisherStart_, *publisherStop_;
    QPushButton *previewShrinkButton_, *previewExpandButton_, *previewMaximizeButton_;
    QAction *connectionInfoAction_, *themeAction_;
    bool shuttingDown_;
    bool layoutMaximized_;
    QList<int> preMaximizeSizes_;
    QImage lastFrame_;
};

#endif
