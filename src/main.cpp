#include <windows.h>
#include <commctrl.h>
#include <tchar.h>
#include <gdiplus.h>
#include <sstream>
#include <iostream>
#include <string>
#include <sys/stat.h>
#include <cstdint>

#include "resources.h"
#include "Utils.h"
#include "images/Screenshot.h"
#include "images/CompositeScreenshot.h"
#include "windows/MainWindow.h"
#include "windows/BackdropWindow.h"

#define ERROR_TITLE "BetterCppShot Error"

const char blackBackdropClassName[] = "BlackBackdropWindow";
const char whiteBackdropClassName[] = "WhiteBackdropWindow";

inline bool FileExists(const std::wstring &name)
{
    return GetFileAttributesW(name.c_str()) != INVALID_FILE_ATTRIBUTES;
}

// made this since the whole program i changed it from utf-8 to widestrign hoping it would work on win98
std::wstring Utf8ToWide(const std::string &utf8)
{
    if (utf8.empty())
        return {};
    int len = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
    std::wstring wide(len - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, (LPWSTR)wide.data(), len);
    return wide;
}

void RemoveIllegalChars(std::string &str)
{
    std::string::iterator it;
    std::string illegalChars = "\\/:?\"<>|*";
    for (it = str.begin(); it < str.end(); ++it)
    {
        bool found = illegalChars.find(*it) != std::string::npos;
        if (found)
            *it = ' ';
    }
}

std::string GetSafeFilenameBase(std::string windowTitle)
{
    RemoveIllegalChars(windowTitle);

    std::string path = CppShot::getSaveDirectory();

    // CreateDirectory with wide string
    CreateDirectoryW(Utf8ToWide(path).c_str(), NULL);

    std::wstring widePath = Utf8ToWide(path);
    std::wstring wideTitle = Utf8ToWide(windowTitle);
    std::wstring fileNameBase;

    unsigned int i = 0;
    do
    {
        std::wstringstream pathbuild;
        pathbuild << widePath << L"\\" << wideTitle << L"_" << i;
        fileNameBase = pathbuild.str();
        i++;
    } while (FileExists(fileNameBase + L"_b1.png") || FileExists(fileNameBase + L"_b2.png"));

    // convertting back to utf8
    int len = WideCharToMultiByte(CP_UTF8, 0, fileNameBase.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string result(len - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, fileNameBase.c_str(), -1, (LPSTR)result.data(), len, nullptr, nullptr);
    return result;
}

static std::wstring GetCurrentWallpaper()
{
    wchar_t buf[MAX_PATH] = {};
    SystemParametersInfoW(SPI_GETDESKWALLPAPER, MAX_PATH, buf, 0);
    return buf;
}

static std::wstring WriteSolidBmp(COLORREF color)
{
    wchar_t tempDir[MAX_PATH];
    GetTempPathW(MAX_PATH, tempDir);

    // file
    std::wstring path = std::wstring(tempDir) + (color == RGB(255, 255, 255) ? L"bcs_white.bmp" : L"bcs_black.bmp");

    // 1x1 bmp, black and white
    // bmp stores pixels as B,G,R.
    BYTE pixel[4] = {GetBValue(color), GetGValue(color), GetRValue(color), 0};

    BITMAPFILEHEADER fh = {};
    BITMAPINFOHEADER ih = {};
    fh.bfType = 0x4D42; // 'BM'
    fh.bfOffBits = sizeof(fh) + sizeof(ih);
    fh.bfSize = fh.bfOffBits + sizeof(pixel);
    ih.biSize = sizeof(ih);
    ih.biWidth = 1;
    ih.biHeight = 1;
    ih.biPlanes = 1;
    ih.biBitCount = 24;
    ih.biSizeImage = sizeof(pixel); // one padded row

    HANDLE hFile = CreateFileW(path.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
        return L"";

    DWORD written;
    WriteFile(hFile, &fh, sizeof(fh), &written, NULL);
    WriteFile(hFile, &ih, sizeof(ih), &written, NULL);
    WriteFile(hFile, pixel, sizeof(pixel), &written, NULL);
    CloseHandle(hFile);
    return path;
}

static void SetWallpaper(const std::wstring &path)
{
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Control Panel\\Desktop", 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS)
    {
        const wchar_t *style = L"2"; // stretchhhh
        const wchar_t *tile = L"0";
        RegSetValueExW(hKey, L"WallpaperStyle", 0, REG_SZ, (const BYTE *)style, (wcslen(style) + 1) * 2);
        RegSetValueExW(hKey, L"TileWallpaper", 0, REG_SZ, (const BYTE *)tile, (wcslen(tile) + 1) * 2);
        RegCloseKey(hKey);
    }
    SystemParametersInfoW(SPI_SETDESKWALLPAPER, 0, (PVOID)path.c_str(), SPIF_UPDATEINIFILE | SPIF_SENDCHANGE);
    // wait for wallpaper
    Sleep(300);
}

// since COM doesn't work on MingW/TDM-GCC toolchain, let's try the plain Win32 way
static void ShellMinimizeAll(bool minimize)
{
    HWND tray = FindWindowA("Shell_TrayWnd", NULL);
    if (tray)
    {
        DWORD_PTR result = 0;
        SendMessageTimeoutA(tray, WM_COMMAND, (WPARAM)(minimize ? 419 : 416), 0, SMTO_ABORTIFHUNG, 1000, &result);
    }
}

// hide/show taskbar
static void SetTaskbarVisible(bool visible)
{
    int cmd = visible ? SW_SHOW : SW_HIDE;

    HWND tray = FindWindowA("Shell_TrayWnd", NULL);
    if (tray)
        ShowWindow(tray, cmd);

    HWND start = FindWindowA("Button", "Start");
    if (start)
        ShowWindow(start, cmd);

    HWND secondary = NULL;
    while ((secondary = FindWindowExA(NULL, secondary, "Shell_SecondaryTrayWnd", NULL)) != NULL)
    {
        ShowWindow(secondary, cmd);
    }
}

static RECT GetPrimaryMonitorRect()
{
    HMONITOR hMon = MonitorFromPoint({0, 0}, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO mi = {};
    mi.cbSize = sizeof(mi);
    if (GetMonitorInfoA(hMon, &mi))
        return mi.rcMonitor;
    // fallback
    RECT r = {0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN)};
    return r;
}

void CaptureDesktopTransparent()
{
    std::wstring originalWallpaper = GetCurrentWallpaper();
    std::wstring whiteBmp = WriteSolidBmp(RGB(255, 255, 255));
    std::wstring blackBmp = WriteSolidBmp(RGB(0, 0, 0));

    if (whiteBmp.empty() || blackBmp.empty())
    {
        MessageBoxA(NULL, "Failed to create temporary wallpaper files.", "BetterCppShot Error", MB_OK | MB_ICONSTOP);
        return;
    }

    RECT monitorRect = GetPrimaryMonitorRect();
    Screenshot whiteShot, blackShot;

    // min all
    ShellMinimizeAll(true);
    Sleep(500); // let minimize animations finish

    // set wallpaper and hid taskbar
    SetWallpaper(whiteBmp);
    SetTaskbarVisible(false);
    Sleep(150);
    whiteShot.captureRect(monitorRect);

    SetWallpaper(blackBmp);
    SetTaskbarVisible(false); // wallpaper change may have revived it
    Sleep(150);
    blackShot.captureRect(monitorRect);

    // restore wallpaper
    if (!originalWallpaper.empty())
        SystemParametersInfoW(SPI_SETDESKWALLPAPER, 0, (PVOID)originalWallpaper.c_str(), SPIF_UPDATEINIFILE | SPIF_SENDCHANGE);
    else
        SystemParametersInfoW(SPI_SETDESKWALLPAPER, 0, (PVOID)L"", SPIF_UPDATEINIFILE | SPIF_SENDCHANGE);

    // taskbar and window
    SetTaskbarVisible(true);
    ShellMinimizeAll(false);

    if (!whiteShot.isCaptured() || !blackShot.isCaptured())
    {
        MessageBoxA(NULL, "Desktop screenshot capture failed.", "BetterCppShot Error", MB_OK | MB_ICONSTOP);
        return;
    }

    auto base = GetSafeFilenameBase("DesktopIcons");

    try
    {
        CompositeScreenshot transparentImage(whiteShot, blackShot, true /* noCrop */);
        transparentImage.save(base + "_b1.png");
    }
    catch (std::runtime_error &e)
    {
        MessageBoxA(NULL, "An error occured while compositing the desktop screenshot.", "BetterCppShot Error", MB_OK | MB_ICONSTOP);
    }
}

void CaptureCompositeScreenshot(HINSTANCE hThisInstance, BackdropWindow &whiteWindow, BackdropWindow &blackWindow, bool creMode, bool captureMask)
{
    HWND desktopWindow = GetDesktopWindow();
    HWND foregroundWindow = GetForegroundWindow();
    HWND taskbar = FindWindowA("Shell_TrayWnd", NULL);
    HWND startButton = FindWindowA("Button", "Start");

    std::pair<Screenshot, Screenshot> shots;
    std::pair<Screenshot, Screenshot> creShots;

    if (foregroundWindow != taskbar && foregroundWindow != startButton)
    {
        ShowWindow(taskbar, 0);
        ShowWindow(startButton, 0);
    }

    whiteWindow.resize(foregroundWindow);
    blackWindow.resize(foregroundWindow);

    SetForegroundWindow(foregroundWindow);

    blackWindow.hide();
    whiteWindow.show();

    whiteWindow.hide();
    blackWindow.show();

    shots.second.capture(foregroundWindow);

    blackWindow.hide();
    whiteWindow.show();

    shots.first.capture(foregroundWindow);

    if (creMode)
    {
        SetForegroundWindow(desktopWindow);
        Sleep(33);
        creShots.first.capture(foregroundWindow);

        whiteWindow.hide();
        blackWindow.show();

        creShots.second.capture(foregroundWindow);
    }

    ShowWindow(taskbar, 1);
    ShowWindow(startButton, 1);

    blackWindow.hide();
    whiteWindow.hide();

    if (!shots.first.isCaptured() || !shots.second.isCaptured())
    {
        MessageBoxA(NULL, "Screenshot is empty, aborting capture.", "BetterCppShot Error", MB_OK | MB_ICONSTOP);
        return;
    }

    wchar_t h[2048];
    GetWindowTextW(foregroundWindow, h, 2048);

    // Convert wchar_t* to std::string
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, h, -1, NULL, 0, NULL, NULL);
    std::string windowTextStr(size_needed - 1, 0); // -1 to remove null terminator
    WideCharToMultiByte(CP_UTF8, 0, h, -1, &windowTextStr[0], size_needed, NULL, NULL);

    auto base = GetSafeFilenameBase(windowTextStr);

    // Detect OS version for shadow handling
    OSVERSIONINFO osvi = {};
    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
    bool isVista = false;
    bool shadowsWereDisabled = false;

    if (GetVersionEx(&osvi))
    {
        isVista = (osvi.dwMajorVersion == 6 && osvi.dwMinorVersion == 0);
    }

    try
    {
        CompositeScreenshot transparentImage(shots.first, shots.second);
        transparentImage.save(base + "_b1.png");

        // Disable shadows for mask capture on all systems to match AeroShotCRE behavior
        if (captureMask)
        {
            if (!isVista)
            {
                BOOL shadowEnabled = FALSE;
                if (SystemParametersInfoA(0x1024, 0, &shadowEnabled, 0) && shadowEnabled)
                {                                                                                         // 0x1024 = SPI_GETDROPSHADOW
                    SystemParametersInfoA(0x1025, 0, (PVOID)FALSE, SPIF_UPDATEINIFILE | SPIF_SENDCHANGE); // 0x1025 = SPI_SETDROPSHADOW
                    shadowsWereDisabled = true;
                    Sleep(100);

                    // Recapture screenshots without shadows for mask generation
                    blackWindow.hide();
                    whiteWindow.show();
                    Sleep(50);
                    whiteWindow.hide();
                    blackWindow.show();
                    Sleep(50);
                    shots.second.capture(foregroundWindow);

                    blackWindow.hide();
                    whiteWindow.show();
                    Sleep(50);
                    shots.first.capture(foregroundWindow);

                    blackWindow.hide();
                    whiteWindow.hide();
                }
            }

            CompositeScreenshot blackOpaqueImage(shots.first, shots.second, transparentImage.getCrop(), true);
            blackOpaqueImage.save(base + "_mask.png");
        }
        // Re-enable shadows if we disabled them
        if (shadowsWereDisabled)
        {
            SystemParametersInfoA(0x1025, 0, (PVOID)TRUE, SPIF_UPDATEINIFILE | SPIF_SENDCHANGE); // 0x1025 = SPI_SETDROPSHADOW
            Sleep(100);
        }

        if (creShots.first.isCaptured() && creShots.second.isCaptured())
        {
            CompositeScreenshot transparentInactiveImage(creShots.first, creShots.second, transparentImage.getCrop());
            transparentInactiveImage.save(base + "_b2.png"); // <- narrow literal
        }
    }
    catch (std::runtime_error &e)
    {
        MessageBoxA(NULL, "An error has occured while capturing the screenshot.", "BetterCppShot Error", MB_OK | MB_ICONSTOP);
        return;
    }
}

static LONG WINAPI exceptionHandler(LPEXCEPTION_POINTERS info)
{
    HWND taskbar = FindWindowA("Shell_TrayWnd", NULL);
    HWND startButton = FindWindowA("Button", "Start");

    ShowWindow(taskbar, 1);
    ShowWindow(startButton, 1);

    char msg[512];
    wsprintfA(msg, "An unhandled exception has occured.\n\nException code: 0x%lx\nException address: 0x%p",
              info->ExceptionRecord->ExceptionCode,
              info->ExceptionRecord->ExceptionAddress);
    MessageBoxA(NULL, msg, "BetterCppShot Error", MB_OK | MB_ICONSTOP);
    return EXCEPTION_CONTINUE_SEARCH;
}

int WINAPI WinMain(HINSTANCE hThisInstance,
                   HINSTANCE hPrevInstance,
                   LPSTR lpszArgument,
                   int nCmdShow)
{
    /*
    AllocConsole();
    freopen("CONOUT$", "w", stdout);
    freopen("CONOUT$", "w", stderr);
    freopen("CONIN$",  "r", stdin);
    */

    SetUnhandledExceptionFilter(exceptionHandler);

    try
    {
        MainWindow window;
        window.show(nCmdShow);

        // actually getting the keybinds
        std::pair<UINT, UINT> hotkey1 = CppShot::loadHotkey("Screenshot", MOD_CONTROL, 0x42);
        std::pair<UINT, UINT> hotkey2 = CppShot::loadHotkey("ScreenshotRegion", MOD_CONTROL | MOD_SHIFT, 0x42);
        std::pair<UINT, UINT> hotkey3 = CppShot::loadHotkey("DesktopTransparent", MOD_CONTROL | MOD_ALT, 0x44); // CTRL+ALT+D default

        UINT mod1 = hotkey1.first, vk1 = hotkey1.second;
        UINT mod2 = hotkey2.first, vk2 = hotkey2.second;
        UINT mod3 = hotkey3.first, vk3 = hotkey3.second;

        std::string hotkey_b1 = CppShot::HotkeyToString(mod1, vk1);
        std::string hotkey_b1_b2 = CppShot::HotkeyToString(mod2, vk2);
        std::string hotkey_desk = CppShot::HotkeyToString(mod3, vk3);

        std::string text_keybind1 = "Unable to register keybind: ";
        text_keybind1 += hotkey_b1;

        std::string text_keybind2 = "Unable to register keybind: ";
        text_keybind2 += hotkey_b1_b2;

        std::string text_keybind3 = "Unable to register keybind: ";
        text_keybind3 += hotkey_desk;

        if (!RegisterHotKey(NULL, 1, mod1, vk1))
            MessageBoxA(NULL, text_keybind1.c_str(), ERROR_TITLE, 0x10);

        if (!RegisterHotKey(NULL, 2, mod2, vk2))
            MessageBoxA(NULL, text_keybind2.c_str(), ERROR_TITLE, 0x10);

        if (!RegisterHotKey(NULL, 3, mod3, vk3))
            MessageBoxA(NULL, text_keybind3.c_str(), ERROR_TITLE, 0x10);

        BackdropWindow whiteWindow(RGB(255, 255, 255), whiteBackdropClassName);
        BackdropWindow blackWindow(RGB(0, 0, 0), blackBackdropClassName);

        Gdiplus::GdiplusStartupInput gpStartupInput;
        ULONG_PTR gpToken;
        if (Gdiplus::GdiplusStartup(&gpToken, &gpStartupInput, NULL) != Gdiplus::Ok)
        {
            MessageBoxA(NULL, "Failed to initialize GDI+.\nPlease install the GDI+ redistributable.", "BetterCppShot Error", MB_OK | MB_ICONSTOP);
            return 1;
        }

        MSG messages = {};
        while (GetMessage(&messages, NULL, 0, 0) > 0)
        {
            if (messages.message == WM_QUIT)
                break;
            if (messages.message == WM_HOTKEY)
            {
                if (messages.wParam == 1) {
                    // Getting the captureMask value again here to ensure it reflects the latest user preference
                    bool captureMask = CppShot::getRegistryInt("CaptureMask", 0) != 0;
                    CaptureCompositeScreenshot(hThisInstance, whiteWindow, blackWindow, false, captureMask);
                }
                else if (messages.wParam == 2) {
                    // Getting the captureMask value again here to ensure it reflects the latest user preference
                    bool captureMask = CppShot::getRegistryInt("CaptureMask", 0) != 0;
                    CaptureCompositeScreenshot(hThisInstance, whiteWindow, blackWindow, true, captureMask);
                }
                else if (messages.wParam == 3) {
                    CaptureDesktopTransparent();
                }
            }
            TranslateMessage(&messages);
            DispatchMessage(&messages);
        }

        UnregisterHotKey(NULL, 1);
        UnregisterHotKey(NULL, 2);
        UnregisterHotKey(NULL, 3);
        Gdiplus::GdiplusShutdown(gpToken);
    }
    catch (std::exception &e)
    {
        MessageBoxA(NULL, e.what(), "Startup Error", MB_OK | MB_ICONSTOP);
        return 1;
    }
    catch (...)
    {
        MessageBoxA(NULL, "Unknown exception at startup", "Startup Error", MB_OK | MB_ICONSTOP);
        return 1;
    }

    ExitProcess(0);
}