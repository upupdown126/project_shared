#ifndef SMART_HOME_CLIENT_MAIN_WINDOW_H
#define SMART_HOME_CLIENT_MAIN_WINDOW_H

#include "protocol_client.h"

#include <QMainWindow>
#include <QMediaPlayer>
#include <QThread>

class QLabel;
class QCheckBox;
class QLineEdit;
class QPlainTextEdit;
class QProcess;
class QPushButton;
class QSpinBox;
class QStackedWidget;
class QTreeWidget;
class QVideoWidget;
class VideoDecoder;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = 0);
    ~MainWindow();
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
private:
    bool selection(CameraDeviceDto& camera, quint32& channel) const;
    QString selectedStream(const CameraDeviceDto& camera) const;
    void sendPtz(const QString& command);
    void loadSettings();
    void saveSettings() const;
    void startWebcamPublisher();
    void stopWebcamPublisher();
    QString ffmpegExecutable() const;

    ProtocolClient protocol_;
    QVector<CameraDeviceDto> cameras_;
    QThread decoderThread_;
    VideoDecoder* decoder_;
    QMediaPlayer player_;
    QTreeWidget* tree_;
    QLabel* liveView_;
    QVideoWidget* playbackView_;
    QStackedWidget* videoStack_;
    QPlainTextEdit* logView_;
    QProcess* publisher_;
    QLineEdit *host_, *port_, *username_, *password_;
    QLineEdit *cameraId_, *serial_, *cameraIp_, *rtsp_, *rtmp_;
    QLineEdit *webcamDevice_, *publishUrl_;
    QSpinBox *cameraType_, *channels_, *speed_;
    QLineEdit *beginMs_, *endMs_;
    QCheckBox *autoLogin_, *autoPublish_;
    QPushButton *themeButton_, *publisherStart_, *publisherStop_;
    bool shuttingDown_;
};

#endif
