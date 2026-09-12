#include "pch.h"
#include "SshAccount.h"

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

void SshAccount::set_name(const std::string& name)
{
    name_ = name;
}

void SshAccount::set_user(const std::string& user)
{
    user_ = user;
}

void SshAccount::set_host(const std::string& host)
{
    host_ = host;
}

void SshAccount::set_password(const std::string& password)
{
    password_ = password;
}

} // namespace AFS_Core
