#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void clearLog();
void logIntToFile(int i);
void logPointerToFile(void* i);
void logToFile(const char* msg);

#ifdef __cplusplus
}
#endif