#ifndef __GETFROMDEVICE_H__
#define __GETFROMDEVICE_H__

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

int ReadIMKeyEnFromEEProm(BYTE* Buffer, int Len);
int GetLabelSerialNumber(BYTE* SerialNumber, DWORD* Len);

#endif