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

        typedef struct HipcMetadata {
            u32 type;
            u32 num_send_statics;
            u32 num_send_buffers;
            u32 num_recv_buffers;
            u32 num_exch_buffers;
            u32 num_data_words;
            u32 num_recv_statics; // also accepts HIPC_AUTO_RECV_STATIC
            u32 send_pid;
            u32 num_copy_handles;
            u32 num_move_handles;
        } HipcMetadata;

        namespace logger {                           

            bool openFile() {
                if(opened) return true;

                // Try to open the file
                opened = R_SUCCEEDED(OpenFile(std::addressof(handle), LogFilename, OpenMode_Write | OpenMode_AllowAppend));                

                // If the file could not be opened 
                if(!opened) {
                    // try to create a new one
                    if(R_SUCCEEDED(CreateFile(LogFilename, 0))) {
                        // Then open it
                        opened = R_SUCCEEDED(OpenFile(std::addressof(handle), LogFilename, OpenMode_Write | OpenMode_AllowAppend));
                    }
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

            /*void debugHipcParsedRequest(void* hdr) {
                HipcHeader* h = (HipcHeader*)hdr;

            }
                
                logToFile("HipcHeader contents:\n");
                logToFile("  type               : %u\n", h->type);
                logToFile("  num_send_statics   : %u\n", h->num_send_statics);
                logToFile("  num_send_buffers   : %u\n", h->num_send_buffers);
                logToFile("  num_recv_buffers   : %u\n", h->num_recv_buffers);
                logToFile("  num_exch_buffers   : %u\n", h->num_exch_buffers);
                logToFile("  num_data_words     : %u\n", h->num_data_words);
                logToFile("  recv_static_mode   : %u\n", h->recv_static_mode);
                logToFile("  padding            : %u\n", h->padding);
                logToFile("  recv_list_offset   : %u\n", h->recv_list_offset);
                logToFile("  has_special_header : %u\n", h->has_special_header);
            }*/

            void debugHipcMetaHeader(void* hdr) {
                HipcMetadata* h = (HipcMetadata*)hdr;

                logToFile("HipcHeader contents:\n");
                logToFile("  type               : %u\n", h->type);
                logToFile("  num_send_statics   : %u\n", h->num_send_statics);
                logToFile("  num_send_buffers   : %u\n", h->num_send_buffers);
                logToFile("  num_recv_buffers   : %u\n", h->num_recv_buffers);
                logToFile("  num_exch_buffers   : %u\n", h->num_exch_buffers);
                logToFile("  num_data_words     : %u\n", h->num_data_words);
                logToFile("  num_recv_statics   : %u\n", h->num_recv_statics);
                logToFile("  send_pid           : %u\n", h->send_pid);
                logToFile("  num_copy_handles   : %u\n", h->num_copy_handles);
                logToFile("  num_move_handles   : %u\n", h->num_move_handles);
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