#include "smart_home/logger.hpp"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <stdexcept>

#if defined(_WIN32)
#include <direct.h>
#define SMART_HOME_LOG_MKDIR(path) _mkdir(path)
#else
#include <sys/stat.h>
#define SMART_HOME_LOG_MKDIR(path) mkdir(path, 0755)
#endif

namespace smart_home {

Logger::Logger() {}

Logger& Logger::instance() {
    static Logger logger;
    return logger;
}

void Logger::open(const std::string& file_path) {
    std::lock_guard<std::mutex> lock(mutex_);
    const std::size_t separator = file_path.find_last_of("/\\");
    if (separator != std::string::npos) {
        const std::string directory = file_path.substr(0, separator);
        std::string current;
        for (std::size_t i = 0; i < directory.size(); ++i) {
            current.push_back(directory[i]);
            if ((directory[i] == '/' || directory[i] == '\\') && current.size() > 1)
                SMART_HOME_LOG_MKDIR(current.c_str());
        }
        if (!current.empty()) SMART_HOME_LOG_MKDIR(current.c_str());
    }
    output_.open(file_path.c_str(), std::ios::out | std::ios::app);
    if (!output_) {
        throw std::runtime_error("cannot open log file: " + file_path);
    }
}

void Logger::log(Level level, const std::string& message) {
    const char* names[] = {"INFO", "WARN", "ERROR"};
    const std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
    const std::time_t timestamp = std::chrono::system_clock::to_time_t(now);
    std::tm local_time;
#if defined(_WIN32)
    localtime_s(&local_time, &timestamp);
#else
    localtime_r(&timestamp, &local_time);
#endif

    std::lock_guard<std::mutex> lock(mutex_);
    std::ostream& stream = output_.is_open() ? static_cast<std::ostream&>(output_)
                                             : static_cast<std::ostream&>(std::cerr);
    stream << std::put_time(&local_time, "%Y-%m-%d %H:%M:%S") << " ["
           << names[level] << "] " << message << std::endl;
}

void Logger::info(const std::string& message) { log(Info, message); }
void Logger::warning(const std::string& message) { log(Warning, message); }
void Logger::error(const std::string& message) { log(Error, message); }

}  // namespace smart_home
