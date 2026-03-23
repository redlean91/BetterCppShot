#include <windows.h>
#include <commctrl.h>
#include <tchar.h>
#include <gdiplus.h>
#include <sstream>
#include <iostream>
#include <string>
#include <sys/stat.h>

#include "resources.h"
#include "Utils.h"
#include "images/Screenshot.h"
#include "images/CompositeScreenshot.h"
#include "windows/MainWindow.h"
#include "windows/BackdropWindow.h"

#define ERROR_TITLE "BetterCppShot Error"

const char blackBackdropClassName[] = "BlackBackdropWindow";
const char whiteBackdropClassName[] = "WhiteBackdropWindow";

inline bool FileExists(const std::string& name) {
    return GetFileAttributesA(name.c_str()) != INVALID_FILE_ATTRIBUTES && GetLastError() != ERROR_FILE_NOT_FOUND;
}

void RemoveIllegalChars(std::string& str) {
    std::string::iterator it;
    std::string illegalChars = "\\/:?\"<>|*";
    for (it = str.begin(); it < str.end(); ++it) {
        bool found = illegalChars.find(*it) != std::string::npos;
        if (found) *it = ' ';
    }
}

std::string GetSafeFilenameBase(std::string windowTitle) {
    RemoveIllegalChars(windowTitle);

    std::string path = CppShot::getSaveDirectory();

    // CreateDirectory with wide string
    CreateDirectoryA(path.c_str(), NULL);

    std::stringstream pathbuild;
    std::string fileNameBase;

    unsigned int i = 0;
    do {
        pathbuild.str("");
        pathbuild << path << "\\" << windowTitle << "_" << i;
        fileNameBase = pathbuild.str();
        i++;
    } while (FileExists(fileNameBase + "_b1.png") || FileExists(fileNameBase + "_b2.png"));

    return fileNameBase;
}

void CaptureCompositeScreenshot(HINSTANCE hThisInstance, BackdropWindow& whiteWindow, BackdropWindow& blackWindow, bool creMode) {
    HWND desktopWindow = GetDesktopWindow();
    HWND foregroundWindow = GetForegroundWindow();
    HWND taskbar = FindWindowA("Shell_TrayWnd", NULL);
    HWND startButton = FindWindowA("Button", "Start");

    std::pair<Screenshot, Screenshot> shots;
    std::pair<Screenshot, Screenshot> creShots;

    if (foregroundWindow != taskbar && foregroundWindow != startButton) {
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

    if (creMode) {
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

    if (!shots.first.isCaptured() || !shots.second.isCaptured()) {
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

    try {
        CompositeScreenshot transparentImage(shots.first, shots.second);
        transparentImage.save(base + "_b1.png");

        if (creShots.first.isCaptured() && creShots.second.isCaptured()) {
            CompositeScreenshot transparentInactiveImage(creShots.first, creShots.second, transparentImage.getCrop());
            transparentInactiveImage.save(base + "_b2.png"); // <- narrow literal
        }
    } catch (std::runtime_error& e) {
        MessageBoxA(NULL, "An error has occured while capturing the screenshot.", "BetterCppShot Error", MB_OK | MB_ICONSTOP);
        return;
    }
}

static LONG WINAPI exceptionHandler(LPEXCEPTION_POINTERS info) {
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

    try {
        MainWindow window;
        window.show(nCmdShow);

        // actually getting the keybinds
        std::pair<UINT, UINT> hotkey1 = CppShot::loadHotkey("Screenshot",             MOD_CONTROL, 0x42);
        std::pair<UINT, UINT> hotkey2 = CppShot::loadHotkey("ScreenshotRegion",       MOD_ALT,     0x53);

        UINT mod1 = hotkey1.first,   vk1 = hotkey1.second;
        UINT mod2 = hotkey2.first,   vk2 = hotkey2.second;

        std::string hotkey_b1 = CppShot::HotkeyToString(mod1, vk1);
        std::string hotkey_b1_b2 = CppShot::HotkeyToString(mod2, vk2);

        std::string text_keybind1 = "Unable to register keybind: ";
        text_keybind1 += hotkey_b1;

        std::string text_keybind2 = "Unable to register keybind: ";
        text_keybind2 += hotkey_b1_b2;

        if (!RegisterHotKey(NULL, 1, mod1, vk1))
            MessageBoxA(NULL, text_keybind1.c_str(), ERROR_TITLE, 0x10);

        if (!RegisterHotKey(NULL, 2, mod2, vk2))
            MessageBoxA(NULL, text_keybind2.c_str(), ERROR_TITLE, 0x10);

        BackdropWindow whiteWindow(RGB(255, 255, 255), whiteBackdropClassName);
        BackdropWindow blackWindow(RGB(0, 0, 0), blackBackdropClassName);

        Gdiplus::GdiplusStartupInput gpStartupInput;
        ULONG_PTR gpToken;
        if (Gdiplus::GdiplusStartup(&gpToken, &gpStartupInput, NULL) != Gdiplus::Ok) {
            MessageBoxA(NULL, "Failed to initialize GDI+.\nPlease install the GDI+ redistributable.", "BetterCppShot Error", MB_OK | MB_ICONSTOP);
            return 1;
        }

        MSG messages = {};
        while (GetMessage(&messages, NULL, 0, 0) > 0) {
            if (messages.message == WM_QUIT)
                break;
            if (messages.message == WM_HOTKEY) {
                if (messages.wParam == 1)
                    CaptureCompositeScreenshot(hThisInstance, whiteWindow, blackWindow, false);
                else if (messages.wParam == 2)
                    CaptureCompositeScreenshot(hThisInstance, whiteWindow, blackWindow, true);
            }
            TranslateMessage(&messages);
            DispatchMessage(&messages);
        }

        UnregisterHotKey(NULL, 1);
        UnregisterHotKey(NULL, 2);
        Gdiplus::GdiplusShutdown(gpToken);

    } catch (std::exception& e) {
        MessageBoxA(NULL, e.what(), "Startup Error", MB_OK | MB_ICONSTOP);
        return 1;
    } catch (...) {
        MessageBoxA(NULL, "Unknown exception at startup", "Startup Error", MB_OK | MB_ICONSTOP);
        return 1;
    }

    ExitProcess(0);
}