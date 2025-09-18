#include "logger.h"
#include <switch.h>
#include <stdio.h>
#include <string.h>

const char* LogFilename = "sdmc:/atmosphere/logs/nsparentalcontrol.log";

void clearLog() {
    FILE* f = fopen(LogFilename, "w");
    fclose(f);
}

void logIntToFile(int i) {
    FILE* f = fopen(LogFilename, "a");
    fprintf(f, "%i\n", i);
    fclose(f);
}

void logPointerToFile(void* i) {
    FILE* f = fopen(LogFilename, "a");    
    fprintf(f, "%p\n", &i);
    fprintf(f, "%p\n", i);
    fclose(f);
}

void logToFile(const char* msg) {
    FILE* f = fopen(LogFilename, "a");
    fprintf(f, "%s\n", msg);
    fclose(f);
}