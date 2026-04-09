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
#include "hasp_api.h"
#include "qxteeprom.h"
#include "libqsys.h"



int ReadIMKeyEnFromEEProm(BYTE* DataBuf, int DataLen) {

	int rtn = 0;

	qxt_eeprom_handle_t my_handle = EEPROM_HANDLE_INIT;
	qxt_eeprom_desc_t selected_desc = EEPROM_DESC_NULL;
	qxt_memory_handle_t my_mem = MEMORY_HANDLE_INIT;
	char found = 0;
	uint32_t addr, channel;

	// Passing the initialized handle to Open function in order to get a
	// valid handle if no error occurs
	if (qxtEepromOpen(&my_handle))
	{
		puts("Error opening protected eeprom library");
		return -1;
	}

	addr = 0x56;
	channel = 0;

	while (qxtEepromScan(my_handle, &selected_desc) == 0)
	{
		if (selected_desc.device_addr == addr && selected_desc.device_channel == channel)
		{
			found = 1;
			break;
		}
	}

	if (!found)
	{
		puts("Device not found");
		return -2;
	}

	// Get exclusive access to device
	rtn = qxtEepromSelect(my_handle, &selected_desc, &my_mem);
	if (rtn != Q_SUCCESS)
	{
		printf("Error on selecting device, rtn = 0x%08x\n", rtn);
		return -3;
	}

	uint8_t read_value = 0;
	int i = 0;
	for (i = 0; i < DataLen; i++) {
		if (qxtEepromRead(my_mem, i, &read_value, 1))
		{
			printf("Error while reading the device memory at address %#02x\n", i);
			break;
		}

		DataBuf[i] = read_value;
	}

	if (i != DataLen) {
		qxtEepromRelease(my_mem);
		qxtEepromClose(my_handle);
		return -4;
	}

	qxtEepromRelease(my_mem);
	qxtEepromClose(my_handle);

	return rtn;
}

int GetLabelSerialNumber(BYTE* SerialNumber, DWORD* Len) {

	int rtn = 0;

	QRESULT ret_code;
	HANDLE h;
	HANDLE hquery;
	char reg_exp[256] = {
	".*/Silver Label"
	//"/QSYS/HW/Core/Logging Processor/Silver Label"
	};

	ret_code = qsysOpen(&h);
	if (ret_code != 0) {
		return -1;
	}

	ret_code = qsysMakeQuery(h, reg_exp, &hquery);
	if (ret_code != 0) {
		qsysClose(h);
		return -2;
	}

	char key[256] = { 0 };
	char value[256] = { 0 };

	ret_code = qsysFirstEntry(hquery, key, 256, value, 256);
	if (ret_code != 0) {
		qsysClose(h);
		return -3;
	}

	memcpy(SerialNumber, value, strlen(value));
	*Len = strlen(value);

	qsysClose(h);

	return rtn;
}
