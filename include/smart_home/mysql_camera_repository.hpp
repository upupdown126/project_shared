#ifndef SMART_HOME_MYSQL_CAMERA_REPOSITORY_HPP
#define SMART_HOME_MYSQL_CAMERA_REPOSITORY_HPP

#include "smart_home/camera_repository.hpp"

#include <memory>

namespace smart_home {

#if defined(SMART_HOME_HAS_MYSQL)
class MysqlCameraRepository : public CameraRepository {
public:
    MysqlCameraRepository(const std::string& host, std::uint16_t port,
                          const std::string& username, const std::string& password,
                          const std::string& database);
    ~MysqlCameraRepository();
    bool save(const CameraDevice& camera);
    std::vector<CameraDevice> list();
    bool find(std::uint32_t id, CameraDevice& output);
private:
    struct Implementation;
    std::unique_ptr<Implementation> implementation_;
};
#endif

}  // namespace smart_home
#endif
