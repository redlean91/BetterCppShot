#include "Node.h"
#include "../windows/Window.h"
#include <commctrl.h>
#include <windows.h>

// Use a simple incrementing ID for child controls
static int s_nextNodeId = 1000;

Node::Node(const char *className, DWORD dwStyle, Window *parent)
{
    m_id = s_nextNodeId++;

    m_window = CreateWindowExA(
        0,
        className,
        "Default",
        dwStyle,
        10,
        10,
        500,
        100,
        parent->getWindow(),
        (HMENU)(INT_PTR)m_id,
        (HINSTANCE)GetWindowLong(parent->getWindow(), GWLP_HINSTANCE),
        NULL);

    if (!m_window)
    {
        char msg[256];
        wsprintfA(msg, "CreateWindowExA failed for class '%s' with error: %lu", className, GetLastError());
        MessageBoxA(NULL, msg, "Node Error", MB_OK);
        return;
    }

    SetWindowLongA(m_window, GWLP_USERDATA, (LONG_PTR)this);
    m_parent = parent;
    m_parent->addChild(this);
}

Node &Node::setPosition(int x, int y)
{
    m_position = {x, y};
    auto scale = m_parent->getScaleFactor();
    SetWindowPos(m_window, NULL, (int)(x * scale), (int)(y * scale), 0, 0, SWP_NOSIZE | SWP_NOZORDER);
    return *this;
}

Node &Node::setSize(int width, int height)
{
    m_size = {width, height};
    auto scale = m_parent->getScaleFactor();
    SetWindowPos(m_window, NULL, 0, 0, (int)(width * scale), (int)(height * scale), SWP_NOMOVE | SWP_NOZORDER);
    return *this;
}

Node &Node::setTitle(const char *title)
{
    SetWindowTextA(m_window, title);
    return *this;
}

Node &Node::forceResize()
{
    setPosition(m_position.first, m_position.second);
    setSize(m_size.first, m_size.second);
    return *this;
}

HWND Node::getWindow()
{
    return m_window;
}