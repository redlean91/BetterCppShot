#pragma once

#include <windows.h>
#include <gdiplus.h>
#include <string>

class Screenshot
{
protected:
	RECT m_captureRect;
	Gdiplus::Bitmap *m_image = nullptr;
	HWND m_window = nullptr;

	RECT createRect();
	void encoderClsid();

	static Gdiplus::Bitmap *buildBitmapFromDIB(HWND window, BYTE *bits, int width, int height);
	static void applyWindowShapeAsAlpha(HWND window, BYTE *bits, int width, int height);
	static Gdiplus::Bitmap *copyToGdiplusBitmap(const BYTE *bits, int width, int height, Gdiplus::PixelFormat format);
	static Gdiplus::Bitmap *attachShadow(HWND window, Gdiplus::Bitmap *frame);

public:
	Screenshot();
	Screenshot(HWND window);
	~Screenshot();
	void capture(HWND window);
	void captureDirect(HWND window);
	void captureWgc(HWND window);
	void captureRect(RECT rect);
	void save(const std::string &path);
	bool isCaptured();
	Gdiplus::Bitmap *getBitmap() const;
	RECT getCaptureRect() const;

	// Direct capture renders the window itself via PrintWindow instead of
	// photographing it on top of black/white backdrops.
	static bool isDirectCaptureSupported();
	static bool saveBitmap(Gdiplus::Bitmap *image, const std::string &path);
};