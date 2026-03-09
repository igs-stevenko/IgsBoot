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

#pragma warning(disable:4996)

int WriteToFile(const char* filename, unsigned char* buf, int len) {

	int rtn = 0;

	FILE* fout = fopen(filename, "wb");
	if (!fout) {
		printf("fopen %s error\n", filename);
		return -1;
	}
	rtn = fwrite(buf, 1, len, fout);
	if (rtn != len) {
		printf("Write file error\n");
		rtn = -2;
	}
	else {
		printf("Write file OK, rtn = %d\n", rtn);
		rtn = 0;
	}

	if (fout) {
		rtn = fclose(fout);
		if (rtn != 0) {
			return -3;
		}
	}

	return rtn;
}

int ReadFromFile(const char* filename, unsigned char* buf, int len) {

	int rtn = 0;
	FILE* fin = fopen(filename, "rb");
	if (!fin) return -1;

	rtn = fread(buf, 1, len, fin);
	if (rtn != len) {
		rtn = -1;
	}
	else {
		rtn = 0;
	}

	return rtn;
}