#pragma once

#define INSIDE_USB_LOCATION_DOWN "PCIROOT(0)#PCI(1400)#USBROOT(0)#USB(1)#USB(1)"
#define INSIDE_USB_LOCATION_UP "PCIROOT(0)#PCI(1400)#USBROOT(0)#USB(1)#USB(2)"

struct InsideUsbInfo {

    int PortDown_found;
    char PortDown_location[512];
    char PortDown_mountpath[512];

    int PortUp_found;
    char PortUp_location[512];
    char PortUp_mountpath[512];
};

void EnumerateUsbStorage();
int ScanInsideUsbDisk(struct InsideUsbInfo* UsbInfo);
void UsbInfo_Init(struct InsideUsbInfo* UsbInfo);
void ListUsbInfo(struct InsideUsbInfo* UsbInfo);