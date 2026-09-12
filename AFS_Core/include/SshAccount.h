#pragma once
#include <string>

namespace AFS_Core {

class SshAccount {
    std::string name_;
    std::string user_;
    std::string host_;
    std::string password_;
public:
    const std::string& name() const;
    const std::string& user() const;
    const std::string& host() const;
    const std::string& password() const;

    void set_name(const std::string& name);
    void set_user(const std::string& user);
    void set_host(const std::string& host);
    void set_password(const std::string& password);
};

} // namespace AFS_Core
