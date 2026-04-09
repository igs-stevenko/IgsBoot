#pragma once


struct InsideUsbInfo {

    int PortDown_found;
    char PortDown_location[512];
    char PortDown_mountpath[512];

    int PortUp_found;
    char PortUp_location[512];
    char PortUp_mountpath[512];
};

int ScanInsideUsbDisk(struct InsideUsbInfo* UsbInfo);
void UsbInfo_Init(struct InsideUsbInfo* UsbInfo);
void ListUsbInfo(struct InsideUsbInfo* UsbInfo);