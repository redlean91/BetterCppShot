#pragma once

#include <windows.h>
#include <gdiplus.h>

// Direct window capture through Windows.Graphics.Capture.
// Captures the window itself - its content (including DirectComposition and
// hardware accelerated surfaces), the rounded corners and the original DWM
// drop shadow - with real per-pixel alpha. No backdrops, no screen region.
//
// Available on Windows 10 1803+. Everything is resolved at runtime, so the
// binary keeps working on older systems (callers should fall back to
// Screenshot::captureDirect / the classic backdrop method).
namespace WgcCapture {
	bool isSupported();
	Gdiplus::Bitmap *captureWindow(HWND window);

	// True when the window's display runs in HDR. On HDR displays WGC captures
	// the composited (HDR-boosted, blown-out) presentation of the window and
	// GDI screen reads come back black, while PrintWindow reads the app's own
	// SDR surface - so capture order should prefer PrintWindow there.
	bool isHdrDisplay(HWND window);
}
