#pragma once
#include <switch.h>

/** Testla global variables */
u32 __nx_applet_type = AppletType_None;
u32 __nx_fs_num_sessions = 1;
u32  __nx_nv_transfermem_size = 0x40000;
ViLayerFlags __nx_vi_stray_layer_flags = (ViLayerFlags)0;

namespace tsl::cfg {
    u16 LayerWidth  = 0;
    u16 LayerHeight = 0;
    u16 LayerPosX   = 0;
    u16 LayerPosY   = 0;
    u16 FramebufferWidth  = 0;
    u16 FramebufferHeight = 0;
    u64 launchCombo = HidNpadButton_L | HidNpadButton_Down | HidNpadButton_StickR;
}