#include <iostream>
#include <fstream>
#include <vector>
#include <cstdio>
#include <windows.h>
#include <string>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <stdint.h>
#include <tbs.h>
#include <ncrypt.h>
#include <commctrl.h>
#include <thread>
#include <gdiplus.h>
#include <memory>

#include "IgsBoot.h"

// GDI+ (for background image)
#pragma comment(lib, "gdiplus.lib")
// GradientFill (fallback background gradient)
#pragma comment(lib, "Msimg32.lib")

HWND hWnd;
int progressValue = 0;

#define UPDATE_PERCENT (WM_APP + 1)
#define WM_UPDATE_TEXT (WM_APP + 2)
#define WM_SET_BANNER (WM_APP + 3)

static ULONG_PTR g_gdiplusToken = 0;
static std::wstring g_backgroundImagePath; // optional: load from file path
static std::unique_ptr<Gdiplus::Image> g_backgroundImage;
static std::wstring g_titleText = L"Loading ..";
static std::wstring g_bannerText;
static bool g_bannerIsError = false;
static int g_percent = 0;

static void PostBannerText(const std::wstring& text, bool isError)
{
	if (!hWnd || !IsWindow(hWnd)) return;
	const size_t n = text.size() + 1;
	wchar_t* p = new wchar_t[n];
	wcscpy_s(p, n, text.c_str());
	if (!PostMessageW(hWnd, WM_SET_BANNER, (WPARAM)p, (LPARAM)(isError ? 1 : 0)))
		delete[] p;
}

void HideProgress()
{
	if (!hWnd || !IsWindow(hWnd)) return;
	// Drop topmost first to avoid blocking the launched game's window.
	SetWindowPos(hWnd, HWND_NOTOPMOST, 0, 0, 0, 0,
		SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
	ShowWindow(hWnd, SW_HIDE);
}

void BackgroundProgress()
{
	if (!hWnd || !IsWindow(hWnd)) return;
	// Keep it visible but not topmost, so the launched game can stay in front.
	SetWindowPos(hWnd, HWND_NOTOPMOST, 0, 0, 0, 0,
		SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
}

static void EnsureGdiplusStarted()
{
	if (g_gdiplusToken != 0) return;
	Gdiplus::GdiplusStartupInput input;
	Gdiplus::Status st = Gdiplus::GdiplusStartup(&g_gdiplusToken, &input, nullptr);
	(void)st;
}

static void LoadBackgroundImageIfAny()
{
	g_backgroundImage.reset();
	if (g_backgroundImagePath.empty()) return;

	EnsureGdiplusStarted();
	auto img = std::make_unique<Gdiplus::Image>(g_backgroundImagePath.c_str());
	if (img && img->GetLastStatus() == Gdiplus::Ok)
	{
		g_backgroundImage = std::move(img);
	}
}

// Optional API: call this BEFORE ShowProgress() if you want a custom background image.
// Supports common formats (png/jpg/bmp) depending on installed codecs.
void SetProgressBackgroundImagePath(const wchar_t* path)
{
	if (!path || !*path) {
		g_backgroundImagePath = L"";
		return;
	}

	DWORD attr = GetFileAttributesW(path);
	if (attr == INVALID_FILE_ATTRIBUTES || (attr & FILE_ATTRIBUTE_DIRECTORY)) {
		g_backgroundImagePath = L"";
		return;
	}

	g_backgroundImagePath = path;
}

void ErrorMessage(int ErrorCode, int CodeLine) {

	std::wstring msg = L"E" + std::to_wstring(ErrorCode) + L":" + std::to_wstring(CodeLine);
	PostBannerText(msg, true);
}

void InfoMessage(const char* Info) {

	if (!Info || !*Info) {
		PostBannerText(L"", false);
		return;
	}
	int len = MultiByteToWideChar(CP_ACP, 0, Info, -1, nullptr, 0);
	if (len <= 0) return;
	std::wstring w;
	w.resize(static_cast<size_t>(len - 1));
	MultiByteToWideChar(CP_ACP, 0, Info, -1, &w[0], len);
	PostBannerText(w, false);
}

DWORD WINAPI MsgThread(LPVOID lpParam) {
	PostBannerText(L"Update Completed", false);
	return 0;
}

void InitProgressBar(HWND hWnd)
{
	// No child controls. We draw everything ourselves in WM_PAINT (double-buffered).
}

// UI thread 的 WindowProc
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_ERASEBKGND:
		// We paint the full background in WM_PAINT to avoid flicker.
		return 1;
	case WM_UPDATE_TEXT:
	{
		
		wchar_t* text = (wchar_t*)wParam;
		//printf("[%s][%d] : %ls (len=%d)\n", __func__, __LINE__, text, wcslen(text));
		g_titleText = (text ? text : L"");
		InvalidateRect(hWnd, NULL, TRUE);
		delete[] text;
		return 0;
	}
	case WM_SET_BANNER:
	{
		wchar_t* text = (wchar_t*)wParam;
		g_bannerText = (text ? text : L"");
		g_bannerIsError = (lParam != 0);
		delete[] text;
		InvalidateRect(hWnd, NULL, TRUE);
		return 0;
	}
	case WM_CREATE:
		LoadBackgroundImageIfAny();
		InitProgressBar(hWnd);
		return 0;

	case WM_SIZE:
	{
		InvalidateRect(hWnd, nullptr, TRUE);
		return 0;
	}

	case UPDATE_PERCENT:   // 背景 thread 來的訊息
	{
		int percent = (int)wParam;
		if (percent < 0) percent = 0;
		if (percent > 100) percent = 100;

		g_percent = percent;
		InvalidateRect(hWnd, nullptr, FALSE);

		return 0;
	}

	case WM_PAINT:
	{
		PAINTSTRUCT ps{};
		HDC hdc = BeginPaint(hWnd, &ps);
		RECT rc{};
		GetClientRect(hWnd, &rc);

		// Double-buffer everything to avoid flicker.
		EnsureGdiplusStarted();
		const int w = rc.right - rc.left;
		const int h = rc.bottom - rc.top;

		HDC memDC = CreateCompatibleDC(hdc);
		HBITMAP memBmp = CreateCompatibleBitmap(hdc, w, h);
		HGDIOBJ oldBmp = SelectObject(memDC, memBmp);

		Gdiplus::Graphics g(memDC);
		g.SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);
		g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);

		// Paint background (image if provided; otherwise a dark gradient)

		if (g_backgroundImage)
		{
			Gdiplus::Rect dest(0, 0, w, h);
			g.DrawImage(g_backgroundImage.get(), dest);

			// Add a subtle dark overlay to ensure text readability.
			Gdiplus::SolidBrush overlay(Gdiplus::Color(140, 0, 0, 0));
			g.FillRectangle(&overlay, dest);
		}
		else
		{
			// Simple vertical gradient
			TRIVERTEX vert[2]{};
			vert[0].x = 0; vert[0].y = 0;
			vert[0].Red = 0x1010; vert[0].Green = 0x1010; vert[0].Blue = 0x1414; vert[0].Alpha = 0xFFFF;
			vert[1].x = w; vert[1].y = h;
			vert[1].Red = 0x0000; vert[1].Green = 0x0000; vert[1].Blue = 0x0000; vert[1].Alpha = 0xFFFF;
			GRADIENT_RECT gRect{ 0,1 };
			GradientFill(memDC, vert, 2, &gRect, 1, GRADIENT_FILL_RECT_V);

			// subtle vignette overlay
			Gdiplus::Rect dest(0, 0, w, h);
			Gdiplus::SolidBrush overlay(Gdiplus::Color(80, 0, 0, 0));
			g.FillRectangle(&overlay, dest);
		}

		// Draw title + percent ourselves (prevents overlap/flicker from transparent STATIC controls)
		int progressY = (h * 60 / 100);
		const bool showBanner = !g_bannerText.empty();
		const int titleTop = showBanner ? (progressY - 125) : (progressY - 140);

		Gdiplus::FontFamily family(L"Segoe UI");
		Gdiplus::Font bannerFont(&family, 56.0f, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
		Gdiplus::Font titleFont(&family, 40.0f, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
		Gdiplus::Font percentFont(&family, 22.0f, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
		Gdiplus::SolidBrush white(Gdiplus::Color(240, 255, 255, 255));
		Gdiplus::SolidBrush whiteDim(Gdiplus::Color(210, 255, 255, 255));
		Gdiplus::SolidBrush bannerError(Gdiplus::Color(255, 255, 140, 140));
		Gdiplus::SolidBrush bannerInfo(Gdiplus::Color(255, 255, 250, 210));

		Gdiplus::StringFormat center;
		center.SetAlignment(Gdiplus::StringAlignmentCenter);
		center.SetLineAlignment(Gdiplus::StringAlignmentCenter);
		center.SetTrimming(Gdiplus::StringTrimmingEllipsisCharacter);

		if (showBanner)
		{
			Gdiplus::RectF bannerRect(0.0f, (Gdiplus::REAL)(progressY - 255), (Gdiplus::REAL)w, 95.0f);
			Gdiplus::SolidBrush* bannerBrush = g_bannerIsError ? &bannerError : &bannerInfo;
			g.DrawString(g_bannerText.c_str(), -1, &bannerFont, bannerRect, &center, bannerBrush);
		}

		Gdiplus::RectF titleRect(0.0f, (Gdiplus::REAL)titleTop, (Gdiplus::REAL)w, 70.0f);
		g.DrawString(g_titleText.c_str(), -1, &titleFont, titleRect, &center, &white);

		wchar_t pctBuf[16];
		swprintf(pctBuf, 16, L"%d%%", g_percent);
		Gdiplus::RectF pctRect(0.0f, (Gdiplus::REAL)(progressY + 34), (Gdiplus::REAL)w, 30.0f);
		g.DrawString(pctBuf, -1, &percentFont, pctRect, &center, &whiteDim);

		// Draw a modern progress bar (rounded, soft glow)
		const int barW = (w < 900) ? (w * 70 / 100) : 720;
		const int barH = 18;
		const int barX = (w - barW) / 2;
		const int barY = progressY;

		Gdiplus::RectF barRect((Gdiplus::REAL)barX, (Gdiplus::REAL)barY, (Gdiplus::REAL)barW, (Gdiplus::REAL)barH);
		Gdiplus::SolidBrush barBg(Gdiplus::Color(120, 20, 20, 20));
		Gdiplus::SolidBrush barFill(Gdiplus::Color(230, 0, 170, 255));

		// Rounded-rect path helper (GraphicsPath can't be copied/returned by value)
		auto addRoundRect = [](Gdiplus::GraphicsPath& path, const Gdiplus::RectF& r, Gdiplus::REAL radius) {
			const Gdiplus::REAL d = radius * 2.0f;
			path.Reset();
			path.AddArc(r.X, r.Y, d, d, 180, 90);
			path.AddArc(r.X + r.Width - d, r.Y, d, d, 270, 90);
			path.AddArc(r.X + r.Width - d, r.Y + r.Height - d, d, d, 0, 90);
			path.AddArc(r.X, r.Y + r.Height - d, d, d, 90, 90);
			path.CloseFigure();
		};

		Gdiplus::GraphicsPath bgPath;
		addRoundRect(bgPath, barRect, 9.0f);
		g.FillPath(&barBg, &bgPath);

		const float pct = (g_percent < 0) ? 0.0f : (g_percent > 100 ? 100.0f : (float)g_percent);
		if (pct > 0.0f)
		{
			Gdiplus::RectF fillRect = barRect;
			fillRect.Width = barRect.Width * (pct / 100.0f);
			Gdiplus::GraphicsPath fillPath;
			addRoundRect(fillPath, fillRect, 9.0f);
			g.FillPath(&barFill, &fillPath);
		}

		// Present buffer
		BitBlt(hdc, 0, 0, w, h, memDC, 0, 0, SRCCOPY);
		SelectObject(memDC, oldBmp);
		DeleteObject(memBmp);
		DeleteDC(memDC);

		EndPaint(hWnd, &ps);
		return 0;
	}

	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}

	return DefWindowProcW(hWnd, msg, wParam, lParam);
}

void SetProgress(DWORD percent)
{
	if (percent > 100) percent = 100;

	PostMessageW(hWnd, UPDATE_PERCENT, percent, 0);
}

void SetProgressText(const wchar_t* title)
{
	wchar_t* text = new wchar_t[128];
	swprintf(text, 128, L"%ls", title);

	PostMessageW(hWnd, WM_UPDATE_TEXT, (WPARAM)text, 0);
}

void DestoryProgress()
{
	PostMessageW(hWnd, WM_DESTROY, 0, 0);
}


void ShowProgress(DWORD Mode) {

	HINSTANCE hInst = GetModuleHandle(NULL);
	WNDCLASS wc = { 0 };

	wc.lpfnWndProc = WndProc;
	wc.hInstance = hInst;
	wc.lpszClassName = L"MyWin32Window";
	wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
	LPCTSTR windowTitle = TEXT("");

	RegisterClass(&wc);

	// Fullscreen (primary monitor)
	int screenWidth = GetSystemMetrics(SM_CXSCREEN);
	int screenHeight = GetSystemMetrics(SM_CYSCREEN);

	hWnd = CreateWindow(
		L"MyWin32Window",
		//windowTitle,
		L"",
		WS_POPUP,
		0, 0,
		screenWidth, screenHeight,
		NULL, NULL, hInst, NULL
	);

	
	// Text is drawn in WM_PAINT via GDI+, not STATIC controls.

	// Bring to front, full screen, no caption
	SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, screenWidth, screenHeight, SWP_SHOWWINDOW);
	UpdateWindow(hWnd);

	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
}