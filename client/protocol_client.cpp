#include "protocol_client.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QNetworkProxy>
#include <QUrl>
#include <QtGlobal>
#include <stdexcept>

namespace {
void append32(QByteArray& out, quint32 value) {
    out.append(char(value >> 24)); out.append(char(value >> 16));
    out.append(char(value >> 8)); out.append(char(value));
}
quint16 read16(const QByteArray& in, int& at) {
    if (at < 0 || in.size() - at < 2) throw std::runtime_error("truncated uint16");
    const uchar* p = reinterpret_cast<const uchar*>(in.constData() + at); at += 2;
    return (quint16(p[0]) << 8) | p[1];
}
quint32 read32(const QByteArray& in, int& at) {
    if (at < 0 || in.size() - at < 4) throw std::runtime_error("truncated uint32");
    const uchar* p = reinterpret_cast<const uchar*>(in.constData() + at); at += 4;
    return (quint32(p[0]) << 24) | (quint32(p[1]) << 16) |
           (quint32(p[2]) << 8) | p[3];
}
void appendText(QByteArray& out, const QString& text) {
    const QByteArray utf8 = text.toUtf8(); append32(out, quint32(utf8.size())); out.append(utf8);
}
QString readText(const QByteArray& in, int& at) {
    const quint32 length = read32(in, at);
    if (length > 1024U * 1024U || length > quint32(in.size() - at))
        throw std::runtime_error("invalid text length");
    const QString value = QString::fromUtf8(in.constData() + at, int(length)); at += int(length);
    return value;
}
}

ProtocolClient::ProtocolClient(QObject* parent)
    : QObject(parent), authOperation_(NoAuth) {
    // The monitor server is reached directly over the LAN.  Do not inherit a
    // browser/system proxy, because QTcpSocket cannot use every proxy type
    // exposed by Windows (for example an automatic HTTP proxy configuration).
    socket_.setProxy(QNetworkProxy::NoProxy);
    qRegisterMetaType<QVector<CameraDeviceDto> >("QVector<CameraDeviceDto>");
    connect(&socket_, &QTcpSocket::connected, this, &ProtocolClient::connected);
    connect(&socket_, &QTcpSocket::disconnected, this, &ProtocolClient::disconnected);
    connect(&socket_, &QTcpSocket::readyRead, this, &ProtocolClient::readAvailable);
    connect(&socket_, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::error),
            this, &ProtocolClient::socketError);
}
void ProtocolClient::connectToServer(const QString& host, quint16 port) {
    receiveBuffer_.clear(); assemblies_.clear(); socket_.connectToHost(host, port);
}
void ProtocolClient::disconnectFromServer() { socket_.disconnectFromHost(); }
void ProtocolClient::registerUser(const QString& user, const QString& password) {
    authOperation_ = Registering; pendingPassword_ = password;
    sendPacket(RegisterSection1, user.toUtf8());
}
void ProtocolClient::login(const QString& user, const QString& password) {
    authOperation_ = LoggingIn; pendingPassword_ = password;
    sendPacket(LoginSection1, user.toUtf8());
}
void ProtocolClient::requestCameras() { sendPacket(GetCameras); }
void ProtocolClient::saveCamera(const CameraDeviceDto& camera) {
    sendPacket(SaveCamera, encodeCamera(camera));
}
void ProtocolClient::startStream(quint32 id, const QString& url) {
    sendPacket(GetStream, QByteArray::number(id) + " " + url.toUtf8());
}
void ProtocolClient::stopStream() { sendPacket(StopStream); assemblies_.clear(); }
void ProtocolClient::ptz(quint32 channel, const QString& command, int speed) {
    const QByteArray value = QByteArray::number(channel) + " " + command.toUtf8() + " " +
        QByteArray::number(speed) + " " + QByteArray::number(QDateTime::currentSecsSinceEpoch());
    sendPacket(PtzControl, value);
}
void ProtocolClient::startRecording(quint32 id, quint32 channel, const QString& url) {
    sendPacket(StartRecording, QByteArray::number(id) + " " + QByteArray::number(channel) +
               " " + url.toUtf8());
}
void ProtocolClient::stopRecording(quint32 id, quint32 channel) {
    sendPacket(StopRecording, QByteArray::number(id) + " " + QByteArray::number(channel));
}
void ProtocolClient::requestPlayback(quint32 id, quint32 channel, qint64 begin, qint64 end) {
    sendPacket(GetPlaybackUrl, QByteArray::number(id) + " " + QByteArray::number(channel) +
               " " + QByteArray::number(begin) + " " + QByteArray::number(end));
}
void ProtocolClient::sendPacket(quint32 type, const QByteArray& value) {
    if (socket_.state() != QAbstractSocket::ConnectedState) {
        emit protocolError(tr("尚未连接服务器")); return;
    }
    QByteArray wire; wire.reserve(8 + value.size()); append32(wire, type);
    append32(wire, quint32(value.size())); wire.append(value); socket_.write(wire);
}
void ProtocolClient::readAvailable() {
    receiveBuffer_.append(socket_.readAll());
    while (receiveBuffer_.size() >= 8) {
        int at = 0; const quint32 type = read32(receiveBuffer_, at);
        const quint32 length = read32(receiveBuffer_, at);
        if (length > 16U * 1024U * 1024U) {
            receiveBuffer_.clear(); emit protocolError(tr("服务器消息超过 16 MiB 限制")); return;
        }
        if (receiveBuffer_.size() < 8 + int(length)) return;
        const QByteArray value = receiveBuffer_.mid(8, int(length));
        receiveBuffer_.remove(0, 8 + int(length)); dispatch(type, value);
    }
}
void ProtocolClient::dispatch(quint32 type, const QByteArray& value) {
    try {
        if ((type == RegisterSection1Ok && authOperation_ == Registering) ||
            (type == LoginSection1Ok && authOperation_ == LoggingIn)) {
            const quint32 next = authOperation_ == Registering ? RegisterSection2 : LoginSection2;
            sendPacket(next, passwordDigest(value, pendingPassword_)); return;
        }
        if (type == RegisterSection2Ok) { authOperation_ = NoAuth; pendingPassword_.clear();
            emit registrationFinished(); emit message(tr("注册成功")); return; }
        if (type == LoginSection2Ok) { authOperation_ = NoAuth; pendingPassword_.clear();
            emit authenticated(); emit message(tr("登录成功")); requestCameras(); return; }
        if (type == Cameras) { emit camerasReceived(decodeCameras(value)); return; }
        if (type == StreamMetadata) { emit videoMetadata(value); return; }
        if (type == StreamPacket) { consumeFragment(value); return; }
        if (type == PlaybackUrl) { emit playbackUrlReceived(QUrl(QString::fromUtf8(value))); return; }
        if (type == Error || type == LoginSection1Error || type == LoginSection2Error ||
            type == RegisterSection1Error || type == RegisterSection2Error) {
            emit protocolError(QString::fromUtf8(value)); return;
        }
        emit message(QString::fromUtf8(value));
    } catch (const std::exception& error) { emit protocolError(QString::fromUtf8(error.what())); }
}
void ProtocolClient::consumeFragment(const QByteArray& value) {
    int at = 0; const quint32 id = read32(value, at); const quint16 index = read16(value, at);
    const quint16 count = read16(value, at); const quint32 length = read32(value, at);
    if (!count || index >= count || length != quint32(value.size() - at))
        throw std::runtime_error("invalid packet fragment");
    if (index == 0) assemblies_[id] = Assembly{count, 0, QByteArray()};
    if (!assemblies_.contains(id)) throw std::runtime_error("missing first packet fragment");
    Assembly& assembly = assemblies_[id];
    if (assembly.count != count || assembly.next != index) {
        assemblies_.remove(id); throw std::runtime_error("out-of-order packet fragment");
    }
    assembly.data.append(value.constData() + at, int(length)); ++assembly.next;
    if (assembly.data.size() > 16 * 1024 * 1024) {
        assemblies_.remove(id); throw std::runtime_error("video packet exceeds size limit");
    }
    if (assembly.next == assembly.count) {
        const QByteArray packet = assembly.data; assemblies_.remove(id);
        emit compressedVideoPacket(packet);
    }
}
QVector<CameraDeviceDto> ProtocolClient::decodeCameras(const QByteArray& value) const {
    int at = 0; const quint32 count = read32(value, at);
    if (count > 100000) throw std::runtime_error("too many cameras");
    QVector<CameraDeviceDto> output; output.reserve(int(count));
    for (quint32 i = 0; i < count; ++i) {
        CameraDeviceDto camera; camera.id = read32(value, at); camera.type = read32(value, at);
        camera.channels = read32(value, at); camera.serialNumber = readText(value, at);
        camera.ip = readText(value, at); camera.rtspUrl = readText(value, at);
        camera.rtmpUrl = readText(value, at); output.append(camera);
    }
    if (at != value.size()) throw std::runtime_error("trailing camera data");
    return output;
}
QByteArray ProtocolClient::encodeCamera(const CameraDeviceDto& camera) const {
    QByteArray out; append32(out, 1); append32(out, camera.id); append32(out, camera.type);
    append32(out, camera.channels); appendText(out, camera.serialNumber); appendText(out, camera.ip);
    appendText(out, camera.rtspUrl); appendText(out, camera.rtmpUrl); return out;
}
QByteArray ProtocolClient::passwordDigest(const QByteArray& setting, const QString& password) {
    return QCryptographicHash::hash(setting + password.toUtf8(), QCryptographicHash::Md5).toHex();
}
void ProtocolClient::socketError(QAbstractSocket::SocketError) {
    emit protocolError(socket_.errorString());
}
