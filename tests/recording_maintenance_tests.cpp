#include "smart_home/md5.hpp"
#include "smart_home/recording_maintenance.hpp"

#include <fstream>
#include <iostream>

static smart_home::RecordingSegment add(smart_home::InMemoryRecordingRepository& repository,
    const std::string& path, std::int64_t start, std::int64_t end, const std::string& data) {
    std::ofstream output(path.c_str(), std::ios::binary); output << data; output.close();
    smart_home::RecordingSegment segment = {0, 1, 0, path, start, end,
        static_cast<std::uint64_t>(data.size()), smart_home::md5_hex(data), false};
    repository.insert(segment); return segment;
}

int main() {
    smart_home::MountedStorage storage("recording_maintenance_test_storage");
    smart_home::InMemoryRecordingRepository repository;
    const std::int64_t now = 200000000;
    const std::string oldPath = storage.prepare_segment_path(1, 0, now - 3 * 86400000LL, 1);
    const std::string newPath = storage.prepare_segment_path(1, 0, now, 2);
    add(repository, oldPath, now - 3 * 86400000LL, now - 3 * 86400000LL + 1000, "old");
    smart_home::RecordingSegment recent = add(repository, newPath, now - 1000, now, "recent");
    smart_home::RecordingMaintenance maintenance(repository, storage, 1, 0);
    smart_home::MaintenanceReport first = maintenance.run_once(now);
    if (first.deleted != 1 || first.verified != 1 || repository.list_all().size() != 1) {
        std::cerr << "FAIL: retention cleanup mismatch" << std::endl; return 1;
    }
    std::ofstream damaged(newPath.c_str(), std::ios::binary | std::ios::trunc); damaged << "changed"; damaged.close();
    smart_home::MaintenanceReport second = maintenance.run_once(now);
    if (second.corrupt != 1) { std::cerr << "FAIL: checksum damage not detected" << std::endl; return 1; }
    storage.remove_file(newPath); repository.erase(recent.id);
    std::cout << "Recording maintenance test passed" << std::endl; return 0;
}
