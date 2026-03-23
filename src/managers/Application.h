#pragma once

#include <windows.h>
#include <string>

class Application
{
    std::string m_saveDirectory;

public:
    static Application &get();
    std::string getSaveDirectory() const;

private:
    Application();
    ~Application() = default;
    Application(const Application &) = default;
    Application &operator=(const Application &) = default;
};