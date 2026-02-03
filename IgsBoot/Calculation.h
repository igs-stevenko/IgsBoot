#ifndef __CALCULATION_H__
#define __CALCULATION_H__

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

uint32_t crc32(const BYTE* data, DWORD length);
int Aes256Decrypt(BYTE* Key, BYTE* IV, BYTE* Input, DWORD InputLen, BYTE* Output, DWORD* OutputLen);

#endif
