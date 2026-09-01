#pragma once
#include <string>

class SshAccount {
    std::string name_;
    std::string user_;
    std::string host_;
    std::string password_;
public:
    // Getter
    const std::string& name() const;
    const std::string& user() const;
    const std::string& host() const;
    const std::string& password() const;
    
    // Setter
    void setName(const std::string& name);
    void setUser(const std::string& user);
    void setHost(const std::string& host);
    void setPassword(const std::string& password);
};