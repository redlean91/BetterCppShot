#include "Screenshot.h"
#include "..\Utils.h"

Screenshot::Screenshot() {}

Screenshot::Screenshot(HWND window) {
	capture(window);
}

Screenshot::~Screenshot() {
	delete m_image;
}

void Screenshot::capture(HWND window) {
	delete m_image;

    m_window = window;
	RECT rct = createRect();

	HDC hdc = GetDC(HWND_DESKTOP);
    HDC memdc = CreateCompatibleDC(hdc);
    HBITMAP hbitmap = CreateCompatibleBitmap(hdc, rct.right - rct.left, rct.bottom - rct.top);

    SelectObject(memdc, hbitmap);
    BitBlt(memdc, 0, 0, rct.right - rct.left, rct.bottom - rct.top, hdc, rct.left, rct.top, SRCCOPY );

    DeleteDC(memdc);
    ReleaseDC(HWND_DESKTOP, hdc);

	m_image = new Gdiplus::Bitmap(hbitmap, NULL);
	//delete hbitmap;
}

RECT Screenshot::createRect() {
    return m_captureRect = CppShot::getCaptureRect(m_window);
}

void Screenshot::save(const std::string& path) {
	// This is supposed to be gathered from the OS but the encoder CLSID has never changed, so this is safe enough
	// The old version of cppshot had a bug that made it use the hardcoded one anyway
    CLSID pngEncoder = {0x557cf406, 0x1a04, 0x11d3, {0x9a, 0x73, 0x00, 0x00, 0xf8, 0x1e, 0xf3, 0x2e} } ;
    
    // Convert std::string (UTF-8 or ANSI) to std::wstring
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, NULL, 0);
    std::wstring wpath(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, &wpath[0], size_needed);

    m_image->Save(wpath.c_str(), &pngEncoder, NULL);
}

bool Screenshot::isCaptured() {
    return m_image != nullptr;
}

Gdiplus::Bitmap* Screenshot::getBitmap() const {
	return m_image;
}

RECT Screenshot::getCaptureRect() const {
    return m_captureRect;
}