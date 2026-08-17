#include <iostream>
#include <fstream>
#include <vector>
#include <cstdio>
#include <windows.h>
#include <string>
#include <stdint.h>
#include <tbs.h>
#include <ncrypt.h>
#include <commctrl.h>


int HKLM_ReadRegValueD(const char* RegName, const char* ValName, DWORD *Val) {

	HKEY hKey;
	LONG status;
	DWORD value = 0;
	DWORD valueSize = sizeof(value);
	DWORD type = 0;

	status = RegOpenKeyExA(
		HKEY_LOCAL_MACHINE,
		RegName,
		0,
		KEY_READ,
		&hKey
	);

	if (status != ERROR_SUCCESS) {
		printf("RegOpenKeyExA failed: %ld\n", status);
		return -1;
	}

	status = RegQueryValueExA(
		hKey,
		ValName,
		NULL,
		&type,
		(LPBYTE)&value,
		&valueSize
	);

	if (!(status == ERROR_SUCCESS && type == REG_DWORD)) {
		printf("RegQueryValueEx failed: %ld\n", status);
		return -2;
	}

	RegCloseKey(hKey);

	*Val = value;

	return 0;
}

int HKCU_ReadRegValueD(const char* RegName, const char* ValName, DWORD* Val) {

	HKEY hKey;
	LONG status;
	DWORD value = 0;
	DWORD valueSize = sizeof(value);
	DWORD type = 0;

	status = RegOpenKeyExA(
		HKEY_CURRENT_USER,
		RegName,
		0,
		KEY_READ,
		&hKey
	);

	if (status != ERROR_SUCCESS) {
		printf("RegOpenKeyExA failed: %ld\n", status);
		return -1;
	}

	status = RegQueryValueExA(
		hKey,
		ValName,
		NULL,
		&type,
		(LPBYTE)&value,
		&valueSize
	);

	if (!(status == ERROR_SUCCESS && type == REG_DWORD)) {
		printf("RegQueryValueEx failed: %ld\n", status);
		return -2;
	}

	RegCloseKey(hKey);

	*Val = value;

	return 0;
}

int HKLM_WriteRegValueBin(const char* RegName, const char* ValName, const BYTE* Data, DWORD DataLen) {

	HKEY hKey;
	LONG status;
	DWORD disposition = 0;

	status = RegCreateKeyExA(
		HKEY_LOCAL_MACHINE,
		RegName,
		0,
		NULL,
		REG_OPTION_NON_VOLATILE,
		KEY_WRITE,
		NULL,
		&hKey,
		&disposition
	);

	if (status != ERROR_SUCCESS) {
		printf("HKLM_WriteRegValueBin: RegCreateKeyExA failed: %ld\n", status);
		return -1;
	}

	status = RegSetValueExA(
		hKey,
		ValName,
		0,
		REG_BINARY,
		Data,
		DataLen
	);

	if (status != ERROR_SUCCESS) {
		printf("HKLM_WriteRegValueBin: RegSetValueExA failed: %ld\n", status);
		RegCloseKey(hKey);
		return -2;
	}

	/* 確保寫入立即刷到磁碟 */
	RegFlushKey(hKey);
	RegCloseKey(hKey);

	return 0;
}

int CheckShell() {

	int rtn = 0;

	HKEY hKey = nullptr;
	const std::wstring& subKey = L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Winlogon";
	const std::wstring& valueName = L"Shell";

	// 開啟 Key（強制 64-bit registry）
	LONG status = RegOpenKeyExW(
		HKEY_LOCAL_MACHINE,
		subKey.c_str(),
		0,
		KEY_READ | KEY_WOW64_64KEY,
		&hKey
	);

	if (status != ERROR_SUCCESS)
		return -1;

	DWORD type = 0;
	DWORD size = 0;

	// 🔹 第一次：取得長度（bytes）
	status = RegQueryValueExW(
		hKey,
		valueName.c_str(),
		nullptr,
		&type,
		nullptr,
		&size
	);

	if (status != ERROR_SUCCESS || type != REG_SZ || size == 0)
	{
		RegCloseKey(hKey);
		return -2;
	}

	// size 是「bytes」，包含 NULL terminator
	// 轉成 wchar_t 數量
	DWORD wcharCount = size / sizeof(wchar_t);

	std::vector<wchar_t> buffer(wcharCount);

	// 🔹 第二次：真正讀資料
	status = RegQueryValueExW(
		hKey,
		valueName.c_str(),
		nullptr,
		nullptr,
		reinterpret_cast<LPBYTE>(buffer.data()),
		&size
	);

	RegCloseKey(hKey);

	if (status != ERROR_SUCCESS)
		return -3;

	std::wstring wstr(buffer.data());
	std::string narrow(wstr.begin(), wstr.end());

	if (strncmp("explorer.exe", narrow.data(), strlen("explorer.exe")) != 0) {
		return -4;
	}

	return 0;
}


int CheckRegister(void) {

	int rtn = 0;
	DWORD regValue = 0;
	
	
	//-----------------------

	rtn = HKLM_ReadRegValueD("SYSTEM\\ControlSet001\\Services\\Kbdclass", "Start", &regValue);
	if (rtn != 0) {
		printf("[HKLM] Read SYSTEM\\ControlSet001\\Services\\Kbdclass\\Start failed\n");
		return -1;
	}

	if (regValue != 4) {
		printf("[HKLM ]ERROR SYSTEM\\ControlSet001\\Services\\Kbdclass\\Start = %d\n", regValue);
		return -1;
	}

	//-----------------------

	rtn = HKLM_ReadRegValueD("SYSTEM\\ControlSet001\\Services\\Kbdclass", "Type", &regValue);
	if (rtn != 0) {
		printf("[HKLM] Read SYSTEM\\ControlSet001\\Services\\Kbdclass\\Type failed\n");
		return -1;
	}

	if (regValue != 16) {
		printf("[HKLM] ERROR SYSTEM\\ControlSet001\\Services\\Kbdclass\\Type = %d\n", regValue);
		return -1;
	}

	//-----------------------

	rtn = HKLM_ReadRegValueD("SYSTEM\\ControlSet001\\Services\\Kbdhid", "Start", &regValue);
	if (rtn != 0) {
		printf("[HKLM] Read SYSTEM\\ControlSet001\\Services\\Kbdclass\\Start failed\n");
		return -1;
	}

	if (regValue != 4) {
		printf("[HKLM] ERROR SYSTEM\\ControlSet001\\Services\\Kbdhid\\Start = %d\n", regValue);
		return -1;
	}

	//-----------------------

	rtn = HKLM_ReadRegValueD("SYSTEM\\ControlSet001\\Services\\Kbdhid", "Start", &regValue);
	if (rtn != 0) {
		printf("[HKLM] Read SYSTEM\\ControlSet001\\Services\\Kbdclass\\Start failed\n");
		return -1;
	}

	if (regValue != 4) {
		printf("[HKLM] ERROR SYSTEM\\ControlSet001\\Services\\Kbdhid\\Start = %d\n", regValue);
		return -1;
	}

	//-----------------------

	rtn = HKLM_ReadRegValueD("SYSTEM\\ControlSet001\\Services\\Kbdhid", "Type", &regValue);
	if (rtn != 0) {
		printf("[HKLM] Read SYSTEM\\ControlSet001\\Services\\Kbdclass\\Type failed\n");
		return -1;
	}

	if (regValue != 16) {
		printf("[HKLM] ERROR SYSTEM\\ControlSet001\\Services\\Kbdhid\\Type = %d\n", regValue);
		return -1;
	}

	//-----------------------

	rtn = HKLM_ReadRegValueD("SYSTEM\\ControlSet001\\Services\\Mouclass", "Start", &regValue);
	if (rtn != 0) {
		printf("[HKLM] Read SYSTEM\\ControlSet001\\Services\\Mouclass\\Start failed\n");
		return -1;
	}

	if (regValue != 4) {
		printf("[HKLM] ERROR SYSTEM\\ControlSet001\\Services\\Mouclass\\Start = %d\n", regValue);
		return -1;
	}

	//-----------------------

	rtn = HKLM_ReadRegValueD("SYSTEM\\ControlSet001\\Services\\Mouclass", "Type", &regValue);
	if (rtn != 0) {
		printf("[HKLM] Read SYSTEM\\ControlSet001\\Services\\Mouclass\\Type failed\n");
		return -1;
	}

	if (regValue != 16) {
		printf("[HKLM] ERROR SYSTEM\\ControlSet001\\Services\\Mouclass\\Type = %d\n", regValue);
		return -1;
	}

	//-----------------------

	rtn = HKLM_ReadRegValueD("SYSTEM\\ControlSet001\\Services\\Mouclass", "Type", &regValue);
	if (rtn != 0) {
		printf("[HKLM] Read SYSTEM\\ControlSet001\\Services\\Mouclass\\Type failed\n");
		return -1;
	}

	if (regValue != 16) {
		printf("[HKLM] ERROR SYSTEM\\ControlSet001\\Services\\Mouclass\\Type = %d\n", regValue);
		return -1;
	}

	//-----------------------

	rtn = HKLM_ReadRegValueD("SYSTEM\\ControlSet001\\Services\\Mouhid", "Start", &regValue);
	if (rtn != 0) {
		printf("[HKLM] Read SYSTEM\\ControlSet001\\Services\\Mouhid\\Start failed\n");
		return -1;
	}

	if (regValue != 4) {
		printf("[HKLM] ERROR SYSTEM\\ControlSet001\\Services\\Mouhid\\Start = %d\n", regValue);
		return -1;
	}

	//-----------------------

	rtn = HKLM_ReadRegValueD("SYSTEM\\ControlSet001\\Services\\Mouhid", "Type", &regValue);
	if (rtn != 0) {
		printf("[HKLM] Read SYSTEM\\ControlSet001\\Services\\Mouhid\\Type failed\n");
		return -1;
	}

	if (regValue != 16) {
		printf("[HKLM] ERROR SYSTEM\\ControlSet001\\Services\\Mouhid\\Type = %d\n", regValue);
		return -1;
	}

	//-----------------------

	rtn = HKCU_ReadRegValueD("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer", "NoFileURL", &regValue);
	if (rtn != 0) {
		printf("[HKCU] Read SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer\\NoFileURL failed\n");
		return -1;
	}

	if (regValue != 1) {
		printf("[HKCU] ERROR SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer\\NoFileURL = %d\n", regValue);
		return -1;
	}

	//-----------------------

	rtn = HKCU_ReadRegValueD("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer", "NoClose", &regValue);
	if (rtn != 0) {
		printf("[HKCU] Read SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer\\NoClose failed\n");
		return -1;
	}

	if (regValue != 1) {
		printf("[HKCU] ERROR SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer\\NoClose = %d\n", regValue);
		return -1;
	}

	//-----------------------

	rtn = HKCU_ReadRegValueD("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer", "NoControlPanel", &regValue);
	if (rtn != 0) {
		printf("[HKCU] Read SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer\\NoControlPanel failed\n");
		return -1;
	}

	if (regValue != 1) {
		printf("[HKCU] ERROR SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer\\NoControlPanel = %d\n", regValue);
		return -1;
	}

	//-----------------------

	rtn = HKCU_ReadRegValueD("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer", "NoFind", &regValue);
	if (rtn != 0) {
		printf("[HKCU] Read SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer\\NoFind failed\n");
		return -1;
	}

	if (regValue != 1) {
		printf("[HKCU] ERROR SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer\\NoFind = %d\n", regValue);
		return -1;
	}

	//-----------------------

	rtn = HKCU_ReadRegValueD("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer", "NoRun", &regValue);
	if (rtn != 0) {
		printf("[HKCU] Read SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer\\NoRun failed\n");
		return -1;
	}

	if (regValue != 1) {
		printf("[HKCU] ERROR SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer\\NoRun = %d\n", regValue);
		return -1;
	}

	//-----------------------

	rtn = HKLM_ReadRegValueD("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer", "NoRun", &regValue);
	if (rtn != 0) {
		printf("[HKLM] Read SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer\\NoRun failed\n");
		return -1;
	}

	if (regValue != 1) {
		printf("[HKLM] ERROR SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer\\NoRun = %d\n", regValue);
		return -1;
	}

	//-----------------------

	rtn = HKLM_ReadRegValueD("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\System", "EnableLUA", &regValue);
	if (rtn != 0) {
		printf("[HKLM] Read SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\System\\EnableLUA failed\n");
		return -1;
	}

	if (regValue != 0) {
		printf("[HKLM] ERROR SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\System\\EnableLUA = %d\n", regValue);
		return -1;
	}

	//-----------------------

	rtn = HKCU_ReadRegValueD("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer", "NoTrayItemsDisplay", &regValue);
	if (rtn != 0) {
		printf("[HKCU] Read SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer\\NoTrayItemsDisplay failed\n");
		return -1;
	}

	if (regValue != 1) {
		printf("[HKCU] ERROR SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer\\NoTrayItemsDisplay = %d\n", regValue);
		return -1;
	}

	//-----------------------

	rtn = HKCU_ReadRegValueD("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer", "NoDesktop", &regValue);
	if (rtn != 0) {
		printf("[HKCU] Read SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer\\NoDesktop failed\n");
		return -1;
	}

	if (regValue != 1) {
		printf("[HKCU] ERROR SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer\\NoDesktop = %d\n", regValue);
		return -1;
	}

	//-----------------------

	rtn = HKCU_ReadRegValueD("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer", "NoStartMenuMorePrograms", &regValue);
	if (rtn != 0) {
		printf("[HKCU] Read SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer\\NoStartMenuMorePrograms failed\n");
		return -1;
	}

	if (regValue != 1) {
		printf("[HKCU] ERROR SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer\\NoStartMenuMorePrograms = %d\n", regValue);
		return -1;
	}

	//-----------------------

	rtn = HKLM_ReadRegValueD("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer", "NoStartMenuMorePrograms", &regValue);
	if (rtn != 0) {
		printf("[HKLM] Read SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer\\NoStartMenuMorePrograms failed\n");
		return -1;
	}

	if (regValue != 1) {
		printf("[HKLM] ERROR SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer\\NoStartMenuMorePrograms = %d\n", regValue);
		return -1;
	}

	//-----------------------

	rtn = HKCU_ReadRegValueD("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer", "NoDrives", &regValue);
	if (rtn != 0) {
		printf("[HKCU] Read SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer\\NoDrives failed\n");
		return -1;
	}

	if (regValue != 67108863) {
		printf("[HKCU] ERROR SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer\\NoDrives = %d\n", regValue);
		return -1;
	}

	//-----------------------

	rtn = HKLM_ReadRegValueD("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer", "NoDrives", &regValue);
	if (rtn != 0) {
		printf("[HKLM] Read SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer\\NoDrives failed\n");
		return -1;
	}

	if (regValue != 67108863) {
		printf("[HKLM] ERROR SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer\\NoDrives = %d\n", regValue);
		return -1;
	}

	//-----------------------

	rtn = HKCU_ReadRegValueD("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\System", "NoFileSysPage", &regValue);
	if (rtn != 0) {
		printf("[HKCU] Read SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\System\\NoFileSysPage failed\n");
		return -1;
	}

	if (regValue != 1) {
		printf("[HKCU] ERROR SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\System\\NoFileSysPage = %d\n", regValue);
		return -1;
	}

	//-----------------------

	rtn = HKLM_ReadRegValueD("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\System", "NoFileSysPage", &regValue);
	if (rtn != 0) {
		printf("[HKLM] Read SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\System\\NoFileSysPage failed\n");
		return -1;
	}

	if (regValue != 1) {
		printf("[HKLM] ERROR SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\System\\NoFileSysPage = %d\n", regValue);
		return -1;
	}

	rtn = CheckShell();
	if(rtn != 0){
		printf("[HKLM] Read SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Winlogon\\Shell failed\n");
		return -1;
	}
	
	return 0;
}
