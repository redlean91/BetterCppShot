#pragma once

#include "Screenshot.h"

#include <windows.h>
#include <gdiplus.h>
#include <string>

class CompositeScreenshot : public Screenshot {
	Gdiplus::Rect m_crop;
	RECT m_captureRect;
	bool m_noCrop = false;
	bool m_blackOpaque = false;
	int m_minAlpha = 254;

	void init(const Screenshot& white, const Screenshot& black);
	void differentiateAlpha(Gdiplus::Bitmap* whiteShot, Gdiplus::Bitmap* blackShot);
	void cropImage();
	Gdiplus::Rect calculateCrop();
public:
	Gdiplus::Rect getCrop();
	CompositeScreenshot(const Screenshot& whiteShot, const Screenshot& blackShot);
	CompositeScreenshot(const Screenshot& whiteShot, const Screenshot& blackShot, Gdiplus::Rect crop);
	CompositeScreenshot(const Screenshot& whiteShot, const Screenshot& blackShot, Gdiplus::Rect crop, bool blackOpaque);
	CompositeScreenshot(const Screenshot& whiteShot, const Screenshot& blackShot, bool noCrop);
	CompositeScreenshot(const Screenshot& whiteShot, const Screenshot& blackShot, bool noCrop, bool blackOpaque);
};