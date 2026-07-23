#ifndef SMART_HOME_RECORDING_MAINTENANCE_HPP
#define SMART_HOME_RECORDING_MAINTENANCE_HPP

#include "smart_home/recording.hpp"

#include <atomic>
#include <cstdint>
#include <thread>

namespace smart_home {

struct MaintenanceReport {
    std::size_t verified;
    std::size_t corrupt;
    std::size_t deleted;
    std::uint64_t remaining_bytes;
};

class RecordingMaintenance {
public:
    RecordingMaintenance(RecordingRepository& repository, const MountedStorage& storage,
                         std::uint32_t retention_days, std::uint64_t maximum_bytes);
    ~RecordingMaintenance();
    MaintenanceReport run_once(std::int64_t current_time_ms);
    void start(std::uint32_t interval_seconds);
    void stop();
private:
    RecordingRepository& repository_;
    const MountedStorage& storage_;
    std::uint32_t retention_days_;
    std::uint64_t maximum_bytes_;
    std::atomic<bool> stopping_;
    std::thread worker_;
};

}  // namespace smart_home
#endif
