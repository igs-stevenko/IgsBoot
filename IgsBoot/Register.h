#ifndef __REGISTER_H__
#define __REGISTER_H__

int CheckRegister(void);
int HKLM_WriteRegValueBin(const char* RegName, const char* ValName, const BYTE* Data, DWORD DataLen);

#endif
