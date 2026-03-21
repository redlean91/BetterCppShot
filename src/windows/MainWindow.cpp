#include "MainWindow.h"
#include "../ui/Button.h"
#include "../version.h"
#include <tchar.h>
#include <shellapi.h>
#include "../Utils.h"
#include <windows.h>
#include <shlobj.h>
#include <string>

MainWindow::MainWindow() : Window((HBRUSH)(COLOR_BTNFACE + 1), L"MainCreWindow", L"BCppShot", 0, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX) {
    setSize(230, 270);
    this->addButton()
        .setCallback([this]() { onOpenExplorer(); })
        .setPosition(10, 10)
        .setSize(200, 30)
        .setTitle(L"Open Screenshots Folder");
    this->addButton()
        .setCallback([this]() { onOpenExplorer_change(); })
        .setPosition(10, 50)
        .setSize(200, 30)
        .setTitle(L"Change Screenshots Folder");
    this->addButton()
        .setCallback([this]() { onChangeKeybinds(); })
        .setPosition(10, 90)
        .setSize(200, 30)
        .setTitle(L"Change Keybinds");

    // Keybinds
    this->addLabel(L"Active keybinds:", 57, 130, 200, 20);

    // actually getting the keybinds
    std::pair<UINT, UINT> hotkey1 = CppShot::loadHotkey(L"Screenshot",       MOD_CONTROL, 0x42);
    std::pair<UINT, UINT> hotkey2 = CppShot::loadHotkey(L"ScreenshotRegion", MOD_ALT,     0x53);

    UINT mod1 = hotkey1.first, vk1 = hotkey1.second;
    UINT mod2 = hotkey2.first, vk2 = hotkey2.second;

    std::wstring hotkey_b1      = CppShot::HotkeyToString(mod1, vk1);
    std::wstring hotkey_b1_text = L"_b1:             ";
    hotkey_b1_text += hotkey_b1;
    this->addLabel(hotkey_b1_text.c_str(), 10, 150, 200, 20);

    std::wstring hotkey_b1_b2      = CppShot::HotkeyToString(mod2, vk2);
    std::wstring hotkey_b1_b2_text = L"_b1 + _b2:   ";
    hotkey_b1_b2_text += hotkey_b1_b2;
    this->addLabel(hotkey_b1_b2_text.c_str(), 10, 170, 200, 20);

    this->addLabel(L"BetterCppShot, by Redlean", 10, 210, 200, 20);
}

void MainWindow::onOpenExplorer() {
    char pathA[MAX_PATH];
    WideCharToMultiByte(CP_ACP, 0, CppShot::getSaveDirectory().c_str(), -1, pathA, MAX_PATH, NULL, NULL);
    ShellExecuteA(NULL, "open", "explorer", pathA, NULL, SW_SHOWNORMAL);
}

void MainWindow::onOpenExplorer_change() {
    BROWSEINFOA bi;
    ZeroMemory(&bi, sizeof(bi));
    bi.hwndOwner = NULL;
    bi.lpszTitle = "Select a folder for screenshots:";
    bi.ulFlags   = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;

    LPITEMIDLIST pidl = SHBrowseForFolderA(&bi);
    if (pidl == NULL)
        return;

    char pathA[MAX_PATH];
    if (SHGetPathFromIDListA(pidl, pathA)) {
        wchar_t pathW[MAX_PATH];
        MultiByteToWideChar(CP_ACP, 0, pathA, -1, pathW, MAX_PATH);
        CppShot::changeRegistry(L"Path", pathW);
    }

    IMalloc* imalloc = NULL;
    if (SUCCEEDED(SHGetMalloc(&imalloc))) {
        imalloc->Free(pidl);
        imalloc->Release();
    }
}

// ── Hotkey dialog helpers ────────────────────────────────────────────────────

struct KeyCapture {
    UINT mod, vk;
    HWND hPreview;
};

static KeyCapture g_cap1, g_cap2;

static LRESULT CALLBACK HotkeySubclassProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR id, DWORD_PTR data) {
    if (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN) {
        UINT vk = (UINT)wParam;
        if (vk == VK_CONTROL || vk == VK_SHIFT || vk == VK_MENU || vk == VK_LWIN || vk == VK_RWIN)
            return 0;

        UINT mod = 0;
        if (GetKeyState(VK_CONTROL) & 0x8000) mod |= MOD_CONTROL;
        if (GetKeyState(VK_SHIFT)   & 0x8000) mod |= MOD_SHIFT;
        if (GetKeyState(VK_MENU)    & 0x8000) mod |= MOD_ALT;
        if ((GetKeyState(VK_LWIN) | GetKeyState(VK_RWIN)) & 0x8000) mod |= MOD_WIN;

        KeyCapture* cap = (KeyCapture*)data;
        cap->mod = mod;
        cap->vk  = vk;
        SetWindowTextW(hWnd, CppShot::HotkeyToString(mod, vk).c_str());
        return 0;
    }
    return DefSubclassProc(hWnd, msg, wParam, lParam);
}

static bool g_dlgClosed = false;

static LRESULT CALLBACK HotkeyDlgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_COMMAND: {
            int id = LOWORD(wParam);
            if (id == 103) { // OK
                CppShot::saveHotkey(L"Screenshot",       g_cap1.mod, g_cap1.vk);
                CppShot::saveHotkey(L"ScreenshotRegion", g_cap2.mod, g_cap2.vk);
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
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

void MainWindow::onChangeKeybinds() {
    std::pair<UINT, UINT> hotkey1 = CppShot::loadHotkey(L"Screenshot",       MOD_CONTROL, 0x42);
    std::pair<UINT, UINT> hotkey2 = CppShot::loadHotkey(L"ScreenshotRegion", MOD_ALT,     0x53);

    g_cap1 = { hotkey1.first, hotkey1.second, NULL };
    g_cap2 = { hotkey2.first, hotkey2.second, NULL };

    WNDCLASSW wc    = {};
    wc.lpfnWndProc  = HotkeyDlgProc;
    wc.hInstance    = GetModuleHandle(NULL);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = L"HotkeyDlg";
    wc.hCursor      = LoadCursor(NULL, IDC_ARROW);
    RegisterClassW(&wc);

    HWND hDlg = CreateWindowExW(
        WS_EX_DLGMODALFRAME,
        L"HotkeyDlg", L"Change Keybinds",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT, 310, 250,
        this->getWindow(), NULL, GetModuleHandle(NULL), NULL
    );

    CreateWindowW(L"STATIC", L"_b1",        WS_CHILD | WS_VISIBLE,         10, 15, 120, 20, hDlg, NULL,       GetModuleHandle(NULL), NULL);
    CreateWindowW(L"STATIC", L"_b1 + _b2", WS_CHILD | WS_VISIBLE,         10, 75, 120, 20, hDlg, NULL,       GetModuleHandle(NULL), NULL);

    HWND hPreview1 = CreateWindowW(L"EDIT", CppShot::HotkeyToString(g_cap1.mod, g_cap1.vk).c_str(),
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_READONLY | ES_CENTER,
        10, 35, 280, 25, hDlg, (HMENU)101, GetModuleHandle(NULL), NULL);

    HWND hPreview2 = CreateWindowW(L"EDIT", CppShot::HotkeyToString(g_cap2.mod, g_cap2.vk).c_str(),
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_READONLY | ES_CENTER,
        10, 95, 280, 25, hDlg, (HMENU)102, GetModuleHandle(NULL), NULL);

    g_cap1.hPreview = hPreview1;
    g_cap2.hPreview = hPreview2;

    CreateWindowW(L"STATIC", L"Click a box then press your desired key combo.", WS_CHILD | WS_VISIBLE | SS_CENTER,
        10, 130, 280, 20, hDlg, NULL, GetModuleHandle(NULL), NULL);

    CreateWindowW(L"STATIC", L"*A restart of the application is required.", WS_CHILD | WS_VISIBLE | SS_CENTER,
        10, 150, 280, 20, hDlg, NULL, GetModuleHandle(NULL), NULL);

    CreateWindowW(L"BUTTON", L"OK",     WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 60,  175, 80, 28, hDlg, (HMENU)103, GetModuleHandle(NULL), NULL);
    CreateWindowW(L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE,                   155, 175, 80, 28, hDlg, (HMENU)104, GetModuleHandle(NULL), NULL);

    SetWindowSubclass(hPreview1, HotkeySubclassProc, 1, (DWORD_PTR)&g_cap1);
    SetWindowSubclass(hPreview2, HotkeySubclassProc, 2, (DWORD_PTR)&g_cap2);

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
    while (!g_dlgClosed && GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
        if (!IsWindow(hDlg))
            break;
    }

    RemoveWindowSubclass(hPreview1, HotkeySubclassProc, 1);
    RemoveWindowSubclass(hPreview2, HotkeySubclassProc, 2);
    UnregisterClassW(L"HotkeyDlg", GetModuleHandle(NULL));
}