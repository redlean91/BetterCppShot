#if defined(UNICODE) && !defined(_UNICODE)
#define _UNICODE
#elif defined(_UNICODE) && !defined(UNICODE)
#define UNICODE
#endif

#include "Window.h"
#include "../ui/Button.h"
#include "../resources.h"
#include "../Utils.h"
#include <windows.h>
#include <commctrl.h>
#include <stdexcept>
#include <tchar.h>

Window::Window(HBRUSH brush, const TCHAR* className, const TCHAR* title, DWORD dwExStyle, DWORD dwStyle) {
    auto instance = GetModuleHandle(NULL);

    // Initialise common controls.
    INITCOMMONCONTROLSEX icc;
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_WIN95_CLASSES;
    InitCommonControlsEx(&icc);

    // Get icon path relative to exe location
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(NULL, exePath, MAX_PATH);
    wchar_t* lastSlash = wcsrchr(exePath, L'\\');
    if (lastSlash) *(lastSlash + 1) = L'\0';
    wchar_t iconPath[MAX_PATH];
    wsprintfW(iconPath, L"%sres\\cppshot32.ico", exePath);

    HICON hIconBig   = (HICON)LoadImage(NULL, iconPath, IMAGE_ICON, 32, 32, LR_LOADFROMFILE);
    HICON hIconSmall = (HICON)LoadImage(NULL, iconPath, IMAGE_ICON, 16, 16, LR_LOADFROMFILE);

    WNDCLASSEX wincl;
    wincl.hInstance = instance;
    wincl.lpszClassName = className;
    wincl.lpfnWndProc = Window::windowProcedure;
    wincl.style = CS_DBLCLKS;
    wincl.cbSize = sizeof(WNDCLASSEX);
    wincl.hIcon = hIconBig ? hIconBig : LoadIcon(NULL, IDI_APPLICATION);
    wincl.hIconSm = hIconSmall ? hIconSmall : LoadIcon(NULL, IDI_APPLICATION);
    wincl.hCursor = LoadCursor(NULL, IDC_ARROW);
    wincl.lpszMenuName = NULL;
    wincl.cbClsExtra = 0;
    wincl.cbWndExtra = 0;
    wincl.hbrBackground = brush;

    if (!RegisterClassEx(&wincl)) throw std::runtime_error("Unable to create window");

    m_window = CreateWindowEx(
        dwExStyle,
        className,
        title,
        dwStyle,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        544,
        375,
        HWND_DESKTOP,
        NULL,
        instance,
        NULL
    );

    SetWindowLongPtr(m_window, GWLP_USERDATA, (LONG_PTR)this);

    if (hIconBig && hIconSmall) {
        SetClassLongPtr(m_window, GCLP_HICON,   (LONG_PTR)hIconBig);
        SetClassLongPtr(m_window, GCLP_HICONSM, (LONG_PTR)hIconSmall);
        SendMessage(m_window, WM_SETICON, ICON_BIG,   (LPARAM)hIconBig);
        SendMessage(m_window, WM_SETICON, ICON_SMALL, (LPARAM)hIconSmall);
    }

    // Force remove resize border and maximize box regardless of passed style
    LONG style = GetWindowLong(m_window, GWL_STYLE);
    style &= ~(WS_THICKFRAME | WS_MAXIMIZEBOX);
    SetWindowLong(m_window, GWL_STYLE, style);
    SetWindowPos(m_window, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
}

LRESULT CALLBACK Window::windowProcedure(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_DESTROY) {
        LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
        if (!(exStyle & WS_EX_TOOLWINDOW)) {
            UnregisterHotKey(NULL, 1);
            UnregisterHotKey(NULL, 2);
            PostQuitMessage(0);
        }
        return 0;
    }
    if (auto ptr = reinterpret_cast<Window*>(GetWindowLongPtr(hwnd, GWLP_USERDATA))) return ptr->handleMessage(message, wParam, lParam);
    return DefWindowProc(hwnd, message, wParam, lParam);
}

HWND Window::getWindow() {
    return m_window;
}

HWND Window::addLabel(const wchar_t* text, int x, int y, int width, int height) {
    return CreateWindowW(
        L"STATIC",
        text,
        WS_CHILD | WS_VISIBLE,
        x, y, width, height,
        m_window,
        NULL,
        GetModuleHandleW(NULL),
        NULL
    );
}

Window& Window::show(int nCmdShow) const {
    ShowWindow(m_window, nCmdShow);
    return *const_cast<Window*>(this);
}

Window& Window::hide() const {
    ShowWindow(m_window, 0);
    return *const_cast<Window*>(this);
}

Window& Window::setSize(int width, int height) {
    auto scale = getScaleFactor();
    SetWindowPos(m_window, NULL, 0, 0, width * scale, height * scale, SWP_NOMOVE | SWP_NOZORDER);
    return *this;
}

LRESULT Window::handleMessage(UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_COMMAND:
        switch (HIWORD(wParam)) {
        case BN_CLICKED:
            auto ptr = reinterpret_cast<Button*>(GetWindowLongPtr((HWND)lParam, GWLP_USERDATA));
            ptr->onClick();
            break;
        }

        switch (LOWORD(wParam)) {
        case ID_FILE_OPEN:
            break;
        case ID_FILE_EXIT:
            DestroyWindow(m_window);
            break;
        }
        break;
    case WM_DESTROY:
    {
        LONG style = GetWindowLong(m_window, GWL_EXSTYLE);
        if (!(style & WS_EX_TOOLWINDOW)) {
            UnregisterHotKey(NULL, 1);
            UnregisterHotKey(NULL, 2);
            PostQuitMessage(0);
        }
        break;
    }
    case 0x02E0: // WM_DPICHANGED
        for (auto child : m_children) child->forceResize();
    default:
        return DefWindowProc(m_window, message, wParam, lParam);
    }

    return 0;
}

void Window::addChild(Node* child) {
    m_children.push_back(child);
}

Button& Window::addButton() {
    Button* button = new Button(this);
    return *button;
}

unsigned int Window::getDPI() {
    return CppShot::getDPIForWindow(m_window);
}

double Window::getScaleFactor() {
    return getDPI() / 96.0;
}

Window::~Window() {
    for (auto child : m_children) delete child;
}