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
#include <algorithm>

#include "IgsBoot.h"
#include "GitVersion.h"

// GDI+ (for background image)
#pragma comment(lib, "gdiplus.lib")
// GradientFill (fallback background gradient)
#pragma comment(lib, "Msimg32.lib")

HWND hWnd;
int progressValue = 0;

#define UPDATE_PERCENT (WM_APP + 1)
#define WM_UPDATE_TEXT (WM_APP + 2)
#define WM_SET_BANNER (WM_APP + 3)
#define WM_ANIM_TIMER_ID 1001

static ULONG_PTR g_gdiplusToken = 0;
static std::wstring g_backgroundImagePath; // single image path (legacy)
static std::unique_ptr<Gdiplus::Image> g_backgroundImage; // single image (legacy)

// PNG sequence animation
static std::wstring g_sequenceFolderPath;          // folder containing PNG frames
static std::vector<std::unique_ptr<Gdiplus::Image>> g_frames; // loaded frames
static int g_currentFrame = 0;                     // current frame index
static int g_frameIntervalMs = 33;                 // ~30 FPS by default
static DWORD g_lastFrameTime = 0;                  // timestamp of last frame advance

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
	SetWindowPos(hWnd, HWND_NOTOPMOST, 0, 0, 0, 0,
		SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
	ShowWindow(hWnd, SW_HIDE);
}

void BackgroundProgress()
{
	if (!hWnd || !IsWindow(hWnd)) return;
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

static void LoadSequenceFrames()
{
	g_frames.clear();
	g_currentFrame = 0;

	if (g_sequenceFolderPath.empty()) return;

	EnsureGdiplusStarted();

	// Get screen size for pre-scaling
	int screenW = GetSystemMetrics(SM_CXSCREEN);
	int screenH = GetSystemMetrics(SM_CYSCREEN);
	bool isPortrait = (screenH > screenW);

	// Search for PNG files in the folder
	std::wstring searchPath = g_sequenceFolderPath + L"\\*.png";
	WIN32_FIND_DATAW fd;
	HANDLE hFind = FindFirstFileW(searchPath.c_str(), &fd);
	if (hFind == INVALID_HANDLE_VALUE) return;

	std::vector<std::wstring> filenames;
	do {
		if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
			filenames.push_back(fd.cFileName);
		}
	} while (FindNextFileW(hFind, &fd));
	FindClose(hFind);

	// Sort filenames alphabetically so frame order is correct
	std::sort(filenames.begin(), filenames.end());

	// Determine target width based on orientation:
	// Landscape: 90% of screen width
	// Portrait: 1.5x of original image width (1000 * 1.5 = 1500)
	int targetW;
	if (isPortrait) {
		targetW = 0; // will be set per-frame based on original size * 1.5
	} else {
		targetW = (int)(screenW * 0.90f);
	}

	// Pre-scale each frame to target size
	for (const auto& fname : filenames) {
		std::wstring fullPath = g_sequenceFolderPath + L"\\" + fname;
		Gdiplus::Image srcImg(fullPath.c_str());
		if (srcImg.GetLastStatus() != Gdiplus::Ok) continue;

		int imgW = srcImg.GetWidth();
		int imgH = srcImg.GetHeight();

		// Calculate actual target width
		int actualTargetW = isPortrait ? (int)(imgW * 1.5f) : targetW;
		float scale = (float)actualTargetW / (float)imgW;
		int targetH = (int)(imgH * scale);

		// Create pre-scaled bitmap
		auto scaledBmp = std::make_unique<Gdiplus::Bitmap>(actualTargetW, targetH, PixelFormat32bppARGB);
		if (!scaledBmp || scaledBmp->GetLastStatus() != Gdiplus::Ok) continue;

		Gdiplus::Graphics gfx(scaledBmp.get());
		gfx.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
		gfx.DrawImage(&srcImg, 0, 0, actualTargetW, targetH);

		g_frames.push_back(std::move(scaledBmp));
	}
}

// Set a single static background image path (legacy API)
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

// Set PNG sequence folder path and frame rate
// folderPath: folder containing PNG files (sorted alphabetically = frame order)
// fps: frames per second (e.g. 30)
void SetProgressBackgroundSequence(const wchar_t* folderPath, int fps)
{
	if (!folderPath || !*folderPath) {
		g_sequenceFolderPath = L"";
		return;
	}

	DWORD attr = GetFileAttributesW(folderPath);
	if (attr == INVALID_FILE_ATTRIBUTES || !(attr & FILE_ATTRIBUTE_DIRECTORY)) {
		g_sequenceFolderPath = L"";
		return;
	}

	g_sequenceFolderPath = folderPath;
	if (fps <= 0) fps = 30;
	g_frameIntervalMs = 1000 / fps;
}

void ErrorMessage(int ErrorCode, int CodeLine) {
	std::wstring msg = L"E" + std::to_wstring(ErrorCode) + L":" + std::to_wstring(CodeLine);
	// Set banner directly (thread-safe for simple assignment)
	g_bannerText = msg;
	g_bannerIsError = true;
	// Also try PostMessage for proper repaint
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

// Helper: draw the current background frame (sequence or static image or gradient)
static void DrawBackground(Gdiplus::Graphics& g, HDC memDC, int w, int h)
{
	// Always fill black background first
	Gdiplus::SolidBrush blackBrush(Gdiplus::Color(255, 0, 0, 0));
	g.FillRectangle(&blackBrush, 0, 0, w, h);

	Gdiplus::Image* bgImg = nullptr;

	// Priority: PNG sequence > single image
	if (!g_frames.empty()) {
		bgImg = g_frames[g_currentFrame].get();
	}
	else if (g_backgroundImage) {
		bgImg = g_backgroundImage.get();
	}

	if (bgImg)
	{
		// Draw pre-scaled frame
		// Position: horizontally centered
		// Landscape: vertically centered around 30% from top (upper area)
		// Portrait: vertically centered around 35% from top
		int imgW = bgImg->GetWidth();
		int imgH = bgImg->GetHeight();
		int drawX = (w - imgW) / 2;

		float verticalCenter = 0.50f;
		int drawY = (int)(h * verticalCenter) - (imgH / 2);

		Gdiplus::Rect dest(drawX, drawY, imgW, imgH);
		g.DrawImage(bgImg, dest);
	}
}

// UI thread WindowProc
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_ERASEBKGND:
		return 1;

	case WM_UPDATE_TEXT:
	{
		wchar_t* text = (wchar_t*)wParam;
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
		LoadSequenceFrames();
		InitProgressBar(hWnd);
		// Start animation timer if we have frames
		// Use 16ms interval (high frequency) for smooth animation;
		// actual frame advance is controlled by elapsed time check
		if (!g_frames.empty()) {
			SetTimer(hWnd, WM_ANIM_TIMER_ID, 16, NULL);
		}
		return 0;

	case WM_TIMER:
	{
		if (wParam == WM_ANIM_TIMER_ID && !g_frames.empty()) {
			// Use elapsed time to ensure consistent animation speed regardless of screen
			DWORD now = GetTickCount();
			if (g_lastFrameTime == 0) g_lastFrameTime = now;
			DWORD elapsed = now - g_lastFrameTime;
			if (elapsed >= (DWORD)g_frameIntervalMs) {
				int framesToAdvance = (int)(elapsed / g_frameIntervalMs);
				g_currentFrame = (g_currentFrame + framesToAdvance) % (int)g_frames.size();
				g_lastFrameTime = now;
				InvalidateRect(hWnd, nullptr, FALSE);
			}
		}
		return 0;
	}

	case WM_SIZE:
	{
		InvalidateRect(hWnd, nullptr, TRUE);
		return 0;
	}

	case UPDATE_PERCENT:
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

		EnsureGdiplusStarted();
		const int w = rc.right - rc.left;
		const int h = rc.bottom - rc.top;

		// Double-buffer
		HDC memDC = CreateCompatibleDC(hdc);
		HBITMAP memBmp = CreateCompatibleBitmap(hdc, w, h);
		HGDIOBJ oldBmp = SelectObject(memDC, memBmp);

		Gdiplus::Graphics g(memDC);
		g.SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);
		g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);

		// Draw background
		DrawBackground(g, memDC, w, h);

		// Adaptive layout: use bottom region for all UI elements
		// Portrait mode (h > w): position at 4/5 of screen height
		// Landscape mode (w >= h): position at bottom 1/3
		const bool isPortrait = (h > w);
		const int regionTop = isPortrait ? (h * 3 / 5) : (h * 2 / 3);
		const int regionH = h - regionTop;
		int progressY = regionTop + (regionH * 60 / 100);
		const bool showBanner = !g_bannerText.empty();
		const int bannerTop = regionTop + (regionH * 5 / 100);
		const int titleTop = progressY - (regionH * 20 / 100);

		Gdiplus::FontFamily family(L"Segoe UI");
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
			Gdiplus::REAL bannerFontSize = (h < 800) ? 36.0f : 56.0f;
			Gdiplus::Font bannerFontAdj(&family, bannerFontSize, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
			// Show banner above the title text, within the UI region
			int bannerY = titleTop - (int)(bannerFontSize * 1.5f);
			Gdiplus::RectF bannerRect(0.0f, (Gdiplus::REAL)bannerY, (Gdiplus::REAL)w, (Gdiplus::REAL)(bannerFontSize * 1.4f));
			Gdiplus::SolidBrush* bannerBrush = g_bannerIsError ? &bannerError : &bannerInfo;
			g.DrawString(g_bannerText.c_str(), -1, &bannerFontAdj, bannerRect, &center, bannerBrush);
		}

		Gdiplus::REAL titleFontSize = (h < 800) ? 28.0f : 40.0f;
		Gdiplus::Font titleFontAdj(&family, titleFontSize, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
		Gdiplus::RectF titleRect(0.0f, (Gdiplus::REAL)titleTop, (Gdiplus::REAL)w, (Gdiplus::REAL)(regionH * 20 / 100));
		g.DrawString(g_titleText.c_str(), -1, &titleFontAdj, titleRect, &center, &white);

		wchar_t pctBuf[16];
		swprintf(pctBuf, 16, L"%d%%", g_percent);
		Gdiplus::RectF pctRect(0.0f, (Gdiplus::REAL)(progressY + 24), (Gdiplus::REAL)w, 30.0f);
		g.DrawString(pctBuf, -1, &percentFont, pctRect, &center, &whiteDim);

		// Progress bar
		const int barW = (w < 900) ? (w * 70 / 100) : 720;
		const int barH = 18;
		const int barX = (w - barW) / 2;
		const int barY = progressY;

		Gdiplus::RectF barRect((Gdiplus::REAL)barX, (Gdiplus::REAL)barY, (Gdiplus::REAL)barW, (Gdiplus::REAL)barH);
		Gdiplus::SolidBrush barBg(Gdiplus::Color(120, 20, 20, 20));
		Gdiplus::SolidBrush barFill(Gdiplus::Color(230, 0, 170, 255));

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

		// Draw git version in bottom-right corner
		{
			wchar_t verBuf[64];
			swprintf(verBuf, 64, L"v%hs", GIT_VERSION);
			Gdiplus::Font verFont(&family, 14.0f, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
			Gdiplus::SolidBrush verBrush(Gdiplus::Color(120, 255, 255, 255));
			Gdiplus::StringFormat rightAlign;
			rightAlign.SetAlignment(Gdiplus::StringAlignmentFar);
			rightAlign.SetLineAlignment(Gdiplus::StringAlignmentFar);
			Gdiplus::RectF verRect(0.0f, 0.0f, (Gdiplus::REAL)(w - 10), (Gdiplus::REAL)(h - 10));
			g.DrawString(verBuf, -1, &verFont, verRect, &rightAlign, &verBrush);
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
		KillTimer(hWnd, WM_ANIM_TIMER_ID);
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

	RegisterClass(&wc);

	int screenWidth = GetSystemMetrics(SM_CXSCREEN);
	int screenHeight = GetSystemMetrics(SM_CYSCREEN);

	hWnd = CreateWindow(
		L"MyWin32Window",
		L"",
		WS_POPUP,
		0, 0,
		screenWidth, screenHeight,
		NULL, NULL, hInst, NULL
	);

	SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, screenWidth, screenHeight, SWP_SHOWWINDOW);
	UpdateWindow(hWnd);

	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
}
