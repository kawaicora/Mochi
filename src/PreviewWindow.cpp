//=============================================================================
//  PreviewWindow.cpp — 显示 DrawCapture 暴露的**真实像素**
//  ---------------------------------------------------------------------------
//  这里不做任何重新光栅化。像素来源有两条（见 DrawCapture::GetFrame）：
//    A) 已重定向：原版绘制代码画进我们的常驻缓冲 —— 与原本像素级一致
//    B) 安全失败：直读真实 DDraw surface —— 本身就是原版画面
//  所以预览窗里看到的就是游戏原本该有的样子。
//
//  另负责把鼠标/键盘事件转发回 gamemd 的窗口。
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
int			g_winW = 0, g_winH = 0;		// 窗口客户区
bool		g_classReady = false;
uint64_t	g_lastTitleTick = 0;
uint32_t	g_shownFrames = 0;

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

static void RefreshFromGame()
{
	void* bits = nullptr;
	int w = 0, h = 0, pitch = 0, bpp = 2;
	if (!DrawCapture::GetFrame(&bits, &w, &h, &pitch, &bpp) || !bits)
		return;

	// surface 尺寸变了就重建 DIB
	if (w != g_dibW || h != g_dibH)
	{
		BITMAPINFO bi{};
		bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		bi.bmiHeader.biWidth = w;
		bi.bmiHeader.biHeight = -h;
		bi.bmiHeader.biPlanes = 1;
		bi.bmiHeader.biBitCount = 32;
		bi.bmiHeader.biCompression = BI_RGB;

		if (g_bmp) { DeleteObject(g_bmp); g_bmp = nullptr; }
		if (g_memDC) { DeleteDC(g_memDC); g_memDC = nullptr; }

		HDC screen = GetDC(nullptr);
		g_bmp = CreateDIBSection(screen, &bi, DIB_RGB_COLORS, &g_bits, nullptr, 0);
		ReleaseDC(nullptr, screen);
		if (!g_bmp || !g_bits)
			return;
		g_memDC = CreateCompatibleDC(nullptr);
		if (!g_memDC)
			return;
		SelectObject(g_memDC, g_bmp);
		g_dibW = w;
		g_dibH = h;
	}

	ConvertToDib(bits, w, h, pitch, bpp);

	// 走退路时（真实 surface 被临时锁），用完解锁；重定向模式下这是空操作
	__try
	{
		if (DSurface::Primary)
			DSurface::Primary->Unlock();
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {}

	++g_shownFrames;

	const uint64_t now = GetTickCount64();
	if (now - g_lastTitleTick >= 1000)
	{
		g_lastTitleTick = now;
		const DrawCapture::Stats st = DrawCapture::GetStats();
		wchar_t buf[256];
		swprintf_s(buf, L"Mochi Preview %dx%d bpp=%d %s frames=%u locks=%llu",
			w, h, bpp,
			DrawCapture::IsInstalled() ? L"[capture]" : L"[passthrough]",
			g_shownFrames, (unsigned long long)st.Locks);
		SetWindowTextW(g_hwnd, buf);
		g_shownFrames = 0;
	}
}

// ---- 输入转发 --------------------------------------------------------------
static void ForwardMouse(UINT msg, WPARAM wp, LPARAM lp)
{
	if (!g_gameHwnd || !g_hwnd)
		return;
	RECT rc{};
	if (!GetClientRect(g_hwnd, &rc))
		return;
	const int cw = rc.right, ch = rc.bottom;
	if (cw <= 0 || ch <= 0 || g_dibW <= 0 || g_dibH <= 0)
		return;

	const int cx = (int)(short)LOWORD(lp);
	const int cy = (int)(short)HIWORD(lp);
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

	g_dibW = w;
	g_dibH = h;

	BITMAPINFO bi{};
	bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bi.bmiHeader.biWidth = w;
	bi.bmiHeader.biHeight = -h;
	bi.bmiHeader.biPlanes = 1;
	bi.bmiHeader.biBitCount = 32;
	bi.bmiHeader.biCompression = BI_RGB;

	HDC screen = GetDC(nullptr);
	g_bmp = CreateDIBSection(screen, &bi, DIB_RGB_COLORS, &g_bits, nullptr, 0);
	ReleaseDC(nullptr, screen);
	if (!g_bmp || !g_bits)
	{
		Debug::Log("[Preview] CreateDIBSection failed\n");
		return false;
	}

	g_memDC = CreateCompatibleDC(nullptr);
	if (!g_memDC)
		return false;
	SelectObject(g_memDC, g_bmp);

	int maxW = 1280, maxH = 800;
	g_winW = w;
	g_winH = h;
	if (g_winW > maxW) { g_winH = (int)((int64_t)g_winH * maxW / g_winW); g_winW = maxW; }
	if (g_winH > maxH) { g_winW = (int)((int64_t)g_winW * maxH / g_winH); g_winH = maxH; }

	RECT wr{ 0, 0, g_winW, g_winH };
	AdjustWindowRectEx(&wr, WS_OVERLAPPEDWINDOW, FALSE, 0);

	g_hwnd = CreateWindowExW(WS_EX_TOPMOST, kClassName, L"Mochi Preview",
		WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
		wr.right - wr.left, wr.bottom - wr.top,
		nullptr, nullptr, (HINSTANCE)GetModuleHandleW(nullptr), nullptr);

	if (!g_hwnd)
	{
		Debug::Log("[Preview] CreateWindowEx failed err=%lu\n", GetLastError());
		return false;
	}

	ShowWindow(g_hwnd, SW_SHOW);
	UpdateWindow(g_hwnd);

	if (!g_gameHwnd)
		g_gameHwnd = FindGameWindow();

	Debug::Log("[Preview] ready %dx%d (surface %dx%d), input target=%p\n",
		g_winW, g_winH, w, h, g_gameHwnd);
	return true;
}

void PreviewWindow::RenderFrame()
{
	if (!g_hwnd)
		return;
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
}
