#pragma once
#include <windows.h>
#include <utility>
class Window;
class Node
{
protected:
    HWND m_window = nullptr;
    Window *m_parent;
    std::pair<int, int> m_position;
    std::pair<int, int> m_size;
    int m_id = 0;

public:
    Node(const char *className, DWORD dwStyle, Window *parent);
    Node &setPosition(int x, int y);
    Node &setSize(int width, int height);
    Node &setTitle(const char *title);
    Node &forceResize();
    HWND getWindow();
    int getId() { return m_id; }
};