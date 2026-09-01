#pragma once

class ScopedEnv {
public:
    ScopedEnv(std::string name, const char* value);

    ~ScopedEnv();

    ScopedEnv(const ScopedEnv&) = delete;
    ScopedEnv& operator=(const ScopedEnv&) = delete;

private:
    static void apply(const char* name, const char* value);

    std::string                m_name;
    std::optional<std::string> m_saved;
};

