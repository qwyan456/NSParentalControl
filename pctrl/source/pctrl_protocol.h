#pragma once

typedef enum {
    CmdUndefined = 0,    
    CmdPing,
    CmdPong,
    CmdGetRemainingTime,
    CmdTestTimeout = 99
} PctrlCommands;