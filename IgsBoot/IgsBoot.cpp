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
    MOUNT_FAILED

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

int GetProcessMode() {

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

int BootMode() {

    int rtn = 0;

    /* IMKeyEnLen是磁碟中介Key(密)，因為經過TPM加密，所以長度是256 Bytes 
       IMKeyDeLen是磁碟中介Key(明)，Array長度開出256bytes是因為解密時需要這麼大的空間，而實際上只有48Bytes是有效值 */
    BYTE IMKeyEn[256] = { 0x00 };
    BYTE IMKeyDe[256] = { 0x00 };


    DWORD IMKeyEnLen = sizeof(IMKeyEn);
    DWORD IMKeyDeLen = sizeof(IMKeyDe);

    rtn = ReadIMKeyEnFromEEProm(IMKeyEn, IMKeyEnLen);
    if (rtn != 0){
        ErrorMessage(-GET_IMKEY_FAILED);
        return -GET_IMKEY_FAILED;
    }

    print_hex(IMKeyEn, IMKeyEnLen);

    rtn = TPMUseKeyDec("IGSCardKey", IMKeyEn, IMKeyEnLen, IMKeyDe, &IMKeyDeLen);
    if(rtn != 0){
        ErrorMessage(-TPM_DEC_FAILED);
        return -TPM_DEC_FAILED;
    }

    //printf("IMKeyDeLen = %d\n", IMKeyDeLen);

    BYTE SerialNumber[256] = { 0x00 };
    DWORD SerialNumberLen = 0;

    rtn = GetLabelSerialNumber(SerialNumber, &SerialNumberLen);
    if(rtn != 0){
        ErrorMessage(-GET_UUID_FAILED);
        return -GET_UUID_FAILED;
	}

    BYTE SerialNumberSha256[32] = { 0x0 };
    SHA256(SerialNumber, SerialNumberLen, SerialNumberSha256);

    BYTE IV[16] = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                    0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f 
                  };

    BYTE PartitionKey[33] = { 0x00 };
    DWORD PartitionKeyLen = 0;

    /* 磁碟中介Key(明)使用AES解密後，會變成32Bytes的磁碟Key，是一個字串，所以實質上的array是33bytes */
    rtn = Aes256Decrypt(SerialNumberSha256, IV, IMKeyDe, IMKeyDeLen, PartitionKey, &PartitionKeyLen);
    if(rtn != 0){
        ErrorMessage(-DEC_PARTITIONKEY_FAILED);
        return -DEC_PARTITIONKEY_FAILED;
	}

    rtn = MountPartition(PartitionKey);
    if (rtn != 0) {
        ErrorMessage(-MOUNT_FAILED);
        return -MOUNT_FAILED;
    }

    return rtn;
}

int UpdateMode() {

    int rtn = 0;

    return rtn;
}


int main(int argc, char* argv[])
{
    int  ProcessMode;

    ProcessMode = GetProcessMode();

    if (ProcessMode == BOOT_MODE) {
        BootMode();
    }
    else if(ProcessMode == UPDATE_MODE){
        UpdateMode();
    }

    ShowProgress();

    return 0;
}

