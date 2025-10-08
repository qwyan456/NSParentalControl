#pragma once
#include <string>
#include <vector>
#include <switch.h>

namespace alefbet::pctrl::ipc {

    /*typedef enum {
        IpcInteger = 1,
        IpcDouble = 2,
        IpcString = 3        
    } IpcDataType;

    typedef struct {
        IpcDataType type;
        std::string str;
        u64 value;

        bool isValid() {
            return type > 0;
        }
    } IpcDataElement;

    using IpcData = std::vector<IpcDataElement>;

    IpcData ipcBufferToData(u8* buffer, size_t length);*/

    using UserUid = std::string;
    using UserNickname = std::string;
    constexpr int UserUidMaxLength = 39;

    void startTest();
    UserUid getCurrentUserUid();
    UserNickname getCurrentUserNickname();
    std::string getCurrentTitle();
    u16 getUserUsageTime();
    u16 getUserRemainingTime();

    std::string encodeAdminPin(const std::vector<u64>&);
    std::vector<u64> decodeAdminPin(const std::string&);
    bool verifyPin(const std::string& pin);

}