#include "Utils.h"
#include "managers/Application.h"

#include <iostream>
#include <tchar.h>

std::wstring CppShot::getRegistry(LPCTSTR pszValueName, LPCTSTR defaultValue)
{
    HKEY hKey = NULL;
    LPCTSTR pszSubkey = _T("SOFTWARE\\CppShot");

    // Use RegCreateKeyEx so it creates the key if it doesn't exist yet
    DWORD dwDisposition;
    if (RegCreateKeyEx(
            HKEY_CURRENT_USER,
            pszSubkey,
            0, NULL,
            REG_OPTION_NON_VOLATILE,
            KEY_READ,
            NULL,
            &hKey,
            &dwDisposition) != ERROR_SUCCESS)
    {
        std::cout << "Unable to open/create registry key" << std::endl;
        return std::wstring(defaultValue);
    }

    TCHAR szValue[1024];
    DWORD cbValueLength = sizeof(szValue);

    if (RegQueryValueEx(
            hKey,
            pszValueName,
            NULL,
            NULL,
            reinterpret_cast<LPBYTE>(&szValue),
            &cbValueLength) != ERROR_SUCCESS)
    {
        std::cout << "Unable to read registry value" << std::endl;
        RegCloseKey(hKey);
        return std::wstring(defaultValue);
    }

    RegCloseKey(hKey);
    return std::wstring(szValue);
}

std::wstring CppShot::changeRegistry(LPCTSTR pszValueName, const std::wstring& newValue)
{
    HKEY hKey = NULL;
    LPCTSTR pszSubkey = _T("SOFTWARE\\CppShot");

    // Use RegCreateKeyEx so it creates the key if it doesn't exist yet
    DWORD dwDisposition;
    if (RegCreateKeyEx(
            HKEY_CURRENT_USER,
            pszSubkey,
            0, NULL,
            REG_OPTION_NON_VOLATILE,
            KEY_SET_VALUE,
            NULL,
            &hKey,
            &dwDisposition) != ERROR_SUCCESS)
    {
        std::cout << "Unable to open/create registry key" << std::endl;
        return L"";
    }

    if (RegSetValueEx(
            hKey,
            pszValueName,
            0,
            REG_SZ,
            reinterpret_cast<const BYTE*>(newValue.c_str()),
            (newValue.length() + 1) * sizeof(wchar_t)) != ERROR_SUCCESS)
    {
        std::cout << "Unable to write registry value" << std::endl;
        RegCloseKey(hKey);
        return L"";
    }

    RegCloseKey(hKey);
    return newValue;
}

std::wstring CppShot::getSaveDirectory() {
    return Application::get().getSaveDirectory();
}

const wchar_t* CppShot::statusString(const Gdiplus::Status status) {
    switch (status) {
        case Gdiplus::Ok:                        return L"Ok";
        case Gdiplus::GenericError:              return L"GenericError";
        case Gdiplus::InvalidParameter:          return L"InvalidParameter";
        case Gdiplus::OutOfMemory:               return L"OutOfMemory";
        case Gdiplus::ObjectBusy:                return L"ObjectBusy";
        case Gdiplus::InsufficientBuffer:        return L"InsufficientBuffer";
        case Gdiplus::NotImplemented:            return L"NotImplemented";
        case Gdiplus::Win32Error:                return L"Win32Error";
        case Gdiplus::Aborted:                   return L"Aborted";
        case Gdiplus::FileNotFound:              return L"FileNotFound";
        case Gdiplus::ValueOverflow:             return L"ValueOverflow";
        case Gdiplus::AccessDenied:              return L"AccessDenied";
        case Gdiplus::UnknownImageFormat:        return L"UnknownImageFormat";
        case Gdiplus::FontFamilyNotFound:        return L"FontFamilyNotFound";
        case Gdiplus::FontStyleNotFound:         return L"FontStyleNotFound";
        case Gdiplus::NotTrueTypeFont:           return L"NotTrueTypeFont";
        case Gdiplus::UnsupportedGdiplusVersion: return L"UnsupportedGdiplusVersion";
        case Gdiplus::GdiplusNotInitialized:     return L"GdiplusNotInitialized";
        case Gdiplus::PropertyNotFound:          return L"PropertyNotFound";
        case Gdiplus::PropertyNotSupported:      return L"PropertyNotSupported";
        default:                                 return L"Status Type Not Found.";
    }
}

RECT CppShot::getDesktopRect() {
    RECT rctDesktop;
    rctDesktop.left   = GetSystemMetrics(76);
    rctDesktop.top    = GetSystemMetrics(77);
    rctDesktop.right  = GetSystemMetrics(78) + rctDesktop.left;
    rctDesktop.bottom = GetSystemMetrics(79) + rctDesktop.top;
    return rctDesktop;
}

RECT CppShot::getCaptureRect(HWND window) {
    RECT rct;
    auto rctDesktop = CppShot::getDesktopRect();

    GetWindowRect(window, &rct);
    int dpi    = CppShot::getDPIForWindow(window);
    int offset = 100 * dpi / 96;

    rct.left   = (rctDesktop.left   < (rct.left   - offset)) ? (rct.left   - offset) : rctDesktop.left;
    rct.right  = (rctDesktop.right  > (rct.right  + offset)) ? (rct.right  + offset) : rctDesktop.right;
    rct.bottom = (rctDesktop.bottom > (rct.bottom + offset)) ? (rct.bottom + offset) : rctDesktop.bottom;
    rct.top    = (rctDesktop.top    < (rct.top    - offset)) ? (rct.top    - offset) : rctDesktop.top;

    return rct;
}

BOOL CALLBACK CppShot::getMonitorRectsCallback(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData) {
    std::vector<RECT>* monitors = reinterpret_cast<std::vector<RECT>*>(dwData);
    monitors->push_back(*lprcMonitor);
    return TRUE;
}

std::vector<RECT> CppShot::getMonitorRects() {
    std::vector<RECT> monitors;

    // EnumDisplayMonitors doesn't exist on Win95 — load it dynamically
    typedef BOOL (WINAPI* PFN_EnumDisplayMonitors)(HDC, LPCRECT, MONITORENUMPROC, LPARAM);
    HMODULE hUser32 = GetModuleHandleA("user32.dll");
    PFN_EnumDisplayMonitors pEnum = NULL;
    if (hUser32)
        pEnum = reinterpret_cast<PFN_EnumDisplayMonitors>(GetProcAddress(hUser32, "EnumDisplayMonitors"));

    if (pEnum) {
        pEnum(NULL, NULL, &CppShot::getMonitorRectsCallback, reinterpret_cast<LPARAM>(&monitors));
    } else {
        // Fallback for Win95: just return the primary monitor rect
        RECT rct = {0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN)};
        monitors.push_back(rct);
    }

    return monitors;
}

unsigned int CppShot::getDPIForWindow(HWND window) {
    // Use GetDpiForWindow if available (Win10+)
    if (HMODULE hUser32 = GetModuleHandleA("user32.dll")) {
        if (auto pGetDpiForWindow = reinterpret_cast<UINT(WINAPI*)(HWND)>(GetProcAddress(hUser32, "GetDpiForWindow"))) {
            return pGetDpiForWindow(window);
        }
    }

    // shcore.dll doesn't exist on Win9x — skip it entirely and fall through

    // GetDeviceCaps fallback — works on all Windows versions
    HDC hdc = GetDC(window);
    int dpi = GetDeviceCaps(hdc, LOGPIXELSX);
    ReleaseDC(window, hdc);
    return dpi;
}