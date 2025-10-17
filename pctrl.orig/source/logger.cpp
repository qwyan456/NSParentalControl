#include "logger.h"
#include <stratosphere.hpp>
#include <mutex>

const char* LogFilename = "sdmc:/atmosphere/logs/pctrl_svc.log";

namespace alefbet {
    namespace pctrl {        

        using namespace ams;
        using namespace ams::fs;

        static std::mutex s_mutex;        
        static constinit char g_format_buffer[2 * ams::os::MemoryPageSize];
        static FileHandle handle;
        static size_t offset = 0;
        static bool opened = false;    

        namespace logger {                           

            bool openFile() {
                if(opened) return true;

                // Try to open the file
                opened = R_SUCCEEDED(OpenFile(std::addressof(handle), LogFilename, OpenMode_Write | OpenMode_AllowAppend));                

                // If the file could not be opened try to create a new one
                if(!opened && R_SUCCEEDED(CreateFile(LogFilename, 0))) {
                    // Then open it
                    opened = R_SUCCEEDED(OpenFile(std::addressof(handle), LogFilename, OpenMode_Write | OpenMode_AllowAppend));
                }                 
                
                return opened;
            }
            
            void clearLog() {
                DeleteFile(LogFilename);
            }          

            void logToFile(const char* fmt, ...) {
                std::lock_guard<std::mutex> lock(s_mutex);

                if(openFile()) {
                    std::va_list vl;
                    va_start(vl, fmt);
                    ams::util::VSNPrintf(g_format_buffer, sizeof(g_format_buffer), fmt, vl);
                    va_end(vl);    
                    
                    size_t size = std::strlen(g_format_buffer);
                    if(R_SUCCEEDED(WriteFile(handle, offset, g_format_buffer, size, WriteOption::Flush))) {
                        offset += size;                        
                    }

                    closeFile();
                }                        
                
            }

            void closeFile() {
                if(opened) {
                    CloseFile(handle);
                    opened = false;
                }
            }

        }
    }

}