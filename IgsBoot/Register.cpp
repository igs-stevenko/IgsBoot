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


int ReadRegValue(const char* RegName, const char* ValName, DWORD* Val, DWORD* ValLen) {

	int rtn = 0;

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
		rtn = -2;
	}

	RegCloseKey(hKey);

	return rtn;
}

int CheckRegister(void) {

	int rtn = 0;

	/*
	DWORD RegVal = 0;
	DWORD RegValSize = sizeof(RegVal);
	DWORD t1 = GetTickCount64();
	rtn = ReadRegValue("SYSTEM\\CurrentControlSet\\Services\\mouhid", "Start", &RegVal, &RegValSize);
	DWORD t2 = GetTickCount64();
	if (rtn != 0) {
		printf("ReadRegValue error: %d\n", rtn);
		return -1;
	}
	*/



	return rtn;
}
