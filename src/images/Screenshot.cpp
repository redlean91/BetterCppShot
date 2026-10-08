#include "Screenshot.h"
#include "WgcCapture.h"
#include "../Utils.h"
#include <windows.h>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <vector>

#ifndef PW_RENDERFULLCONTENT
#define PW_RENDERFULLCONTENT 0x00000002
#endif

#ifndef DWMWA_EXTENDED_FRAME_BOUNDS
#define DWMWA_EXTENDED_FRAME_BOUNDS 9
#endif
#ifndef DWMWA_CLOAK
#define DWMWA_CLOAK 13
#endif

typedef BOOL (WINAPI *PFN_PrintWindow)(HWND, HDC, UINT);
typedef int (WINAPI *PFN_GetWindowRgn)(HWND, HRGN);
typedef HRESULT (WINAPI *PFN_DwmGetWindowAttribute)(HWND, DWORD, PVOID, DWORD);
typedef HRESULT (WINAPI *PFN_DwmSetWindowAttribute)(HWND, DWORD, LPCVOID, DWORD);

#define SHADOW_LOG(...) do { if (getenv("BCS_WGC_DEBUG")) { fprintf(stderr, __VA_ARGS__); fflush(stderr); } } while (0)

struct DwmApi {
	HMODULE module = NULL;
	PFN_DwmGetWindowAttribute getAttr = NULL;
	PFN_DwmSetWindowAttribute setAttr = NULL;
};

static const DwmApi& getDwmApi() {
	static DwmApi api;
	static bool init = false;
	if (!init) {
		init = true;
		api.module = LoadLibraryA("dwmapi.dll"); // Vista+
		if (api.module) {
			api.getAttr = (PFN_DwmGetWindowAttribute)GetProcAddress(api.module, "DwmGetWindowAttribute");
			api.setAttr = (PFN_DwmSetWindowAttribute)GetProcAddress(api.module, "DwmSetWindowAttribute");
		}
	}
	return api;
}

struct ScreenDib {
	HBITMAP hbm = NULL;
	BYTE *bits = NULL;
	int w = 0, h = 0;
	void release() { if (hbm) DeleteObject(hbm); hbm = NULL; bits = NULL; w = h = 0; }
};

// BitBlt a screen region into a 32bpp top-down DIB.
static bool captureScreenRectDib(const RECT &rc, ScreenDib &out) {
	int w = rc.right - rc.left, h = rc.bottom - rc.top;
	if (w <= 0 || h <= 0) return false;

	BITMAPINFO bmi = {};
	bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biWidth       = w;
	bmi.bmiHeader.biHeight      = -h;
	bmi.bmiHeader.biPlanes      = 1;
	bmi.bmiHeader.biBitCount    = 32;
	bmi.bmiHeader.biCompression = BI_RGB;

	HDC screen = GetDC(HWND_DESKTOP);
	HDC mem    = CreateCompatibleDC(screen);
	out.hbm    = CreateDIBSection(screen, &bmi, DIB_RGB_COLORS, (void **)&out.bits, NULL, 0);

	BOOL ok = FALSE;
	if (out.hbm && out.bits && mem) {
		HGDIOBJ old = SelectObject(mem, out.hbm);
		ok = BitBlt(mem, 0, 0, w, h, screen, rc.left, rc.top, SRCCOPY);
		SelectObject(mem, old);
	}
	if (mem) DeleteDC(mem);
	ReleaseDC(HWND_DESKTOP, screen);

	if (!ok) { out.release(); return false; }
	out.w = w; out.h = h;
	return true;
}

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
    int delay = CppShot::getRegistryInt("Delay", 0);
    if (delay > 0)
        Sleep((DWORD)delay);
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

bool Screenshot::isDirectCaptureSupported() {
	HMODULE hUser32 = GetModuleHandleA("user32.dll");
	return hUser32 && GetProcAddress(hUser32, "PrintWindow") != NULL;
}

// Best-quality direct capture: Windows.Graphics.Capture renders the window
// itself - content, rounded corners and frame - with real per-pixel alpha.
// On top of that, the original DWM drop shadow is recovered from the screen
// by briefly cloaking the window (an instant, animation-free hide) to
// photograph the background underneath - no backdrops and no synthesized
// shadows. Only available on Windows 10 1803+ (shadow recovery additionally
// needs Windows 8+); leaves the screenshot empty elsewhere so callers can
// fall back.
void Screenshot::captureWgc(HWND window) {
	delete m_image;
	m_image = nullptr;
	m_window = window;

	if (!IsWindow(window)) return;

	int delay = CppShot::getRegistryInt("Delay", 0);
	if (delay > 0)
		Sleep((DWORD)delay);

	Gdiplus::Bitmap *frame = WgcCapture::captureWindow(window);
	if (!frame) return;

	m_image = (CppShot::getRegistryInt("CaptureShadow", 1) != 0)
		? attachShadow(window, frame)
		: frame;

	RECT r = {};
	GetWindowRect(window, &r);
	m_captureRect = r;
}

// Wraps the WGC frame (the visible window rect) in the window's real DWM drop
// shadow. The shadow is not part of any capture surface, so it is recovered
// from the screen: the window is briefly cloaked (an instant, animation-free
// hide - no backdrops and nothing synthesized) to photograph the background
// underneath, and the shadow ring is solved from the visible/cloaked pair.
// Returns `frame` unchanged whenever shadow recovery is unavailable.
Gdiplus::Bitmap *Screenshot::attachShadow(HWND window, Gdiplus::Bitmap *frame) {
	RECT wrect = {};
	if (!GetWindowRect(window, &wrect)) { SHADOW_LOG("shadow: GetWindowRect failed\n"); return frame; }

	const int fw = (int)frame->GetWidth();
	const int fh = (int)frame->GetHeight();
	SHADOW_LOG("shadow: wrect=%ldx%ld frame=%dx%d\n",
		(long)(wrect.right-wrect.left), (long)(wrect.bottom-wrect.top), fw, fh);
	if (fw <= 0 || fh <= 0) return frame;

	// The frame sits inside the window rect: the leftover is the invisible
	// resize borders (0 on Windows < 10). The frame's outer edge is where the
	// shadow begins.
	int marginX = (wrect.right - wrect.left) - fw;
	int marginB = (wrect.bottom - wrect.top) - fh;
	if (marginX < 0) marginX = 0;
	if (marginB < 0) marginB = 0;

	RECT vis = {};
	vis.left   = wrect.left + marginX / 2;
	vis.right  = wrect.right - (marginX - marginX / 2);
	vis.top    = wrect.top;
	vis.bottom = wrect.bottom - marginB;
	if (vis.right <= vis.left || vis.bottom <= vis.top) return frame;

	// Some window rects (e.g. maximized with off-screen frame overhang) are
	// larger than the frame in ways the margin split can't model - never let
	// the visible rect exceed the frame itself.
	while (vis.right - vis.left > fw) { vis.left++; vis.right--; }
	while (vis.bottom - vis.top > fh) { vis.bottom--; }

	// Photograph a padded region around the window; the shadow's true extent
	// is measured from the pixel data, so no hardcoded shadow size is needed.
	const int PAD = 96;
	RECT desktop = CppShot::getDesktopRect();
	RECT cap = { vis.left - PAD, vis.top - PAD, vis.right + PAD, vis.bottom + PAD };
	if (cap.left   < desktop.left)   cap.left   = desktop.left;
	if (cap.top    < desktop.top)    cap.top    = desktop.top;
	if (cap.right  > desktop.right)  cap.right  = desktop.right;
	if (cap.bottom > desktop.bottom) cap.bottom = desktop.bottom;

	// The taskbar loses the window's button while it is hidden - keep the
	// region inside the work area so that shuffle can't pollute the shadow.
	// (Monitor functions are resolved at runtime: they don't exist on Win98.)
	typedef HMONITOR (WINAPI *PFN_MonitorFromWindow)(HWND, DWORD);
	typedef BOOL (WINAPI *PFN_GetMonitorInfoA)(HMONITOR, MONITORINFO *);
	HMODULE hUser32 = GetModuleHandleA("user32.dll");
	PFN_MonitorFromWindow pMonitorFromWindow = hUser32 ? (PFN_MonitorFromWindow)GetProcAddress(hUser32, "MonitorFromWindow") : NULL;
	PFN_GetMonitorInfoA pGetMonitorInfoA = hUser32 ? (PFN_GetMonitorInfoA)GetProcAddress(hUser32, "GetMonitorInfoA") : NULL;
	if (pMonitorFromWindow && pGetMonitorInfoA) {
		HMONITOR hmon = pMonitorFromWindow(window, MONITOR_DEFAULTTONEAREST);
		MONITORINFO mi = {};
		mi.cbSize = sizeof(mi);
		if (pGetMonitorInfoA(hmon, &mi)) {
			if (cap.bottom > mi.rcWork.bottom) cap.bottom = mi.rcWork.bottom;
		}
	}

	const int cw = cap.right - cap.left;
	const int ch = cap.bottom - cap.top;
	if (cw <= 0 || ch <= 0) return frame;

	// Fullscreen windows have no shadow to recover - and hiding them would
	// only cause a needless mode-switch flicker.
	if (cap.left >= vis.left && cap.top >= vis.top &&
		cap.right <= vis.right && cap.bottom <= vis.bottom) {
		SHADOW_LOG("shadow: window covers the screen\n");
		return frame;
	}

	ScreenDib shotFg;
	if (!captureScreenRectDib(cap, shotFg)) { SHADOW_LOG("shadow: fg BitBlt failed\n"); return frame; }

	// Hide the window just long enough to photograph what's underneath.
	// (Cloaking / display affinity would avoid the blink, but Windows
	// restricts both to windows owned by the calling process.)
	ShowWindow(window, SW_HIDE);
	if (IsWindowVisible(window)) {
		SHADOW_LOG("shadow: window refused to hide\n");
		ShowWindow(window, SW_SHOW);
		shotFg.release();
		return frame;
	}
	Sleep(150);

	ScreenDib shotBg;
	bool bgOk = captureScreenRectDib(cap, shotBg);

	ShowWindow(window, SW_SHOW);
	SetForegroundWindow(window); // hiding moved the focus away - give it back

	if (!bgOk) {
		SHADOW_LOG("shadow: bg BitBlt failed\n");
		shotFg.release();
		return frame;
	}

	// Visible rect in capture-region coordinates. The unclamped values place
	// the frame within the output; the scan seeds must be clamped into the
	// region because maximized windows hang their frame outside the screen.
	const int visL0 = vis.left - cap.left;
	const int visT0 = vis.top - cap.top;
	const int visR0 = vis.right - cap.left;
	const int visB0 = vis.bottom - cap.top;

	int seedL = visL0 < 0 ? 0 : visL0;
	int seedT = visT0 < 0 ? 0 : visT0;
	int seedR = visR0 > cw ? cw : visR0;
	int seedB = visB0 > ch ? ch : visB0;
	if (seedR <= seedL || seedB <= seedT) {
		shotFg.release();
		shotBg.release();
		return frame; // window entirely off-screen
	}

	// No room for a shadow around the window (fullscreen/maximized)
	if (seedL == 0 && seedT == 0 && seedR == cw && seedB == ch) {
		SHADOW_LOG("shadow: window fills the capture region\n");
		shotFg.release();
		shotBg.release();
		return frame;
	}

	// Grow the shadow bounding box outward from the visible rect: a column/row
	// is part of the shadow while any of its pixels changed between the two
	// photographs.
	auto differs = [&](int x, int y) -> bool {
		const BYTE *fg = shotFg.bits + ((size_t)y * cw + x) * 4;
		const BYTE *bg = shotBg.bits + ((size_t)y * cw + x) * 4;
		for (int c = 0; c < 3; c++) {
			int d = (int)fg[c] - (int)bg[c];
			if (d < 0) d = -d;
			if (d >= 3) return true;
		}
		return false;
	};

	const int MAXGROW = 96; // shadows are never larger than this
	int L = seedL, T = seedT, R = seedR, B = seedB;
	SHADOW_LOG("shadow: seeds L%d T%d R%d B%d (cw=%d ch=%d)\n", L, T, R, B, cw, ch);
	while (L > visL0 - MAXGROW && L > 0)  { bool any = false; for (int y = T; y < B; y++) if (differs(L - 1, y)) { any = true; break; } if (!any) break; L--; }
	while (R < visR0 + MAXGROW && R < cw) { bool any = false; for (int y = T; y < B; y++) if (differs(R, y))     { any = true; break; } if (!any) break; R++; }
	while (T > visT0 - MAXGROW && T > 0)  { bool any = false; for (int x = L; x < R; x++) if (differs(x, T - 1)) { any = true; break; } if (!any) break; T--; }
	while (B < visB0 + MAXGROW && B < ch) { bool any = false; for (int x = L; x < R; x++) if (differs(x, B))     { any = true; break; } if (!any) break; B++; }

	if (L == seedL && T == seedT && R == seedR && B == seedB) {
		// No shadow found (e.g. maximized window)
		SHADOW_LOG("shadow: none detected\n");
		shotFg.release();
		shotBg.release();
		return frame;
	}

	const int outW = R - L;
	const int outH = B - T;
	SHADOW_LOG("shadow: extent L%d T%d R%d B%d -> %dx%d\n", L, T, R, B, outW, outH);

	Gdiplus::Bitmap *out = new Gdiplus::Bitmap(outW, outH, PixelFormat32bppPARGB);
	if (out->GetLastStatus() != Gdiplus::Ok) {
		delete out;
		shotFg.release();
		shotBg.release();
		return frame;
	}

	Gdiplus::Rect outRc(0, 0, outW, outH);
	Gdiplus::BitmapData outData;
	Gdiplus::Rect frameRc(0, 0, fw, fh);
	Gdiplus::BitmapData frameData;

	bool outLocked = out->LockBits(&outRc, Gdiplus::ImageLockModeWrite, PixelFormat32bppPARGB, &outData) == Gdiplus::Ok;
	bool frameLocked = outLocked && frame->LockBits(&frameRc, Gdiplus::ImageLockModeRead, PixelFormat32bppPARGB, &frameData) == Gdiplus::Ok;
	bool locked = outLocked && frameLocked;

	if (locked) {
		const int offX = visL0 - L; // frame origin within the output
		const int offY = visT0 - T;

		for (int y = 0; y < outH; y++) {
			BYTE *outRow = (BYTE *)outData.Scan0 + y * outData.Stride;
			const BYTE *fgRow = shotFg.bits + ((size_t)(y + T) * cw + L) * 4;
			const BYTE *bgRow = shotBg.bits + ((size_t)(y + T) * cw + L) * 4;

			for (int x = 0; x < outW; x++) {
				BYTE *dst = outRow + x * 4;

				int fx = x - offX, fy = y - offY;
				if (fx >= 0 && fx < fw && fy >= 0 && fy < fh) {
					// Window content (premultiplied, straight from the frame)
					const BYTE *src = (const BYTE *)frameData.Scan0 + fy * frameData.Stride + fx * 4;
					dst[0] = src[0]; dst[1] = src[1]; dst[2] = src[2]; dst[3] = src[3];
				} else {
					// Shadow ring: screen = (1-a)*background + a*black
					// -> a = 1 - screen/background per channel.
					const BYTE *fg = fgRow + x * 4;
					const BYTE *bg = bgRow + x * 4;
					int alpha = 0;
					for (int c = 0; c < 3; c++) {
						if (bg[c] > 16) {
							int a = 255 - (int)fg[c] * 255 / bg[c];
							if (a > alpha) alpha = a;
						}
					}
					if (alpha < 0) alpha = 0;
					if (alpha > 255) alpha = 255;
					dst[0] = 0; dst[1] = 0; dst[2] = 0; dst[3] = (BYTE)alpha;
				}
			}
		}
	}

	if (outLocked)   out->UnlockBits(&outData);
	if (frameLocked) frame->UnlockBits(&frameData);

	shotFg.release();
	shotBg.release();

	if (!locked) {
		delete out;
		return frame;
	}

	delete frame;
	return out;
}

// Captures the window itself with PrintWindow instead of photographing it
// against black/white backdrops. No flicker, and the window may be occluded.
// The resulting bitmap keeps real alpha where the window surface has it
// (rounded corners, transparent/translucent surfaces).
void Screenshot::captureDirect(HWND window) {
	delete m_image;
	m_image = nullptr;
	m_window = window;

	if (!IsWindow(window)) return;

	RECT wr = {};
	if (!GetWindowRect(window, &wr)) return;
	int width  = wr.right  - wr.left;
	int height = wr.bottom - wr.top;
	if (width <= 0 || height <= 0) return;

	int delay = CppShot::getRegistryInt("Delay", 0);
	if (delay > 0)
		Sleep((DWORD)delay);

	HMODULE hUser32 = GetModuleHandleA("user32.dll");
	PFN_PrintWindow pPrintWindow = hUser32
		? (PFN_PrintWindow)GetProcAddress(hUser32, "PrintWindow")
		: NULL;
	if (!pPrintWindow) return;

	// A 32bpp top-down DIB section gives us a real alpha channel to render into
	// (CreateCompatibleBitmap would not preserve one reliably).
	BITMAPINFO bmi = {};
	bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biWidth       = width;
	bmi.bmiHeader.biHeight      = -height;
	bmi.bmiHeader.biPlanes      = 1;
	bmi.bmiHeader.biBitCount    = 32;
	bmi.bmiHeader.biCompression = BI_RGB;

	BYTE* bits = NULL;
	HDC hdcScreen = GetDC(HWND_DESKTOP);
	HBITMAP hDib  = CreateDIBSection(hdcScreen, &bmi, DIB_RGB_COLORS, (void**)&bits, NULL, 0);
	HDC memdc     = CreateCompatibleDC(hdcScreen);
	ReleaseDC(HWND_DESKTOP, hdcScreen);

	if (!hDib || !bits || !memdc) {
		if (hDib)  DeleteObject(hDib);
		if (memdc) DeleteDC(memdc);
		return;
	}

	size_t pixelBytes = (size_t)width * (size_t)height * 4;
	std::memset(bits, 0, pixelBytes);

	HGDIOBJ oldBmp = SelectObject(memdc, hDib);

	// PW_RENDERFULLCONTENT (Win8.1+) renders the real DWM surface incl. alpha
	// and hardware-accelerated content; retry with the classic flags on failure.
	BOOL ok = pPrintWindow(window, memdc, PW_RENDERFULLCONTENT);
	if (!ok) {
		std::memset(bits, 0, pixelBytes);
		ok = pPrintWindow(window, memdc, 0);
	}

	SelectObject(memdc, oldBmp);
	DeleteDC(memdc);

	if (!ok) {
		DeleteObject(hDib);
		return;
	}

	m_image = buildBitmapFromDIB(window, bits, width, height);
	DeleteObject(hDib);

	if (m_image) m_captureRect = wr;
}

Gdiplus::Bitmap* Screenshot::buildBitmapFromDIB(HWND window, BYTE* bits, int width, int height) {
	const size_t pixelCount = (size_t)width * (size_t)height;

	bool anyAlpha    = false; // at least one pixel with alpha != 0
	bool anySemi     = false; // at least one pixel with 0 < alpha < 255
	bool pmViolation = false; // color channel exceeds alpha -> not premultiplied
	bool anyRgb      = false; // at least one pixel with non-zero color

	for (size_t i = 0; i < pixelCount; i++) {
		const BYTE b = bits[i * 4 + 0];
		const BYTE g = bits[i * 4 + 1];
		const BYTE r = bits[i * 4 + 2];
		const BYTE a = bits[i * 4 + 3];

		if (r || g || b) anyRgb = true;
		if (a) {
			anyAlpha = true;
			if (a != 255) {
				anySemi = true;
				if (r > a + 1 || g > a + 1 || b > a + 1) pmViolation = true;
			}
		}
		if (anyRgb && anyAlpha && pmViolation) break; // nothing more to learn
	}

	// Completely empty result (cloaked/unresponsive window) -> let the caller
	// fall back to the backdrop method.
	if (!anyRgb && !anyAlpha) return nullptr;

	if (!anyAlpha) {
		// Classic GDI path: PrintWindow left the alpha channel untouched, so
		// synthesize it from the window's shape instead.
		applyWindowShapeAsAlpha(window, bits, width, height);
		return copyToGdiplusBitmap(bits, width, height, PixelFormat32bppARGB);
	}

	if (anySemi && !pmViolation) {
		// DWM surfaces hold premultiplied alpha — hand it to GDI+ as-is and it
		// will be converted to straight alpha when the PNG is written.
		return copyToGdiplusBitmap(bits, width, height, PixelFormat32bppPARGB);
	}

	// Straight alpha (e.g. fully opaque window with transparent corners).
	return copyToGdiplusBitmap(bits, width, height, PixelFormat32bppARGB);
}

void Screenshot::applyWindowShapeAsAlpha(HWND window, BYTE* bits, int width, int height) {
	const size_t pixelCount = (size_t)width * (size_t)height;

	HMODULE hUser32 = GetModuleHandleA("user32.dll");
	PFN_GetWindowRgn pGetWindowRgn = hUser32
		? (PFN_GetWindowRgn)GetProcAddress(hUser32, "GetWindowRgn")
		: NULL;

	HRGN rgn = CreateRectRgn(0, 0, 0, 0);
	int rgnType = ERROR;
	if (pGetWindowRgn)
		rgnType = pGetWindowRgn(window, rgn);

	if (rgnType == SIMPLEREGION || rgnType == COMPLEXREGION) {
		// Start fully transparent, then mark the region's rects opaque.
		for (size_t i = 0; i < pixelCount; i++)
			bits[i * 4 + 3] = 0;

		DWORD dataSize = GetRegionData(rgn, 0, NULL);
		std::vector<BYTE> buffer(dataSize);
		RGNDATA* data = (RGNDATA*)buffer.data();

		if (dataSize && GetRegionData(rgn, dataSize, data) == dataSize && data->rdh.nCount > 0) {
			const RECT* rects = (const RECT*)data->Buffer;
			for (DWORD i = 0; i < data->rdh.nCount; i++) {
				RECT r = rects[i];
				if (r.left < 0) r.left = 0;
				if (r.top  < 0) r.top  = 0;
				if (r.right  > width)  r.right  = width;
				if (r.bottom > height) r.bottom = height;

				for (int y = r.top; y < r.bottom; y++)
					for (int x = r.left; x < r.right; x++)
						bits[((size_t)y * width + x) * 4 + 3] = 255;
			}
		} else {
			for (size_t i = 0; i < pixelCount; i++)
				bits[i * 4 + 3] = 255;
		}
	} else {
		// No shaping region -> plain rectangular opaque window.
		for (size_t i = 0; i < pixelCount; i++)
			bits[i * 4 + 3] = 255;
	}

	DeleteObject(rgn);
}

Gdiplus::Bitmap* Screenshot::copyToGdiplusBitmap(const BYTE* bits, int width, int height, Gdiplus::PixelFormat format) {
	Gdiplus::Bitmap* bmp = new Gdiplus::Bitmap(width, height, format);
	if (bmp->GetLastStatus() != Gdiplus::Ok) {
		delete bmp;
		return nullptr;
	}

	Gdiplus::Rect rc(0, 0, width, height);
	Gdiplus::BitmapData data;
	if (bmp->LockBits(&rc, Gdiplus::ImageLockModeWrite, format, &data) != Gdiplus::Ok) {
		delete bmp;
		return nullptr;
	}

	const size_t rowBytes = (size_t)width * 4;
	for (int y = 0; y < height; y++) {
		BYTE* dst = (BYTE*)data.Scan0 + y * data.Stride;
		std::memcpy(dst, bits + (size_t)y * rowBytes, rowBytes);
	}

	bmp->UnlockBits(&data);
	return bmp;
}

void Screenshot::captureRect(RECT rect) {
    delete m_image;
    m_captureRect = rect;

	int delay = CppShot::getRegistryInt("Delay", 0);
	if (delay > 0)
		Sleep((DWORD)delay);

    int width  = rect.right  - rect.left;
    int height = rect.bottom - rect.top;

    HDC hdc    = GetDC(HWND_DESKTOP);
    HDC memdc  = CreateCompatibleDC(hdc);
    HBITMAP hbitmap = CreateCompatibleBitmap(hdc, width, height);

    SelectObject(memdc, hbitmap);
    BitBlt(memdc, 0, 0, width, height, hdc, rect.left, rect.top, SRCCOPY);

    DeleteDC(memdc);
    ReleaseDC(HWND_DESKTOP, hdc);

    m_image = new Gdiplus::Bitmap(hbitmap, NULL);
}

bool Screenshot::saveBitmap(Gdiplus::Bitmap* image, const std::string& path) {
	if (!image) return false;

	// This is supposed to be gathered from the OS but the encoder CLSID has never changed, so this is safe enough
	// The old version of cppshot had a bug that made it use the hardcoded one anyway
    CLSID pngEncoder = {0x557cf406, 0x1a04, 0x11d3, {0x9a, 0x73, 0x00, 0x00, 0xf8, 0x1e, 0xf3, 0x2e} } ;

    // Convert std::string (UTF-8 or ANSI) to std::wstring
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, NULL, 0);
    std::wstring wpath(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, &wpath[0], size_needed);

    return image->Save(wpath.c_str(), &pngEncoder, NULL) == Gdiplus::Ok;
}

void Screenshot::save(const std::string& path) {
	saveBitmap(m_image, path);
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