#pragma once
#include <string>

namespace alefbet {
    namespace pctrl { 
        namespace logger {
            bool openFile();
            void clearLog();
            void logToFile(const char *fmt, ...);
            void closeFile();
        }
    }
}