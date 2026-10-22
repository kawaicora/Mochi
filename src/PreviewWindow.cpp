//=============================================================================
//  PreviewWindow.cpp — 显示 DrawCapture 暴露的真实像素
//  ---------------------------------------------------------------------------
//  像素来源（见 DrawCapture::GetFrame）：
//    A) 已重定向：原版绘制代码画进我们的常驻缓冲 —— 与原本像素级一致
//    B) 安全失败：直读真实 DDraw surface —— 本身就是原版画面
//
//  【几何：预览窗跟随 gamemd，而不是反过来】
//    预览窗的**客户区**始终等于 gamemd 窗口的**客户区**（屏幕坐标一致）。
//    这样：
//      * 光标即使被 ClipCursor 锁定，也正好落在窗口范围内
//      * DirectInput 读到的光标位置就是游戏期望的坐标系，不需要任何偏移补偿
//      * 鼠标转发也可以 1:1 直传
//
//  【全屏：只跟随，不自己决定】
//    不做任何独立的 F11/Alt+Enter 切换。检测 gamemd 窗口是否覆盖了整个
//    显示器：是则预览窗用 WS_POPUP 铺满同一块区域，否则用普通窗口样式。
//    即「原本窗口全屏，预览就全屏」。
//=============================================================================
#include "PreviewWindow.h"

#include <YRPP.h>
#include <DrawCapture.h>
#include <Debug.h>

#include <string.h>
#include <stdint.h>

namespace {

constexpr const wchar_t* kClassName = L"MochiPreview";

HWND		g_hwnd = nullptr;
HWND		g_gameHwnd = nullptr;
HDC			g_memDC = nullptr;
HBITMAP		g_bmp = nullptr;
void*		g_bits = nullptr;
int			g_dibW = 0, g_dibH = 0;		// DIB 尺寸 = 游戏 surface 尺寸
bool		g_classReady = false;
uint64_t	g_lastTitleTick = 0;
uint32_t	g_shownFrames = 0;
// 几何同步的去重状态
struct GeoKey
{
	int  x, y, cw, ch;
	bool fullscreen;
	bool operator==(const GeoKey& o) const
	{
		return x == o.x && y == o.y && cw == o.cw && ch == o.ch && fullscreen == o.fullscreen;
	}
};
GeoKey		g_lastGeo = { 0, 0, 0, 0, false };
bool		g_geoValid = false;

// ---- 像素格式转换：游戏缓冲 -> 32bpp DIB -----------------------------------
static void ConvertToDib(const void* src, int srcW, int srcH, int srcPitch, int bpp)
{
	if (!src || !g_bits || srcW <= 0 || srcH <= 0)
		return;

	const int cw = (srcW < g_dibW) ? srcW : g_dibW;
	const int ch = (srcH < g_dibH) ? srcH : g_dibH;
	uint32_t* dstBase = (uint32_t*)g_bits;
	const uint8_t* sBase = (const uint8_t*)src;

	if (bpp == 2)
	{
		for (int y = 0; y < ch; ++y)
		{
			const uint16_t* s = (const uint16_t*)(sBase + (size_t)y * srcPitch);
			uint32_t* d = dstBase + (size_t)y * g_dibW;
			for (int x = 0; x < cw; ++x)
			{
				const uint16_t v = s[x];
				const uint32_t r = (v >> 11) & 0x1F;
				const uint32_t g = (v >> 5) & 0x3F;
				const uint32_t b = v & 0x1F;
				d[x] = 0xFF000000u
					| ((r * 255u / 31u) << 16)
					| ((g * 255u / 63u) << 8)
					| (b * 255u / 31u);
			}
		}
	}
	else if (bpp == 4)
	{
		for (int y = 0; y < ch; ++y)
		{
			const uint32_t* s = (const uint32_t*)(sBase + (size_t)y * srcPitch);
			uint32_t* d = dstBase + (size_t)y * g_dibW;
			for (int x = 0; x < cw; ++x)
				d[x] = s[x] | 0xFF000000u;
		}
	}
	else if (bpp == 1)
	{
		for (int y = 0; y < ch; ++y)
		{
			const uint8_t* s = sBase + (size_t)y * srcPitch;
			uint32_t* d = dstBase + (size_t)y * g_dibW;
			for (int x = 0; x < cw; ++x)
			{
				const uint32_t v = s[x];
				d[x] = 0xFF000000u | (v << 16) | (v << 8) | v;
			}
		}
	}
}

static bool RecreateDib(int w, int h)
{
	if (g_bmp && g_dibW == w && g_dibH == h)
		return true;

	if (g_bmp)   { DeleteObject(g_bmp); g_bmp = nullptr; }
	if (g_memDC) { DeleteDC(g_memDC);     g_memDC = nullptr; }
	g_bits = nullptr;

	BITMAPINFO bi{};
	bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bi.bmiHeader.biWidth = w;
	bi.bmiHeader.biHeight = -h;			// top-down
	bi.bmiHeader.biPlanes = 1;
	bi.bmiHeader.biBitCount = 32;
	bi.bmiHeader.biCompression = BI_RGB;

	HDC screen = GetDC(nullptr);
	g_bmp = CreateDIBSection(screen, &bi, DIB_RGB_COLORS, &g_bits, nullptr, 0);
	ReleaseDC(nullptr, screen);
	if (!g_bmp || !g_bits)
		return false;

	g_memDC = CreateCompatibleDC(nullptr);
	if (!g_memDC)
		return false;
	SelectObject(g_memDC, g_bmp);
	g_dibW = w;
	g_dibH = h;
	return true;
}

//=============================================================================
// 不干预渲染：直接读真实 DDraw surface，按"谁在变"自动选中
//   游戏主画面在 Primary；设置/菜单/加载由引擎直接显示 Alternate。
//   哪块 surface 的内容帧间发生变化，就说明它正在被绘制 -> 显示它。
//   这样不需要 hook 任何 Lock/Blt，也不存在"被我们改坏"的问题。
//=============================================================================
namespace {
struct Watch
{
	Surface*	S;
	const char*	Name;
	uint32_t	LastSum;
	uint64_t	LastChange;
	int			W, H, Pitch, Bpp;
	bool		Valid;
};

constexpr int kWatchCount = 7;
Watch g_watch[kWatchCount];
bool  g_watchInit = false;
int   g_selIdx = -1;		// >=0 表示用户用 F1/F2 手动锁定某块
const char* g_selName = "?";

static void InitWatch()
{
	if (g_watchInit)
		return;
	g_watchInit = true;

	Surface* list[kWatchCount] = {};
	__try
	{
		list[0] = DSurface::Primary;
		list[1] = DSurface::Alternate;
		list[2] = DSurface::Hidden;
		list[3] = DSurface::Composite;
		list[4] = DSurface::Sidebar;
		list[5] = DSurface::Temp;
		list[6] = DSurface::Tile;
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {}

	static const char* names[kWatchCount] = {
		"Primary", "Alternate", "Hidden", "Composite", "Sidebar", "Temp", "Tile"
	};

	for (int i = 0; i < kWatchCount; ++i)
	{
		g_watch[i].S = list[i];
		g_watch[i].Name = names[i];
		g_watch[i].Valid = false;
		g_watch[i].LastSum = 0;
		g_watch[i].LastChange = 0;
	}
}

// 采样校验和：便宜，够判断"这一帧有没有变"
static uint32_t SurfaceSum(const uint8_t* p, int w, int h, int pitch, int bpp)
{
	if (!p || w <= 0 || h <= 0 || pitch <= 0)
		return 0;
	uint32_t s = 2166136261u;
	const size_t total = (size_t)w * (size_t)h;
	const size_t step = 97;
	for (size_t i = 0; i < total; i += step)
	{
		const size_t y = i / (size_t)w;
		const size_t x = i % (size_t)w;
		const uint8_t* q = p + y * (size_t)pitch + x * (size_t)bpp;
		s = (s ^ q[0]) * 16777619u;
		if (bpp > 1)
			s = (s ^ q[1]) * 16777619u;
	}
	return s;
}
}

static void RefreshFromGame()
{
	InitWatch();

	const uint64_t now = GetTickCount64();

	// 逐块探测：锁一次，算校验和，立刻解锁。未接管 Lock 时这是原版 DDraw 锁，安全。
	for (int i = 0; i < kWatchCount; ++i)
	{
		Watch& w = g_watch[i];
		if (!w.S)
			continue;

		void* bits = nullptr;
		__try { bits = w.S->Lock(0, 0); }
		__except (EXCEPTION_EXECUTE_HANDLER) { bits = nullptr; }
		if (!bits)
			continue;

		int ww = w.S->Width, hh = w.S->Height, pp = 0, bb = 2;
		__try { pp = w.S->GetPitch(); }
		__except (EXCEPTION_EXECUTE_HANDLER) { pp = 0; }
		__try { bb = w.S->GetBytesPerPixel(); }
		__except (EXCEPTION_EXECUTE_HANDLER) { bb = 2; }
		if (bb != 1 && bb != 2 && bb != 4) bb = 2;
		if (pp <= 0) pp = ww * bb;

		if (ww > 0 && hh > 0)
		{
			w.W = ww; w.H = hh; w.Pitch = pp; w.Bpp = bb;
			const uint32_t sum = SurfaceSum((const uint8_t*)bits, ww, hh, pp, bb);
			if (sum != w.LastSum)
			{
				w.LastSum = sum;
				w.LastChange = now;
				w.Valid = true;
			}
		}

		__try { w.S->Unlock(); }
		__except (EXCEPTION_EXECUTE_HANDLER) {}
	}

	// 选中：手动优先；否则取最近发生变化的那块（并列优先 Primary）
	int pick = -1;
	if (g_selIdx >= 0 && g_selIdx < kWatchCount && g_watch[g_selIdx].Valid)
		pick = g_selIdx;
	else
	{
		uint64_t bestT = 0;
		for (int i = 0; i < kWatchCount; ++i)
		{
			const Watch& w = g_watch[i];
			if (!w.Valid)
				continue;
			if (pick < 0 || w.LastChange > bestT)
			{
				pick = i;
				bestT = w.LastChange;
			}
		}
	}
	if (pick < 0)
		return;

	Watch& sel = g_watch[pick];
	g_selName = sel.Name;

	void* bits = nullptr;
	__try { bits = sel.S->Lock(0, 0); }
	__except (EXCEPTION_EXECUTE_HANDLER) { bits = nullptr; }
	if (!bits)
		return;

	if (sel.W != g_dibW || sel.H != g_dibH)
	{
		if (!RecreateDib(sel.W, sel.H))
		{
			__try { sel.S->Unlock(); } __except (EXCEPTION_EXECUTE_HANDLER) {}
			return;
		}
	}

	ConvertToDib(bits, sel.W, sel.H, sel.Pitch, sel.Bpp);

	__try { sel.S->Unlock(); } __except (EXCEPTION_EXECUTE_HANDLER) {}

	++g_shownFrames;
	if (now - g_lastTitleTick >= 1000)
	{
		g_lastTitleTick = now;
		wchar_t buf[256];
		swprintf_s(buf, L"Mochi Preview surf=%S %dx%d bpp=%d fps=%u",
			g_selName, sel.W, sel.H, sel.Bpp, g_shownFrames);
		SetWindowTextW(g_hwnd, buf);
		g_shownFrames = 0;
	}
}

//=============================================================================
// 几何同步：预览窗客户区 == gamemd 客户区（屏幕坐标一致）
//=============================================================================
static void SyncPreviewToGame()
{
	if (!g_hwnd || !g_gameHwnd)
		return;

	RECT gr{};
	if (!GetWindowRect(g_gameHwnd, &gr))
		return;

	// gamemd 窗口是否覆盖整个显示器（= 全屏）
	bool gameFullscreen = false;
	HMONITOR mon = MonitorFromWindow(g_gameHwnd, MONITOR_DEFAULTTONEAREST);
	MONITORINFO mi{};
	mi.cbSize = sizeof(mi);
	if (GetMonitorInfoW(mon, &mi))
	{
		const RECT& m = mi.rcMonitor;
		gameFullscreen =
			gr.left <= m.left && gr.top <= m.top &&
			gr.right >= m.right && gr.bottom >= m.bottom;
	}

	// 目标客户区（屏幕坐标）
	POINT gc{ 0, 0 };
	if (!ClientToScreen(g_gameHwnd, &gc))
		return;
	RECT gcr{};
	if (!GetClientRect(g_gameHwnd, &gcr))
		return;
	const int cw = gcr.right - gcr.left;
	const int ch = gcr.bottom - gcr.top;
	if (cw <= 0 || ch <= 0)
		return;

	GeoKey key{ gc.x, gc.y, cw, ch, gameFullscreen };
	if (g_geoValid && key == g_lastGeo)
		return;						// 没变就不动窗口，避免每帧抖动

	g_lastGeo = key;
	g_geoValid = true;

	// 样式只跟随：原本全屏就 WS_POPUP，否则普通窗口
	const LONG_PTR wantStyle = (gameFullscreen ? WS_POPUP : WS_OVERLAPPEDWINDOW) | WS_VISIBLE;
	if (GetWindowLongPtrW(g_hwnd, GWL_STYLE) != wantStyle)
		SetWindowLongPtrW(g_hwnd, GWL_STYLE, wantStyle);

	// 算出预览窗自身的非客户区尺寸
	RECT pwr{}, pcr{};
	GetWindowRect(g_hwnd, &pwr);
	GetClientRect(g_hwnd, &pcr);
	POINT pc{ 0, 0 };
	ClientToScreen(g_hwnd, &pc);
	int offX = pc.x - pwr.left;
	int offY = pc.y - pwr.top;
	int wb = (pwr.right - pwr.left) - (pcr.right - pcr.left);
	int hb = (pwr.bottom - pwr.top) - (pcr.bottom - pcr.top);
	if (wb < 0) wb = 0;
	if (hb < 0) hb = 0;

	SetWindowPos(g_hwnd, HWND_TOP,
		gc.x - offX, gc.y - offY,
		cw + wb, ch + hb,
		SWP_NOACTIVATE | SWP_FRAMECHANGED);

	Debug::Log("[Preview] follow game rect client=(%d,%d) %dx%d fullscreen=%d\n",
		gc.x, gc.y, cw, ch, (int)gameFullscreen);
}

// ---- 输入转发：客户区 1:1 映射到 surface --------------------------------
static void ForwardMouse(UINT msg, WPARAM wp, LPARAM lp)
{
	if (!g_gameHwnd || !g_hwnd)
		return;

	RECT rc{};
	if (!GetClientRect(g_hwnd, &rc))
		return;
	const int cw = rc.right - rc.left;
	const int ch = rc.bottom - rc.top;
	if (cw <= 0 || ch <= 0 || g_dibW <= 0 || g_dibH <= 0)
		return;

	const int cx = (int)(short)LOWORD(lp);
	const int cy = (int)(short)HIWORD(lp);

	// 两边客户区已经对齐，正常情况下 sx/cx 是 1:1；
	// 这里仍按比例换算，兼容 surface 尺寸与客户区不一致的情形。
	const int sx = (int)((int64_t)cx * g_dibW / cw);
	const int sy = (int)((int64_t)cy * g_dibH / ch);

	PostMessageW(g_gameHwnd, msg, wp, MAKELPARAM(sx, sy));
}

LRESULT CALLBACK PreviewProc(HWND h, UINT msg, WPARAM wp, LPARAM lp)
{
	switch (msg)
	{
	case WM_PAINT:
	{
		PAINTSTRUCT ps{};
		HDC dc = BeginPaint(h, &ps);
		if (g_memDC && g_bmp && g_dibW > 0)
		{
			RECT rc{};
			GetClientRect(h, &rc);
			SetStretchBltMode(dc, COLORONCOLOR);
			StretchBlt(dc, 0, 0, rc.right, rc.bottom,
				g_memDC, 0, 0, g_dibW, g_dibH, SRCCOPY);
		}
		EndPaint(h, &ps);
		return 0;
	}
	case WM_ERASEBKGND:
		return 1;

	case WM_MOUSEMOVE:
	case WM_LBUTTONDOWN:
	case WM_LBUTTONUP:
	case WM_RBUTTONDOWN:
	case WM_RBUTTONUP:
	case WM_MBUTTONDOWN:
	case WM_MBUTTONUP:
	case WM_LBUTTONDBLCLK:
	case WM_RBUTTONDBLCLK:
		ForwardMouse(msg, wp, lp);
		return 0;

	case WM_MOUSEWHEEL:
		if (g_gameHwnd)
		{
			POINT pt{ (int)(short)LOWORD(lp), (int)(short)HIWORD(lp) };
			ScreenToClient(h, &pt);
			RECT rc{}; GetClientRect(h, &rc);
			const int sx = (rc.right > 0 && g_dibW > 0) ? (int)((int64_t)pt.x * g_dibW / rc.right) : 0;
			const int sy = (rc.bottom > 0 && g_dibH > 0) ? (int)((int64_t)pt.y * g_dibH / rc.bottom) : 0;
			PostMessageW(g_gameHwnd, msg, wp, MAKELPARAM(sx, sy));
		}
		return 0;

	case WM_KEYDOWN:
		// F1 / F2 在本窗口内切换要显示哪一块 surface，不转发给游戏
		if (wp == VK_F1 || wp == VK_F2)
		{
			const int n = DrawCapture::GetSurfaceCount();
			if (n > 0)
				g_selIdx = (wp == VK_F2) ? (g_selIdx + 1) % n : (g_selIdx + n - 1) % n;
			return 0;
		}
		if (g_gameHwnd)
			PostMessageW(g_gameHwnd, msg, wp, lp);
		return 0;

	case WM_KEYUP:
	case WM_SYSKEYDOWN:
	case WM_SYSKEYUP:
	case WM_CHAR:
		if (g_gameHwnd)
			PostMessageW(g_gameHwnd, msg, wp, lp);
		return 0;

	case WM_CLOSE:
		ShowWindow(h, SW_HIDE);
		return 0;

	case WM_DESTROY:
		g_hwnd = nullptr;
		return 0;
	}
	return DefWindowProcW(h, msg, wp, lp);
}

static bool EnsureClass()
{
	if (g_classReady)
		return true;
	WNDCLASSW wc{};
	wc.lpfnWndProc = PreviewProc;
	wc.hInstance = (HINSTANCE)GetModuleHandleW(nullptr);
	wc.hCursor = LoadCursorW(nullptr, (LPCWSTR)IDC_ARROW);
	wc.hbrBackground = nullptr;
	wc.lpszClassName = kClassName;
	g_classReady = (RegisterClassW(&wc) != 0) || GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
	return g_classReady;
}

static HWND FindGameWindow()
{
	struct Ctx { HWND self; HWND console; HWND found; } ctx{ g_hwnd, GetConsoleWindow(), nullptr };

	EnumWindows([](HWND h, LPARAM lp) -> BOOL {
		Ctx* c = (Ctx*)lp;
		DWORD pid = 0;
		GetWindowThreadProcessId(h, &pid);
		if (pid != GetCurrentProcessId()) return TRUE;
		if (h == c->self || h == c->console) return TRUE;
		c->found = h;
		return FALSE;
		}, (LPARAM)&ctx);

	return ctx.found;
}
}

//=============================================================================
bool PreviewWindow::Create()
{
	if (g_hwnd)
		return true;
	if (!EnsureClass())
	{
		Debug::Log("[Preview] RegisterClass failed err=%lu\n", GetLastError());
		return false;
	}

	int w = 0, h = 0;
	void* probe = nullptr;
	int pitch = 0, bpp = 2;
	if (!DrawCapture::GetFrame(&probe, &w, &h, &pitch, &bpp) || w <= 0 || h <= 0)
	{
		w = 1024; h = 768;
		__try
		{
			if (DSurface::Primary)
			{
				const int pw = DSurface::Primary->Width;
				const int ph = DSurface::Primary->Height;
				if (pw > 0 && ph > 0) { w = pw; h = ph; }
			}
		}
		__except (EXCEPTION_EXECUTE_HANDLER) {}
	}

	if (!RecreateDib(w, h))
	{
		Debug::Log("[Preview] CreateDIBSection failed\n");
		return false;
	}

	if (!g_gameHwnd)
		g_gameHwnd = FindGameWindow();

	RECT wr{ 0, 0, w, h };
	AdjustWindowRectEx(&wr, WS_OVERLAPPEDWINDOW, FALSE, 0);

	g_hwnd = CreateWindowExW(WS_EX_TOPMOST, kClassName, L"Mochi Preview",
		WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT,
		wr.right - wr.left, wr.bottom - wr.top,
		nullptr, nullptr, (HINSTANCE)GetModuleHandleW(nullptr), nullptr);

	if (!g_hwnd)
	{
		Debug::Log("[Preview] CreateWindowEx failed err=%lu\n", GetLastError());
		return false;
	}

	// 创建后立刻跟随 gamemd 的 RECT（含全屏判定）
	g_geoValid = false;
	SyncPreviewToGame();

	Debug::Log("[Preview] ready, surface %dx%d, input target=%p\n", w, h, g_gameHwnd);
	return true;
}

void PreviewWindow::RenderFrame()
{
	if (!g_hwnd)
		return;

	SyncPreviewToGame();		// 游戏改窗口/切全屏时，预览跟着走
	RefreshFromGame();
	InvalidateRect(g_hwnd, nullptr, FALSE);
}

void PreviewWindow::Destroy()
{
	if (g_hwnd) { DestroyWindow(g_hwnd); g_hwnd = nullptr; }
	if (g_memDC) { DeleteDC(g_memDC); g_memDC = nullptr; }
	if (g_bmp) { DeleteObject(g_bmp); g_bmp = nullptr; }
	g_bits = nullptr;
}

HWND PreviewWindow::GetHwnd()
{
	return g_hwnd;
}

void PreviewWindow::SetInputTarget(HWND gameHwnd)
{
	g_gameHwnd = gameHwnd;
	g_geoValid = false;
}
