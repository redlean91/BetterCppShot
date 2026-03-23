#include "Window.h"
#include "../ui/Button.h"
#include "../resources.h"
#include "../Utils.h"
#include <windows.h>
#include <commctrl.h>
#include <stdexcept>
#include <string>

Window::Window(HBRUSH brush, const char* className, const char* title, DWORD dwExStyle, DWORD dwStyle) {
    HINSTANCE instance = GetModuleHandle(NULL);

    // Initialise common controls
    INITCOMMONCONTROLSEX icc;
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_WIN95_CLASSES;
    InitCommonControlsEx(&icc);

    // Get icon path relative to exe location
    char exePath[MAX_PATH];
    GetModuleFileNameA(NULL, exePath, MAX_PATH);
    char* lastSlash = strrchr(exePath, '\\');
    if (lastSlash) *(lastSlash + 1) = '\0';
    char iconPath[MAX_PATH];
    sprintf(iconPath, "%sres\\cppshot32.ico", exePath);

    HICON hIconBig   = (HICON)LoadImageA(NULL, "res\\cppshot32.ico", IMAGE_ICON, 32, 32, LR_LOADFROMFILE);
    HICON hIconSmall = (HICON)LoadImageA(NULL, "res\\cppshot32.ico", IMAGE_ICON, 16, 16, LR_LOADFROMFILE);

    WNDCLASSEXA wincl; // Note the 'A' for ANSI
    wincl.hInstance = instance;
    wincl.lpszClassName = className;
    wincl.lpfnWndProc = Window::windowProcedure;
    wincl.style = CS_DBLCLKS;
    wincl.cbSize = sizeof(WNDCLASSEX);
    wincl.hIcon = hIconBig ? hIconBig : LoadIconA(NULL, (LPCSTR)IDI_APPLICATION);
    wincl.hIconSm = hIconSmall ? hIconSmall : LoadIconA(NULL, (LPCSTR)IDI_APPLICATION);
    wincl.hCursor = LoadCursorA(NULL, (LPCSTR)IDC_ARROW);
    wincl.lpszMenuName = NULL;
    wincl.cbClsExtra = 0;
    wincl.cbWndExtra = 0;
    wincl.hbrBackground = brush;

    if (!RegisterClassExA(&wincl)) throw std::runtime_error("Unable to register window class");

    m_window = CreateWindowExA(
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

    SetWindowLongPtrA(m_window, GWLP_USERDATA, (LONG_PTR)this);

    if (hIconBig && hIconSmall) {
        SetClassLongPtrA(m_window, GCLP_HICON,   (LONG_PTR)hIconBig);
        SetClassLongPtrA(m_window, GCLP_HICONSM, (LONG_PTR)hIconSmall);
        SendMessageA(m_window, WM_SETICON, ICON_BIG,   (LPARAM)hIconBig);
        SendMessageA(m_window, WM_SETICON, ICON_SMALL, (LPARAM)hIconSmall);
    }

    // Force remove resize border and maximize box
    LONG style = GetWindowLongA(m_window, GWL_STYLE);
    style &= ~(WS_THICKFRAME | WS_MAXIMIZEBOX);
    SetWindowLongA(m_window, GWL_STYLE, style);
    SetWindowPos(m_window, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
}

LRESULT CALLBACK Window::windowProcedure(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_DESTROY) {
        LONG exStyle = GetWindowLongA(hwnd, GWL_EXSTYLE);
        if (!(exStyle & WS_EX_TOOLWINDOW)) {
            UnregisterHotKey(NULL, 1);
            UnregisterHotKey(NULL, 2);
            PostQuitMessage(0);
        }
        return 0;
    }

    Window* ptr = reinterpret_cast<Window*>(GetWindowLongPtrA(hwnd, GWLP_USERDATA));
    if (ptr) return ptr->handleMessage(message, wParam, lParam);
    return DefWindowProcA(hwnd, message, wParam, lParam);
}

HWND Window::getWindow() {
    return m_window;
}

HWND Window::addLabel(const char* text, int x, int y, int width, int height) {
    return CreateWindowExA(
        0,
        "STATIC",
        text,
        WS_CHILD | WS_VISIBLE,
        x, y, width, height,
        m_window,
        NULL,
        GetModuleHandle(NULL),
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
    double scale = getScaleFactor();
    SetWindowPos(m_window, NULL, 0, 0, (int)(width * scale), (int)(height * scale), SWP_NOMOVE | SWP_NOZORDER);
    return *this;
}

LRESULT Window::handleMessage(UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_COMMAND:
            switch (HIWORD(wParam)) {
                case BN_CLICKED: {
                    Button* ptr = reinterpret_cast<Button*>(GetWindowLongPtrA((HWND)lParam, GWLP_USERDATA));
                    if (ptr) ptr->onClick();
                    break;
                }
            }
            switch (LOWORD(wParam)) {
                case ID_FILE_OPEN: break;
                case ID_FILE_EXIT: DestroyWindow(m_window); break;
            }
            break;

        case WM_DESTROY: {
            LONG style = GetWindowLongA(m_window, GWL_EXSTYLE);
            if (!(style & WS_EX_TOOLWINDOW)) {
                UnregisterHotKey(NULL, 1);
                UnregisterHotKey(NULL, 2);
                PostQuitMessage(0);
            }
            break;
        }

        case 0x02E0: // WM_DPICHANGED
            for (auto child : m_children) child->forceResize();
            break;

        default:
            return DefWindowProcA(m_window, message, wParam, lParam);
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