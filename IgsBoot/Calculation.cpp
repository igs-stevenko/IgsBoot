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

#define CHUNK 4096

uint32_t crc32(const uint8_t* data, size_t length) {
	uint32_t crc = 0xFFFFFFFF;

	for (size_t i = 0; i < length; i++) {
		crc ^= data[i];

		for (int j = 0; j < 8; j++) {
			if (crc & 1)
				crc = (crc >> 1) ^ 0xEDB88320;
			else
				crc >>= 1;
		}
	}

	return ~crc;
}

int Aes256Decrypt(BYTE *Key,BYTE *IV, BYTE *Input, DWORD InputLen, BYTE *Output, DWORD *OutputLen)
{

	if (Key == NULL || IV == NULL || Input == NULL || Output == NULL || OutputLen == NULL)
		return -1;

	EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
	if (!ctx) return -2;

	if (!EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, Key, IV))
		return -3;


	DWORD Remain = InputLen;
	DWORD Offset = 0;
	DWORD LEN = 0;
	DWORD Total = 0;

	int DecError = 0;

	while (Offset < InputLen) {
		size_t DecLen = (InputLen - Offset > CHUNK)
			? CHUNK
			: InputLen - Offset;

		if (!EVP_DecryptUpdate(
			ctx,
			Output + Total,
			(int*)&LEN,
			Input + Offset,
			(int)DecLen
		)){
			DecError = -1;
			break;
		}

		Total += LEN;
		Offset += DecLen;
	}

	if (DecError != 0) {
		return -4;
	}

	if (!EVP_DecryptFinal_ex(ctx, Output + Total, (int*)&LEN)) {
		return -5;
	}

	Total += LEN;
	*OutputLen = Total;

	return 0;
}
