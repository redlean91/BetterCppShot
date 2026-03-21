#pragma once

#include <windows.h>
#include <gdiplus.h>
#include <string>
#include <vector>
#include <chrono>

namespace CppShot {
    std::wstring getRegistry(LPCTSTR pszValueName, LPCTSTR defaultValue);
    std::wstring getSaveDirectory();
    std::wstring changeRegistry(LPCTSTR pszValueName, const std::wstring& newValue);
    std::wstring HotkeyToString(UINT modifiers, UINT vk);
    void saveHotkey(LPCTSTR name, UINT modifiers, UINT vk);
    std::pair<UINT, UINT> loadHotkey(LPCTSTR name, UINT defaultModifiers, UINT defaultVk);
    const wchar_t* statusString(const Gdiplus::Status status);
    RECT getDesktopRect();
    RECT getCaptureRect(HWND window);
    BOOL CALLBACK getMonitorRectsCallback(HMONITOR unnamedParam1, HDC unnamedParam2, LPRECT unnamedParam3, LPARAM unnamedParam4);
    std::vector<RECT> getMonitorRects();
    unsigned int getDPIForWindow(HWND window);

    inline unsigned __int64 currentTimestamp() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    }
}