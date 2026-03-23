#include "Utils.h"
#include "managers/Application.h"
#include "images/Screenshot.h"

#include <iostream>
#include <tchar.h>
#include <string>
#include <windows.h>

namespace CppShot {

std::string getSaveDirectory() {
    return Application::get().getSaveDirectory();
}

RECT getDesktopRect() {
    RECT rctDesktop;
    rctDesktop.left   = GetSystemMetrics(76);
    rctDesktop.top    = GetSystemMetrics(77);
    rctDesktop.right  = GetSystemMetrics(78) + rctDesktop.left;
    rctDesktop.bottom = GetSystemMetrics(79) + rctDesktop.top;
    return rctDesktop;
}

RECT getCaptureRect(HWND window) {
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

BOOL CALLBACK getMonitorRectsCallback(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData) {
    std::vector<RECT>* monitors = reinterpret_cast<std::vector<RECT>*>(dwData);
    monitors->push_back(*lprcMonitor);
    return TRUE;
}

std::vector<RECT> getMonitorRects() {
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

unsigned int getDPIForWindow(HWND window) {
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

// Convert a hotkey combination to string representation
std::string HotkeyToString(UINT modifiers, UINT vk) {
    std::string result = "";

    if (modifiers & MOD_CONTROL) result += "CTRL+";
    if (modifiers & MOD_ALT)     result += "ALT+";
    if (modifiers & MOD_SHIFT)   result += "SHIFT+";
    if (modifiers & MOD_WIN)     result += "WIN+";

    if (vk >= 0x41 && vk <= 0x5A) { result += (char)vk; return result; }
    if (vk >= 0x30 && vk <= 0x39) { result += (char)vk; return result; }

    if (vk >= VK_F1 && vk <= VK_F24) {
        result += "F" + std::to_string(vk - VK_F1 + 1);
        return result;
    }

    if (vk >= VK_NUMPAD0 && vk <= VK_NUMPAD9) {
        result += "NUM" + std::to_string(vk - VK_NUMPAD0);
        return result;
    }

    switch (vk) {
        case VK_SPACE:      result += "SPACE";     break;
        case VK_RETURN:     result += "ENTER";     break;
        case VK_ESCAPE:     result += "ESC";       break;
        case VK_TAB:        result += "TAB";       break;
        case VK_BACK:       result += "BACKSPACE"; break;
        case VK_DELETE:     result += "DELETE";    break;
        case VK_INSERT:     result += "INSERT";    break;
        case VK_HOME:       result += "HOME";      break;
        case VK_END:        result += "END";       break;
        case VK_PRIOR:      result += "PAGE UP";   break;
        case VK_NEXT:       result += "PAGE DOWN"; break;
        case VK_LEFT:       result += "LEFT";      break;
        case VK_RIGHT:      result += "RIGHT";     break;
        case VK_UP:         result += "UP";        break;
        case VK_DOWN:       result += "DOWN";      break;
        case VK_PRINT:      result += "PRINT";     break;
        case VK_SNAPSHOT:   result += "PRTSC";     break;
        case VK_PAUSE:      result += "PAUSE";     break;
        case VK_CAPITAL:    result += "CAPS";      break;
        case VK_NUMLOCK:    result += "NUMLOCK";   break;
        case VK_SCROLL:     result += "SCROLL";    break;
        case VK_MULTIPLY:   result += "NUM*";      break;
        case VK_ADD:        result += "NUM+";      break;
        case VK_SUBTRACT:   result += "NUM-";      break;
        case VK_DIVIDE:     result += "NUM/";      break;
        case VK_DECIMAL:    result += "NUM.";      break;
        case VK_OEM_1:      result += ";";         break;
        case VK_OEM_2:      result += "/";         break;
        case VK_OEM_3:      result += "`";         break;
        case VK_OEM_4:      result += "[";         break;
        case VK_OEM_5:      result += "\\";        break;
        case VK_OEM_6:      result += "]";         break;
        case VK_OEM_7:      result += "'";         break;
        case VK_OEM_PLUS:   result += "=";         break;
        case VK_OEM_MINUS:  result += "-";         break;
        case VK_OEM_COMMA:  result += ",";         break;
        case VK_OEM_PERIOD: result += ".";         break;
        default: {
            char buf[8];
            sprintf(buf, "0x%02X", vk);
            result += buf;
            break;
        }
    }

    return result;
}

// Registry read/write functions
std::string getRegistry(const char* pszValueName, const char* defaultValue)
{
    HKEY hKey = NULL;
    const char* pszSubkey = "SOFTWARE\\CppShot";

    DWORD dwDisposition;
    if (RegCreateKeyExA(
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
        return std::string(defaultValue);
    }

    char szValue[1024];
    DWORD cbValueLength = sizeof(szValue);

    if (RegQueryValueExA(
            hKey,
            pszValueName,
            NULL,
            NULL,
            reinterpret_cast<LPBYTE>(&szValue),
            &cbValueLength) != ERROR_SUCCESS)
    {
        RegCloseKey(hKey);
        return std::string(defaultValue);
    }

    RegCloseKey(hKey);
    return std::string(szValue);
}

std::string changeRegistry(const char* pszValueName, const std::string& newValue)
{
    HKEY hKey = NULL;
    const char* pszSubkey = "SOFTWARE\\CppShot";

    DWORD dwDisposition;
    if (RegCreateKeyExA(
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
        return "";
    }

    if (RegSetValueExA(
            hKey,
            pszValueName,
            0,
            REG_SZ,
            reinterpret_cast<const BYTE*>(newValue.c_str()),
            (newValue.length() + 1)) != ERROR_SUCCESS)
    {
        RegCloseKey(hKey);
        return "";
    }

    RegCloseKey(hKey);
    return newValue;
}

// Hotkey save/load functions
void saveHotkey(const char* name, UINT modifiers, UINT vk) {
    changeRegistry((std::string(name) + "_mod").c_str(), std::to_string(modifiers));
    changeRegistry((std::string(name) + "_vk").c_str(), std::to_string(vk));
}

std::pair<UINT, UINT> loadHotkey(const char* name, UINT defaultModifiers, UINT defaultVk) {
    UINT modifiers = defaultModifiers;
    UINT vk = defaultVk;

    try {
        modifiers = static_cast<UINT>(
            std::stoul(getRegistry(
                (std::string(name) + "_mod").c_str(),
                std::to_string(defaultModifiers).c_str() // <-- add .c_str() here
            ))
        );
        vk = static_cast<UINT>(
            std::stoul(getRegistry(
                (std::string(name) + "_vk").c_str(),
                std::to_string(defaultVk).c_str() // <-- add .c_str() here
            ))
        );
    } catch (...) {
        // fallback to defaults
    }

    return {modifiers, vk};
}

// Gdiplus status string helper
const char* statusString(const Gdiplus::Status status) {
    switch (status) {
        case Gdiplus::Ok:                        return "Ok";
        case Gdiplus::GenericError:              return "GenericError";
        case Gdiplus::InvalidParameter:          return "InvalidParameter";
        case Gdiplus::OutOfMemory:               return "OutOfMemory";
        case Gdiplus::ObjectBusy:                return "ObjectBusy";
        case Gdiplus::InsufficientBuffer:        return "InsufficientBuffer";
        case Gdiplus::NotImplemented:            return "NotImplemented";
        case Gdiplus::Win32Error:                return "Win32Error";
        case Gdiplus::Aborted:                   return "Aborted";
        case Gdiplus::FileNotFound:              return "FileNotFound";
        case Gdiplus::ValueOverflow:             return "ValueOverflow";
        case Gdiplus::AccessDenied:              return "AccessDenied";
        case Gdiplus::UnknownImageFormat:        return "UnknownImageFormat";
        case Gdiplus::FontFamilyNotFound:        return "FontFamilyNotFound";
        case Gdiplus::FontStyleNotFound:         return "FontStyleNotFound";
        case Gdiplus::NotTrueTypeFont:           return "NotTrueTypeFont";
        case Gdiplus::UnsupportedGdiplusVersion: return "UnsupportedGdiplusVersion";
        case Gdiplus::GdiplusNotInitialized:     return "GdiplusNotInitialized";
        case Gdiplus::PropertyNotFound:          return "PropertyNotFound";
        case Gdiplus::PropertyNotSupported:      return "PropertyNotSupported";
        default:                                 return "Status Type Not Found.";
    }
}

} // namespace CppShot