#include <iostream>
#include <fstream>
#include <vector>
#include <cstdio>
#include <windows.h>
#include <string>
#include <stdint.h>
#include <tbs.h>
#include <ncrypt.h>
#include <commctrl.h>
#include <thread>
#include <setupapi.h>
#include <devguid.h>
#include <stdio.h>
#include <cfgmgr32.h>
#include <initguid.h>
#include <usbiodef.h>
#include <winioctl.h>
#include <string.h>
#include <devpkey.h>
#include "Usb.h"

#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "cfgmgr32.lib")


#pragma warning(disable:4996)


// ----------------------------
// 判斷是否 USB Storage
// ----------------------------
bool IsMassStorage(HDEVINFO hDevInfo, SP_DEVINFO_DATA* dev)
{
    char buf[512] = { 0 };

    if (!SetupDiGetDeviceRegistryPropertyA(
        hDevInfo, dev,
        SPDRP_COMPATIBLEIDS,
        NULL,
        (PBYTE)buf,
        sizeof(buf),
        NULL))
        return false;

    return strstr(buf, "Class_08") != NULL;
}

// ----------------------------
// 解析 VID / PID
// ----------------------------
void ParseVidPid(const char* hwid, std::string& vid, std::string& pid)
{
    const char* v = strstr(hwid, "VID_");
    const char* p = strstr(hwid, "PID_");

    if (v) vid.assign(v + 4, 4);
    if (p) pid.assign(p + 4, 4);
}

// ----------------------------
// 取得 Container ID（核心）
// ----------------------------
bool GetContainerId(HDEVINFO hDevInfo, SP_DEVINFO_DATA* dev, GUID& guid)
{
    DEVPROPTYPE type;

    return SetupDiGetDevicePropertyW(
        hDevInfo,
        dev,
        &DEVPKEY_Device_ContainerId,
        &type,
        (PBYTE)&guid,
        sizeof(GUID),
        NULL,
        0);
}

std::string FindDriveLetterFromPhysicalDrive(int physicalDrive)
{
    char volumeName[MAX_PATH];

    HANDLE hFind = FindFirstVolumeA(volumeName, sizeof(volumeName));
    if (hFind == INVALID_HANDLE_VALUE)
        return "";

    std::string result = "";

    do {
        size_t len = strlen(volumeName);

        
        if (len > 0 && volumeName[len - 1] == '\\')
            volumeName[len - 1] = '\0';
        

        HANDLE hVol = CreateFileA(
            volumeName,
            0,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL,
            OPEN_EXISTING,
            0,
            NULL);

        if (hVol != INVALID_HANDLE_VALUE) {

            STORAGE_DEVICE_NUMBER sdn;
            DWORD bytes;

            if (DeviceIoControl(
                hVol,
                IOCTL_STORAGE_GET_DEVICE_NUMBER,
                NULL, 0,
                &sdn, sizeof(sdn),
                &bytes, NULL))
            {
                if ((int)sdn.DeviceNumber == physicalDrive) {

                    char paths[512] = { 0 };
                    DWORD ret = 0;
                    
                    //printf("physicalDrive = %d\n", physicalDrive);
                    //printf("sdn.DeviceNumber = %d\n", sdn.DeviceNumber);
                    volumeName[len - 1] = '\\';
                    //printf("volumeName = %s\n", volumeName);

                    if (GetVolumePathNamesForVolumeNameA(
                        volumeName,
                        paths,
                        sizeof(paths),
                        &ret) &&
                        paths[0] != '\0')
                    {
                        // ⭐ 找到真正有 drive letter 的
                        CloseHandle(hVol);
                        FindVolumeClose(hFind);
                        return std::string(paths);
                    }

                    // 記錄 fallback，但不要 return
                    result = volumeName;
                }
            }

            CloseHandle(hVol);
        }

        volumeName[len - 1] = '\\';

    } while (FindNextVolumeA(hFind, volumeName, sizeof(volumeName)));

    FindVolumeClose(hFind);

    return result;  // 最後才 fallback
}

// ----------------------------
// 主流程
// ----------------------------
void EnumerateUsbStorage()
{
    HDEVINFO usbInfo = SetupDiGetClassDevs(
        &GUID_DEVCLASS_USB,
        NULL,
        NULL,
        DIGCF_PRESENT);

    SP_DEVINFO_DATA usbDev;
    usbDev.cbSize = sizeof(SP_DEVINFO_DATA);

    for (DWORD i = 0; SetupDiEnumDeviceInfo(usbInfo, i, &usbDev); i++) {

        if (!IsMassStorage(usbInfo, &usbDev))
            continue;

        // Location
        char location[512] = { 0 };
        SetupDiGetDeviceRegistryPropertyA(
            usbInfo,
            &usbDev,
            SPDRP_LOCATION_PATHS,
            NULL,
            (PBYTE)location,
            sizeof(location),
            NULL);

        // VID / PID
        char hwid[512] = { 0 };
        SetupDiGetDeviceRegistryPropertyA(
            usbInfo,
            &usbDev,
            SPDRP_HARDWAREID,
            NULL,
            (PBYTE)hwid,
            sizeof(hwid),
            NULL);

        std::string vid, pid;
        ParseVidPid(hwid, vid, pid);

        // Container ID
        GUID usbGuid;
        if (!GetContainerId(usbInfo, &usbDev, usbGuid))
            continue;
        
        /*
        printf("USB Storage Found\n");
        printf("  Location : %s\n", location);
        printf("  VID/PID  : %s / %s\n", vid.c_str(), pid.c_str());
        */

        // ----------------------------
        // Disk devices
        // ----------------------------
        HDEVINFO diskInfo = SetupDiGetClassDevs(
            &GUID_DEVINTERFACE_DISK,
            NULL,
            NULL,
            DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);

        SP_DEVICE_INTERFACE_DATA ifData;
        ifData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

        for (DWORD j = 0; SetupDiEnumDeviceInterfaces(
            diskInfo, NULL,
            &GUID_DEVINTERFACE_DISK, j, &ifData); j++)
        {
            DWORD required = 0;

            SetupDiGetDeviceInterfaceDetail(
                diskInfo, &ifData,
                NULL, 0,
                &required,
                NULL);

            auto detail =
                (PSP_DEVICE_INTERFACE_DETAIL_DATA_A)malloc(required);

            detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_A);

            SP_DEVINFO_DATA diskDev;
            diskDev.cbSize = sizeof(SP_DEVINFO_DATA);

            if (!SetupDiGetDeviceInterfaceDetailA(
                diskInfo,
                &ifData,
                detail,
                required,
                NULL,
                &diskDev))
            {
                free(detail);
                continue;
            }

            GUID diskGuid;
            if (!GetContainerId(diskInfo, &diskDev, diskGuid)) {
                free(detail);
                continue;
            }

            // 🔥 Container ID match
            if (memcmp(&usbGuid, &diskGuid, sizeof(GUID)) != 0) {
                free(detail);
                continue;
            }

            // 取得 PhysicalDrive
            HANDLE hDisk = CreateFileA(
                detail->DevicePath,
                0,
                FILE_SHARE_READ | FILE_SHARE_WRITE,
                NULL,
                OPEN_EXISTING,
                0,
                NULL);

            if (hDisk == INVALID_HANDLE_VALUE) {
                free(detail);
                continue;
            }

            STORAGE_DEVICE_NUMBER sdn;
            DWORD bytes;

            if (!DeviceIoControl(
                hDisk,
                IOCTL_STORAGE_GET_DEVICE_NUMBER,
                NULL, 0,
                &sdn, sizeof(sdn),
                &bytes,
                NULL))
            {
                CloseHandle(hDisk);
                free(detail);
                continue;
            }

            CloseHandle(hDisk);

            int physicalDrive = sdn.DeviceNumber;

            //printf("  PhysicalDrive: %d\n", physicalDrive);

            // 找 Drive
            std::string drive =
                FindDriveLetterFromPhysicalDrive(physicalDrive);

            /*
            if (!drive.empty())
                printf("  Drive    : %s\n", drive.c_str());
            else
                printf("  Drive    : (not found)\n");
            */

            free(detail);
        }

        SetupDiDestroyDeviceInfoList(diskInfo);

        printf("\n");
    }

    SetupDiDestroyDeviceInfoList(usbInfo);
}


int ScanInsideUsbDisk(struct InsideUsbInfo *UsbInfo) {

    HDEVINFO usbInfo = SetupDiGetClassDevs(
        &GUID_DEVCLASS_USB,
        NULL,
        NULL,
        DIGCF_PRESENT);

    SP_DEVINFO_DATA usbDev;
    usbDev.cbSize = sizeof(SP_DEVINFO_DATA);

    for (DWORD i = 0; SetupDiEnumDeviceInfo(usbInfo, i, &usbDev); i++) {

        if (!IsMassStorage(usbInfo, &usbDev))
            continue;

        // Location
        char location[512] = { 0 };
        SetupDiGetDeviceRegistryPropertyA(
            usbInfo,
            &usbDev,
            SPDRP_LOCATION_PATHS,
            NULL,
            (PBYTE)location,
            sizeof(location),
            NULL);

        // VID / PID
        char hwid[512] = { 0 };
        SetupDiGetDeviceRegistryPropertyA(
            usbInfo,
            &usbDev,
            SPDRP_HARDWAREID,
            NULL,
            (PBYTE)hwid,
            sizeof(hwid),
            NULL);

        std::string vid, pid;
        ParseVidPid(hwid, vid, pid);

        // Container ID
        GUID usbGuid;
        if (!GetContainerId(usbInfo, &usbDev, usbGuid))
            continue;

        if ((strncmp(INSIDE_USB_LOCATION_UP, location, strlen(INSIDE_USB_LOCATION_UP)) != 0) && 
            (strncmp(INSIDE_USB_LOCATION_DOWN, location, strlen(INSIDE_USB_LOCATION_DOWN)) != 0)) {
            /* 不是內部的USB Port，跳過 */
            continue;
        }


        //printf("USB Storage Found\n");
        //printf("  Location : %s\n", location);
        //printf("  VID/PID  : %s / %s\n", vid.c_str(), pid.c_str());

        // ----------------------------
        // Disk devices
        // ----------------------------
        HDEVINFO diskInfo = SetupDiGetClassDevs(
            &GUID_DEVINTERFACE_DISK,
            NULL,
            NULL,
            DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);

        SP_DEVICE_INTERFACE_DATA ifData;
        ifData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

        for (DWORD j = 0; SetupDiEnumDeviceInterfaces(
            diskInfo, NULL,
            &GUID_DEVINTERFACE_DISK, j, &ifData); j++)
        {
            DWORD required = 0;

            SetupDiGetDeviceInterfaceDetail(
                diskInfo, &ifData,
                NULL, 0,
                &required,
                NULL);

            auto detail =
                (PSP_DEVICE_INTERFACE_DETAIL_DATA_A)malloc(required);

            detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_A);

            SP_DEVINFO_DATA diskDev;
            diskDev.cbSize = sizeof(SP_DEVINFO_DATA);

            if (!SetupDiGetDeviceInterfaceDetailA(
                diskInfo,
                &ifData,
                detail,
                required,
                NULL,
                &diskDev))
            {
                free(detail);
                continue;
            }

            GUID diskGuid;
            if (!GetContainerId(diskInfo, &diskDev, diskGuid)) {
                free(detail);
                continue;
            }

            // 🔥 Container ID match
            if (memcmp(&usbGuid, &diskGuid, sizeof(GUID)) != 0) {
                free(detail);
                continue;
            }

            // 取得 PhysicalDrive
            HANDLE hDisk = CreateFileA(
                detail->DevicePath,
                0,
                FILE_SHARE_READ | FILE_SHARE_WRITE,
                NULL,
                OPEN_EXISTING,
                0,
                NULL);

            if (hDisk == INVALID_HANDLE_VALUE) {
                free(detail);
                continue;
            }

            STORAGE_DEVICE_NUMBER sdn;
            DWORD bytes;

            if (!DeviceIoControl(
                hDisk,
                IOCTL_STORAGE_GET_DEVICE_NUMBER,
                NULL, 0,
                &sdn, sizeof(sdn),
                &bytes,
                NULL))
            {
                CloseHandle(hDisk);
                free(detail);
                continue;
            }

            CloseHandle(hDisk);

            int physicalDrive = sdn.DeviceNumber;

            //printf("  PhysicalDrive: %d\n", physicalDrive);

            // 找 Drive
            std::string drive =
                FindDriveLetterFromPhysicalDrive(physicalDrive);

            if (!drive.empty()) {
                //printf("  Drive    : %s\n", drive.c_str());

                if ((strncmp(INSIDE_USB_LOCATION_UP, location, strlen(INSIDE_USB_LOCATION_UP)) == 0)) {
                    /* 內部的USB Port1 */
                    strncpy(UsbInfo->PortUp_mountpath, drive.c_str(), sizeof(UsbInfo->PortUp_mountpath));
                    UsbInfo->PortUp_found = 1;
                }
                else if (strncmp(INSIDE_USB_LOCATION_DOWN, location, strlen(INSIDE_USB_LOCATION_DOWN)) == 0) {
                    /* 內部的USB Port2 */
                    strncpy(UsbInfo->PortDown_mountpath, drive.c_str(), sizeof(UsbInfo->PortDown_mountpath));
                    UsbInfo->PortDown_found = 1;
                }
            }
            else {
                //printf("  Drive    : (not found)");
            }

            free(detail);
        }

        SetupDiDestroyDeviceInfoList(diskInfo);

        printf("\n");
    }

    SetupDiDestroyDeviceInfoList(usbInfo);

    return 0;
}

void UsbInfo_Init(struct InsideUsbInfo* UsbInfo) {
    memset(UsbInfo, 0, sizeof(struct InsideUsbInfo));
	strncpy(UsbInfo->PortDown_location, INSIDE_USB_LOCATION_DOWN, strlen(INSIDE_USB_LOCATION_DOWN));
	strncpy(UsbInfo->PortUp_location, INSIDE_USB_LOCATION_UP, strlen(INSIDE_USB_LOCATION_UP));
}

void ListUsbInfo(struct InsideUsbInfo* UsbInfo) {

    printf("Usb Inside List : \n");

    if (UsbInfo->PortDown_found) {
        printf("UsbDisk found in down port\n");
        printf("mount path : %s\n", UsbInfo->PortDown_mountpath);
    }
    
    if (UsbInfo->PortUp_found) {
        printf("UsbDisk found in up port\n");
        printf("mount path : %s\n", UsbInfo->PortDown_mountpath);
    }

}