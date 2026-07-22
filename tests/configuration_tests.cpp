#include "smart_home/configuration.hpp"

#include <cstdio>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

int failures = 0;

void check(bool condition, const std::string& message) {
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << std::endl;
    }
}

bool load_fails(const std::string& path) {
    try {
        smart_home::Configuration::instance().load(path);
        return false;
    } catch (const std::runtime_error&) {
        return true;
    }
}

}  // namespace

int main() {
    const std::string valid_path = "configuration_valid_test.conf";
    {
        std::ofstream output(valid_path.c_str());
        output << "# comment\n"
               << "ip 127.0.0.1 # inline comment\n"
               << "port 8000\n"
               << "thread_num 4\n";
    }

    smart_home::Configuration& config = smart_home::Configuration::instance();
    config.load(valid_path);
    check(config.get("ip") == "127.0.0.1", "reads string values");
    check(config.get_int("port", 1, 65535) == 8000, "reads bounded integers");
    check(config.get_or("missing", "fallback") == "fallback", "supports defaults");
    check(config.contains("thread_num"), "reports existing keys");

    bool missing_failed = false;
    try {
        config.get("not_present");
    } catch (const std::runtime_error&) {
        missing_failed = true;
    }
    check(missing_failed, "rejects missing required keys");

    const std::string duplicate_path = "configuration_duplicate_test.conf";
    {
        std::ofstream output(duplicate_path.c_str());
        output << "port 8000\nport 9000\n";
    }
    check(load_fails(duplicate_path), "rejects duplicate keys");

    const std::string malformed_path = "configuration_malformed_test.conf";
    {
        std::ofstream output(malformed_path.c_str());
        output << "port 8000 unexpected\n";
    }
    check(load_fails(malformed_path), "rejects malformed lines");

    std::remove(valid_path.c_str());
    std::remove(duplicate_path.c_str());
    std::remove(malformed_path.c_str());

    if (failures == 0) {
        std::cout << "all configuration tests passed" << std::endl;
    }
    return failures == 0 ? 0 : 1;
}

