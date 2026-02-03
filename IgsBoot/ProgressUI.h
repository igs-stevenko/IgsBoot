#ifndef __PROGRESSUI_H__
#define __PROGRESSUI_H__
#include <windows.h>

#pragma once

void InitProgressBar(HWND hWnd);
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
void SetProgress(HWND hWnd, DWORD percent);
void ShowProgress();

#endif
