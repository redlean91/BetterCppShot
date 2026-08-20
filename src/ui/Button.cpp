#include "Button.h"
#include "../themeValues.h"

Button::Button(Window *parent) : Node("BUTTON", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON | (OWNER_DRAW ? BS_OWNERDRAW : 0), parent) {}

Button &Button::setCallback(std::function<void()> callback)
{
    m_onClick = callback;
    return *this;
}

void Button::onClick()
{
    m_onClick();
}