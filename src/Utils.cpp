#include "Utils.h"
#include "managers/Application.h"

#include <iostream>
#include <tchar.h>
#include <string>
#include <windows.h>


std::wstring CppShot::HotkeyToString(UINT modifiers, UINT vk) {
    std::wstring result = L"";

    if (modifiers & MOD_CONTROL) result += L"CTRL+";
    if (modifiers & MOD_ALT)     result += L"ALT+";
    if (modifiers & MOD_SHIFT)   result += L"SHIFT+";
    if (modifiers & MOD_WIN)     result += L"WIN+";

    if (vk >= 0x41 && vk <= 0x5A) { result += (wchar_t)vk; return result; }
    if (vk >= 0x30 && vk <= 0x39) { result += (wchar_t)vk; return result; }

    if (vk >= VK_F1 && vk <= VK_F24) {
        result += L"F" + std::to_wstring(vk - VK_F1 + 1);
        return result;
    }

    if (vk >= VK_NUMPAD0 && vk <= VK_NUMPAD9) {
        result += L"NUM" + std::to_wstring(vk - VK_NUMPAD0);
        return result;
    }

    switch (vk) {
        case VK_SPACE:      result += L"SPACE";     break;
        case VK_RETURN:     result += L"ENTER";     break;
        case VK_ESCAPE:     result += L"ESC";       break;
        case VK_TAB:        result += L"TAB";       break;
        case VK_BACK:       result += L"BACKSPACE"; break;
        case VK_DELETE:     result += L"DELETE";    break;
        case VK_INSERT:     result += L"INSERT";    break;
        case VK_HOME:       result += L"HOME";      break;
        case VK_END:        result += L"END";       break;
        case VK_PRIOR:      result += L"PAGE UP";   break;
        case VK_NEXT:       result += L"PAGE DOWN"; break;
        case VK_LEFT:       result += L"LEFT";      break;
        case VK_RIGHT:      result += L"RIGHT";     break;
        case VK_UP:         result += L"UP";        break;
        case VK_DOWN:       result += L"DOWN";      break;
        case VK_PRINT:      result += L"PRINT";     break;
        case VK_SNAPSHOT:   result += L"PRTSC";     break;
        case VK_PAUSE:      result += L"PAUSE";     break;
        case VK_CAPITAL:    result += L"CAPS";      break;
        case VK_NUMLOCK:    result += L"NUMLOCK";   break;
        case VK_SCROLL:     result += L"SCROLL";    break;
        case VK_MULTIPLY:   result += L"NUM*";      break;
        case VK_ADD:        result += L"NUM+";      break;
        case VK_SUBTRACT:   result += L"NUM-";      break;
        case VK_DIVIDE:     result += L"NUM/";      break;
        case VK_DECIMAL:    result += L"NUM.";      break;
        case VK_OEM_1:      result += L";";         break;
        case VK_OEM_2:      result += L"/";         break;
        case VK_OEM_3:      result += L"`";         break;
        case VK_OEM_4:      result += L"[";         break;
        case VK_OEM_5:      result += L"\\";        break;
        case VK_OEM_6:      result += L"]";         break;
        case VK_OEM_7:      result += L"'";         break;
        case VK_OEM_PLUS:   result += L"=";         break;
        case VK_OEM_MINUS:  result += L"-";         break;
        case VK_OEM_COMMA:  result += L",";         break;
        case VK_OEM_PERIOD: result += L".";         break;
        default:
            wchar_t buf[8];
            swprintf(buf, 8, L"0x%02X", vk);
            result += buf;
            break;
    }

    return result;
}

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

void CppShot::saveHotkey(LPCTSTR name, UINT modifiers, UINT vk) {
    HKEY hKey = NULL;
    LPCTSTR pszSubkey = _T("SOFTWARE\\CppShot");
    DWORD dwDisposition;

    if (RegCreateKeyEx(HKEY_CURRENT_USER, pszSubkey, 0, NULL,
        REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, NULL, &hKey, &dwDisposition) != ERROR_SUCCESS)
        return;

    std::wstring modKey = std::wstring(name) + L"_mod";
    std::wstring vkKey  = std::wstring(name) + L"_vk";

    DWORD mod = (DWORD)modifiers;
    DWORD key = (DWORD)vk;

    RegSetValueEx(hKey, modKey.c_str(), 0, REG_DWORD, (const BYTE*)&mod, sizeof(DWORD));
    RegSetValueEx(hKey, vkKey.c_str(),  0, REG_DWORD, (const BYTE*)&key, sizeof(DWORD));

    RegCloseKey(hKey);
}

std::pair<UINT, UINT> CppShot::loadHotkey(LPCTSTR name, UINT defaultModifiers, UINT defaultVk) {
    HKEY hKey = NULL;
    LPCTSTR pszSubkey = _T("SOFTWARE\\CppShot");

    if (RegCreateKeyEx(HKEY_CURRENT_USER, pszSubkey, 0, NULL,
        REG_OPTION_NON_VOLATILE, KEY_READ, NULL, &hKey, NULL) != ERROR_SUCCESS)
        return { defaultModifiers, defaultVk };

    std::wstring modKey = std::wstring(name) + L"_mod";
    std::wstring vkKey  = std::wstring(name) + L"_vk";

    DWORD mod = 0, key = 0;
    DWORD size = sizeof(DWORD);

    bool modOk = RegQueryValueEx(hKey, modKey.c_str(), NULL, NULL, (LPBYTE)&mod, &size) == ERROR_SUCCESS;
    bool vkOk  = RegQueryValueEx(hKey, vkKey.c_str(),  NULL, NULL, (LPBYTE)&key, &size) == ERROR_SUCCESS;

    RegCloseKey(hKey);

    if (!modOk || !vkOk)
        return { defaultModifiers, defaultVk };

    return { (UINT)mod, (UINT)key };
}