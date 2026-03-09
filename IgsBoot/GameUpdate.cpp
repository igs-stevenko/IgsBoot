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
#include <setupapi.h>
#include <devguid.h>
#include <winioctl.h>
#include "Calculation.h"
#include "GameUpdate.h"
#include "File.h"

#pragma warning(disable:4996)
#pragma comment(lib, "setupapi.lib")

#define LOCAL_XIMAGE_PATH "C:\\Program Files (x86)\\IGS\\x.img"
#define UPDATE_IMGAGE "x.img"
#define MD5_FILE "x_md5.bin"


bool FileExists(const char* FilePath)
{

    if (!FilePath) {
        return false;
    }

    DWORD attr = GetFileAttributesA(FilePath);

    if (attr == INVALID_FILE_ATTRIBUTES)
    {
        printf("Error Code: %lu\n", GetLastError());
        return false;
    }

    return !(attr & FILE_ATTRIBUTE_DIRECTORY);
}

bool GetFirstUSBMountedPath(BYTE* Path)
{
    if (!Path)
        return false;

    HDEVINFO hDevInfo = SetupDiGetClassDevs(
        &GUID_DEVINTERFACE_DISK,
        NULL,
        NULL,
        DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);

    if (hDevInfo == INVALID_HANDLE_VALUE)
        return false;

    SP_DEVICE_INTERFACE_DATA ifData;
    ifData.cbSize = sizeof(ifData);

    for (DWORD i = 0;
        SetupDiEnumDeviceInterfaces(
            hDevInfo, NULL,
            &GUID_DEVINTERFACE_DISK,
            i, &ifData);
        i++)
    {
        DWORD requiredSize = 0;
        SetupDiGetDeviceInterfaceDetail(
            hDevInfo, &ifData,
            NULL, 0,
            &requiredSize, NULL);

        PSP_DEVICE_INTERFACE_DETAIL_DATA detail =
            (PSP_DEVICE_INTERFACE_DETAIL_DATA)malloc(requiredSize);

        detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

        if (SetupDiGetDeviceInterfaceDetail(
            hDevInfo, &ifData,
            detail, requiredSize,
            NULL, NULL))
        {
            HANDLE hDisk = CreateFile(
                detail->DevicePath,
                0,
                FILE_SHARE_READ | FILE_SHARE_WRITE,
                NULL,
                OPEN_EXISTING,
                0,
                NULL);

            if (hDisk != INVALID_HANDLE_VALUE)
            {
                STORAGE_PROPERTY_QUERY query = {};
                query.PropertyId = StorageDeviceProperty;
                query.QueryType = PropertyStandardQuery;

                BYTE buffer[512];
                DWORD bytes;

                if (DeviceIoControl(
                    hDisk,
                    IOCTL_STORAGE_QUERY_PROPERTY,
                    &query,
                    sizeof(query),
                    buffer,
                    sizeof(buffer),
                    &bytes,
                    NULL))
                {
                    STORAGE_DEVICE_DESCRIPTOR* desc =
                        (STORAGE_DEVICE_DESCRIPTOR*)buffer;

                    if (desc->BusType == BusTypeUsb)
                    {
                        STORAGE_DEVICE_NUMBER number;
                        if (DeviceIoControl(
                            hDisk,
                            IOCTL_STORAGE_GET_DEVICE_NUMBER,
                            NULL, 0,
                            &number,
                            sizeof(number),
                            &bytes,
                            NULL))
                        {
                            CloseHandle(hDisk);
                            free(detail);

                            // 現在比對磁碟機字母
                            char drives[512];
                            DWORD len = GetLogicalDriveStringsA(
                                sizeof(drives), drives);

                            char* drive = drives;
                            while (*drive)
                            {
                                char volumePath[64];
                                sprintf_s(volumePath,
                                    "\\\\.\\%c:",
                                    drive[0]);

                                HANDLE hVol = CreateFileA(
                                    volumePath,
                                    0,
                                    FILE_SHARE_READ |
                                    FILE_SHARE_WRITE,
                                    NULL,
                                    OPEN_EXISTING,
                                    0,
                                    NULL);

                                if (hVol != INVALID_HANDLE_VALUE)
                                {
                                    STORAGE_DEVICE_NUMBER volNumber;
                                    if (DeviceIoControl(
                                        hVol,
                                        IOCTL_STORAGE_GET_DEVICE_NUMBER,
                                        NULL, 0,
                                        &volNumber,
                                        sizeof(volNumber),
                                        &bytes,
                                        NULL))
                                    {
                                        if (volNumber.DeviceNumber ==
                                            number.DeviceNumber)
                                        {
                                            *Path = drive[0];
                                            CloseHandle(hVol);
                                            SetupDiDestroyDeviceInfoList(
                                                hDevInfo);
                                            return true;
                                        }
                                    }
                                    CloseHandle(hVol);
                                }

                                drive += strlen(drive) + 1;
                            }
                        }
                    }
                }

                CloseHandle(hDisk);
            }
        }

        free(detail);
    }

    SetupDiDestroyDeviceInfoList(hDevInfo);
    return false;
}


/* 執行內容 :
	偵測是否有USB Storage掛載起來，沒有則回傳USB_STORAGE_NOT_DETECTED

   參數 :
    參數1.若有USB Storage被掛載起來，則此參數會被放入掛仔的路徑

   回傳值 :
      USB_STORAGE_DETECTED : 有USB Storage被掛載起來
      USB_STORAGE_NOT_DETECTED : 沒有任何USB Storage被掛載起來

 */
int DetectUSBStorage(BYTE *MountPath)
{
    if (!MountPath) {
        return -1;
    }

    BYTE pathBuf[128] = { 0 };

    if (GetFirstUSBMountedPath(pathBuf))
    {
        sprintf((char *)MountPath, "%s", pathBuf);
		return USB_STORAGE_DETECTED;
    }
    else
    {
        return USB_STORAGE_NOT_DETECTED;
    }
}


/* 執行內容 :
	查看是否有x.img在指定路徑上

   參數 :
	參數1.提供檔案完整路徑

   回傳值 :
      USB_STORAGE_DETECTED : 檔案存在
      USB_XIMAGE_NOT_DETECTED : 檔案不存在

 */
int DetectFile (BYTE *XImagePath)
{
    if (!XImagePath) {
        return -1;
    }

    int rtn = 0;
	bool FileFound = false;

    FileFound = FileExists((const char *)XImagePath);
    if (FileFound == true) {
        rtn = FILE_EXIST;
    }
    else {
		rtn = FILE_NOT_EXIST;
    }

    return rtn;
}

/*
*  執行內容 :
    1.取得參數1.檔案的MD5數值
   參數 :
    參數1.目標路徑
    參數2. 該檔案的MD5數值
   回傳值 :
       0.無錯誤
       負數:檔案不存在或任何動作失敗

*/
int GetMD5(BYTE* ImagePath, BYTE* MD5) {
    
    int rtn = 0;

    /* 如果這兩個陣列是空的，則返回錯誤 */
    if (ImagePath == NULL || MD5 == NULL) {
        return -1;
    }

    BYTE ImageMD5[16] = { 0 };
    rtn = CalcFileMD5((const char*)ImagePath, ImageMD5);
    if (rtn != 0) {
        return -2;
    }

    memcpy(MD5, ImageMD5, sizeof(ImageMD5));

    return rtn;
}


int RemoveFile(BYTE *FilePath) {
    
    if (!(DeleteFileA((const char *)FilePath)))
    {
        return -1;
    }

    return 0;
}

int CopyFile(BYTE *srcPath, BYTE *dstPath)
{
	int rtn = 0;
    BOOL cancel = FALSE;

    BOOL result = CopyFileExA(
        (const char *)srcPath,          // 來源
        (const char *)dstPath,          // 目標
        NULL,             // 不使用進度 callback
        NULL,
        &cancel,
        COPY_FILE_RESTARTABLE
    );

    if (!result)
    {
        DWORD err = GetLastError();
        return -1;
    }

    HANDLE hFile = CreateFileA(
        (const char*)dstPath,
        GENERIC_WRITE,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (hFile == INVALID_HANDLE_VALUE) {
        printf("Open failed\n");
        return -1;
    }

    if (!FlushFileBuffers(hFile)) {
        printf("Flush failed %d\n", GetLastError());
    }

    CloseHandle(hFile);

    return rtn;
}