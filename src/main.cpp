#include <windows.h>
#include <commctrl.h>
#include <tchar.h>
#include <gdiplus.h>
#include <sstream>
#include <iostream>
#include <string>
#include <vector>
#include <sys/stat.h>
#include <cstdint>

#include "resources.h"
#include "Utils.h"
#include "images/Screenshot.h"
#include "images/CompositeScreenshot.h"
#include "windows/MainWindow.h"
#include "windows/BackdropWindow.h"

#define ERROR_TITLE "BetterCppShot Error"

#ifndef GWL_HWNDPARENT
#define GWL_HWNDPARENT (-8)
#endif

const char blackBackdropClassName[] = "BlackBackdropWindow";
const char whiteBackdropClassName[] = "WhiteBackdropWindow";
static HWND g_mainWindow = NULL;

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

static std::string GetCurrentWallpaper()
{
    char buf[MAX_PATH] = {};
    SystemParametersInfoA(SPI_GETDESKWALLPAPER, MAX_PATH, buf, 0);
    return buf;
}

static std::string WriteSolidBmp(COLORREF color, int width, int height)
{
    char tempDir[MAX_PATH];
    GetTempPathA(MAX_PATH, tempDir);

    // file
    std::string path = std::string(tempDir) + (color == RGB(255, 255, 255) ? "bcs_white.bmp" : "bcs_black.bmp");

    int rowSize = (width * 3 + 3) & ~3;
    DWORD imageSize = rowSize * height;
    std::vector<BYTE> pixels(imageSize, 0);
    for (int row = 0; row < height; ++row)
    {
        BYTE *pixel = &pixels[row * rowSize];
        for (int column = 0; column < width; ++column)
        {
            pixel[column * 3] = GetBValue(color);
            pixel[column * 3 + 1] = GetGValue(color);
            pixel[column * 3 + 2] = GetRValue(color);
        }
    }

    BITMAPFILEHEADER fh = {};
    BITMAPINFOHEADER ih = {};
    fh.bfType = 0x4D42; // 'BM'
    fh.bfOffBits = sizeof(fh) + sizeof(ih);
    fh.bfSize = fh.bfOffBits + imageSize;
    ih.biSize = sizeof(ih);
    ih.biWidth = width;
    ih.biHeight = height;
    ih.biPlanes = 1;
    ih.biBitCount = 24;
    ih.biSizeImage = imageSize;

    HANDLE hFile = CreateFileA(path.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
        return "";

    DWORD written;
    WriteFile(hFile, &fh, sizeof(fh), &written, NULL);
    WriteFile(hFile, &ih, sizeof(ih), &written, NULL);
    WriteFile(hFile, pixels.data(), imageSize, &written, NULL);
    CloseHandle(hFile);
    return path;
}

static void SetWallpaper(const std::string &path)
{
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, "Control Panel\\Desktop", 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS)
    {
        const char *style = "2"; // stretchhhh
        const char *tile = "0";
        RegSetValueExA(hKey, "WallpaperStyle", 0, REG_SZ, (const BYTE *)style, strlen(style) + 1);
        RegSetValueExA(hKey, "TileWallpaper", 0, REG_SZ, (const BYTE *)tile, strlen(tile) + 1);
        RegCloseKey(hKey);
    }
    SystemParametersInfoA(SPI_SETDESKWALLPAPER, 0, (PVOID)path.c_str(), SPIF_UPDATEINIFILE | SPIF_SENDCHANGE);
    HWND desktop = GetDesktopWindow();
    InvalidateRect(desktop, NULL, TRUE);
    UpdateWindow(desktop);
    HWND progman = FindWindowA("Progman", NULL);
    if (progman)
    {
        InvalidateRect(progman, NULL, TRUE);
        UpdateWindow(progman);
    }
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

static void SetCaptureTaskbarButtonVisible(bool visible)
{
    HWND mainWindow = g_mainWindow;
    if (!mainWindow)
        return;

    LONG exStyle = GetWindowLongA(mainWindow, GWL_EXSTYLE);
    if (visible)
    {
        exStyle &= ~WS_EX_TOOLWINDOW;
        exStyle |= WS_EX_APPWINDOW;
        SetWindowLongPtrA(mainWindow, GWL_HWNDPARENT, (LONG_PTR)NULL);
    }
    else
    {
        exStyle &= ~WS_EX_APPWINDOW;
        exStyle |= WS_EX_TOOLWINDOW;
        SetWindowLongPtrA(mainWindow, GWL_HWNDPARENT, (LONG_PTR)GetDesktopWindow());
    }

    SetWindowLongA(mainWindow, GWL_EXSTYLE, exStyle);
    SetWindowPos(mainWindow, NULL, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
}

static void SetCaptureAppVisible(bool visible)
{
    HWND mainWindow = g_mainWindow;
    if (!mainWindow)
        return;

    ShowWindow(mainWindow, visible ? SW_SHOWNOACTIVATE : SW_HIDE);
    SetWindowPos(mainWindow, NULL, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE |
                     (visible ? SWP_SHOWWINDOW : SWP_HIDEWINDOW));
    UpdateWindow(mainWindow);
}

static bool ActivateWindow(HWND window)
{
    HWND currentForeground = GetForegroundWindow();
    DWORD currentThread = GetCurrentThreadId();
    DWORD foregroundThread = currentForeground ? GetWindowThreadProcessId(currentForeground, NULL) : 0;
    bool attached = foregroundThread != 0 && foregroundThread != currentThread &&
                    AttachThreadInput(currentThread, foregroundThread, TRUE);

    BringWindowToTop(window);
    SetActiveWindow(window);
    SetForegroundWindow(window);
    bool activated = false;
    for (int attempt = 0; attempt < 10 && !activated; ++attempt)
    {
        Sleep(5);
        activated = GetForegroundWindow() == window;
        if (!activated)
            SetForegroundWindow(window);
    }

    if (attached)
        AttachThreadInput(currentThread, foregroundThread, FALSE);

    return activated;
}

void CaptureDesktopTransparent()
{
    std::string originalWallpaper = GetCurrentWallpaper();
    RECT monitorRect = GetPrimaryMonitorRect();
    Screenshot whiteShot, blackShot;
    int monitorWidth = monitorRect.right - monitorRect.left;
    int monitorHeight = monitorRect.bottom - monitorRect.top;
    std::string whiteBmp = WriteSolidBmp(RGB(255, 255, 255), monitorWidth, monitorHeight);
    std::string blackBmp = WriteSolidBmp(RGB(0, 0, 0), monitorWidth, monitorHeight);

    if (whiteBmp.empty() || blackBmp.empty())
    {
        MessageBoxA(NULL, "Failed to create temporary wallpaper files.", "BetterCppShot Error", MB_OK | MB_ICONSTOP);
        return;
    }

    SetCaptureTaskbarButtonVisible(false);
    SetCaptureAppVisible(false);

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
        SetWallpaper(originalWallpaper);
    else
        SetWallpaper("");

    // taskbar and window
    SetTaskbarVisible(true);
    ShellMinimizeAll(false);
    SetCaptureAppVisible(true);
    SetCaptureTaskbarButtonVisible(true);

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

void CaptureTaskbar(BackdropWindow &whiteWindow, BackdropWindow &blackWindow)
{
    HWND taskbar = FindWindowA("Shell_TrayWnd", NULL);
    if (!taskbar)
    {
        MessageBoxA(NULL, "The taskbar could not be found.", "Taskbar Capture Error", MB_OK | MB_ICONSTOP);
        return;
    }

    RECT taskbarRect = {};
    GetWindowRect(taskbar, &taskbarRect);
    if (taskbarRect.right <= taskbarRect.left || taskbarRect.bottom <= taskbarRect.top)
    {
        MessageBoxA(NULL, "The taskbar has an invalid capture rectangle.", "Taskbar Capture Error", MB_OK | MB_ICONSTOP);
        return;
    }

    SetCaptureTaskbarButtonVisible(false);
    SetCaptureAppVisible(false);

    whiteWindow.resize(taskbar);
    blackWindow.resize(taskbar);

    blackWindow.hide();
    whiteWindow.show();
    SetWindowPos(whiteWindow.getWindow(), HWND_BOTTOM, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    SetWindowPos(taskbar, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
    RedrawWindow(taskbar, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
    Screenshot whiteShot;
    whiteShot.captureRect(taskbarRect);

    whiteWindow.hide();
    blackWindow.show();
    SetWindowPos(blackWindow.getWindow(), HWND_BOTTOM, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    SetWindowPos(taskbar, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
    RedrawWindow(taskbar, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
    Screenshot blackShot;
    blackShot.captureRect(taskbarRect);

    blackWindow.hide();
    SetWindowPos(taskbar, HWND_NOTOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    Sleep(100); // Waiting a bit so that it can actually screenshot correcctly
    SetCaptureAppVisible(true);
    SetCaptureTaskbarButtonVisible(true);

    try
    {
        if (!whiteShot.isCaptured() || !blackShot.isCaptured())
            throw std::runtime_error("The taskbar screenshot was not captured.");

        auto base = GetSafeFilenameBase("Taskbar");
        CompositeScreenshot result(whiteShot, blackShot, true /* noCrop */);
        result.save(base + "_b1.png");
    }
    catch (std::runtime_error &e)
    {
        MessageBoxA(NULL, e.what(), "Taskbar Capture Error", MB_OK | MB_ICONSTOP);
    }
};

void CaptureCompositeScreenshot(HINSTANCE hThisInstance, BackdropWindow &whiteWindow, BackdropWindow &blackWindow, bool creMode, bool captureMask)
{
    HWND foregroundWindow = GetForegroundWindow();
    HWND taskbar = FindWindowA("Shell_TrayWnd", NULL);
    HWND startButton = FindWindowA("Button", "Start");

    std::pair<Screenshot, Screenshot> shots;
    std::pair<Screenshot, Screenshot> creShots;

    SetCaptureTaskbarButtonVisible(false);

    HWND captureApp = FindWindowA("MainCreWindow", NULL);
    bool hideCaptureApp = captureApp != foregroundWindow;
    if (hideCaptureApp)
        SetCaptureAppVisible(false);

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
        SetWindowPos(blackWindow.getWindow(), HWND_TOP, -32000, -32000, 1, 1, SWP_SHOWWINDOW);
        if (ActivateWindow(blackWindow.getWindow()))
        {
            RedrawWindow(foregroundWindow, NULL, NULL, RDW_INVALIDATE | RDW_FRAME | RDW_UPDATENOW);
            Sleep(33);
            creShots.first.capture(foregroundWindow);
        }

        whiteWindow.hide();
        blackWindow.resize(foregroundWindow);
        blackWindow.show();

        creShots.second.capture(foregroundWindow);
    }

    ShowWindow(taskbar, 1);
    ShowWindow(startButton, 1);

    blackWindow.hide();
    whiteWindow.hide();
    if (hideCaptureApp)
        SetCaptureAppVisible(true);
    SetCaptureTaskbarButtonVisible(true);

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
        g_mainWindow = window.getWindow();
        window.show(nCmdShow);

        // actually getting the keybinds
        std::pair<UINT, UINT> hotkey1 = CppShot::loadHotkey("Screenshot", MOD_CONTROL, 0x42);
        std::pair<UINT, UINT> hotkey2 = CppShot::loadHotkey("ScreenshotRegion", MOD_CONTROL | MOD_SHIFT, 0x42);
        std::pair<UINT, UINT> hotkey3 = CppShot::loadHotkey("DesktopTransparent", MOD_CONTROL | MOD_ALT, 0x44); // CTRL+ALT+D default
        std::pair<UINT, UINT> hotkey4 = CppShot::loadHotkey("ScreenshotTaskbar", MOD_CONTROL | MOD_ALT, 0x54); // CTRL+ALT+T default

        UINT mod1 = hotkey1.first, vk1 = hotkey1.second;
        UINT mod2 = hotkey2.first, vk2 = hotkey2.second;
        UINT mod3 = hotkey3.first, vk3 = hotkey3.second;
        UINT mod4 = hotkey4.first, vk4 = hotkey4.second;

        std::string hotkey_b1 = CppShot::HotkeyToString(mod1, vk1);
        std::string hotkey_b1_b2 = CppShot::HotkeyToString(mod2, vk2);
        std::string hotkey_desk = CppShot::HotkeyToString(mod3, vk3);
        std::string hotkey_tb = CppShot::HotkeyToString(mod4, vk4);

        std::string text_keybind1 = "Unable to register keybind: ";
        text_keybind1 += hotkey_b1;

        std::string text_keybind2 = "Unable to register keybind: ";
        text_keybind2 += hotkey_b1_b2;

        std::string text_keybind3 = "Unable to register keybind: ";
        text_keybind3 += hotkey_desk;

        std::string text_keybind4 = "Unable to register keybind: ";
        text_keybind4 += hotkey_tb;

        if (!RegisterHotKey(NULL, 1, mod1, vk1))
            MessageBoxA(NULL, text_keybind1.c_str(), ERROR_TITLE, 0x10);

        if (!RegisterHotKey(NULL, 2, mod2, vk2))
            MessageBoxA(NULL, text_keybind2.c_str(), ERROR_TITLE, 0x10);

        if (!RegisterHotKey(NULL, 3, mod3, vk3))
            MessageBoxA(NULL, text_keybind3.c_str(), ERROR_TITLE, 0x10);

        if (!RegisterHotKey(NULL, 4, mod4, vk4))
            MessageBoxA(NULL, text_keybind4.c_str(), ERROR_TITLE, 0x10);

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
                else if (messages.wParam == 4) {
                    CaptureTaskbar(whiteWindow, blackWindow);
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