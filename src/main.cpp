#include "smart_home/configuration.hpp"
#include "smart_home/auth_protocol.hpp"
#include "smart_home/auth_service.hpp"
#include "smart_home/camera_api.hpp"
#include "smart_home/camera_protocol.hpp"
#include "smart_home/camera_device_protocol.hpp"
#include "smart_home/camera_repository.hpp"
#include "smart_home/logger.hpp"
#include "smart_home/mysql_user_repository.hpp"
#include "smart_home/mysql_recording_repository.hpp"
#include "smart_home/mysql_camera_repository.hpp"
#include "smart_home/stream_protocol.hpp"
#include "smart_home/hls_http_server.hpp"
#include "smart_home/playback_protocol.hpp"
#include "smart_home/recording.hpp"
#include "smart_home/recording_manager.hpp"
#include "smart_home/recording_maintenance.hpp"
#include "smart_home/tcp_server.hpp"
#include "smart_home/user_repository.hpp"

#include <iostream>
#include <memory>
#include <stdexcept>

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <server.conf>" << std::endl;
        return 2;
    }

    try {
        smart_home::Configuration& config = smart_home::Configuration::instance();
        config.load(argv[1]);

        const std::string ip = config.get("ip");
        const int port = config.get_int("port", 1, 65535);
        const int thread_count = config.get_int("thread_num", 1, 1024);
        const int task_count = config.get_int("task_num", 1, 10000000);
        config.get("video_path");

        smart_home::Logger& logger = smart_home::Logger::instance();
        logger.open(config.get("log_file"));
        logger.info("configuration loaded; server endpoint=" + ip + ":" +
                    std::to_string(port) + ", workers=" +
                    std::to_string(thread_count) + ", task_limit=" +
                    std::to_string(task_count));
        std::unique_ptr<smart_home::UserRepository> users;
        std::unique_ptr<smart_home::RecordingRepository> recordings;
        std::unique_ptr<smart_home::CameraRepository> camera_devices;
        const std::string database_driver = config.get_or("database_driver", "memory");
        if (database_driver == "memory") {
            users.reset(new smart_home::InMemoryUserRepository());
            recordings.reset(new smart_home::InMemoryRecordingRepository());
            camera_devices.reset(new smart_home::InMemoryCameraRepository());
            logger.warning("using volatile in-memory user repository");
        } else if (database_driver == "mysql") {
#if defined(SMART_HOME_HAS_MYSQL)
            users.reset(new smart_home::MysqlUserRepository(
                config.get("database_host"),
                static_cast<std::uint16_t>(config.get_int("database_port", 1, 65535)),
                config.get("database_user"), config.get("database_password"),
                config.get("database_name")));
            recordings.reset(new smart_home::MysqlRecordingRepository(
                config.get("database_host"),
                static_cast<std::uint16_t>(config.get_int("database_port", 1, 65535)),
                config.get("database_user"), config.get("database_password"),
                config.get("database_name")));
            camera_devices.reset(new smart_home::MysqlCameraRepository(
                config.get("database_host"),
                static_cast<std::uint16_t>(config.get_int("database_port", 1, 65535)),
                config.get("database_user"), config.get("database_password"),
                config.get("database_name")));
#else
            throw std::runtime_error("server was built without MySQL client support");
#endif
        } else {
            throw std::runtime_error("unsupported database_driver: " + database_driver);
        }
        smart_home::AuthService authentication(*users);
        smart_home::AuthProtocolHandler auth_protocol(authentication);
        std::unique_ptr<smart_home::HttpClient> http_client;
#if defined(SMART_HOME_HAS_CURL)
        http_client.reset(new smart_home::CurlHttpClient(
            config.get_int("http_timeout_seconds", 1, 300) * 1000L));
#else
        http_client.reset(new smart_home::SocketHttpClient(
            config.get_int("http_timeout_seconds", 1, 300)));
#endif
        smart_home::CameraApi camera_api(*http_client, config.get("camera_base_url"),
                                         config.get("camera_api_secret"));
        smart_home::CameraProtocolHandler camera_protocol(camera_api);
        smart_home::CameraDeviceProtocolHandler camera_device_protocol(*camera_devices);
        smart_home::MountedStorage recording_storage(config.get("video_path"));
        const std::uint64_t storage_limit = static_cast<std::uint64_t>(
            config.get_int("storage_max_gib", 0, 1000000)) * 1024ULL * 1024ULL * 1024ULL;
        smart_home::RecordingMaintenance recording_maintenance(
            *recordings, recording_storage,
            static_cast<std::uint32_t>(config.get_int("recording_retention_days", 0, 36500)),
            storage_limit);
        recording_maintenance.start(static_cast<std::uint32_t>(
            config.get_int("recording_maintenance_seconds", 1, 86400)));
        smart_home::HlsHttpServer hls_server(*recordings, recording_storage, "/media",
                                              config.get("hls_access_secret"));
        hls_server.start(config.get("hls_ip"),
                         static_cast<std::uint16_t>(config.get_int("hls_port", 1, 65535)));
        smart_home::PlaybackProtocolHandler playback_protocol(
            *recordings, config.get("hls_public_url"), config.get("hls_access_secret"),
            config.get_int("hls_url_ttl_seconds", 1, 86400));
#if defined(SMART_HOME_HAS_FFMPEG)
        smart_home::RecordingManager recording_manager(
            [] { return std::unique_ptr<smart_home::MediaSource>(new smart_home::FfmpegMediaSource()); },
            [] { return std::unique_ptr<smart_home::TsSegmentWriter>(new smart_home::FfmpegTsSegmentWriter()); },
            *recordings, recording_storage,
            config.get_int("recording_segment_seconds", 1, 3600) * 1000LL,
            config.get_int("recording_retry_seconds", 0, 3600) * 1000LL);
        smart_home::StreamProtocolHandler stream_protocol(
            [] { return std::unique_ptr<smart_home::MediaSource>(new smart_home::FfmpegMediaSource()); },
            static_cast<std::size_t>(config.get_int("stream_ring_capacity", 1, 100000)),
            static_cast<std::size_t>(config.get_int("stream_fragment_size", 1024, 1048576)));
#else
        smart_home::RecordingManager recording_manager(
            []() -> std::unique_ptr<smart_home::MediaSource> {
                throw std::runtime_error("server was built without FFmpeg development libraries");
            },
            []() -> std::unique_ptr<smart_home::TsSegmentWriter> {
                throw std::runtime_error("server was built without FFmpeg development libraries");
            }, *recordings, recording_storage,
            config.get_int("recording_segment_seconds", 1, 3600) * 1000LL,
            config.get_int("recording_retry_seconds", 0, 3600) * 1000LL);
        smart_home::StreamProtocolHandler stream_protocol(
            []() -> std::unique_ptr<smart_home::MediaSource> {
                throw std::runtime_error("server was built without FFmpeg development libraries");
            });
#endif
        smart_home::RecordingProtocolHandler recording_protocol(recording_manager);
        smart_home::TcpServer server(static_cast<std::size_t>(thread_count),
                                     static_cast<std::size_t>(task_count));
        server.set_message_handler([&auth_protocol, &camera_protocol, &stream_protocol,
                                    &playback_protocol, &recording_protocol,
                                    &camera_device_protocol](
            const smart_home::TcpServer::ConnectionPtr& connection,
            const smart_home::TlvPacket& packet) {
            if (auth_protocol.handle(connection, packet)) return;
            if (!auth_protocol.is_authenticated(connection)) {
                connection->send(smart_home::TlvPacket(
                    smart_home::TaskType::Error, "authentication required"));
                return;
            }
            if (camera_protocol.handle(connection, packet)) return;
            if (camera_device_protocol.handle(connection, packet)) return;
            if (recording_protocol.handle(connection, packet)) return;
            if (playback_protocol.handle(connection, packet)) return;
            if (!stream_protocol.handle(connection, packet)) {
                connection->send(smart_home::TlvPacket(
                    smart_home::TaskType::Error, "unknown or unavailable task"));
            }
        });
        server.set_close_handler([&auth_protocol, &stream_protocol](
            const smart_home::TcpServer::ConnectionPtr& connection) {
            auth_protocol.connection_closed(connection);
            stream_protocol.connection_closed(connection);
        });
        server.run(ip, static_cast<std::uint16_t>(port));
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "startup failed: " << error.what() << std::endl;
        return 1;
    }
}
