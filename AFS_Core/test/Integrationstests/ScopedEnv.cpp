#include "pch.h"
#include "ScopedEnv.h"
#ifdef _WIN32
  #include <windows.h>
#endif

ScopedEnv::ScopedEnv(std::string name, const char* value)   // value == nullptr -> löschen
    : m_name(std::move(name))
{
    if (const char* old = std::getenv(m_name.c_str()))
        m_saved = old;                            // sofort kopieren!
    apply(m_name.c_str(), value);
}

ScopedEnv::~ScopedEnv()
{ 
    apply(m_name.c_str(), m_saved ? m_saved->c_str() : nullptr); 
}

void ScopedEnv::apply(const char* name, const char* value)
{
#ifdef _WIN32
    _putenv_s(name, value ? value : "");   // "" löscht im CRT-Block
    SetEnvironmentVariableA(name, value);  // nullptr löscht im Win32-Block
#else
    if (value) setenv(name, value, 1); else unsetenv(name);
#endif
}