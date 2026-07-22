#ifndef SMART_HOME_CLIENT_PROTOCOL_CLIENT_H
#define SMART_HOME_CLIENT_PROTOCOL_CLIENT_H

#include <QByteArray>
#include <QHash>
#include <QObject>
#include <QTcpSocket>
#include <QUrl>
#include <QVector>

struct CameraDeviceDto {
    quint32 id;
    quint32 type;
    quint32 channels;
    QString serialNumber;
    QString ip;
    QString rtspUrl;
    QString rtmpUrl;
};
Q_DECLARE_METATYPE(CameraDeviceDto)
Q_DECLARE_METATYPE(QVector<CameraDeviceDto>)

class ProtocolClient : public QObject {
    Q_OBJECT
public:
    enum TaskType {
        LoginSection1 = 1, LoginSection1Ok, LoginSection1Error,
        LoginSection2, LoginSection2Ok, LoginSection2Error,
        RegisterSection1, RegisterSection1Ok, RegisterSection1Error,
        RegisterSection2, RegisterSection2Ok, RegisterSection2Error,
        UserReady, GetCameras, Cameras, GetStream, StreamMetadata,
        StreamPacket, StopStream, CameraHttpRequest, CameraHttpResponse,
        PtzControl, GetRecordings, Recordings, Error, GetPlaybackUrl,
        PlaybackUrl, StartRecording, StopRecording, RecordingStatus,
        SaveCamera, CameraSaved
    };

    explicit ProtocolClient(QObject* parent = 0);
    void connectToServer(const QString& host, quint16 port);
    void disconnectFromServer();
    void registerUser(const QString& username, const QString& password);
    void login(const QString& username, const QString& password);
    void requestCameras();
    void saveCamera(const CameraDeviceDto& camera);
    void startStream(quint32 cameraId, const QString& url);
    void stopStream();
    void ptz(quint32 channel, const QString& command, int speed);
    void startRecording(quint32 cameraId, quint32 channel, const QString& url);
    void stopRecording(quint32 cameraId, quint32 channel);
    void requestPlayback(quint32 cameraId, quint32 channel,
                         qint64 beginMs, qint64 endMs);

signals:
    void connected();
    void disconnected();
    void authenticated();
    void registrationFinished();
    void camerasReceived(const QVector<CameraDeviceDto>& cameras);
    void videoMetadata(const QByteArray& data);
    void compressedVideoPacket(const QByteArray& data);
    void playbackUrlReceived(const QUrl& url);
    void message(const QString& text);
    void protocolError(const QString& text);

private slots:
    void readAvailable();
    void socketError(QAbstractSocket::SocketError error);

private:
    struct Assembly { quint16 count; quint16 next; QByteArray data; };
    enum AuthOperation { NoAuth, Registering, LoggingIn };
    void sendPacket(quint32 type, const QByteArray& value = QByteArray());
    void dispatch(quint32 type, const QByteArray& value);
    void consumeFragment(const QByteArray& value);
    QVector<CameraDeviceDto> decodeCameras(const QByteArray& value) const;
    QByteArray encodeCamera(const CameraDeviceDto& camera) const;
    static QByteArray passwordDigest(const QByteArray& setting, const QString& password);

    QTcpSocket socket_;
    QByteArray receiveBuffer_;
    QHash<quint32, Assembly> assemblies_;
    AuthOperation authOperation_;
    QString pendingPassword_;
};

#endif
