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

/*
int TPMUseKeyDec(const char* KeyName, BYTE* DataIn, DWORD DataInLen, BYTE* DataOut, DWORD* DataOutLen) {

	int rtn = 0;

	SECURITY_STATUS status;
	NCRYPT_PROV_HANDLE hProv = 0;
	NCRYPT_KEY_HANDLE hKey = 0;
	wchar_t nameW[128] = { 0x00 };
	mbstowcs(nameW, KeyName, strlen(KeyName) + 1);

	BYTE decrypted[256];
	DWORD decryptedSize = sizeof(decrypted);

	// Step 1: 打開 TPM 金鑰儲存提供者
	status = NCryptOpenStorageProvider(
		&hProv,
		MS_PLATFORM_CRYPTO_PROVIDER,   // 使用 TPM
		0);
	if (status != ERROR_SUCCESS) {
		printf("Open provider failed: 0x%X\n", status);
		return -1;
	}


	status = NCryptOpenKey(
		hProv,
		&hKey,
		nameW,   // ⚡ 你建立金鑰時的名稱
		0,
		0);

	if (status != ERROR_SUCCESS) {
		printf("Open key failed: 0x%X\n", status);
		return -2;
	}

	// Step 4: 使用 TPM RSA 私鑰解密（私鑰在 TPM 內）

	status = NCryptDecrypt(
		hKey,
		DataIn, DataInLen,
		NULL,
		DataOut, *DataOutLen,
		DataOutLen,
		NCRYPT_PAD_PKCS1_FLAG);

	if (status != ERROR_SUCCESS) {
		printf("Decrypt failed: 0x%X\n", status);
		return -3;
	}

	NCryptFreeObject(hKey);
	NCryptFreeObject(hProv);

	return rtn;
}
*/

int TPMUseKeyDec(
    const char* KeyName,
    BYTE* DataIn,
    DWORD DataInLen,
    BYTE* DataOut,
    DWORD* DataOutLen
) {
    SECURITY_STATUS status;
    NCRYPT_PROV_HANDLE hProv = 0;
    NCRYPT_KEY_HANDLE hKey = 0;
    wchar_t nameW[128] = { 0 };

    mbstowcs(nameW, KeyName, strlen(KeyName) + 1);

    // 1. Open provider
    status = NCryptOpenStorageProvider(
        &hProv,
        MS_PLATFORM_CRYPTO_PROVIDER,
        0
    );
    if (status != ERROR_SUCCESS) return -1;

    // 2. Open key
    status = NCryptOpenKey(
        hProv,
        &hKey,
        nameW,
        0,
        0
    );
    if (status != ERROR_SUCCESS) return -2;

    // 3. OAEP padding info (⚠️ 必須跟加密一致)
    BCRYPT_OAEP_PADDING_INFO oaep = {
        BCRYPT_SHA256_ALGORITHM,
        NULL,
        0
    };

    DWORD plainLen = 0;

    // 4. 第一次呼叫：問需要多大 buffer
    status = NCryptDecrypt(
        hKey,
        DataIn,
        DataInLen,
        &oaep,
        NULL,
        0,
        &plainLen,
        NCRYPT_PAD_OAEP_FLAG
    );
    if (status != ERROR_SUCCESS) {
        NCryptFreeObject(hKey);
        NCryptFreeObject(hProv);
        printf("[%s][%d]: 0x%X\n", __func__, __LINE__, status);
        return -3;
    }

    // 呼叫端必須確保 DataOut 夠大
    if (*DataOutLen < plainLen) {
        NCryptFreeObject(hKey);
        NCryptFreeObject(hProv);
        printf("plainLen = %d, DataOutLen = %d\n", plainLen, *DataOutLen);
        printf("[%s][%d]\n", __func__, __LINE__);
        return -4;
    }

    // 5. 第二次呼叫：真正解密
    status = NCryptDecrypt(
        hKey,
        DataIn,
        DataInLen,
        &oaep,
        DataOut,
        plainLen,
        &plainLen,
        NCRYPT_PAD_OAEP_FLAG
    );
    if (status != ERROR_SUCCESS) {
        NCryptFreeObject(hKey);
        NCryptFreeObject(hProv);
        printf("[%s][%d]: 0x%X\n", __func__, __LINE__, status);
        return -5;
    }

    *DataOutLen = plainLen;

    NCryptFreeObject(hKey);
    NCryptFreeObject(hProv);
    return 0;
}
