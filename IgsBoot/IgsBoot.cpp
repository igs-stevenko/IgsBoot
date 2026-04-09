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
#include "Reboot.h"
#include "Usb.h"

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

struct InsideUsbInfo UsbInfo;
BYTE MountPath[512] = { 0 };
int ProcessMode = 0;
int Status = -1;

int UI_PercentStart = 0;
int UI_PercentEnd = 0;
int UI_Reset = 0;
int UI_Stop = 0;

void UI_SetPercent(int start, int end) {
    UI_PercentStart = start;
    UI_PercentEnd = end;
    UI_Reset = 1;
}

void UI_SetStop() {
    UI_Stop = 1;
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
   
    /* 掃描內部的USB Port */
    UsbInfo_Init(&UsbInfo);
    ScanInsideUsbDisk(&UsbInfo);
    ListUsbInfo(&UsbInfo);

    /* 偵測有沒有USB內部的Port Storage掛載起來，沒有則進入BOOT_MODE */
    if (UsbInfo.PortDown_found == 0 && UsbInfo.PortUp_found == 0) {
        return BOOT_MODE;
    }

    /* 檢查內部的USB下Port掛載起來的磁碟區內，是否有x.img */
    if (UsbInfo.PortDown_found) {
        BYTE XImagePath[512] = { 0 };
        sprintf((char*)XImagePath, "%s%s", UsbInfo.PortDown_mountpath, XIMAGE);
        printf("XImagePath = %s\n", XImagePath);
        rtn = DetectFile(XImagePath);
        if (rtn == FILE_EXIST) {
            /* 找到x.img，把這個槽的路徑記到MountPath這個全域變數內 */
            memcpy(MountPath, UsbInfo.PortDown_mountpath, strlen(UsbInfo.PortDown_mountpath));
            return UPDATE_MODE;
        }
    }
    
    /* 檢查內部的USB上Port掛載起來的磁碟區內，是否有x.img */
    if(UsbInfo.PortUp_found) {
        BYTE XImagePath[512] = { 0 };
        sprintf((char*)XImagePath, "%s%s", UsbInfo.PortUp_mountpath, XIMAGE);
        printf("XImagePath = %s\n", XImagePath);
        rtn = DetectFile(XImagePath);
        if (rtn == FILE_EXIST) {
            /* 找到x.img，把這個槽的路徑記到MountPath這個全域變數內 */
            memcpy(MountPath, UsbInfo.PortUp_mountpath, strlen(UsbInfo.PortUp_mountpath));
            return UPDATE_MODE;
        }
    }

    /* 都沒有的話，進入BOOT_MODE */
    return BOOT_MODE;
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


    UI_SetPercent(0, 10);
    //檢查Register的設置是否都正確 
    /*
    rtn = CheckRegister();
    if(rtn != 0){
        ErrorMessage(BM_REGISTER_CHECK_FAILED, __LINE__);
        return -BM_REGISTER_CHECK_FAILED;
	}
    */

    // TPM 1 分鐘還沒 ready → 直接 fail
    if (!WaitForTPM(60)) {
        UI_SetStop();
        ErrorMessage(-BM_TPM_FAILED, __LINE__);
        return -BM_TPM_FAILED;
    }
    

    UI_SetPercent(0, 20);

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
        UI_SetStop();
        ErrorMessage(-BM_GET_IMKEY_FAILED, __LINE__);
        return -BM_GET_IMKEY_FAILED;
	} 

    UI_SetPercent(20, 30);

    //print_hex(IMKeyEn, IMKeyEnLen);

    //TPM 也加入Retry 3次的機制
    for(i = 0; i < 3; i++) {
        rtn = TPMUseKeyDec("OCP", IMKeyEn, IMKeyEnLen, IMKeyDe, &IMKeyDeLen);
        if(rtn != 0){
            Sleep(1000); // 等待 1 秒後重試
            continue;
        }

        break;
	}
    if(i == 3){
        UI_SetStop();
        ErrorMessage(-BM_TPM_DEC_FAILED, __LINE__);
		return -BM_TPM_DEC_FAILED;
	}

    UI_SetPercent(30, 50);

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
        UI_SetStop();
        ErrorMessage(-BM_GET_UUID_FAILED, __LINE__);
		return -BM_GET_UUID_FAILED;
	}

    UI_SetPercent(50, 70);

    BYTE SerialNumberSha256[32] = { 0x0 };
    SHA256(SerialNumber, SerialNumberLen, SerialNumberSha256);

    BYTE IV[16] = { 0xA7,0x3C,0x91,0xF2,0x5D,0x8E,0x47,0x1B,
                    0xC9,0x22,0x6F,0xD4,0x0A,0xB8,0x73,0x5E
                  };

    /* 磁碟中介Key(明)使用AES解密後，會變成32Bytes的磁碟Key，是一個字串，所以實質上的array是33bytes */
    BYTE PartitionKey[33] = { 0x00 };
    DWORD PartitionKeyLen = 0;

    rtn = Aes256Decrypt(SerialNumberSha256, IV, IMKeyDe, IMKeyDeLen, PartitionKey, &PartitionKeyLen);
    if(rtn != 0){
        UI_SetStop();
        ErrorMessage(-BM_DEC_PARTITIONKEY_FAILED, __LINE__);
        return -BM_DEC_PARTITIONKEY_FAILED;
	}

    UI_SetPercent(50, 99);

    rtn = MountPartition(PartitionKey);
    if (rtn != 0) {
        UI_SetStop();
        ErrorMessage(-BM_MOUNT_FAILED, __LINE__);
        return -BM_MOUNT_FAILED;
    }

    UI_SetPercent(99, 100);

    /* 啟動遊戲 */
    EnsureAlwaysRunning(L"X:\\Game\\Golden HoYeah.exe", L"X:\\Game");

    return rtn;
}

int TryMountUsbXImg(BYTE *XImagePath) {

    int rtn = 0;
    int i = 0;

    /* IMKeyEnLen是磁碟中介Key(密)，因為經過TPM加密，所以長度是256 Bytes
       IMKeyDeLen是磁碟中介Key(明)，Array長度開出256bytes是因為解密時需要這麼大的空間，而實際上只有48Bytes是有效值 */
    BYTE IMKeyEn[256] = { 0x00 };
    BYTE IMKeyDe[256] = { 0x00 };
    DWORD IMKeyEnLen = sizeof(IMKeyEn);
    DWORD IMKeyDeLen = sizeof(IMKeyDe);


    // TPM 1 分鐘還沒 ready → 直接 fail
    if (!WaitForTPM(60)) {
        return -1;
    }


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

    if (i == 3) {
        return -2;
    }

    //TPM 也加入Retry 3次的機制
    for (i = 0; i < 3; i++) {
        rtn = TPMUseKeyDec("OCP", IMKeyEn, IMKeyEnLen, IMKeyDe, &IMKeyDeLen);
        if (rtn != 0) {
            Sleep(1000); // 等待 1 秒後重試
            continue;
        }

        break;
    }
    if (i == 3) {
        return -3;
    }


    BYTE SerialNumber[256] = { 0x00 };
    DWORD SerialNumberLen = 0;


    for (i = 0; i < 3; i++) {
        rtn = GetLabelSerialNumber(SerialNumber, &SerialNumberLen);
        if (rtn != 0) {
            Sleep(1000); // 等待 1 秒後重試
            continue;
        }
        break;
    }
    if (i == 3) {
        return -4;
    }

    BYTE SerialNumberSha256[32] = { 0x0 };
    SHA256(SerialNumber, SerialNumberLen, SerialNumberSha256);

    BYTE IV[16] = { 0xA7,0x3C,0x91,0xF2,0x5D,0x8E,0x47,0x1B,
                    0xC9,0x22,0x6F,0xD4,0x0A,0xB8,0x73,0x5E
    };

    /* 磁碟中介Key(明)使用AES解密後，會變成32Bytes的磁碟Key，是一個字串，所以實質上的array是33bytes */
    BYTE PartitionKey[33] = { 0x00 };
    DWORD PartitionKeyLen = 0;

    rtn = Aes256Decrypt(SerialNumberSha256, IV, IMKeyDe, IMKeyDeLen, PartitionKey, &PartitionKeyLen);
    if (rtn != 0) {
        return -5;
    }

    rtn = MountPartitionU(PartitionKey, XImagePath);
    if (rtn != 0) {
        return -6;
    }

    return rtn;
}

int UpdateMode() {

    int rtn = 0;       
    
    UI_SetPercent(0, 5);

	BYTE XImagePath[512] = { 0 };
    sprintf((char*)XImagePath, "%s%s", MountPath, XIMAGE);

    /* 檢查usb槽內是否有x.img */
    rtn = DetectFile(XImagePath);
    if(rtn != FILE_EXIST){
        UI_SetStop();
        ErrorMessage(-UM_XIMAGE_NOT_FOUND, __LINE__);
		return -UM_XIMAGE_NOT_FOUND;
	}

    /* 嘗試使用現在機器上的Key去掛起這個x.img，若掛不起來則代表不匹配，不進行更新 */
    rtn = TryMountUsbXImg(XImagePath);
    if (rtn != 0) {
        UI_SetStop();
        ErrorMessage(-UM_XIMAGE_MOUNT_ERRER, __LINE__);
		return -UM_XIMAGE_MOUNT_ERRER;
    }

    /* 若掛載成功後，Delay一下在進行卸載 */
    Sleep(1000);

    /* 卸載X槽 */
	rtn = UnMountPartitionX();
    if (rtn != 0) {
        UI_SetStop();
        ErrorMessage(-UM_XIMAGE_UNMOUNT_ERRER, __LINE__);
        return -UM_XIMAGE_UNMOUNT_ERRER;
    }

    Sleep(1000);

    UI_SetPercent(5, 10);

    /* 取得USB內x_md5.bin的數值 */
    BYTE MD5[16] = { 0 };
    BYTE XImageMd5Path[128] = { 0 };
    sprintf((char*)XImageMd5Path, "%s%s", MountPath, MD5_FILE);
    rtn = ReadFromFile((const char*)XImageMd5Path, MD5, 16);
    if (rtn != 0) {
        UI_SetStop();
        ErrorMessage(-UM_GET_MD5_FAILED, __LINE__);
        return -UM_GET_MD5_FAILED;
    }

    UI_SetPercent(10, 70);

    /* 取得USB槽內的x.img的md5，取得失敗則要跳錯 */
	BYTE USBXImageMD5[16] = { 0 };
    rtn = GetMD5(XImagePath, USBXImageMD5);
    if (rtn != 0) {
        UI_SetStop();
        ErrorMessage(-UM_GET_MD5_FAILED, __LINE__);
		return -UM_GET_MD5_FAILED;
    }

    UI_SetPercent(70, 80);

    /* 比對USBXImageMD5[]與MD5[]是否相同，若不相同則停止 */
    if (memcmp(USBXImageMD5, MD5, sizeof(USBXImageMD5)) != 0) {
        UI_SetStop();
        ErrorMessage(-UM_CHECK_MD5_FAILED, __LINE__);
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
                return -UM_XIMAGE_SAME_MD5;
            }
        }

        UI_SetPercent(80, 90);
    }

    UI_SetPercent(80, 90);

    /* 刪除C槽內的x.img檔案，不管有沒有刪除成功都繼續進行 */
    RemoveFile((BYTE *)LOCAL_XIMAGE_PATH);

    /* 複製usb內的x.im到C槽指定位置 */
    rtn = CopyFile(XImagePath, (BYTE*)LOCAL_XIMAGE_PATH);
    if(rtn != 0){
        UI_SetStop();
        ErrorMessage(-UM_XIMAGE_COPY_FAILED, __LINE__);
		return -UM_XIMAGE_COPY_FAILED; 
	}

    UI_SetPercent(90, 95);

    BYTE LocalXImageMD5[16] = { 0 };
    rtn = GetMD5((BYTE*)LOCAL_XIMAGE_PATH, LocalXImageMD5);
    if (rtn != 0) {
        UI_SetStop();
        ErrorMessage(-UM_XIMAGE_COPY_FAILED, __LINE__);
        return -UM_XIMAGE_COPY_FAILED;
    }

    UI_SetPercent(95, 99);

    if (memcmp(USBXImageMD5, LocalXImageMD5, sizeof(USBXImageMD5)) != 0) {
        UI_SetStop();
        ErrorMessage(-UM_XIMAGE_COPY_FAILED, __LINE__);
        return -UM_XIMAGE_COPY_FAILED;
    }

    UI_SetPercent(99, 100);

    /* 更新完成 */
    Sleep(3000);

    CreateThread(NULL, 0, MsgThread, NULL, 0, NULL);

    Sleep(2000);

    Reboot();

    return rtn;
}

void GenUIThread(int mode)
{
    int rtn = 0;

    ShowProgress(ProcessMode);
}

void UIThread(int mode) {

	int NowPercent = 0;
    int TargetPercent = 0;
    int toggle = 0;
    int count = 0;
    int speed = 0;

    while (1) {
        
        if (UI_Stop) {
            break;
        }

        if (UI_Reset == 1) {
            UI_Reset = 0;
            NowPercent = UI_PercentStart;
            TargetPercent = UI_PercentEnd;
        }
      

        Sleep(500);

        if (ProcessMode == BOOT_MODE) {
            if (toggle == 0) {
                SetProgressText(TEXT("Game Loading"));
                toggle++;
            }
            else if(toggle == 1){
                SetProgressText(TEXT("Game Loading ."));
                toggle++;
            }
            else {
                SetProgressText(TEXT("Game Loading .."));
                toggle = 0;
            }

            speed = 1;
        }
        else {
            if (toggle == 0) {
                SetProgressText(TEXT("Game Updating"));
                toggle++;
            }
            else if (toggle == 1) {
                SetProgressText(TEXT("Game Updating ."));
                toggle++;
            }
            else {
                SetProgressText(TEXT("Game Updating .."));
                toggle = 0;
            }

            speed = 6;
        }


        /* 每多少speed才進一次%數 ，當100%時，直接進入 */
        if (count % speed == 0 || NowPercent >= 100) {

            SetProgress(NowPercent);

            if (NowPercent < TargetPercent) {
                NowPercent++;
            }

            if (NowPercent >= 100) {
                NowPercent = 100;
                if (ProcessMode == BOOT_MODE) {
                    SetProgressText(TEXT("Game Start"));
                }
                else {
					SetProgressText(TEXT("Update Completed"));
                }
            }
        }

        count++;
    }
}

int main(int argc, char* argv[])
{
    int rtn = 0;

    FreeConsole();

    std::thread t(GenUIThread, ProcessMode);
    t.detach();   // 讓 thread 自己跑，不阻塞主程式

    Sleep(1000);

    std::thread pr(UIThread, 0);
    pr.detach();   // 讓 thread 自己跑，不阻塞主程式
    
    ProcessMode = GetMode();

    if (ProcessMode == BOOT_MODE) {

        rtn = BootMode();
        if (rtn != 0) {
            printf("[%s][%d]\n", __func__, __LINE__);
        }
    }
    else if (ProcessMode == UPDATE_MODE) {

        rtn = UpdateMode();
        if (rtn == -UM_XIMAGE_SAME_MD5) {
            /* 如果更新碟遊戲與C槽遊戲相同 */
            ProcessMode = BOOT_MODE;
            BootMode();
        }
    }

    while (true) {
        Sleep(1);
    }

    return 0;
}

