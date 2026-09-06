#include "pch.h"
#include "SshAccount.h"

// Getter

namespace AFS_Core {

const std::string& SshAccount::name() const
{
    return name_;
}

const std::string& SshAccount::user() const
{
    return user_;
}

const std::string& SshAccount::host() const
{
    return host_;
}

const std::string& SshAccount::password() const
{
    return password_;
}

// Setter
void SshAccount::setName(const std::string& name)
{
    name_ = name;
}

void SshAccount::setUser(const std::string& user)
{
    user_ = user;
}

void SshAccount::setHost(const std::string& host)
{
    host_ = host;
}

void SshAccount::setPassword(const std::string& password)
{
    password_ = password;
}

} // namespace AFS_Core
