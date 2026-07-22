set(CMAKE_AUTOMOC ON)
add_executable(SmartHomeMonitorClient WIN32
    main.cpp
    main_window.cpp
    protocol_client.cpp
    video_decoder.cpp
    client_theme.h
    main_window.h
    protocol_client.h
    video_decoder.h
)
target_link_libraries(SmartHomeMonitorClient PRIVATE
    Qt5::Widgets Qt5::Network Qt5::Multimedia Qt5::MultimediaWidgets
)
if(FFMPEG_FOUND AND FFMPEG_SWSCALE_FOUND)
    target_link_libraries(SmartHomeMonitorClient PRIVATE PkgConfig::FFMPEG PkgConfig::FFMPEG_SWSCALE)
    target_compile_definitions(SmartHomeMonitorClient PRIVATE SMART_CLIENT_HAS_FFMPEG=1)
endif()
target_include_directories(SmartHomeMonitorClient PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
