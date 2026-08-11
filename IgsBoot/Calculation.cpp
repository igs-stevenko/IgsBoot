#include <iostream>
#include <fstream>
#include <vector>
#include <cstdio>
#include <windows.h>
#include <string>
#include <openssl/evp.h>
#include <openssl/params.h>
#include <openssl/rand.h>
#include <stdint.h>
#include <tbs.h>
#include <ncrypt.h>
#include <commctrl.h>

#define CHUNK_4K 4096
#define CHUNK_4M (1024 * 1024 * 4)   // 4MB buffer

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
		size_t DecLen = (InputLen - Offset > CHUNK_4K)
			? CHUNK_4K
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


int CalcFileHMACSHA1(const char* filename, const unsigned char* key, int keyLen, unsigned char* out_hmac)
{
	if (keyLen <= 0) {
		return -1;
	}

	FILE* fp = NULL;
	fopen_s(&fp, filename, "rb");
	if (!fp) {
		printf("Open file failed\n");
		return -1;
	}

	EVP_MAC* mac = EVP_MAC_fetch(NULL, "HMAC", NULL);
	if (!mac) {
		fclose(fp);
		return -2;
	}

	EVP_MAC_CTX* ctx = EVP_MAC_CTX_new(mac);
	if (!ctx) {
		EVP_MAC_free(mac);
		fclose(fp);
		return -2;
	}

	OSSL_PARAM params[] = {
		OSSL_PARAM_construct_utf8_string("digest", (char*)"SHA1", 0),
		OSSL_PARAM_construct_end()
	};

	if (EVP_MAC_init(ctx, key, keyLen, params) != 1) {
		EVP_MAC_CTX_free(ctx);
		EVP_MAC_free(mac);
		fclose(fp);
		return -3;
	}

	unsigned char* buffer = (unsigned char*)malloc(CHUNK_4M);
	if (!buffer) {
		EVP_MAC_CTX_free(ctx);
		EVP_MAC_free(mac);
		fclose(fp);
		return -4;
	}

	size_t bytesRead = 0;

	while ((bytesRead = fread(buffer, 1, CHUNK_4M, fp)) > 0)
	{
		if (EVP_MAC_update(ctx, buffer, bytesRead) != 1) {
			free(buffer);
			EVP_MAC_CTX_free(ctx);
			EVP_MAC_free(mac);
			fclose(fp);
			return -5;
		}
	}

	if (ferror(fp)) {
		free(buffer);
		EVP_MAC_CTX_free(ctx);
		EVP_MAC_free(mac);
		fclose(fp);
		return -5;
	}

	size_t md_len = 0;
	if (EVP_MAC_final(ctx, out_hmac, &md_len, 20) != 1) {
		free(buffer);
		EVP_MAC_CTX_free(ctx);
		EVP_MAC_free(mac);
		fclose(fp);
		return -6;
	}

	free(buffer);
	fclose(fp);
	EVP_MAC_CTX_free(ctx);
	EVP_MAC_free(mac);

	return 0;
}
