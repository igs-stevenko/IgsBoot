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

#include "IgsBoot.h"

HWND hWnd;
HWND hTitle;
HWND hProgress;
HWND hLabel;
HFONT hFont;
int progressValue = 0;

#define UPDATE_PERCENT (WM_APP + 1)
#define WM_UPDATE_TEXT (WM_APP + 2)

void InitProgressBar(HWND hWnd)
{
	INITCOMMONCONTROLSEX icex;
	icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
	icex.dwICC = ICC_PROGRESS_CLASS;
	InitCommonControlsEx(&icex);

	hProgress = CreateWindowEx(
		0, PROGRESS_CLASS, NULL,
		WS_CHILD | WS_VISIBLE,
		20, 90, 325, 25,
		hWnd, NULL, GetModuleHandle(NULL), NULL
	);
	

	SendMessage(hProgress, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
	SendMessage(hProgress, PBM_SETPOS, 0, 0);

	// 百分比文字 Label，在進度條下面
	hLabel = CreateWindowExW(
		0, L"STATIC", L"0%",
		WS_CHILD | WS_VISIBLE | SS_CENTER,
		20, 120, 325, 20,
		hWnd, nullptr, GetModuleHandleW(nullptr), nullptr
	);
}

// UI thread 的 WindowProc
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_UPDATE_TEXT:
	{
		
		wchar_t* text = (wchar_t*)wParam;
		//printf("[%s][%d] : %ls (len=%d)\n", __func__, __LINE__, text, wcslen(text));
		SetWindowTextW(hTitle, text);
		InvalidateRect(hTitle, NULL, TRUE);
		UpdateWindow(hTitle);
		delete[] text;
		return 0;
	}
	case WM_CTLCOLORSTATIC:
	{
		if ((HWND)lParam == hTitle)
		{
			HDC hdc = (HDC)wParam;

			SetBkMode(hdc, TRANSPARENT);
			SetTextColor(hdc, RGB(0, 0, 0)); // optional

			return (LRESULT)GetSysColorBrush(COLOR_WINDOW);
		}
		break;
	}
	case WM_CREATE:
		InitProgressBar(hWnd);
		return 0;

	case UPDATE_PERCENT:   // 背景 thread 來的訊息
	{
		int percent = (int)wParam;
		if (percent < 0) percent = 0;
		if (percent > 100) percent = 100;

		SendMessageW(hProgress, PBM_SETPOS, percent, 0);

		wchar_t buf[16];
		swprintf(buf, 16, L"%d%%", percent);
		SetWindowTextW(hLabel, buf);

		return 0;
	}

	case WM_DESTROY:
		PostQuitMessage(0);
		DestroyWindow(hWnd);
		return 0;
	}

	return DefWindowProcW(hWnd, msg, wParam, lParam);
}

void SetProgress(DWORD percent)
{
	if (percent < 0) percent = 0;
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
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	LPCTSTR windowTitle = TEXT("");

	RegisterClass(&wc);

	// ====== 你原本想要的「內容區大小」 ======
	int clientWidth = 400;
	int clientHeight = 200;

	// ====== 用來計算「含邊框的實際視窗大小」 ======
	RECT rc = { 0, 0, clientWidth, clientHeight };
	AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);

	int winWidth = rc.right - rc.left;
	int winHeight = rc.bottom - rc.top;

	// ====== 取得螢幕大小 ======
	int screenWidth = GetSystemMetrics(SM_CXSCREEN);
	int screenHeight = GetSystemMetrics(SM_CYSCREEN);

	// ====== 用「修正後大小」來置中 ======
	int x = (screenWidth - winWidth) / 2;
	int y = (screenHeight - winHeight) / 2;

	hWnd = CreateWindow(
		L"MyWin32Window",
		//windowTitle,
		L"",
		WS_OVERLAPPEDWINDOW,
		x, y,
		clientWidth, clientHeight,
		NULL, NULL, hInst, NULL
	);

	
	hFont = CreateFont(
		-40, 0, 0, 0,
		FW_NORMAL,
		FALSE, FALSE, FALSE,
		DEFAULT_CHARSET,
		OUT_DEFAULT_PRECIS,
		CLIP_DEFAULT_PRECIS,
		DEFAULT_QUALITY,
		DEFAULT_PITCH | FF_DONTCARE,
		L"Arial"
	);

	// ====== 新增大字標題 ======
	hTitle = CreateWindow(
		L"STATIC",
		L"Loading ..",   // 用你原本的文字
		WS_VISIBLE | WS_CHILD | SS_LEFT,
		20, 20, clientWidth, 60,   //  位置 + 高度
		hWnd,                     //  一定要指定 parent
		NULL,
		hInst,
		NULL
	);

	SendMessage(hTitle, WM_SETFONT, (WPARAM)hFont, TRUE);

	ShowWindow(hWnd, SW_SHOW);
	UpdateWindow(hWnd);

	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
}