#ifndef __PROGRESSUI_H__
#define __PROGRESSUI_H__
#include <windows.h>

#pragma once

void InitProgressBar(HWND hWnd);
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
void SetProgress(DWORD percent);
void ShowProgress(DWORD Mode);
void DestoryProgress();
void SetProgressText(const wchar_t* title);
// Make progress UI not block the launched game window.
void HideProgress();
void BackgroundProgress();
void ErrorMessage(int ErrorCode, int CodeLine);
void InfoMessage(const char* Info);
DWORD WINAPI MsgThread(LPVOID lpParam);
void SetProgressBackgroundImagePath(const wchar_t* path);


#endif
