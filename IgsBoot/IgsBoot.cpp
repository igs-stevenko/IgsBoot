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
#include "IgsBoot.h"
#include "ProgressUI.h"
#include "TPM.h"
#include "Calculation.h"
#include "GetFromDevice.h"
#include "Register.h"
#include "Other.h"
#include "GameUpdate.h"
#include "File.h"

#pragma warning(disable:4996)
#pragma comment(lib, "tbs.lib")
#pragma comment(lib, "libcrypto.lib")
#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(linker,"\"/manifestdependency:type='win32' \
 name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
 processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#define XIMAGE "x.img"
#define MD5_FILE "x_md5.bin"
#define LOCAL_XIMAGE_PATH "C:\\Program Files (x86)\\IGS\\x.img"


void ErrorMessage(int ErrorCode) {
    
    std::string msg = "E" + std::to_string(ErrorCode);

    MessageBoxA(nullptr, msg.c_str(), "Debug", MB_OK);
}

void InfoMessage(const char *Info) {

    std::string msg = Info;

    MessageBoxA(nullptr, msg.c_str(), "INFO", MB_OK);
}

void print_hex(unsigned char* buf, int len) {

    int i = 0;
    for (i = 0; i < len; i++) {
        if (i % 16 == 0)	printf("\n");
        printf("0x%02x, ", buf[i]);
    }
}

int GetMode(void) {

    int rtn = 0;

	BYTE MountPath[128] = { 0 };
    

    /* 偵測有沒有USB Storage掛載起來，沒有則進入BOOT_MODE */
    rtn = DetectUSBStorage(MountPath);
    if(rtn == USB_STORAGE_NOT_DETECTED) {
        return BOOT_MODE;
	}
    else {
        BYTE XImagePath[128] = { 0 };
        sprintf((char*)XImagePath, "%s:\\%s", MountPath, XIMAGE);

        /* 檢查usb槽內是否有x.img，沒有則進入BOOT_MODE */
        rtn = DetectFile(XImagePath);
        if (rtn == FILE_NOT_EXIST) {
            rtn = BOOT_MODE;
        }
        else {
            rtn = UPDATE_MODE;
        }
    }

    return rtn;
}


bool LaunchTarget(const wchar_t* exePath,
    const wchar_t* workDir,
    PROCESS_INFORMATION& pi)
{
    STARTUPINFOW si = { 0 };
    si.cb = sizeof(si);

    wchar_t cmdLine[MAX_PATH * 2];
    swprintf_s(cmdLine, L"\"%s\"", exePath);

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
	int i = 0;

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
        ErrorMessage(BM_REGISTER_CHECK_FAILED);
        return -BM_REGISTER_CHECK_FAILED;
	}

    SetProgress(10);

    //每個與實體Device通訊的地方都要加上Retry
    //EEPROM Retry 3次
    for (i = 0; i < 3; i++) {
        rtn = ReadIMKeyEnFromEEProm(IMKeyEn, IMKeyEnLen);
        if (rtn != 0) {
			Sleep(1000); // 等待 1 秒後重試
            continue;
        }

        break;
    }

    if(i == 3){
        ErrorMessage(-BM_GET_IMKEY_FAILED);
        return -BM_GET_IMKEY_FAILED;
	}

    SetProgress(20);

    //print_hex(IMKeyEn, IMKeyEnLen);


    //TPM 也加入Retry 3次的機制
    for(i = 0; i < 3; i++) {
        rtn = TPMUseKeyDec("IGSCardKey", IMKeyEn, IMKeyEnLen, IMKeyDe, &IMKeyDeLen);
        if(rtn != 0){
            Sleep(1000); // 等待 1 秒後重試
            continue;
        }

        break;
	}
    if(i == 3){
        ErrorMessage(-BM_TPM_DEC_FAILED);
		return -BM_TPM_DEC_FAILED;
	}

    SetProgress(30);

    //printf("IMKeyDeLen = %d\n", IMKeyDeLen);

    BYTE SerialNumber[256] = { 0x00 };
    DWORD SerialNumberLen = 0;


	//取得SerialNumber也加入Retry 3次的機制
    for (i = 0; i < 3; i++) {
        rtn = GetLabelSerialNumber(SerialNumber, &SerialNumberLen);
        if(rtn != 0){
            Sleep(1000); // 等待 1 秒後重試
            continue;
        }
        break;
	}
    if(i == 3){
        ErrorMessage(-BM_GET_UUID_FAILED);
		return -BM_GET_UUID_FAILED;
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
        ErrorMessage(-BM_DEC_PARTITIONKEY_FAILED);
        return -BM_DEC_PARTITIONKEY_FAILED;
	}

    SetProgress(70);

    rtn = MountPartition(PartitionKey);
    if (rtn != 0) {
        ErrorMessage(-BM_MOUNT_FAILED);
        return -BM_MOUNT_FAILED;
    }

    SetProgress(100);

    return rtn;
}

int UpdateMode() {

    int rtn = 0;

    Sleep(1000);

    /* 取得usb掛載在哪一個槽 */
    BYTE MountPath[128] = { 0 };
    rtn = DetectUSBStorage(MountPath);
    if (rtn != USB_STORAGE_DETECTED) {
        ErrorMessage(-UM_USB_CHECK_FAILED);
        return -UM_USB_CHECK_FAILED;
    }

    printf("MountPath = %s\n", MountPath);
    
    SetProgress(10);

	BYTE XImagePath[128] = { 0 };
    sprintf((char*)XImagePath, "%s:\\%s", MountPath, XIMAGE);

    /* 檢查usb槽內是否有x.img */
    rtn = DetectFile(XImagePath);
    if(rtn != FILE_EXIST){
        ErrorMessage(-UM_XIMAGE_NOT_FOUND);
		return -UM_XIMAGE_NOT_FOUND;
	}

    SetProgress(15);

    /* 取得USB槽內的x.img的md5，取得失敗則要跳錯 */
	BYTE USBXImageMD5[16] = { 0 };
    rtn = GetMD5(XImagePath, USBXImageMD5);
    if (rtn != 0) {
        ErrorMessage(-UM_GET_MD5_FAILED);
		return -UM_GET_MD5_FAILED;
    }

    SetProgress(50);

    /* 取得USB內x_md5.bin的數值 */
    BYTE MD5[16] = { 0 };
	BYTE XImageMd5Path[128] = { 0 };
    sprintf((char*)XImageMd5Path, "%s:\\%s", MountPath, MD5_FILE);
    rtn = ReadFromFile((const char*)XImageMd5Path, MD5, 16);
    if (rtn != 0) {
        ErrorMessage(-UM_GET_MD5_FAILED);
        return -UM_GET_MD5_FAILED;
    }

    SetProgress(55);

    /* 比對USBXImageMD5[]與MD5[]是否相同，若不相同則停止 */
    if (memcmp(USBXImageMD5, MD5, sizeof(USBXImageMD5)) != 0) {
        ErrorMessage(-UM_CHECK_MD5_FAILED);
        return UM_GET_MD5_FAILED;
    }

    /* 檢查C槽內的x.img是否存在，若存在才去取得md5，若檔案不存在，則直接進行更新 */
    rtn = DetectFile((BYTE *)LOCAL_XIMAGE_PATH);
    if (rtn == FILE_EXIST) {

        /* 取得C槽內的x.img md5 ，如果Return錯誤就直接進行更新 */
        BYTE LocalXImageMD5[16] = { 0 };
        rtn = GetMD5((BYTE*)LOCAL_XIMAGE_PATH, LocalXImageMD5);
        if (rtn == 0) {
            /* 取得MD5成功，進行比對
            /* 比對C槽內的x.img使否與USB內的x.img相同 */
            /* 比對USBXImageMD5[]與LocalXImageMD5是否相同，若相同則不更新(代表檔案相同) */
            if (memcmp(USBXImageMD5, LocalXImageMD5, sizeof(USBXImageMD5)) == 0) {
                InfoMessage("Same Game, Please unplug usb storage");
                return -UM_XIMAGE_SAME_MD5;
            }
        }

        SetProgress(80);
    }


    /* 刪除C槽內的x.img檔案，不管有沒有刪除成功都繼續進行 */
    RemoveFile((BYTE *)LOCAL_XIMAGE_PATH);

    /* 複製usb內的x.im到C槽指定位置 */
    rtn = CopyFile(XImagePath, (BYTE*)LOCAL_XIMAGE_PATH);
    if(rtn != 0){
        ErrorMessage(-M_XIMAGE_COPY_FAILED);
		return -M_XIMAGE_COPY_FAILED; 
	}

    SetProgress(100);

    /* 更新完成，跳出更新完成的視窗 */
    InfoMessage("Update Completed, Please Reboot");

    return rtn;
}

void WorkerThread(int mode)
{
    int rtn = 0;


    if (!WaitForTPM(60)) {
        // TPM 1 分鐘還沒 ready → 直接 fail
        ErrorMessage(-BM_TPM_FAILED);
        return;
    }


    SetProgress(5);

    if (mode == BOOT_MODE) {
        
        rtn = BootMode();
        if (rtn != 0) {

        }
    }
    else if (mode == UPDATE_MODE) {

        rtn = UpdateMode();
        if (rtn != 0) {

        }
    }

    return;
}

int main(int argc, char* argv[])
{
    int  ProcessMode;

    //FreeConsole();

    ProcessMode = GetMode();

    std::thread t(WorkerThread, ProcessMode);
    t.detach();   // 讓 thread 自己跑，不阻塞主程式

    /* 在100%前，不會離開這支API */
    ShowProgress(ProcessMode);

    if (ProcessMode == BOOT_MODE) {

        printf("[%s][%d]\n", __func__, __LINE__);
        EnsureAlwaysRunning(L"X:\\Game\\Golden HoYeah.exe", L"X:\\Game");
    }
    else {
        printf("[%s][%d]\n", __func__, __LINE__);
        while (true)
        {
            Sleep(1);
        }
    }

    return 0;
}

