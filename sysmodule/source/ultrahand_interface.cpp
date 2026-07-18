#include "ultrahand_interface.h"
#include <ctime>
#include <cstring>
#include <format>
#include <switch.h>
#include <json.hpp>
#include "logger.h"

using namespace alefbet::pctrl::logger;

const char* FLAGPATH = "/config/ultrahand/flags/NOTIFICATIONS.flag";
static FsFileSystem sdmc;

void UltraHandInterface::writeNotification(const std::string& message, int fontSize, int priority) 
{    
    if(!R_SUCCEEDED(fsOpenSdCardFileSystem(&sdmc))) {
        logError("[Ultrahand] Could not open SD Card\n");
        return;
    }

    // Check if flag file exists
    FsFile handle;
    FsDirEntryType fileType;    

    if (R_FAILED(fsFsGetEntryType(&sdmc, FLAGPATH, &fileType))) {
        // Flag file does not exist, do nothing
        logInfo("[Ultrahand] Notifications are not enabled in Ultrahand\n");
        return;
    }

    // 清理上一条通知文件：UltraHand 显示后不会自动删除 .notify，
    // 长期运行会累积成百上千个文件，拖慢其 300ms 轮询目录的速度（间接导致提示偶发不显示）。
    // 每条通知只保留最新一个即可（已显示的旧文件不会再被 UltraHand 重显）。
    static std::string lastFilename;

    // Generate filename with timestamp
    u64 tick = armGetSystemTick();
    std::string filename = "NSParentalControl-" + std::to_string(tick) + ".notify";
    std::string fullPath = "/config/ultrahand/notifications/" + filename;

    if(R_FAILED(fsFsCreateFile(&sdmc, fullPath.c_str(), 0, 0))) {
        logError("[Ultrahand] Could not create notification file\n");
        return;
    }

    // 新文件已创建成功，再删除上一条（避免创建失败时把旧通知也弄丢）
    if(!lastFilename.empty()) {
        std::string prev = "/config/ultrahand/notifications/" + lastFilename;
        fsFsDeleteFile(&sdmc, prev.c_str()); // best-effort
    }
    lastFilename = filename; // 记录以便下次清理

    if(R_SUCCEEDED(fsFsOpenFile(&sdmc, fullPath.c_str(), FsOpenMode_Write | FsOpenMode_Append, &handle))) {        
        nlohmann::json j;
        j["text"] = message;
        j["font_size"] = fontSize;
        j["priority"] = priority;
        
        const auto notif = j.dump(2);
        const auto s_data = notif.c_str();

        if(R_SUCCEEDED(fsFileWrite(&handle, 0, s_data, std::strlen(s_data), FsWriteOption_Flush))) {
            logDebug("[Ultrahand] Notification file has been written\n");
        } else {
            logError("[Ultrahand] Notification file has not been written\n");
        }

        fsFileClose(&handle);
    }

    fsFsClose(&sdmc);
}

/*! \brief Shows a short notification to draw users attention */
void UltraHandInterface::drawAttention() 
{    
    writeNotification("Parental Control", 32, 2);
}