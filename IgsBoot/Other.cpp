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
#pragma warning(disable:4996)
int MountPartition(BYTE* key) {

	int rtn = 0;
	char cmd[512];
	sprintf(cmd,
		"\"C:\\VeraCrypt\\VeraCrypt-x64.exe\" "
		"/v \"C:\\Program Files (x86)\\IGS\\x.img\" "
		"/l X "
		"/p %s "
		"/q /s /m ro",
		key   // 密碼直接塞進來
	);

	STARTUPINFOA si;
	PROCESS_INFORMATION pi;
	ZeroMemory(&si, sizeof(si));
	ZeroMemory(&pi, sizeof(pi));
	si.cb = sizeof(si);

	// 用 CreateProcess 執行
	BOOL ok = CreateProcessA(
		NULL,
		cmd,            // 你的指令
		NULL,
		NULL,
		FALSE,
		CREATE_NO_WINDOW,
		NULL,
		NULL,
		&si,
		&pi
	);

	if (!ok) {
		printf("CreateProcess failed, error=%d\n", GetLastError());
		return 1;
	}

	WaitForSingleObject(pi.hProcess, INFINITE);

	DWORD exitCode = 0;
	GetExitCodeProcess(pi.hProcess, &exitCode);

	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);

	printf("VeraCrypt exit code = %lu\n", exitCode);

	if (exitCode != 0) {
		return 2;
	}

	return 0;
}

int MountPartitionU(BYTE* key, BYTE *path) {

	int rtn = 0;
	char cmd[512];
	sprintf(cmd,
		"\"C:\\VeraCrypt\\VeraCrypt-x64.exe\" "
		"/v \"%s\" "
		"/l X "
		"/p %s "
		"/q /s /m ro",
		path ,key 
	);

	STARTUPINFOA si;
	PROCESS_INFORMATION pi;
	ZeroMemory(&si, sizeof(si));
	ZeroMemory(&pi, sizeof(pi));
	si.cb = sizeof(si);

	// 用 CreateProcess 執行
	BOOL ok = CreateProcessA(
		NULL,
		cmd,            // 你的指令
		NULL,
		NULL,
		FALSE,
		CREATE_NO_WINDOW,
		NULL,
		NULL,
		&si,
		&pi
	);

	if (!ok) {
		printf("CreateProcess failed, error=%d\n", GetLastError());
		return 1;
	}

	WaitForSingleObject(pi.hProcess, INFINITE);

	DWORD exitCode = 0;
	GetExitCodeProcess(pi.hProcess, &exitCode);

	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);

	printf("VeraCrypt exit code = %lu\n", exitCode);

	if (exitCode != 0) {
		return 2;
	}

	return 0;
}

int UnMountPartitionX(void) {

	int rtn = 0;
	char cmd[512];
	sprintf(cmd,
		"\"C:\\VeraCrypt\\VeraCrypt-x64.exe\" /d X /f /q /s"
	);

	STARTUPINFOA si;
	PROCESS_INFORMATION pi;
	ZeroMemory(&si, sizeof(si));
	ZeroMemory(&pi, sizeof(pi));
	si.cb = sizeof(si);

	// 用 CreateProcess 執行
	BOOL ok = CreateProcessA(
		NULL,
		cmd,            // 你的指令
		NULL,
		NULL,
		FALSE,
		CREATE_NO_WINDOW,
		NULL,
		NULL,
		&si,
		&pi
	);

	if (!ok) {
		printf("CreateProcess failed, error=%d\n", GetLastError());
		return 1;
	}

	WaitForSingleObject(pi.hProcess, INFINITE);

	DWORD exitCode = 0;
	GetExitCodeProcess(pi.hProcess, &exitCode);

	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);

	printf("VeraCrypt exit code = %lu\n", exitCode);

	if (exitCode != 0) {
		return 2;
	}

	return 0;
}