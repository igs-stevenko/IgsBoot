#ifndef __TPM_H__
#define __TPM_H__

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

int TPMUseKeyDec(const char* KeyName, BYTE* DataIn, DWORD DataInLen, BYTE* DataOut, DWORD* DataOutLen);


#endif