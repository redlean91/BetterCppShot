#pragma once

#include <windows.h>
#include <gdiplus.h>
#include <string>
#include <vector>
#include <chrono>

namespace CppShot {
    std::string getRegistry(const char* pszValueName, const char* defaultValue);
    int getRegistryInt(const char* pszValueName, const int defaultValue);
    std::string getSaveDirectory();
    std::string changeRegistry(const char* pszValueName, const std::string& newValue);
    int changeRegistryInt(const char* pszValueName, const int newValue);
    std::string HotkeyToString(UINT modifiers, UINT vk);
    void saveHotkey(const char* name, UINT modifiers, UINT vk);
    std::pair<UINT, UINT> loadHotkey(const char* name, UINT defaultModifiers, UINT defaultVk);
    const char* statusString(const Gdiplus::Status status);
    RECT getDesktopRect();
    RECT getCaptureRect(HWND window);
    BOOL CALLBACK getMonitorRectsCallback(HMONITOR unnamedParam1, HDC unnamedParam2, LPRECT unnamedParam3, LPARAM unnamedParam4);
    std::vector<RECT> getMonitorRects();
    unsigned int getDPIForWindow(HWND window);

    inline unsigned __int64 currentTimestamp() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    }
}