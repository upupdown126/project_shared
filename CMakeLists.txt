cmake_minimum_required(VERSION 3.16)
project(SmartHomeMonitor VERSION 0.1.0 LANGUAGES CXX)

find_package(Threads REQUIRED)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(SMART_HOME_BUILD_TESTS "Build unit tests" ON)
option(SMART_HOME_BUILD_QT_CLIENT "Build Qt desktop client when dependencies are available" ON)
if(WIN32)
    set(SMART_HOME_MYSQL_DEFAULT OFF)
else()
    set(SMART_HOME_MYSQL_DEFAULT ON)
endif()
option(SMART_HOME_WITH_MYSQL "Build the MySQL user repository when client libraries are found"
       ${SMART_HOME_MYSQL_DEFAULT})

add_library(smart_home_core
    src/configuration.cpp
    src/logger.cpp
    src/tlv.cpp
    src/thread_pool.cpp
    src/tcp_server.cpp
    src/auth_service.cpp
    src/user_repository.cpp
    src/auth_protocol.cpp
    src/md5.cpp
    src/camera_api.cpp
    src/socket_http_client.cpp
    src/camera_protocol.cpp
    src/camera_repository.cpp
    src/camera_device_protocol.cpp
    src/media_wire.cpp
    src/media_stream.cpp
    src/stream_protocol.cpp
    src/recording.cpp
    src/segment_recorder.cpp
    src/recording_manager.cpp
    src/recording_maintenance.cpp
    src/hls_http_server.cpp
    src/hls_auth.cpp
    src/playback_protocol.cpp
)
target_include_directories(smart_home_core PUBLIC include)
target_link_libraries(smart_home_core PUBLIC Threads::Threads)
find_package(CURL QUIET)
if(CURL_FOUND)
    target_sources(smart_home_core PRIVATE src/curl_http_client.cpp)
    target_link_libraries(smart_home_core PUBLIC CURL::libcurl)
    target_compile_definitions(smart_home_core PUBLIC SMART_HOME_HAS_CURL=1)
    message(STATUS "libcurl HTTP client enabled")
endif()
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(FFMPEG QUIET IMPORTED_TARGET libavformat libavcodec libavutil)
    if(FFMPEG_FOUND)
        target_sources(smart_home_core PRIVATE src/ffmpeg_media_source.cpp)
        target_sources(smart_home_core PRIVATE src/ffmpeg_ts_writer.cpp)
        target_link_libraries(smart_home_core PUBLIC PkgConfig::FFMPEG)
        target_compile_definitions(smart_home_core PUBLIC SMART_HOME_HAS_FFMPEG=1)
        message(STATUS "FFmpeg RTSP/RTMP media source enabled")
    endif()
endif()

if(SMART_HOME_BUILD_QT_CLIENT)
    find_package(Qt5 5.14 COMPONENTS Widgets Network Multimedia MultimediaWidgets QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(FFMPEG_SWSCALE QUIET IMPORTED_TARGET libswscale)
    endif()
    if(Qt5_FOUND)
        add_subdirectory(client)
        message(STATUS "Qt monitoring client enabled")
    else()
        message(STATUS "Qt 5.14 not found; desktop client target disabled")
    endif()
endif()
if(SMART_HOME_WITH_MYSQL)
    find_path(MYSQL_INCLUDE_DIR mysql.h
        HINTS
            "/usr/include/mysql"
            "C:/Program Files/MySQL/MySQL Server 8.0/include")
    find_library(MYSQL_CLIENT_LIBRARY NAMES mysqlclient libmysql
        HINTS
            "/usr/lib/x86_64-linux-gnu"
            "C:/Program Files/MySQL/MySQL Server 8.0/lib")
    if(MYSQL_INCLUDE_DIR AND MYSQL_CLIENT_LIBRARY)
        target_sources(smart_home_core PRIVATE src/mysql_user_repository.cpp)
        target_sources(smart_home_core PRIVATE src/mysql_recording_repository.cpp)
        target_sources(smart_home_core PRIVATE src/mysql_camera_repository.cpp)
        target_include_directories(smart_home_core PRIVATE "${MYSQL_INCLUDE_DIR}")
        target_link_libraries(smart_home_core PUBLIC "${MYSQL_CLIENT_LIBRARY}")
        target_compile_definitions(smart_home_core PUBLIC SMART_HOME_HAS_MYSQL=1)
        message(STATUS "MySQL user repository enabled")
    else()
        message(STATUS "MySQL client development files not found; adapter disabled")
    endif()
endif()
if(WIN32)
    target_link_libraries(smart_home_core PUBLIC ws2_32)
endif()

add_executable(SmartHomeMonitorServer src/main.cpp)
target_link_libraries(SmartHomeMonitorServer PRIVATE smart_home_core)

if(SMART_HOME_BUILD_TESTS)
    enable_testing()
    add_executable(configuration_tests tests/configuration_tests.cpp)
    target_link_libraries(configuration_tests PRIVATE smart_home_core)
    add_test(NAME configuration_tests COMMAND configuration_tests)

    add_executable(tlv_tests tests/tlv_tests.cpp)
    target_link_libraries(tlv_tests PRIVATE smart_home_core)
    add_test(NAME tlv_tests COMMAND tlv_tests)

    add_executable(thread_pool_tests tests/thread_pool_tests.cpp)
    target_link_libraries(thread_pool_tests PRIVATE smart_home_core)
    add_test(NAME thread_pool_tests COMMAND thread_pool_tests)

    add_executable(tcp_server_tests tests/tcp_server_tests.cpp)
    target_link_libraries(tcp_server_tests PRIVATE smart_home_core)
    add_test(NAME tcp_server_tests COMMAND tcp_server_tests)

    add_executable(auth_service_tests tests/auth_service_tests.cpp)
    target_link_libraries(auth_service_tests PRIVATE smart_home_core)
    add_test(NAME auth_service_tests COMMAND auth_service_tests)

    add_executable(camera_api_tests tests/camera_api_tests.cpp)
    target_link_libraries(camera_api_tests PRIVATE smart_home_core)
    add_test(NAME camera_api_tests COMMAND camera_api_tests)

    add_executable(camera_repository_tests tests/camera_repository_tests.cpp)
    target_link_libraries(camera_repository_tests PRIVATE smart_home_core)
    add_test(NAME camera_repository_tests COMMAND camera_repository_tests)

    add_executable(http_client_tests tests/http_client_tests.cpp)
    target_link_libraries(http_client_tests PRIVATE smart_home_core)
    add_test(NAME http_client_tests COMMAND http_client_tests)

    add_executable(media_wire_tests tests/media_wire_tests.cpp)
    target_link_libraries(media_wire_tests PRIVATE smart_home_core)
    add_test(NAME media_wire_tests COMMAND media_wire_tests)

    add_executable(ring_buffer_tests tests/ring_buffer_tests.cpp)
    target_link_libraries(ring_buffer_tests PRIVATE smart_home_core)
    add_test(NAME ring_buffer_tests COMMAND ring_buffer_tests)

    add_executable(media_stream_tests tests/media_stream_tests.cpp)
    target_link_libraries(media_stream_tests PRIVATE smart_home_core)
    add_test(NAME media_stream_tests COMMAND media_stream_tests)

    add_executable(recording_tests tests/recording_tests.cpp)
    target_link_libraries(recording_tests PRIVATE smart_home_core)
    add_test(NAME recording_tests COMMAND recording_tests)

    add_executable(segment_recorder_tests tests/segment_recorder_tests.cpp)
    target_link_libraries(segment_recorder_tests PRIVATE smart_home_core)
    add_test(NAME segment_recorder_tests COMMAND segment_recorder_tests)

    add_executable(recording_manager_tests tests/recording_manager_tests.cpp)
    target_link_libraries(recording_manager_tests PRIVATE smart_home_core)
    add_test(NAME recording_manager_tests COMMAND recording_manager_tests)

    add_executable(recording_maintenance_tests tests/recording_maintenance_tests.cpp)
    target_link_libraries(recording_maintenance_tests PRIVATE smart_home_core)
    add_test(NAME recording_maintenance_tests COMMAND recording_maintenance_tests)

    add_executable(hls_http_server_tests tests/hls_http_server_tests.cpp)
    target_link_libraries(hls_http_server_tests PRIVATE smart_home_core)
    add_test(NAME hls_http_server_tests COMMAND hls_http_server_tests)
endif()
