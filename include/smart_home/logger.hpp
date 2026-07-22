#ifndef SMART_HOME_LOGGER_HPP
#define SMART_HOME_LOGGER_HPP

#include <fstream>
#include <mutex>
#include <string>

namespace smart_home {

class Logger {
public:
    enum Level { Info, Warning, Error };

    static Logger& instance();
    void open(const std::string& file_path);
    void log(Level level, const std::string& message);
    void info(const std::string& message);
    void warning(const std::string& message);
    void error(const std::string& message);

private:
    Logger();
    Logger(const Logger&);
    Logger& operator=(const Logger&);

    mutable std::mutex mutex_;
    std::ofstream output_;
};

}  // namespace smart_home

#endif

