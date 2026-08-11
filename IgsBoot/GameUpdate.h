#pragma once
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
#include <openssl/sha.h>

enum {
	FILE_EXIST = 0,
	FILE_NOT_EXIST
};

enum {
	USB_STORAGE_DETECTED = 0,
	USB_STORAGE_NOT_DETECTED,
};

int DetectUSBStorage(BYTE* MountPath);
int DetectFile(BYTE* ImagePath);
int GetSHA1(BYTE* ImagePath, const BYTE* Key, int KeyLen, BYTE* SHA1Hash);
int RemoveFile(BYTE* FilePath);
int CopyFile(BYTE* srcPath, BYTE* dstPath);