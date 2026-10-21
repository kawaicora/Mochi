//=============================================================================
//  PreviewWindow.cpp — 把采集到的绘制命令自己光栅化到窗口里
//  ---------------------------------------------------------------------------
//  用途：Phase 1 验证。游戏不出像素了，这个窗口是唯一能"看见"东西的地方。
//        它同时把输入事件转发回 gamemd 的窗口。
//
//  光栅化范围：FillRect / DrawRect / DrawLine / SetPixel / Ellipse / GradientLine
//              未采集的类型（CopyFrom、SHP 精灵）这里画不出来 —— 那是外部
//              render 进程的活。
//=============================================================================
#include "PreviewWindow.h"

#include <YRPP.h>
#include <DrawCapture.h>
#include <Debug.h>

#include <cstring>
#include <stdint.h>
#include <vector>

namespace {

constexpr const wchar_t* kClassName = L"MochiPreview";

HWND		g_hwnd = nullptr;
HWND		g_gameHwnd = nullptr;
HDC			g_memDC = nullptr;
HBITMAP		g_bmp = nullptr;
void*		g_bits = nullptr;
int			g_w = 0, g_h = 0;
int			g_winW = 0, g_winH = 0;
bool		g_classReady = false;

// ---- 像素写入 --------------------------------------------------------------
inline uint32_t ToBGRA(uint32_t colorref)
{
	// COLORREF = 0x00BBGGRR  ->  DIB 32bpp(BI_RGB) 内存序 B,G,R,A
	const uint32_t r = colorref & 0xFF;
	const uint32_t g = (colorref >> 8) & 0xFF;
	const uint32_t b = (colorref >> 16) & 0xFF;
	return 0xFF000000u | (r << 16) | (g << 8) | b;
}

inline void PutPixel(int x, int y, uint32_t colorref)
{
	if (x < 0 || y < 0 || x >= g_w || y >= g_h || !g_bits)
		return;
	((uint32_t*)g_bits)[(size_t)y * (size_t)g_w + (size_t)x] = ToBGRA(colorref);
}

static void FillRectPx(int x, int y, int w, int h, uint32_t colorref)
{
	if (!g_bits)
		return;
	int x0 = x, y0 = y, x1 = x + w, y1 = y + h;
	if (x0 < 0) x0 = 0;
	if (y0 < 0) y0 = 0;
	if (x1 > g_w) x1 = g_w;
	if (y1 > g_h) y1 = g_h;
	if (x0 >= x1 || y0 >= y1)
		return;

	const uint32_t c = ToBGRA(colorref);
	uint32_t* base = (uint32_t*)g_bits;
	for (int yy = y0; yy < y1; ++yy)
	{
		uint32_t* row = base + (size_t)yy * (size_t)g_w;
		for (int xx = x0; xx < x1; ++xx)
			row[xx] = c;
	}
}

static void RectOutline(int x, int y, int w, int h, uint32_t colorref)
{
	FillRectPx(x, y, w, 1, colorref);
	FillRectPx(x, y + h - 1, w, 1, colorref);
	FillRectPx(x, y, 1, h, colorref);
	FillRectPx(x + w - 1, y, 1, h, colorref);
}

static void LinePx(int x0, int y0, int x1, int y1, uint32_t colorref)
{
	int dx = x1 - x0, dy = y1 - y0;
	int sx = (dx >= 0) ? 1 : -1;
	int sy = (dy >= 0) ? 1 : -1;
	if (dx < 0) dx = -dx;
	if (dy < 0) dy = -dy;

	int err = dx - dy;
	for (;;)
	{
		PutPixel(x0, y0, colorref);
		if (x0 == x1 && y0 == y1)
			break;
		const int e2 = err * 2;
		if (e2 > -dy) { err -= dy; x0 += sx; }
		if (e2 < dx)  { err += dx; y0 += sy; }
	}
}

static void EllipseOutline(int cx, int cy, int rx, int ry, uint32_t colorref)
{
	if (rx <= 0 || ry <= 0)
		return;
	// 中点椭圆，够预览用
	int x = 0, y = ry;
	long long d1 = (long long)ry * ry - (long long)rx * rx * ry + (long long)rx * rx / 4;
	int dx = 2LL * ry * ry * x;
	int dy = 2LL * rx * rx * y;

	while (dx < dy)
	{
		PutPixel(cx + x, cy + y, colorref); PutPixel(cx - x, cy + y, colorref);
		PutPixel(cx + x, cy - y, colorref); PutPixel(cx - x, cy - y, colorref);
		if (d1 < 0) { ++x; dx += 2LL * ry * ry; d1 += dx + (long long)ry * ry; }
		else { ++x; --y; dx += 2LL * ry * ry; dy -= 2LL * rx * rx; d1 += dx - dy + (long long)ry * ry; }
	}
	long long d2 = (long long)ry * ry * ((long long)x + 1) * ((long long)x + 1)
		+ (long long)rx * rx * ((long long)y - 1) * ((long long)y - 1)
		- (long long)rx * rx * ry * ry;
	while (y >= 0)
	{
		PutPixel(cx + x, cy + y, colorref); PutPixel(cx - x, cy + y, colorref);
		PutPixel(cx + x, cy - y, colorref); PutPixel(cx - x, cy - y, colorref);
		if (d2 > 0) { --y; dy -= 2LL * rx * rx; d2 += (long long)rx * rx - dy; }
		else { --y; ++x; dx += 2LL * ry * ry; dy -= 2LL * rx * rx; d2 += dx - dy + (long long)rx * rx; }
	}
}

// ---- 每帧光栅化 ------------------------------------------------------------
static void RasterizeFrame()
{
	if (!g_bits || g_w <= 0 || g_h <= 0)
		return;

	// 清屏
	const size_t npix = (size_t)g_w * (size_t)g_h;
	uint32_t* base = (uint32_t*)g_bits;
	for (size_t i = 0; i < npix; ++i)
		base[i] = 0xFF181818u;

	const DrawCaptureProto::Header* hdr = nullptr;
	const DrawCaptureProto::Cmd* cmds = nullptr;
	uint32_t count = 0;
	if (!DrawCapture::ReadLatest(&hdr, &cmds, &count) || !cmds)
		return;

	if (count > DrawCaptureProto::kMaxCmds)
		count = DrawCaptureProto::kMaxCmds;

	using namespace DrawCaptureProto;
	for (uint32_t i = 0; i < count; ++i)
	{
		const Cmd& c = cmds[i];
		switch (c.Type)
		{
		case kFillRect:     FillRectPx(c.X, c.Y, c.W, c.H, c.Color); break;
		case kDrawRect:     RectOutline(c.X, c.Y, c.W, c.H, c.Color); break;
		case kDrawLine:     LinePx(c.X, c.Y, c.W, c.H, c.Color); break;
		case kGradientLine: LinePx(c.X, c.Y, c.W, c.H, c.Color); break;
		case kSetPixel:     PutPixel(c.X, c.Y, c.Color); break;
		case kEllipse:      EllipseOutline(c.X, c.Y, c.W, c.H, c.Color); break;
		default:            break;	// kFill / kCopyFrom / 未采集：预览忽略
		}
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
	const int cw = rc.right - rc.left;
	const int ch = rc.bottom - rc.top;
	if (cw <= 0 || ch <= 0 || g_w <= 0 || g_h <= 0)
		return;

	const int cx = (int)(short)LOWORD(lp);
	const int cy = (int)(short)HIWORD(lp);

	// 客户区 -> surface 坐标
	const int sx = (int)((int64_t)cx * g_w / cw);
	const int sy = (int)((int64_t)cy * g_h / ch);

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
		if (g_memDC && g_bmp)
		{
			RECT rc{};
			GetClientRect(h, &rc);
			SetStretchBltMode(dc, COLORONCOLOR);
			StretchBlt(dc, 0, 0, rc.right, rc.bottom,
				g_memDC, 0, 0, g_w, g_h, SRCCOPY);
		}
		EndPaint(h, &ps);
		return 0;
	}
	case WM_ERASEBKGND:
		return 1;

	// ---- 鼠标：转发给 gamemd ----
	case WM_MOUSEMOVE:
	case WM_LBUTTONDOWN:
	case WM_LBUTTONUP:
	case WM_RBUTTONDOWN:
	case WM_RBUTTONUP:
	case WM_MBUTTONDOWN:
	case WM_MBUTTONUP:
		ForwardMouse(msg, wp, lp);
		return 0;

	case WM_LBUTTONDBLCLK:
	case WM_RBUTTONDBLCLK:
		ForwardMouse(msg, wp, lp);
		return 0;

	case WM_MOUSEWHEEL:
	{
		if (g_gameHwnd)
		{
			POINT pt{ (int)(short)LOWORD(lp), (int)(short)HIWORD(lp) };
			ScreenToClient(g_hwnd, &pt);
			RECT rc{};
			GetClientRect(h, &rc);
			const int cw = rc.right, ch = rc.bottom;
			const int sx = (cw > 0 && g_w > 0) ? (int)((int64_t)pt.x * g_w / cw) : 0;
			const int sy = (ch > 0 && g_h > 0) ? (int)((int64_t)pt.y * g_h / ch) : 0;
			PostMessageW(g_gameHwnd, msg, wp, MAKELPARAM(sx, sy));
		}
		return 0;
	}

	// ---- 键盘：直接转发 ----
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

// 找本进程里"不是预览窗、不是控制台"的顶层窗口，当作 gamemd 的窗口
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

static bool CreateDib(int w, int h)
{
	if (g_bmp && g_w == w && g_h == h)
		return true;

	if (g_memDC)
	{
		DeleteDC(g_memDC);
		g_memDC = nullptr;
	}
	if (g_bmp)
	{
		DeleteObject(g_bmp);
		g_bmp = nullptr;
	}

	BITMAPINFO bi{};
	bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bi.bmiHeader.biWidth = w;
	bi.bmiHeader.biHeight = -h;		// top-down
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
	g_w = w;
	g_h = h;
	return true;
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

	// surface 尺寸
	int w = 1024, h = 768;
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

	if (!CreateDib(w, h))
	{
		Debug::Log("[Preview] CreateDib failed\n");
		return false;
	}

	// 窗口尺寸：等比缩到最大 1280x800
	int maxW = 1280, maxH = 800;
	g_winW = w;
	g_winH = h;
	if (g_winW > maxW)
	{
		g_winH = (int)((int64_t)g_winH * maxW / g_winW);
		g_winW = maxW;
	}
	if (g_winH > maxH)
	{
		g_winW = (int)((int64_t)g_winW * maxH / g_winH);
		g_winH = maxH;
	}

	RECT wr{ 0, 0, g_winW, g_winH };
	AdjustWindowRectEx(&wr, WS_OVERLAPPEDWINDOW, FALSE, 0);

	g_hwnd = CreateWindowExW(WS_EX_TOPMOST, kClassName, L"Mochi Preview (draw capture)",
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

	// 输入转发目标
	if (!g_gameHwnd)
		g_gameHwnd = FindGameWindow();

	Debug::Log("[Preview] window ready %dx%d (surface %dx%d), input target=%p\n",
		g_winW, g_winH, w, h, g_gameHwnd);
	return true;
}

void PreviewWindow::RenderFrame()
{
	if (!g_hwnd)
		return;
	RasterizeFrame();
	InvalidateRect(g_hwnd, nullptr, FALSE);
}

void PreviewWindow::Destroy()
{
	if (g_hwnd)
	{
		DestroyWindow(g_hwnd);
		g_hwnd = nullptr;
	}
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
