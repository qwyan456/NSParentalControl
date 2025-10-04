#include "logger.h"
#include <switch.h>
#include <stdio.h>
#include <string.h>

const char* logfilename = "sdmc:/switch/pctrl_ovl.log";
void setLogFilename(const char* filename) {
    logfilename = filename;
}

void clearLog() {
    FILE* f = fopen(logfilename, "w");
    fclose(f);
}

void logIntToFile(int i) {
    FILE* f = fopen(logfilename, "a");
    fprintf(f, "%i\n", i);
    fclose(f);
}

void logPointerToFile(void* i) {
    FILE* f = fopen(logfilename, "a");    
    fprintf(f, "%p\n", &i);
    fprintf(f, "%p\n", i);
    fclose(f);
}

void logToFile(const char* msg) {
    FILE* f = fopen(logfilename, "a");
    fprintf(f, "%s\n", msg);
    fclose(f);
}