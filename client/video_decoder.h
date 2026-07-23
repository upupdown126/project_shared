#ifndef SMART_HOME_CLIENT_VIDEO_DECODER_H
#define SMART_HOME_CLIENT_VIDEO_DECODER_H

#include <QByteArray>
#include <QImage>
#include <QObject>

class VideoDecoder : public QObject {
    Q_OBJECT
public:
    explicit VideoDecoder(QObject* parent = 0);
    ~VideoDecoder();
public slots:
    void configure(const QByteArray& metadata);
    void decode(const QByteArray& packet);
    void reset();
signals:
    void frameReady(const QImage& image);
    void decoderError(const QString& message);
private:
    struct Private;
    Private* d_;
};

#endif
