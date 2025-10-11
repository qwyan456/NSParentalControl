#include "logger.h"
#include <stratosphere.hpp>

const char* LogFilename = "sdmc:/atmosphere/logs/nsparentalcontrol.log";

namespace alefbet {
    namespace pctrl {        

        using namespace ams::fs;

        namespace {
            int offset = 0;

            FileHandle openFile() {
                FileHandle handle{nullptr};

                if(R_SUCCEEDED(fs::CreateFile(LogFilename, 0))) {                    
                    if(R_SUCCEEDED(OpenFile(std::addressof(handle), LogFilename, OpenMode_All))) {
                        return handle;
                    }
                }                
            }
        }

        namespace logger {            

            void clearLog() {
                DeleteFile(LogFilename);
            }

            void logIntToFile(int i) {
                auto handle = openFile();
                if(handle.handle != nullptr) {
                    WriteFile(handle, )
                }
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
        }
    }

}