// IgsBoot.cpp : 此檔案包含 'main' 函式。程式會於該處開始執行及結束執行。
//

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

#include "ProgressUI.h"
#include "TPM.h"
#include "Calculation.h"
#include "GetFromDevice.h"
#include "Register.h"
#include "Other.h"

#pragma warning(disable:4996)
#pragma comment(lib, "tbs.lib")
#pragma comment(lib, "libcrypto.lib")
#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(linker,"\"/manifestdependency:type='win32' \
 name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
 processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

enum {
    BOOT_MODE = 0,
    UPDATE_MODE
};

enum {
    GET_IMKEY_FAILED = 1,
    TPM_DEC_FAILED,
    GET_UUID_FAILED,
	DEC_PARTITIONKEY_FAILED,
    MOUNT_FAILED,
    TPM_FAILED,
    REGISTER_CHECK_FAILED

};


void ErrorMessage(int ErrorCode) {
    
    std::string msg = "E" + std::to_string(ErrorCode);

    MessageBoxA(nullptr, msg.c_str(), "Debug", MB_OK);
}

void print_hex(unsigned char* buf, int len) {

    int i = 0;
    for (i = 0; i < len; i++) {
        if (i % 16 == 0)	printf("\n");
        printf("0x%02x, ", buf[i]);
    }
}

int GetProcessMode(void) {

    int rtn = 0;

TODO:
    /*
    rtn = DetectUSBStorage()
    if(rtn == USB_STORAGE_DETECTED) {
        return UPDATE_MODE;
	}
    else{
        return BOOT_MODE;
    }
    */

    return BOOT_MODE;
}

bool LaunchGame()
{
    STARTUPINFOW si = { 0 };
    PROCESS_INFORMATION pi = { 0 };
    si.cb = sizeof(si);

    // 一定要用完整路徑
    wchar_t cmdLine[] = L"\"X:\\ImageLoader\\ImageLoader.exe\"";

    BOOL ok = CreateProcessW(
        NULL,           // lpApplicationName
        cmdLine,        // lpCommandLine（可改成含參數）
        NULL, NULL,
        FALSE,
        CREATE_NO_WINDOW,   // 不顯示黑視窗
        NULL,
        L"X:\\ImageLoader\\",            // ⭐ Working Directory（非常重要）
        &si,
        &pi
    );

    if (!ok) {
        DWORD err = GetLastError();
        // 這裡你可以記 log
        return false;
    }

    // 若你「不需要等 final.exe 結束」
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return true;
}

bool LaunchTarget(const wchar_t* exePath,
    const wchar_t* workDir,
    PROCESS_INFORMATION& pi)
{
    STARTUPINFOW si = { 0 };
    si.cb = sizeof(si);

    wchar_t cmdLine[MAX_PATH * 2];
    swprintf_s(cmdLine, L"\"%s\"", exePath);

    printf("[INFO] : %s %d\n", __func__, __LINE__);


    BOOL rtn = CreateProcessW(
        NULL,
        cmdLine,
        NULL, NULL,
        FALSE,
        0,
        NULL,
        workDir,
        &si,
        &pi
    );
	if (rtn != TRUE) {
        DWORD err = GetLastError();
        printf("CreateProcessW failed: 0x%X\n", err);
        return false;
    }

    return rtn;
}

void EnsureAlwaysRunning(const wchar_t* exePath,
    const wchar_t* workDir)
{
    PROCESS_INFORMATION pi = { 0 };
    DWORD lastLaunch = 0;

    while (true)
    {
        // 尚未啟動 or 已經結束
        if (pi.hProcess == NULL ||
            WaitForSingleObject(pi.hProcess, 0) == WAIT_OBJECT_0)
        {
            // 清理舊 handle
            if (pi.hProcess) {
                CloseHandle(pi.hProcess);
                CloseHandle(pi.hThread);
                pi = {};
            }

            // 啟動節流（避免 crash loop）
            DWORD now = GetTickCount();
            if (now - lastLaunch >= 3000)
            {
                if (LaunchTarget(exePath, workDir, pi))
                    lastLaunch = now;
            }
        }

        Sleep(1000); // 輪詢間隔
    }
}

int BootMode() {

    int rtn = 0;

    /* IMKeyEnLen是磁碟中介Key(密)，因為經過TPM加密，所以長度是256 Bytes 
       IMKeyDeLen是磁碟中介Key(明)，Array長度開出256bytes是因為解密時需要這麼大的空間，而實際上只有48Bytes是有效值 */
    BYTE IMKeyEn[256] = { 0x00 };
    BYTE IMKeyDe[256] = { 0x00 };


    DWORD IMKeyEnLen = sizeof(IMKeyEn);
    DWORD IMKeyDeLen = sizeof(IMKeyDe);

TODO:
    //檢查Register的設置是否都正確
    rtn = CheckRegister();
    if(rtn != 0){
        ErrorMessage(REGISTER_CHECK_FAILED);
        return -REGISTER_CHECK_FAILED;
	}

    SetProgress(10);

    rtn = ReadIMKeyEnFromEEProm(IMKeyEn, IMKeyEnLen);
    if (rtn != 0){
        ErrorMessage(-GET_IMKEY_FAILED);
        return -GET_IMKEY_FAILED;
    }

    SetProgress(20);

    print_hex(IMKeyEn, IMKeyEnLen);

    rtn = TPMUseKeyDec("IGSCardKey", IMKeyEn, IMKeyEnLen, IMKeyDe, &IMKeyDeLen);
    if(rtn != 0){
        ErrorMessage(-TPM_DEC_FAILED);
        return -TPM_DEC_FAILED;
    }

    SetProgress(30);

    //printf("IMKeyDeLen = %d\n", IMKeyDeLen);

    BYTE SerialNumber[256] = { 0x00 };
    DWORD SerialNumberLen = 0;

    rtn = GetLabelSerialNumber(SerialNumber, &SerialNumberLen);
    if(rtn != 0){
        ErrorMessage(-GET_UUID_FAILED);
        return -GET_UUID_FAILED;
	}

    SetProgress(50);

    BYTE SerialNumberSha256[32] = { 0x0 };
    SHA256(SerialNumber, SerialNumberLen, SerialNumberSha256);

    BYTE IV[16] = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                    0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f 
                  };

    /* 磁碟中介Key(明)使用AES解密後，會變成32Bytes的磁碟Key，是一個字串，所以實質上的array是33bytes */
    BYTE PartitionKey[33] = { 0x00 };
    DWORD PartitionKeyLen = 0;

    rtn = Aes256Decrypt(SerialNumberSha256, IV, IMKeyDe, IMKeyDeLen, PartitionKey, &PartitionKeyLen);
    if(rtn != 0){
        ErrorMessage(-DEC_PARTITIONKEY_FAILED);
        return -DEC_PARTITIONKEY_FAILED;
	}

    SetProgress(70);

    rtn = MountPartition(PartitionKey);
    if (rtn != 0) {
        ErrorMessage(-MOUNT_FAILED);
        return -MOUNT_FAILED;
    }

    SetProgress(100);

    return rtn;
}

int UpdateMode() {

    int rtn = 0;

    return rtn;
}

void WorkerThread(int mode)
{
    int rtn = 0;

    if (!WaitForTPM(60)) {
        // TPM 1 分鐘還沒 ready → 直接 fail
        ErrorMessage(-TPM_FAILED);
        return;
    }

    SetProgress(5);

    if (mode == BOOT_MODE) {
        rtn = BootMode();
        if (rtn != 0) {
            //ErrorMessage(rtn);
        }
    }
    else if (mode == UPDATE_MODE) {
        SetProgress(5);
        UpdateMode();
    }
}

int main(int argc, char* argv[])
{
    int  ProcessMode;

    //FreeConsole();

    ProcessMode = GetProcessMode();

    std::thread t(WorkerThread, ProcessMode);
    t.detach();   // 讓 thread 自己跑，不阻塞主程式

    ShowProgress();

    EnsureAlwaysRunning(L"X:\\Game\\Golden HoYeah.exe", L"X:\\Game");

    return 0;
}

