#include "smart_home/auth_service.hpp"

#include <iostream>
#include <string>

namespace {
int failures = 0;
void check(bool condition, const std::string& message) {
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << std::endl;
    }
}
}

int main() {
    smart_home::InMemoryUserRepository users;
    smart_home::AuthService auth(users);

    const smart_home::AuthResult invalid = auth.begin_registration("bad user");
    check(!invalid.success, "rejects unsafe usernames");

    const smart_home::AuthResult begin = auth.begin_registration("alice");
    check(begin.success && begin.value.size() == 12 && begin.value.substr(0, 3) == "$1$",
          "returns an MD5-crypt compatible random setting");
    check(!auth.begin_registration("alice").success,
          "rejects a concurrent registration for the same username");
    const smart_home::AuthResult second = auth.begin_registration("bob");
    check(second.success && second.value != begin.value, "generates a per-user salt");
    const smart_home::AuthResult cancelled = auth.begin_registration("carol");
    auth.cancel_registration("carol");
    check(cancelled.success && auth.begin_registration("carol").success,
          "releases an abandoned registration");

    check(auth.finish_registration("alice", "client-generated-digest").success,
          "stores a completed two-stage registration");
    check(users.size() == 1, "persists exactly one user");
    check(!auth.begin_registration("alice").success, "rejects duplicate users");

    const smart_home::AuthResult login = auth.begin_login("alice");
    check(login.success && login.value == begin.value, "login returns the stored setting");
    check(!auth.finish_login("alice", "wrong-digest").success,
          "rejects an incorrect encrypted password");
    check(auth.finish_login("alice", "client-generated-digest").success,
          "accepts the matching encrypted password");
    check(!auth.begin_login("unknown").success, "does not reveal unknown account details");

    if (failures == 0) std::cout << "all authentication tests passed" << std::endl;
    return failures == 0 ? 0 : 1;
}
