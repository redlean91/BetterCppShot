#include "WgcCapture.h"

#include <d3d11.h>
#include <dxgi.h>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <vector>

static bool wgcDebug() {
	static int dbg = -1;
	if (dbg < 0) dbg = getenv("BCS_WGC_DEBUG") ? 1 : 0;
	return dbg != 0;
}
#define WGC_LOG(...) do { if (wgcDebug()) fprintf(stderr, __VA_ARGS__); } while (0)

// ============================================================================
// Minimal Windows.Graphics.Capture ABI declarations.
//
// The TDM-GCC 5.1 toolchain predates the WinRT SDK headers, so the handful of
// interfaces used here is declared manually. Vtable layouts and GUIDs match
// the public ABI (windows.graphics.capture.h / windows.graphics.directx.
// direct3d11.h / windows.graphics.capture.interop.h). All entry points are
// resolved with LoadLibrary/GetProcAddress so the executable still loads on
// systems without these DLLs (anything before Windows 10).
// ============================================================================

typedef struct WgcHString__ *WgcHString;

enum WgcTrustLevel {
	WgcTrustLevel_Base = 0,
	WgcTrustLevel_Partial = 1,
	WgcTrustLevel_Full = 2
};

struct WgcSizeInt32 {
	INT32 Width;
	INT32 Height;
};

struct IWgcInspectable : public IUnknown {
	virtual HRESULT STDMETHODCALLTYPE GetIids(ULONG *iidCount, IID **iids) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetRuntimeClassName(WgcHString *className) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetTrustLevel(WgcTrustLevel *trustLevel) = 0;
};

struct IWgcClosable : public IWgcInspectable {
	virtual HRESULT STDMETHODCALLTYPE Close() = 0;
};

struct IWgcDirect3DDevice : public IWgcInspectable {
	virtual HRESULT STDMETHODCALLTYPE Trim() = 0;
};

struct IWgcDirect3DSurface : public IWgcInspectable {
	virtual HRESULT STDMETHODCALLTYPE get_Description(void *value) = 0;
};

struct IWgcDxgiInterfaceAccess : public IUnknown {
	virtual HRESULT STDMETHODCALLTYPE GetInterface(REFIID iid, void **p) = 0;
};

struct IWgcCaptureItem : public IWgcInspectable {
	virtual HRESULT STDMETHODCALLTYPE get_DisplayName(WgcHString *value) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_Size(WgcSizeInt32 *value) = 0;
	virtual HRESULT STDMETHODCALLTYPE add_Closed(IUnknown *handler, INT64 *token) = 0;
	virtual HRESULT STDMETHODCALLTYPE remove_Closed(INT64 token) = 0;
};

struct IWgcCaptureFrame : public IWgcInspectable {
	virtual HRESULT STDMETHODCALLTYPE get_Surface(IWgcDirect3DSurface **value) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_SystemRelativeTime(INT64 *value) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_ContentSize(WgcSizeInt32 *value) = 0;
};

struct IWgcCaptureSession : public IWgcInspectable {
	virtual HRESULT STDMETHODCALLTYPE StartCapture() = 0;
};

struct IWgcCaptureSession2 : public IWgcInspectable {
	virtual HRESULT STDMETHODCALLTYPE get_IsCursorCaptureEnabled(unsigned char *value) = 0;
	virtual HRESULT STDMETHODCALLTYPE put_IsCursorCaptureEnabled(unsigned char value) = 0;
};

struct IWgcCaptureFramePool : public IWgcInspectable {
	virtual HRESULT STDMETHODCALLTYPE Recreate(IWgcDirect3DDevice *device, INT32 pixelFormat, INT32 numberOfBuffers, WgcSizeInt32 size) = 0;
	virtual HRESULT STDMETHODCALLTYPE TryGetNextFrame(IWgcCaptureFrame **result) = 0;
	virtual HRESULT STDMETHODCALLTYPE add_FrameArrived(IUnknown *handler, INT64 *token) = 0;
	virtual HRESULT STDMETHODCALLTYPE remove_FrameArrived(INT64 token) = 0;
	virtual HRESULT STDMETHODCALLTYPE CreateCaptureSession(IWgcCaptureItem *item, IWgcCaptureSession **result) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_DispatcherQueue(IUnknown **value) = 0;
};

struct IWgcFramePoolStatics2 : public IWgcInspectable {
	virtual HRESULT STDMETHODCALLTYPE CreateFreeThreaded(IWgcDirect3DDevice *device, INT32 pixelFormat, INT32 numberOfBuffers, WgcSizeInt32 size, IWgcCaptureFramePool **result) = 0;
};

struct IWgcCaptureItemInterop : public IUnknown {
	virtual HRESULT STDMETHODCALLTYPE CreateForWindow(HWND window, REFIID iid, void **result) = 0;
	virtual HRESULT STDMETHODCALLTYPE CreateForMonitor(HMONITOR monitor, REFIID iid, void **result) = 0;
};

// ITypedEventHandler<IDirect3D11CaptureFramePool*, IInspectable*>
// (a delegate interface: IUnknown + Invoke)
struct IWgcFrameArrivedHandler : public IUnknown {
	virtual HRESULT STDMETHODCALLTYPE Invoke(IWgcCaptureFramePool *sender, IWgcInspectable *args) = 0;
};

static const GUID IID_IWgcCaptureItemInterop  = {0x3628e81b, 0x3cac, 0x4c60, {0xb7, 0xf4, 0x23, 0xce, 0x0e, 0x0c, 0x33, 0x56}};
static const GUID IID_IWgcCaptureItem         = {0x79c3f95b, 0x31f7, 0x4ec2, {0xa4, 0x64, 0x63, 0x2e, 0xf5, 0xd3, 0x07, 0x60}};
static const GUID IID_IWgcCaptureFramePool    = {0x24eb6d22, 0x1975, 0x422e, {0x82, 0xe7, 0x78, 0x0d, 0xbd, 0x8d, 0xdf, 0x24}};
static const GUID IID_IWgcFramePoolStatics2   = {0x589b103f, 0x6bbc, 0x5df5, {0xa9, 0x91, 0x02, 0xe2, 0x8b, 0x3b, 0x66, 0xd5}};
static const GUID IID_IWgcCaptureFrame        = {0xfa50c623, 0x38da, 0x4b32, {0xac, 0xf3, 0xfa, 0x97, 0x34, 0xad, 0x80, 0x0e}};
static const GUID IID_IWgcCaptureSession      = {0x814e42a9, 0xf70f, 0x4ad7, {0x93, 0x9b, 0xfd, 0xdc, 0xc6, 0xeb, 0x88, 0x0d}};
static const GUID IID_IWgcCaptureSession2     = {0x2c39ae40, 0x7d2e, 0x5044, {0x80, 0x4e, 0x8b, 0x67, 0x99, 0xd4, 0xcf, 0x9e}};
static const GUID IID_IWgcDirect3DDevice      = {0xa37624ab, 0x8d5f, 0x4650, {0x9d, 0x3e, 0x9e, 0xae, 0x3d, 0x9b, 0xc6, 0x70}};
static const GUID IID_IWgcDirect3DSurface     = {0x0bf4a146, 0x13c1, 0x4694, {0xbe, 0xe3, 0x7a, 0xbf, 0x15, 0xea, 0xf5, 0x86}};
static const GUID IID_IWgcDxgiInterfaceAccess = {0xa9b3d012, 0x3df2, 0x4ee3, {0xb8, 0xd1, 0x86, 0x95, 0xf4, 0x57, 0xd3, 0xc1}};
static const GUID IID_IWgcClosable            = {0x30d5a829, 0x7fa4, 0x4026, {0x83, 0xbb, 0xd7, 0x5b, 0xae, 0x4e, 0xa9, 0x9e}};
static const GUID IID_IWgcDxgiDevice          = {0x54ec77fa, 0x1377, 0x44e6, {0x8c, 0x32, 0x88, 0xfd, 0x5f, 0x44, 0xc8, 0x4c}};
static const GUID IID_IWgcD3D11Texture2D      = {0x6f15aaf2, 0xd208, 0x4e89, {0x9a, 0xb4, 0x48, 0x95, 0x35, 0xd3, 0x4f, 0x9c}};
static const GUID IID_IWgcFrameArrivedHandler = {0x51a947f7, 0x79cf, 0x5a3e, {0xa3, 0xa5, 0x12, 0x89, 0xcf, 0xa6, 0xdf, 0xe8}};
static const GUID IID_IWgcUnknown             = {0x00000000, 0x0000, 0x0000, {0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};
static const GUID IID_IWgcAgileObject         = {0x94ea2b94, 0xe9cc, 0x49e0, {0xc0, 0xff, 0xee, 0x64, 0xca, 0x8f, 0x5b, 0x90}};

// Signals a Win32 event whenever the frame pool produces a frame.
// Raw agile COM object - called on WGC's internal worker thread.
class WgcFrameArrivedHandler : public IWgcFrameArrivedHandler {
	LONG m_ref;
	HANDLE m_event;
public:
	WgcFrameArrivedHandler(HANDLE ev) : m_ref(1), m_event(ev) {}

	void setEvent(HANDLE ev) { m_event = ev; }

	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppv) override {
		if (!ppv) return E_POINTER;
		if (IsEqualGUID(riid, IID_IWgcUnknown) || IsEqualGUID(riid, IID_IWgcFrameArrivedHandler) ||
			IsEqualGUID(riid, IID_IWgcAgileObject)) {
			*ppv = static_cast<IUnknown *>(this);
			AddRef();
			return S_OK;
		}
		*ppv = NULL;
		return E_NOINTERFACE;
	}
	ULONG STDMETHODCALLTYPE AddRef() override { return InterlockedIncrement(&m_ref); }
	ULONG STDMETHODCALLTYPE Release() override {
		ULONG r = InterlockedDecrement(&m_ref);
		// Never actually deleted: the frame pool's worker thread may still
		// invoke us right after remove_FrameArrived/Release returns.
		return r;
	}
	HRESULT STDMETHODCALLTYPE Invoke(IWgcCaptureFramePool *sender, IWgcInspectable *args) override {
		(void)sender; (void)args;
		if (m_event) SetEvent(m_event);
		return S_OK;
	}
};

// Process-lifetime handler + event: avoids a use-after-free race where the
// pool's worker thread fires Invoke just after a capture finishes.
static WgcFrameArrivedHandler *frameHandler() {
	static WgcFrameArrivedHandler *handler = new WgcFrameArrivedHandler(NULL);
	return handler;
}
static HANDLE frameEvent() {
	static HANDLE ev = CreateEvent(NULL, FALSE, FALSE, NULL);
	return ev;
}

const INT32 WgcPixelFormat_B8G8R8A8UIntNormalized = 87;

// ---------------------------------------------------------------------------
// HDR plumbing: on HDR displays the compositor works in scRGB where SDR white
// sits above 1.0, so frames are captured as FP16 and tone-mapped to SDR
// ourselves (normalized by the display's SDR white level).
// Everything below is resolved at runtime to keep the binary loadable on
// systems without these APIs.
// ---------------------------------------------------------------------------

typedef LONG (WINAPI *PFN_WgcQueryDisplayConfig)(UINT32, UINT32 *, void *, UINT32 *, void *, void *);
typedef LONG (WINAPI *PFN_WgcGetDisplayConfigBufferSizes)(UINT32, UINT32 *, UINT32 *);
typedef LONG (WINAPI *PFN_WgcDisplayConfigGetDeviceInfo)(void *);
typedef HMONITOR (WINAPI *PFN_WgcMonitorFromWindow)(HWND, DWORD);
typedef BOOL (WINAPI *PFN_WgcGetMonitorInfoA)(HMONITOR, void *);

struct WgcDeviceInfoHeader { DWORD type; DWORD size; LUID adapterId; DWORD id; };
struct WgcSourceDeviceName { WgcDeviceInfoHeader header; wchar_t viewGdiDeviceName[32]; };
struct WgcAdvancedColorInfo {
	WgcDeviceInfoHeader header;
	union {
		struct { DWORD supported : 1; DWORD enabled : 1; DWORD wideColor : 1; DWORD forceDisabled : 1; DWORD reserved : 28; } s;
		DWORD value;
	} u;
	DWORD colorEncoding;
	DWORD depth;
};
struct WgcMonitorInfoEx {
	DWORD cbSize; RECT rcMonitor; RECT rcWork; DWORD dwFlags; char szDevice[32];
};
struct WgcPathSourceInfo { LUID adapterId; DWORD id; DWORD modeInfoIdx; DWORD statusFlags; };
struct WgcRational { DWORD num, den; };
struct WgcPathTargetInfo {
	LUID adapterId; DWORD id; DWORD modeInfoIdx;
	DWORD outputTechnology; DWORD rotation; DWORD scaling;
	WgcRational refreshRate; DWORD scanLineOrdering;
	BOOL targetAvailable; DWORD statusFlags;
};
struct WgcPathInfo { WgcPathSourceInfo sourceInfo; WgcPathTargetInfo targetInfo; DWORD flags; };

// Returns whether the window's monitor runs in HDR.
static bool getMonitorHdr(HWND window) {
	HMODULE user32 = GetModuleHandleA("user32.dll");
	if (!user32) return false;
	PFN_WgcMonitorFromWindow pMonitorFromWindow = (PFN_WgcMonitorFromWindow)GetProcAddress(user32, "MonitorFromWindow");
	PFN_WgcGetMonitorInfoA pGetMonitorInfoA = (PFN_WgcGetMonitorInfoA)GetProcAddress(user32, "GetMonitorInfoA");
	PFN_WgcQueryDisplayConfig pQueryDisplayConfig = (PFN_WgcQueryDisplayConfig)GetProcAddress(user32, "QueryDisplayConfig");
	PFN_WgcGetDisplayConfigBufferSizes pGetDisplayConfigBufferSizes = (PFN_WgcGetDisplayConfigBufferSizes)GetProcAddress(user32, "GetDisplayConfigBufferSizes");
	PFN_WgcDisplayConfigGetDeviceInfo pDisplayConfigGetDeviceInfo = (PFN_WgcDisplayConfigGetDeviceInfo)GetProcAddress(user32, "DisplayConfigGetDeviceInfo");
	if (!pMonitorFromWindow || !pGetMonitorInfoA || !pQueryDisplayConfig || !pGetDisplayConfigBufferSizes || !pDisplayConfigGetDeviceInfo)
		return false;

	HMONITOR hmon = pMonitorFromWindow(window, 2 /*MONITOR_DEFAULTTONEAREST*/);
	WgcMonitorInfoEx mi = {};
	mi.cbSize = sizeof(mi);
	if (!pGetMonitorInfoA(hmon, &mi)) return false;

	// Map the GDI device name to a display config path (adapter + target)
	UINT32 numPaths = 0, numModes = 0;
	const UINT32 QDC_ONLY_ACTIVE_PATHS = 2;
	if (pGetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS, &numPaths, &numModes) != 0 || numPaths == 0)
		return false;

	std::vector<WgcPathInfo> paths(numPaths);
	std::vector<BYTE> modes((size_t)numModes * 128); // DISPLAYCONFIG_MODE_INFO is small; generous sizing
	if (pQueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS, &numPaths, paths.data(), &numModes, modes.data(), NULL) != 0)
		return false;

	LUID adapterId = {};
	DWORD targetId = 0;
	bool found = false;
	for (UINT32 i = 0; i < numPaths && !found; i++) {
		WgcSourceDeviceName name = {};
		name.header.type = 1; // DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME
		name.header.size = sizeof(name);
		name.header.adapterId = paths[i].sourceInfo.adapterId;
		name.header.id = paths[i].sourceInfo.id;
		if (pDisplayConfigGetDeviceInfo(&name) != 0) continue;

		char gdiName[32] = {};
		WideCharToMultiByte(CP_ACP, 0, name.viewGdiDeviceName, -1, gdiName, sizeof(gdiName), NULL, NULL);
		if (_stricmp(gdiName, mi.szDevice) == 0) {
			adapterId = paths[i].targetInfo.adapterId;
			targetId = paths[i].targetInfo.id;
			found = true;
		}
	}
	if (!found) return false;

	// Is HDR actually on for this display?
	WgcAdvancedColorInfo aci = {};
	aci.header.type = 9; // DISPLAYCONFIG_DEVICE_INFO_GET_ADVANCED_COLOR_INFO
	aci.header.size = sizeof(aci);
	aci.header.adapterId = adapterId;
	aci.header.id = targetId;
	if (pDisplayConfigGetDeviceInfo(&aci) != 0 || !aci.u.s.enabled)
		return false;

	return true;
}

bool WgcCapture::isHdrDisplay(HWND window) {
	return getMonitorHdr(window);
}

// ---------------------------------------------------------------------------
// Dynamically resolved entry points
// ---------------------------------------------------------------------------

typedef HRESULT (WINAPI *PFN_WgcRoInitialize)(int);
typedef HRESULT (WINAPI *PFN_WgcRoGetActivationFactory)(WgcHString, REFIID, void **);
typedef HRESULT (WINAPI *PFN_WgcWindowsCreateString)(LPCWSTR, UINT32, WgcHString *);
typedef HRESULT (WINAPI *PFN_WgcWindowsDeleteString)(WgcHString);
typedef HRESULT (WINAPI *PFN_WgcD3D11CreateDevice)(IDXGIAdapter *, D3D_DRIVER_TYPE, HMODULE, UINT, const D3D_FEATURE_LEVEL *, UINT, UINT, ID3D11Device **, D3D_FEATURE_LEVEL *, ID3D11DeviceContext **);
typedef HRESULT (WINAPI *PFN_WgcCreateDeviceFromDxgi)(IDXGIDevice *, IWgcInspectable **);

struct WgcApiTable {
	HMODULE combase = NULL;
	HMODULE d3d11 = NULL;
	PFN_WgcRoInitialize roInitialize = NULL;
	PFN_WgcRoGetActivationFactory roGetActivationFactory = NULL;
	PFN_WgcWindowsCreateString windowsCreateString = NULL;
	PFN_WgcWindowsDeleteString windowsDeleteString = NULL;
	PFN_WgcD3D11CreateDevice d3d11CreateDevice = NULL;
	PFN_WgcCreateDeviceFromDxgi createDeviceFromDxgi = NULL;
	bool supported = false;
};

static WgcApiTable g_wgc;
static bool g_wgcInit = false;

static void wgcInit() {
	if (g_wgcInit) return;
	g_wgcInit = true;

	g_wgc.combase = LoadLibraryA("combase.dll"); // Windows 8+
	g_wgc.d3d11   = LoadLibraryA("d3d11.dll");   // Vista+
	if (!g_wgc.combase || !g_wgc.d3d11) return;

	g_wgc.roInitialize           = (PFN_WgcRoInitialize)          GetProcAddress(g_wgc.combase, "RoInitialize");
	g_wgc.roGetActivationFactory = (PFN_WgcRoGetActivationFactory)GetProcAddress(g_wgc.combase, "RoGetActivationFactory");
	g_wgc.windowsCreateString    = (PFN_WgcWindowsCreateString)   GetProcAddress(g_wgc.combase, "WindowsCreateString");
	g_wgc.windowsDeleteString    = (PFN_WgcWindowsDeleteString)   GetProcAddress(g_wgc.combase, "WindowsDeleteString");
	g_wgc.d3d11CreateDevice      = (PFN_WgcD3D11CreateDevice)     GetProcAddress(g_wgc.d3d11,   "D3D11CreateDevice");
	g_wgc.createDeviceFromDxgi   = (PFN_WgcCreateDeviceFromDxgi)  GetProcAddress(g_wgc.d3d11,   "CreateDirect3D11DeviceFromDXGIDevice");

	if (!g_wgc.roInitialize || !g_wgc.roGetActivationFactory || !g_wgc.windowsCreateString ||
		!g_wgc.windowsDeleteString || !g_wgc.d3d11CreateDevice || !g_wgc.createDeviceFromDxgi)
		return;

	// RoGetActivationFactory requires the runtime to be initialized first
	g_wgc.roInitialize(1); // RO_INIT_MULTITHREADED

	// Probe the runtime class - it only exists on Windows 10 1803+
	static const wchar_t itemClass[] = L"Windows.Graphics.Capture.GraphicsCaptureItem";
	WgcHString hs = NULL;
	if (FAILED(g_wgc.windowsCreateString(itemClass, (UINT32)(sizeof(itemClass) / sizeof(itemClass[0]) - 1), &hs)))
		return;

	void *factory = NULL;
	HRESULT hr = g_wgc.roGetActivationFactory(hs, IID_IWgcCaptureItemInterop, &factory);
	g_wgc.windowsDeleteString(hs);

	if (FAILED(hr) || !factory) {
		WGC_LOG("WGC probe: RoGetActivationFactory failed hr=0x%08lx\n", (unsigned long)hr);
		return;
	}
	((IUnknown *)factory)->Release();

	g_wgc.supported = true;
}

// Minimal reference guard so every early return cleans COM objects up.
template <typename T>
struct WgcRef {
	T *p;
	WgcRef() : p(NULL) {}
	~WgcRef() { if (p) p->Release(); }
	T **operator&() { return &p; }
	operator T *() const { return p; }
	T *operator->() const { return p; }
};

// Calls IClosable::Close on destruction, on every code path.
template <typename T>
struct WgcCloser {
	T *p;
	WgcCloser() : p(NULL) {}
	void attach(T *obj) {
		if (obj) obj->AddRef();
		if (p) closeOne(p);
		p = obj;
	}
	~WgcCloser() { if (p) closeOne(p); }
	static void closeOne(T *obj) {
		IWgcClosable *c = NULL;
		if (SUCCEEDED(obj->QueryInterface(IID_IWgcClosable, (void **)&c)) && c) {
			c->Close();
			c->Release();
		}
		obj->Release();
	}
};

bool WgcCapture::isSupported() {
	wgcInit();
	return g_wgc.supported;
}

Gdiplus::Bitmap *WgcCapture::captureWindow(HWND window) {
	wgcInit();
	if (!g_wgc.supported || !IsWindow(window)) return NULL;

	g_wgc.roInitialize(1); // RO_INIT_MULTITHREADED; already-initialized is fine

	// --- D3D11 device (BGRA support is mandatory for WGC) ---
	const D3D_FEATURE_LEVEL levels[] = {
		D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0,
		D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0,
		D3D_FEATURE_LEVEL_9_3,  D3D_FEATURE_LEVEL_9_2, D3D_FEATURE_LEVEL_9_1
	};
	const UINT levelCount = sizeof(levels) / sizeof(levels[0]);

	WgcRef<ID3D11Device> device;
	WgcRef<ID3D11DeviceContext> context;
	HRESULT hr = g_wgc.d3d11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL,
		D3D11_CREATE_DEVICE_BGRA_SUPPORT, levels, levelCount,
		D3D11_SDK_VERSION, &device, NULL, &context);
	if (FAILED(hr)) {
		hr = g_wgc.d3d11CreateDevice(NULL, D3D_DRIVER_TYPE_WARP, NULL,
			D3D11_CREATE_DEVICE_BGRA_SUPPORT, levels, levelCount,
			D3D11_SDK_VERSION, &device, NULL, &context);
	}
	if (FAILED(hr)) { WGC_LOG("WGC: D3D11CreateDevice failed hr=0x%08lx\n", (unsigned long)hr); return NULL; }

	WgcRef<IDXGIDevice> dxgiDevice;
	if (FAILED(hr = device->QueryInterface(IID_IWgcDxgiDevice, (void **)&dxgiDevice)))
		{ WGC_LOG("WGC: QI IDXGIDevice failed hr=0x%08lx\n", (unsigned long)hr); return NULL; }

	WgcRef<IWgcInspectable> inspectable;
	if (FAILED(hr = g_wgc.createDeviceFromDxgi(dxgiDevice, &inspectable)))
		{ WGC_LOG("WGC: CreateDirect3D11DeviceFromDXGIDevice failed hr=0x%08lx\n", (unsigned long)hr); return NULL; }

	WgcRef<IWgcDirect3DDevice> winrtDevice;
	if (FAILED(hr = inspectable->QueryInterface(IID_IWgcDirect3DDevice, (void **)&winrtDevice)))
		{ WGC_LOG("WGC: QI IDirect3DDevice failed hr=0x%08lx\n", (unsigned long)hr); return NULL; }

	// --- capture item for the window ---
	static const wchar_t itemClass[] = L"Windows.Graphics.Capture.GraphicsCaptureItem";
	WgcHString hs = NULL;
	if (FAILED(hr = g_wgc.windowsCreateString(itemClass, (UINT32)(sizeof(itemClass) / sizeof(itemClass[0]) - 1), &hs)))
		{ WGC_LOG("WGC: WindowsCreateString failed hr=0x%08lx\n", (unsigned long)hr); return NULL; }

	WgcRef<IWgcCaptureItemInterop> interop;
	hr = g_wgc.roGetActivationFactory(hs, IID_IWgcCaptureItemInterop, (void **)&interop);
	g_wgc.windowsDeleteString(hs);
	if (FAILED(hr)) { WGC_LOG("WGC: interop factory failed hr=0x%08lx\n", (unsigned long)hr); return NULL; }

	WgcRef<IWgcCaptureItem> item;
	if (FAILED(hr = interop->CreateForWindow(window, IID_IWgcCaptureItem, (void **)&item)))
		{ WGC_LOG("WGC: CreateForWindow failed hr=0x%08lx\n", (unsigned long)hr); return NULL; }

	WgcSizeInt32 size = {};
	if (FAILED(hr = item->get_Size(&size)) || size.Width <= 0 || size.Height <= 0)
		{ WGC_LOG("WGC: get_Size failed hr=0x%08lx size=%ldx%ld\n", (unsigned long)hr, (long)size.Width, (long)size.Height); return NULL; }

	// On HDR displays WGC captures the HDR-boosted presentation of the window
	// which comes out overexposed; callers should prefer PrintWindow there
	// (see isHdrDisplay). FP16 frames were found to be unreliable as well, so
	// the pool is always plain BGRA8.
	INT32 pixelFormat = WgcPixelFormat_B8G8R8A8UIntNormalized;

	// --- frame pool + session ---
	static const wchar_t poolClass[] = L"Windows.Graphics.Capture.Direct3D11CaptureFramePool";
	WgcHString hsPool = NULL;
	if (FAILED(hr = g_wgc.windowsCreateString(poolClass, (UINT32)(sizeof(poolClass) / sizeof(poolClass[0]) - 1), &hsPool)))
		{ WGC_LOG("WGC: CreateString(pool) failed hr=0x%08lx\n", (unsigned long)hr); return NULL; }

	WgcRef<IWgcFramePoolStatics2> statics2;
	hr = g_wgc.roGetActivationFactory(hsPool, IID_IWgcFramePoolStatics2, (void **)&statics2);
	g_wgc.windowsDeleteString(hsPool);
	if (FAILED(hr)) { WGC_LOG("WGC: pool statics2 failed hr=0x%08lx\n", (unsigned long)hr); return NULL; }

	WgcRef<IWgcCaptureFramePool> pool;
	// One buffer: the pool can't produce another frame while we hold ours,
	// so its worker thread can never race our read of the D3D texture.
	hr = statics2->CreateFreeThreaded(winrtDevice, pixelFormat, 1, size, &pool);
	if (FAILED(hr))
		{ WGC_LOG("WGC: CreateFreeThreaded failed hr=0x%08lx\n", (unsigned long)hr); return NULL; }

	WgcRef<IWgcCaptureSession> session;
	if (FAILED(hr = pool->CreateCaptureSession(item, &session)))
		{ WGC_LOG("WGC: CreateCaptureSession failed hr=0x%08lx\n", (unsigned long)hr); return NULL; }

	// Guarantee session/pool Close() on every exit path from here on - an
	// orphaned capture session keeps producing frames on WGC worker threads
	// and eventually crashes into released objects.
	WgcCloser<IWgcCaptureSession> sessionCleaner;
	sessionCleaner.attach(session);
	WgcCloser<IWgcCaptureFramePool> poolCleaner;
	poolCleaner.attach(pool);

	// Hide the cursor from the capture when supported (1809+)
	WgcRef<IWgcCaptureSession2> session2;
	if (SUCCEEDED(session->QueryInterface(IID_IWgcCaptureSession2, (void **)&session2)) && session2.p)
		session2->put_IsCursorCaptureEnabled(0);

	// The frame pool only delivers frames to a FrameArrived subscriber -
	// plain TryGetNextFrame polling never sees them.
	HANDLE ev = frameEvent();
	ResetEvent(ev);
	WgcFrameArrivedHandler *handler = frameHandler();
	handler->setEvent(ev);

	INT64 arrivedToken = 0;
	if (FAILED(hr = pool->add_FrameArrived(handler, &arrivedToken))) {
		WGC_LOG("WGC: add_FrameArrived failed hr=0x%08lx\n", (unsigned long)hr);
		return NULL;
	}

	hr = session->StartCapture();
	if (FAILED(hr))
		WGC_LOG("WGC: StartCapture failed hr=0x%08lx\n", (unsigned long)hr);

	WgcRef<IWgcCaptureFrame> frame;
	if (SUCCEEDED(hr) && WaitForSingleObject(ev, 4000) == WAIT_OBJECT_0) {
		// Grab the oldest queued frame; poll briefly as a safety net
		for (int i = 0; i < 100 && !frame.p; i++) {
			pool->TryGetNextFrame(&frame);
			if (!frame.p) Sleep(5);
		}
	}
	if (!frame.p)
		WGC_LOG("WGC: no frame arrived within timeout (hr=0x%08lx)\n", (unsigned long)hr);

	pool->remove_FrameArrived(arrivedToken);
	handler->setEvent(NULL);

	// Stop frame production immediately - the pool's worker thread must not
	// touch our D3D device while we read the frame on this thread.
	sessionCleaner.attach(NULL);

	if (!frame.p) return NULL;

	// --- read the pixels back ---
	WgcRef<IWgcDirect3DSurface> surface;
	if (FAILED(hr = frame->get_Surface(&surface)))
		{ WGC_LOG("WGC: get_Surface failed hr=0x%08lx\n", (unsigned long)hr); return NULL; }

	WgcRef<IWgcDxgiInterfaceAccess> access;
	if (FAILED(hr = surface->QueryInterface(IID_IWgcDxgiInterfaceAccess, (void **)&access)))
		{ WGC_LOG("WGC: QI DxgiInterfaceAccess failed hr=0x%08lx\n", (unsigned long)hr); return NULL; }

	WgcRef<ID3D11Texture2D> texture;
	if (FAILED(hr = access->GetInterface(IID_IWgcD3D11Texture2D, (void **)&texture)))
		{ WGC_LOG("WGC: GetInterface(ID3D11Texture2D) failed hr=0x%08lx\n", (unsigned long)hr); return NULL; }

	D3D11_TEXTURE2D_DESC desc = {};
	texture->GetDesc(&desc);
	if (desc.Width == 0 || desc.Height == 0)
		return NULL;

	D3D11_TEXTURE2D_DESC stagingDesc = desc;
	stagingDesc.Usage          = D3D11_USAGE_STAGING;
	stagingDesc.BindFlags      = 0;
	stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	stagingDesc.MiscFlags      = 0;

	WgcRef<ID3D11Texture2D> staging;
	if (FAILED(hr = device->CreateTexture2D(&stagingDesc, NULL, &staging)))
		{ WGC_LOG("WGC: CreateTexture2D(staging) failed hr=0x%08lx\n", (unsigned long)hr); return NULL; }

	context->CopyResource(staging, texture);

	D3D11_MAPPED_SUBRESOURCE map = {};
	if (FAILED(hr = context->Map(staging, 0, D3D11_MAP_READ, 0, &map)))
		{ WGC_LOG("WGC: Map failed hr=0x%08lx\n", (unsigned long)hr); return NULL; }

	// WGC frames are premultiplied - GDI+ handles PARGB natively, including
	// converting to straight alpha when saving PNG.
	Gdiplus::Bitmap *bmp = new Gdiplus::Bitmap(desc.Width, desc.Height, PixelFormat32bppPARGB);
	bool ok = bmp->GetLastStatus() == Gdiplus::Ok;

	Gdiplus::Rect rc(0, 0, desc.Width, desc.Height);
	Gdiplus::BitmapData bmpData;
	if (ok) ok = bmp->LockBits(&rc, Gdiplus::ImageLockModeWrite, PixelFormat32bppPARGB, &bmpData) == Gdiplus::Ok;
	if (ok) {
		for (UINT y = 0; y < desc.Height; y++) {
			std::memcpy((BYTE *)bmpData.Scan0 + y * bmpData.Stride,
						(BYTE *)map.pData + y * map.RowPitch,
						(size_t)desc.Width * 4);
		}
		bmp->UnlockBits(&bmpData);
	}

	context->Unmap(staging, 0);

	if (!ok) {
		delete bmp;
		return NULL;
	}
	return bmp;
}
