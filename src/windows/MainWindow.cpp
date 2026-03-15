#include "MainWindow.h"
#include "../ui/Button.h"
#include "../version.h"
#include <tchar.h>
#include <shellapi.h>
#include "../Utils.h"
#include <windows.h>
#include <shlobj.h>

MainWindow::MainWindow() : Window((HBRUSH)(COLOR_BTNFACE + 1), L"MainCreWindow", L"BCppShot", 0, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX) {
    setSize(230, 150);
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
    this->addLabel(L"BetterCppShot, by Redlean", 10, 90, 200, 20);
}

void MainWindow::onOpenExplorer() {
    // Use ANSI version for Win98 compatibility
    char pathA[MAX_PATH];
    WideCharToMultiByte(CP_ACP, 0, CppShot::getSaveDirectory().c_str(), -1, pathA, MAX_PATH, NULL, NULL);
    ShellExecuteA(NULL, "open", "explorer", pathA, NULL, SW_SHOWNORMAL);
}

void MainWindow::onOpenExplorer_change() {
    BROWSEINFOA bi;
    ZeroMemory(&bi, sizeof(bi));
    bi.hwndOwner = NULL;
    bi.lpszTitle = "Select a folder for screenshots:";
    bi.ulFlags = BIF_RETURNONLYFSDIRS;

    LPITEMIDLIST pidl = SHBrowseForFolderA(&bi);
    if (pidl == NULL)
        return;

    char pathA[MAX_PATH];
    if (SHGetPathFromIDListA(pidl, pathA)) {
        // Convert to wide for registry storage
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