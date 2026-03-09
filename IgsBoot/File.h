#pragma once

int WriteToFile(const char* filename, unsigned char* buf, int len);
int ReadFromFile(const char* filename, unsigned char* buf, int len);