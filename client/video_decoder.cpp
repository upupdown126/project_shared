#include "video_decoder.h"

#if defined(SMART_CLIENT_HAS_FFMPEG)
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/error.h>
#include <libswscale/swscale.h>
}

#include <cstring>
#include <new>
#include <stdexcept>

namespace {
quint32 u32(const QByteArray& data, int& at) {
    if (data.size() - at < 4) throw std::runtime_error("truncated video metadata");
    const uchar* p = reinterpret_cast<const uchar*>(data.constData() + at); at += 4;
    return (quint32(p[0]) << 24) | (quint32(p[1]) << 16) | (quint32(p[2]) << 8) | p[3];
}
quint64 u64(const QByteArray& data, int& at) {
    if (data.size() - at < 8) throw std::runtime_error("truncated video packet");
    quint64 value = 0; for (int i = 0; i < 8; ++i) value = (value << 8) | uchar(data[at++]);
    return value;
}
QString ffError(int code) {
    char text[AV_ERROR_MAX_STRING_SIZE] = {0}; av_strerror(code, text, sizeof(text));
    return QString::fromUtf8(text);
}
}

struct VideoDecoder::Private {
    AVCodecContext* codec;
    AVFrame* frame;
    SwsContext* scaler;
    Private() : codec(0), frame(av_frame_alloc()), scaler(0) {}
};

VideoDecoder::VideoDecoder(QObject* parent) : QObject(parent), d_(new Private) {}
VideoDecoder::~VideoDecoder() { reset(); av_frame_free(&d_->frame); delete d_; }

void VideoDecoder::reset() {
    if (d_->codec) avcodec_free_context(&d_->codec);
    if (d_->scaler) { sws_freeContext(d_->scaler); d_->scaler = 0; }
}

void VideoDecoder::configure(const QByteArray& metadata) {
    try {
        reset(); int at = 0; u32(metadata, at); const quint32 codecId = u32(metadata, at);
        const quint32 width = u32(metadata, at), height = u32(metadata, at);
        u32(metadata, at); u32(metadata, at); const quint32 extraLength = u32(metadata, at);
        if (extraLength > quint32(metadata.size() - at) || at + int(extraLength) != metadata.size())
            throw std::runtime_error("invalid codec extra data");
        const AVCodec* decoder = avcodec_find_decoder(static_cast<AVCodecID>(codecId));
        if (!decoder) throw std::runtime_error("FFmpeg decoder not found");
        d_->codec = avcodec_alloc_context3(decoder);
        if (!d_->codec) throw std::bad_alloc();
        d_->codec->width = int(width); d_->codec->height = int(height);
        if (extraLength) {
            d_->codec->extradata = static_cast<uint8_t*>(av_mallocz(extraLength + AV_INPUT_BUFFER_PADDING_SIZE));
            if (!d_->codec->extradata) throw std::bad_alloc();
            std::memcpy(d_->codec->extradata, metadata.constData() + at, extraLength);
            d_->codec->extradata_size = int(extraLength);
        }
        const int status = avcodec_open2(d_->codec, decoder, 0);
        if (status < 0) throw std::runtime_error(ffError(status).toUtf8().constData());
    } catch (const std::exception& error) {
        reset(); emit decoderError(QString::fromUtf8(error.what()));
    }
}

void VideoDecoder::decode(const QByteArray& wire) {
    try {
        if (!d_->codec) throw std::runtime_error("decoder metadata has not arrived");
        int at = 0; u32(wire, at); u32(wire, at);
        const qint64 pts = static_cast<qint64>(u64(wire, at));
        const qint64 dts = static_cast<qint64>(u64(wire, at));
        const quint32 flags = u32(wire, at), length = u32(wire, at);
        if (length > quint32(wire.size() - at) || at + int(length) != wire.size())
            throw std::runtime_error("invalid compressed packet length");
        AVPacket* packet = av_packet_alloc();
        if (!packet) throw std::bad_alloc();
        int status = av_new_packet(packet, int(length));
        if (status >= 0 && length) std::memcpy(packet->data, wire.constData() + at, length);
        packet->pts = pts; packet->dts = dts; packet->flags = int(flags);
        if (status >= 0) status = avcodec_send_packet(d_->codec, packet);
        av_packet_free(&packet);
        if (status < 0 && status != AVERROR(EAGAIN))
            throw std::runtime_error(ffError(status).toUtf8().constData());
        while ((status = avcodec_receive_frame(d_->codec, d_->frame)) >= 0) {
            d_->scaler = sws_getCachedContext(d_->scaler, d_->frame->width, d_->frame->height,
                static_cast<AVPixelFormat>(d_->frame->format), d_->frame->width, d_->frame->height,
                AV_PIX_FMT_RGB24, SWS_BILINEAR, 0, 0, 0);
            if (!d_->scaler) throw std::runtime_error("could not create RGB converter");
            QImage image(d_->frame->width, d_->frame->height, QImage::Format_RGB888);
            uint8_t* destination[4] = {image.bits(), 0, 0, 0};
            int strides[4] = {image.bytesPerLine(), 0, 0, 0};
            sws_scale(d_->scaler, d_->frame->data, d_->frame->linesize, 0,
                      d_->frame->height, destination, strides);
            emit frameReady(image);
            av_frame_unref(d_->frame);
        }
        if (status != AVERROR(EAGAIN) && status != AVERROR_EOF)
            throw std::runtime_error(ffError(status).toUtf8().constData());
    } catch (const std::exception& error) { emit decoderError(QString::fromUtf8(error.what())); }
}
#else

struct VideoDecoder::Private {};

VideoDecoder::VideoDecoder(QObject* parent) : QObject(parent), d_(new Private) {}
VideoDecoder::~VideoDecoder() { delete d_; }
void VideoDecoder::reset() {}
void VideoDecoder::configure(const QByteArray&) {
    emit decoderError(tr("当前客户端未链接兼容的 FFmpeg SDK，实时解码不可用；HLS 回放仍可使用"));
}
void VideoDecoder::decode(const QByteArray&) {}

#endif
