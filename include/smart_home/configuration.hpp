#ifndef SMART_HOME_CONFIGURATION_HPP
#define SMART_HOME_CONFIGURATION_HPP

#include <map>
#include <mutex>
#include <string>

namespace smart_home {

class Configuration {
public:
    static Configuration& instance();

    void load(const std::string& file_path);
    std::string get(const std::string& key) const;
    std::string get_or(const std::string& key, const std::string& fallback) const;
    int get_int(const std::string& key, int minimum, int maximum) const;
    bool contains(const std::string& key) const;
    std::map<std::string, std::string> snapshot() const;
    std::string source_path() const;

private:
    Configuration();
    Configuration(const Configuration&);
    Configuration& operator=(const Configuration&);

    static std::string trim(const std::string& value);

    mutable std::mutex mutex_;
    std::map<std::string, std::string> values_;
    std::string source_path_;
};

}  // namespace smart_home

#endif

