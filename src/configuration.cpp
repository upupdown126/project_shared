#include "smart_home/configuration.hpp"

#include <cerrno>
#include <climits>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace smart_home {

Configuration::Configuration() {}

Configuration& Configuration::instance() {
    static Configuration configuration;
    return configuration;
}

std::string Configuration::trim(const std::string& value) {
    const std::string whitespace(" \t\r\n");
    const std::string::size_type first = value.find_first_not_of(whitespace);
    if (first == std::string::npos) {
        return std::string();
    }
    const std::string::size_type last = value.find_last_not_of(whitespace);
    return value.substr(first, last - first + 1);
}

void Configuration::load(const std::string& file_path) {
    std::ifstream input(file_path.c_str());
    if (!input) {
        throw std::runtime_error("cannot open configuration file: " + file_path);
    }

    std::map<std::string, std::string> parsed;
    std::string line;
    std::size_t line_number = 0;
    while (std::getline(input, line)) {
        ++line_number;
        const std::string::size_type comment = line.find('#');
        if (comment != std::string::npos) {
            line.erase(comment);
        }
        line = trim(line);
        if (line.empty()) {
            continue;
        }

        std::istringstream fields(line);
        std::string key;
        std::string value;
        std::string extra;
        if (!(fields >> key >> value) || (fields >> extra)) {
            std::ostringstream message;
            message << "invalid configuration at line " << line_number;
            throw std::runtime_error(message.str());
        }
        if (parsed.find(key) != parsed.end()) {
            throw std::runtime_error("duplicate configuration key: " + key);
        }
        parsed[key] = value;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    values_.swap(parsed);
    source_path_ = file_path;
}

std::string Configuration::get(const std::string& key) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const std::map<std::string, std::string>::const_iterator found = values_.find(key);
    if (found == values_.end()) {
        throw std::runtime_error("missing configuration key: " + key);
    }
    return found->second;
}

std::string Configuration::get_or(const std::string& key, const std::string& fallback) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const std::map<std::string, std::string>::const_iterator found = values_.find(key);
    return found == values_.end() ? fallback : found->second;
}

int Configuration::get_int(const std::string& key, int minimum, int maximum) const {
    const std::string text = get(key);
    errno = 0;
    char* end = NULL;
    const long value = std::strtol(text.c_str(), &end, 10);
    if (errno == ERANGE || end == text.c_str() || *end != '\0' ||
        value < minimum || value > maximum || value < INT_MIN || value > INT_MAX) {
        std::ostringstream message;
        message << "configuration key '" << key << "' must be an integer in ["
                << minimum << ", " << maximum << "]";
        throw std::runtime_error(message.str());
    }
    return static_cast<int>(value);
}

bool Configuration::contains(const std::string& key) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return values_.find(key) != values_.end();
}

std::map<std::string, std::string> Configuration::snapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return values_;
}

std::string Configuration::source_path() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return source_path_;
}

}  // namespace smart_home

