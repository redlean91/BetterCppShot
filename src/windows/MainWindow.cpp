#include "MainWindow.h"
#include "../ui/Button.h"
#include "version.h"
#include "../resources.h"
#include <shellapi.h>
#include "../Utils.h"
#include <windows.h>
#include <shlobj.h>
#include <string>

MainWindow::MainWindow() : Window((HBRUSH)(COLOR_BTNFACE + 1), "MainCreWindow", "BetterCppShot", 0, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX) {
    setSize(330, 250);
    this->addButton()
        .setCallback([this]() { onOpenExplorer(); })
        .setPosition(10, 10)
        .setSize(300, 30)
        .setTitle("Open Screenshots Folder");
    this->addButton()
        .setCallback([this]() { onOpenSettings(); })
        .setPosition(10, 50)
        .setSize(300, 30)
        .setTitle("Settings");
    this->addButton()
        .setCallback([this]() { onOpenAbout(); })
        .setPosition(10, 90)
        .setSize(300, 30)
        .setTitle("About");

    // Keybinds
    this->addLabel("Active keybinds:", 10, 125, 300, 20, true);

    std::pair<UINT, UINT> hotkey1 = CppShot::loadHotkey("Screenshot",         MOD_CONTROL,             0x42);
    std::pair<UINT, UINT> hotkey2 = CppShot::loadHotkey("ScreenshotRegion",   MOD_CONTROL | MOD_SHIFT, 0x42);
    std::pair<UINT, UINT> hotkey3 = CppShot::loadHotkey("DesktopTransparent", MOD_CONTROL | MOD_ALT,   0x44);

    UINT mod1 = hotkey1.first, vk1 = hotkey1.second;
    UINT mod2 = hotkey2.first, vk2 = hotkey2.second;
    UINT mod3 = hotkey3.first, vk3 = hotkey3.second;

    std::string hotkey_b1      = CppShot::HotkeyToString(mod1, vk1);
    std::string hotkey_b1_text =    "Active: " + hotkey_b1;
    this->addLabel(hotkey_b1_text.c_str(), 10, 150, 200, 20);

    std::string hotkey_b1_b2      = CppShot::HotkeyToString(mod2, vk2);
    std::string hotkey_b1_b2_text = "Active and inactive: " + hotkey_b1_b2;
    this->addLabel(hotkey_b1_b2_text.c_str(), 10, 170, 300, 20);

    std::string hotkey_desk      = CppShot::HotkeyToString(mod3, vk3);
    std::string hotkey_desk_text =  "Desktop: " + hotkey_desk;
    this->addLabel(hotkey_desk_text.c_str(), 10, 190, 200, 20);
}

void MainWindow::onOpenExplorer() {
    std::string path = CppShot::getSaveDirectory();
    ShellExecuteA(NULL, "open", path.c_str(), NULL, NULL, SW_SHOWNORMAL);
}

void MainWindow::onOpenExplorer_change() {
    BROWSEINFOA bi = {};
    bi.hwndOwner = NULL;
    bi.lpszTitle = "Select a folder for screenshots:";
    bi.ulFlags   = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;

    LPITEMIDLIST pidl = SHBrowseForFolderA(&bi);
    if (!pidl) return;

    char path[MAX_PATH];
    if (SHGetPathFromIDListA(pidl, path)) {
        CppShot::changeRegistry("Path", path);
    }

    IMalloc* imalloc = nullptr;
    if (SUCCEEDED(SHGetMalloc(&imalloc))) {
        imalloc->Free(pidl);
        imalloc->Release();
    }
}

// about stuff

static LRESULT CALLBACK AboutWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_COMMAND:
            if (LOWORD(wParam) == 1) { // OK button
                DestroyWindow(hWnd);
            }
            else if (LOWORD(wParam) == 2) {
                ShellExecuteA(hWnd, "open", "https://github.com/redlean91/BetterCppShot", NULL, NULL, SW_SHOWNORMAL);
            }
            return 0;

        case WM_CLOSE:
            DestroyWindow(hWnd);
            return 0;
    }
    return DefWindowProcA(hWnd, msg, wParam, lParam);
}

void MainWindow::onOpenAbout() {
    const char* className = "AboutWnd";
    double scale = this->getScaleFactor();
    auto S = [scale](int v) { return (int)(v * scale); };

    HINSTANCE instance = GetModuleHandle(NULL);
    HICON hIcon = (HICON) LoadImage(instance, MAKEINTRESOURCE(IDI_APPICON), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE | LR_DEFAULTCOLOR | LR_SHARED);

    WNDCLASSA wc = {};
    wc.lpfnWndProc   = AboutWndProc;
    wc.hInstance     = GetModuleHandle(NULL);
    wc.hIcon         = hIcon;
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = className;
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    RegisterClassA(&wc);

    HWND hDlg = CreateWindowExA(
        WS_EX_DLGMODALFRAME,
        className,
        "About BetterCppShot",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT,
        S(250), S(260),
        this->getWindow(),
        NULL,
        GetModuleHandle(NULL),
        NULL
    );

    // Center over parent
    RECT rcParent, rcDlg;
    GetWindowRect(this->getWindow(), &rcParent);
    GetWindowRect(hDlg, &rcDlg);
    int x = rcParent.left + (rcParent.right - rcParent.left - (rcDlg.right - rcDlg.left)) / 2;
    int y = rcParent.top + (rcParent.bottom - rcParent.top - (rcDlg.bottom - rcDlg.top)) / 2;
    SetWindowPos(hDlg, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);

    CreateWindowA("STATIC", "BetterCppShot",
        WS_CHILD | WS_VISIBLE | SS_CENTER,
        S(10), S(10), S(230), S(20),
        hDlg, NULL, GetModuleHandle(NULL), NULL);

    CreateWindowA("STATIC", PROJECT_VERSION,
        WS_CHILD | WS_VISIBLE | SS_CENTER,
        S(10), S(40), S(230), S(20),
        hDlg, NULL, GetModuleHandle(NULL), NULL);

    CreateWindowA("STATIC", "CppShot by Cvolton",
        WS_CHILD | WS_VISIBLE | SS_CENTER,
        S(10), S(70), S(230), S(20),
        hDlg, NULL, GetModuleHandle(NULL), NULL);

    CreateWindowA("STATIC", "Edited by: Redlean and contributors",
        WS_CHILD | WS_VISIBLE | SS_CENTER,
        S(10), S(110), S(230), S(20),
        hDlg, NULL, GetModuleHandle(NULL), NULL);

    CreateWindowA("BUTTON", "OK",
        WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
        S(85), S(190), S(80), S(30),
        hDlg, (HMENU)1, GetModuleHandle(NULL), NULL);

    CreateWindowA("BUTTON", "GitHub",
        WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
        S(85), S(150), S(80), S(30),
        hDlg, (HMENU)2, GetModuleHandle(NULL), NULL);

    HFONT font = CppShot::createScaledFont(hDlg);
    CppShot::applyFontToChildren(hDlg, font);

    ShowWindow(hDlg, SW_SHOW);
    UpdateWindow(hDlg);

    EnableWindow(this->getWindow(), FALSE);

    MSG msg = {};
    bool closed = false;
    while (!closed && GetMessageA(&msg, NULL, 0, 0)) {
        if (!IsWindow(hDlg)) break;
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
        if (!IsWindow(hDlg)) closed = true;
    }

    EnableWindow(this->getWindow(), TRUE);
    SetActiveWindow(this->getWindow());
    DeleteObject(font);
}

// settings stuff

static bool g_settingsClosed = false;

static LRESULT CALLBACK SettingsWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE:
            {
                CREATESTRUCTA* cs = (CREATESTRUCTA*)lParam;
                MainWindow* mainWnd = (MainWindow*)cs->lpCreateParams;

                SetWindowLongPtrA(hWnd, GWLP_USERDATA, (LONG_PTR)mainWnd);
                return 0;
            }
        case WM_CLOSE:
            DestroyWindow(hWnd);
            return 0;

        case WM_DESTROY:
            g_settingsClosed = true; // IMPORTANT
            return 0;

        case WM_COMMAND:
            {
                int id = LOWORD(wParam);

                MainWindow* mainWnd = (MainWindow*)GetWindowLongPtrA(hWnd, GWLP_USERDATA);

                if (id == 202) { // Close
                    DestroyWindow(hWnd);
                } else if (id == 203) {
                    mainWnd->onOpenExplorer_change();
                } else if (id == 204) {
                    mainWnd->onChangeKeybinds();
                } else if (id == 205) {
                    mainWnd->onChangeDelay();
                } else if (id == 206) {
                    bool checked = (IsDlgButtonChecked(hWnd, 206) == BST_CHECKED);
                    CppShot::changeRegistryInt("CaptureMask", checked ? 1 : 0);
                }

                return 0;
            }
    }
    return DefWindowProcA(hWnd, msg, wParam, lParam);
}

void MainWindow::onOpenSettings() {
    const char* className = "SettingsWnd";
    double scale = this->getScaleFactor();
    auto S = [scale](int v) { return (int)(v * scale); };

    HINSTANCE instance = GetModuleHandle(NULL);
    HICON hIcon = (HICON) LoadImage(instance, MAKEINTRESOURCE(IDI_APPICON), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE | LR_DEFAULTCOLOR | LR_SHARED);

    WNDCLASSA wc = {};
    wc.lpfnWndProc   = SettingsWndProc;
    wc.hIcon = hIcon;
    wc.hInstance     = GetModuleHandle(NULL);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = className;
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    RegisterClassA(&wc);

    HWND hDlg = CreateWindowExA(
        WS_EX_DLGMODALFRAME, className, "Settings",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT,
        S(230), S(220),
        this->getWindow(), NULL, GetModuleHandle(NULL), this
    );

    CreateWindowA("BUTTON", "Change Screenshot Folder", WS_CHILD | WS_VISIBLE,
        S(10), S(10), S(200), S(30), hDlg, (HMENU)203, GetModuleHandle(NULL), NULL);

    CreateWindowA("BUTTON", "Change Keybinds", WS_CHILD | WS_VISIBLE,
        S(10), S(50), S(200), S(30), hDlg, (HMENU)204, GetModuleHandle(NULL), NULL);

    CreateWindowA("BUTTON", "Delay", WS_CHILD | WS_VISIBLE,
        S(10), S(90), S(200), S(30), hDlg, (HMENU)205, GetModuleHandle(NULL), NULL);

    HWND hMaskCheck = CreateWindowA("BUTTON", "Capture Mask", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        S(10), S(125), S(200), S(20), hDlg, (HMENU)206, GetModuleHandle(NULL), NULL);

    CreateWindowA("BUTTON", "Close", WS_CHILD | WS_VISIBLE,
        S(75), S(150), S(70), S(30), hDlg, (HMENU)202, GetModuleHandle(NULL), NULL);

    HFONT font = CppShot::createScaledFont(hDlg);
    CppShot::applyFontToChildren(hDlg, font);
    CheckDlgButton(hDlg, 206, CppShot::getRegistryInt("CaptureMask", 0) ? BST_CHECKED : BST_UNCHECKED);

    // Center over parent
    RECT rcParent, rcDlg;
    GetWindowRect(this->getWindow(), &rcParent);
    GetWindowRect(hDlg, &rcDlg);
    int x = rcParent.left + (rcParent.right  - rcParent.left - (rcDlg.right  - rcDlg.left)) / 2;
    int y = rcParent.top  + (rcParent.bottom - rcParent.top  - (rcDlg.bottom - rcDlg.top))  / 2;
    SetWindowPos(hDlg, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);

    ShowWindow(hDlg, SW_SHOW);
    UpdateWindow(hDlg);

    EnableWindow(this->getWindow(), FALSE);
    g_settingsClosed = false;
    MSG msg = {};
    while (!g_settingsClosed && GetMessageA(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
        if (!IsWindow(hDlg)) break;
    }

    EnableWindow(this->getWindow(), TRUE);
    SetActiveWindow(this->getWindow());
    DeleteObject(font);
}

// ── Hotkey dialog helpers ──────────────────────────────────────────────

struct KeyCapture {
    UINT mod, vk;
    HWND hPreview;
};

static KeyCapture g_cap1, g_cap2, g_cap3;

static WNDPROC g_oldProc1 = NULL, g_oldProc2 = NULL, g_oldProc3 = NULL;

static LRESULT captureHotkeyMsg(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam, WNDPROC oldProc, KeyCapture* cap) {
    if (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN) {
        UINT vk = (UINT)wParam;
        if (vk == VK_CONTROL || vk == VK_SHIFT || vk == VK_MENU || vk == VK_LWIN || vk == VK_RWIN)
            return 0;
        UINT mod = 0;
        if (GetKeyState(VK_CONTROL) & 0x8000) mod |= MOD_CONTROL;
        if (GetKeyState(VK_SHIFT)   & 0x8000) mod |= MOD_SHIFT;
        if (GetKeyState(VK_MENU)    & 0x8000) mod |= MOD_ALT;
        if ((GetKeyState(VK_LWIN) | GetKeyState(VK_RWIN)) & 0x8000) mod |= MOD_WIN;
        cap->mod = mod;
        cap->vk  = vk;
        SetWindowTextA(hWnd, CppShot::HotkeyToString(mod, vk).c_str());
        return 0;
    }
    return CallWindowProcA(oldProc, hWnd, msg, wParam, lParam);
}

static LRESULT CALLBACK HotkeySubclassProc1(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    return captureHotkeyMsg(hWnd, msg, wParam, lParam, g_oldProc1, &g_cap1);
}
static LRESULT CALLBACK HotkeySubclassProc2(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    return captureHotkeyMsg(hWnd, msg, wParam, lParam, g_oldProc2, &g_cap2);
}
static LRESULT CALLBACK HotkeySubclassProc3(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    return captureHotkeyMsg(hWnd, msg, wParam, lParam, g_oldProc3, &g_cap3);
}

static bool g_dlgClosed = false;

static LRESULT CALLBACK HotkeyDlgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_COMMAND: {
            int id = LOWORD(wParam);
            if (id == 103) { // OK
                CppShot::saveHotkey("Screenshot",         g_cap1.mod, g_cap1.vk);
                CppShot::saveHotkey("ScreenshotRegion",   g_cap2.mod, g_cap2.vk);
                CppShot::saveHotkey("DesktopTransparent", g_cap3.mod, g_cap3.vk);
                DestroyWindow(hWnd);
            } else if (id == 104) { // Cancel
                DestroyWindow(hWnd);
            }
            return 0;
        }
        case WM_CLOSE:
            DestroyWindow(hWnd);
            return 0;
        case WM_DESTROY:
            g_dlgClosed = true;
            return 0;
    }
    return DefWindowProcA(hWnd, msg, wParam, lParam);
}

void MainWindow::onChangeKeybinds() {
    double scale = this->getScaleFactor();
    auto S = [scale](int v) { return (int)(v * scale); };

    std::pair<UINT, UINT> hotkey1 = CppShot::loadHotkey("Screenshot",         MOD_CONTROL,           0x42);
    std::pair<UINT, UINT> hotkey2 = CppShot::loadHotkey("ScreenshotRegion",   MOD_ALT,               0x53);
    std::pair<UINT, UINT> hotkey3 = CppShot::loadHotkey("DesktopTransparent", MOD_CONTROL | MOD_ALT, 0x44);

    g_cap1 = { hotkey1.first, hotkey1.second, NULL };
    g_cap2 = { hotkey2.first, hotkey2.second, NULL };
    g_cap3 = { hotkey3.first, hotkey3.second, NULL };

    HINSTANCE instance = GetModuleHandle(NULL);
    HICON hIcon = (HICON) LoadImage(instance, MAKEINTRESOURCE(IDI_APPICON), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE | LR_DEFAULTCOLOR | LR_SHARED);

    WNDCLASSA wc = {};
    wc.lpfnWndProc   = HotkeyDlgProc;
    wc.hInstance     = GetModuleHandle(NULL);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = "HotkeyDlg";
    wc.hIcon = hIcon;
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    RegisterClassA(&wc);

    HWND hDlg = CreateWindowExA(
        WS_EX_DLGMODALFRAME,
        "HotkeyDlg", "Change Keybinds",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT, S(310), S(335),
        this->getWindow(), NULL, GetModuleHandle(NULL), NULL
    );

    CreateWindowA("STATIC", "_b1",        WS_CHILD | WS_VISIBLE, S(10), S(55),  S(280), S(20), hDlg, NULL, GetModuleHandle(NULL), NULL);
    CreateWindowA("STATIC", "_b1 + _b2",  WS_CHILD | WS_VISIBLE, S(10), S(115), S(280), S(20), hDlg, NULL, GetModuleHandle(NULL), NULL);
    CreateWindowA("STATIC", "Desktop",    WS_CHILD | WS_VISIBLE, S(10), S(175), S(280), S(20), hDlg, NULL, GetModuleHandle(NULL), NULL);

    HWND hPreview1 = CreateWindowA("EDIT", CppShot::HotkeyToString(g_cap1.mod, g_cap1.vk).c_str(),
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_READONLY | ES_CENTER,
        S(10), S(75), S(280), S(25), hDlg, (HMENU)101, GetModuleHandle(NULL), NULL);

    HWND hPreview2 = CreateWindowA("EDIT", CppShot::HotkeyToString(g_cap2.mod, g_cap2.vk).c_str(),
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_READONLY | ES_CENTER,
        S(10), S(135), S(280), S(25), hDlg, (HMENU)102, GetModuleHandle(NULL), NULL);

    HWND hPreview3 = CreateWindowA("EDIT", CppShot::HotkeyToString(g_cap3.mod, g_cap3.vk).c_str(),
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_READONLY | ES_CENTER,
        S(10), S(195), S(280), S(25), hDlg, (HMENU)105, GetModuleHandle(NULL), NULL);

    g_cap1.hPreview = hPreview1;
    g_cap2.hPreview = hPreview2;
    g_cap3.hPreview = hPreview3;

    CreateWindowA("STATIC", "Click a box then press your desired key combo.", WS_CHILD | WS_VISIBLE | SS_CENTER,
        S(10), S(15), S(280), S(40), hDlg, NULL, GetModuleHandle(NULL), NULL);

    CreateWindowA("STATIC", "*A restart of the application is required.", WS_CHILD | WS_VISIBLE | SS_CENTER,
        S(10), S(230), S(280), S(20), hDlg, NULL, GetModuleHandle(NULL), NULL);

    CreateWindowA("BUTTON", "OK",     WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, S(60),  S(258), S(80), S(28), hDlg, (HMENU)103, GetModuleHandle(NULL), NULL);
    CreateWindowA("BUTTON", "Cancel", WS_CHILD | WS_VISIBLE,                   S(155), S(258), S(80), S(28), hDlg, (HMENU)104, GetModuleHandle(NULL), NULL);

    g_oldProc1 = (WNDPROC)SetWindowLongPtrA(hPreview1, GWLP_WNDPROC, (LONG_PTR)HotkeySubclassProc1);
    g_oldProc2 = (WNDPROC)SetWindowLongPtrA(hPreview2, GWLP_WNDPROC, (LONG_PTR)HotkeySubclassProc2);
    g_oldProc3 = (WNDPROC)SetWindowLongPtrA(hPreview3, GWLP_WNDPROC, (LONG_PTR)HotkeySubclassProc3);

    HFONT font = CppShot::createScaledFont(hDlg);
    CppShot::applyFontToChildren(hDlg, font);

    // Center over parent
    RECT rcParent, rcDlg;
    GetWindowRect(this->getWindow(), &rcParent);
    GetWindowRect(hDlg, &rcDlg);
    int x = rcParent.left + (rcParent.right  - rcParent.left - (rcDlg.right  - rcDlg.left)) / 2;
    int y = rcParent.top  + (rcParent.bottom - rcParent.top  - (rcDlg.bottom - rcDlg.top))  / 2;
    SetWindowPos(hDlg, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);

    ShowWindow(hDlg, SW_SHOW);
    UpdateWindow(hDlg);

    g_dlgClosed = false;
    MSG msg = {};
    while (!g_dlgClosed && GetMessageA(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
        if (!IsWindow(hDlg))
            break;
    }

    DeleteObject(font);
    UnregisterClassA("HotkeyDlg", GetModuleHandle(NULL));
};
// delay

struct Delay {
    UINT delay;
    HWND hPReview;
};

static Delay new_delay;
static HFONT g_delayFont = nullptr;

static LRESULT CALLBACK DelayDlgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_COMMAND: {
            int id = LOWORD(wParam);
            if (id == 303) { // OK

                char buffer[64];

                GetDlgItemTextA(hWnd, 1001, buffer, sizeof(buffer));

                int value = 0;
                try {
                    value = std::stoi(buffer);
                } catch (...) {
                    value = 0;
                }

                CppShot::changeRegistryInt("Delay", value);
                DestroyWindow(hWnd);
            } else if (id == 304) { // Cancel
                DestroyWindow(hWnd);
            } 
            return 0;
        }
        case WM_CLOSE:
            DestroyWindow(hWnd);
            return 0;
        case WM_DESTROY:
            if (g_delayFont) { DeleteObject(g_delayFont); g_delayFont = nullptr; }
            return 0;
    }
    return DefWindowProcA(hWnd, msg, wParam, lParam);
};

void MainWindow::onChangeDelay() {
    double scale = this->getScaleFactor();
    auto S = [scale](int v) { return (int)(v * scale); };

    int m_delay = CppShot::getRegistryInt("Delay", 0);
    new_delay.delay = m_delay;
    std::string delay_str = std::to_string(new_delay.delay);

    WNDCLASSA wc = {};
    wc.lpfnWndProc = DelayDlgProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = "DelayDlg";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassA(&wc);

    HWND hDlg = CreateWindowExA(
        WS_EX_DLGMODALFRAME,
        "DelayDlg",
        "Change Delay",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT, S(310), S(180),
        this->getWindow(), NULL, GetModuleHandle(NULL), NULL
    );

    HWND hEdit = CreateWindowExA(
        WS_EX_CLIENTEDGE,
        "EDIT",
        delay_str.c_str(),
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_CENTER | ES_NUMBER,
        S(50), S(60), S(200), S(25),
        hDlg,
        (HMENU)1001,
        GetModuleHandle(NULL),
        NULL
    );

    CreateWindowA("STATIC", "Click on the text box and write your delay in milliseconds.", WS_CHILD | WS_VISIBLE | SS_CENTER,
        S(10), S(10), S(280), S(40), hDlg, NULL, GetModuleHandle(NULL), NULL);

    CreateWindowA("BUTTON", "OK",     WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, S(60),  S(105), S(80), S(28), hDlg, (HMENU)303, GetModuleHandle(NULL), NULL);
    CreateWindowA("BUTTON", "Cancel", WS_CHILD | WS_VISIBLE,                   S(155), S(105), S(80), S(28), hDlg, (HMENU)304, GetModuleHandle(NULL), NULL);

    g_delayFont = CppShot::createScaledFont(hDlg);
    CppShot::applyFontToChildren(hDlg, g_delayFont);

    // Center over parent
    RECT rcParent, rcDlg;
    GetWindowRect(this->getWindow(), &rcParent);
    GetWindowRect(hDlg, &rcDlg);
    int x = rcParent.left + (rcParent.right  - rcParent.left - (rcDlg.right  - rcDlg.left)) / 2;
    int y = rcParent.top  + (rcParent.bottom - rcParent.top  - (rcDlg.bottom - rcDlg.top))  / 2;
    SetWindowPos(hDlg, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);

    ShowWindow(hDlg, SW_SHOW);
    UpdateWindow(hDlg);
}