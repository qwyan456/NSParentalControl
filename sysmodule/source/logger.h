#pragma once
#include <string>
#include <cstdarg>

namespace alefbet {
    namespace pctrl { 
        namespace logger {
            typedef enum {
                DEBUG = 0,                
                INFO = 5,
                ERROR = 10
            } LogLevel;

            bool openFile();
            void clearLog();            
            void debugHipcMetaHeader(void* hdr);
            //void debugHipcDataHeader(void* hdr);
            void closeFile();            
            void logDebug(const char *fmt, ...);
            void logInfo(const char *fmt, ...);
            void logError(const char *fmt, ...);
            
            void setLogLevel(LogLevel level);
            LogLevel currentLogLevel();            

            namespace priv {
                void logToFile(const char *fmt, va_list);
            }
        }
    }
}

