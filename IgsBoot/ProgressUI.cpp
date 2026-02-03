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


HWND hWnd;
HWND hProgress;
HWND hLabel;
int progressValue = 0;

#define UPDATE_PERCENT (WM_APP + 1)

void InitProgressBar(HWND hWnd)
{
	INITCOMMONCONTROLSEX icex;
	icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
	icex.dwICC = ICC_PROGRESS_CLASS;
	InitCommonControlsEx(&icex);

	hProgress = CreateWindowEx(
		0, PROGRESS_CLASS, NULL,
		WS_CHILD | WS_VISIBLE,
		20, 20, 300, 25,
		hWnd, NULL, GetModuleHandle(NULL), NULL
	);

	SendMessage(hProgress, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
	SendMessage(hProgress, PBM_SETPOS, 0, 0);

	// 百分比文字 Label，在進度條下面
	hLabel = CreateWindowExW(
		0, L"STATIC", L"0%",
		WS_CHILD | WS_VISIBLE | SS_CENTER,
		20, 50, 300, 20,
		hWnd, nullptr, GetModuleHandleW(nullptr), nullptr
	);
}

// UI thread 的 WindowProc
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
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

		// ★ 收到 100% 時（UI 控制後續動作）
		if (percent == 100) {
			// 例如關閉視窗
			DestroyWindow(hWnd);

			// 或發訊息給主程式
			// PostMessageW(hWnd, WM_APP + 2, 0, 0);
		}

		return 0;
	}

	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProcW(hWnd, msg, wParam, lParam);
}

void SetProgress(HWND hWnd, DWORD percent)
{
	if (percent < 0) percent = 0;
	if (percent > 100) percent = 100;

	PostMessageW(hWnd, UPDATE_PERCENT, percent, 0);
}


void ShowProgress() {

	HINSTANCE hInst = GetModuleHandle(NULL);

	WNDCLASS wc = { 0 };
	wc.lpfnWndProc = WndProc;
	wc.hInstance = hInst;
	wc.lpszClassName = L"MyWin32Window";
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

	RegisterClass(&wc);

	hWnd = CreateWindow(
		L"MyWin32Window",
		L"Auto Progress Demo",
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT,
		400, 150,
		NULL, NULL, hInst, NULL
	);

	ShowWindow(hWnd, SW_SHOW);
	UpdateWindow(hWnd);

	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
}