#pragma once
#include <switch.h>

namespace alefbet::pctrl::gui {

    #define TRY_AND_RETURN(res_expr, message)                                 \
    {                                                                         \
        if (const auto _tmp_r_try_rc = (res_expr); _tmp_r_try_rc != 0) {      \
            alefbet::pctrl::logger::logToFile(message);                       \
            alefbet::pctrl::logger::logToFile("Error %i\n", _tmp_r_try_rc);   \
            return 1;                                                         \
        }                                                                     \
    }                                                                         \
    
    class ScreenTimeout {            
        private:
            void     InitializeFrameBufferPointer();
            ::Result SetupDisplayInternal();
            ::Result SetupDisplayExternal();
            ::Result PrepareScreenForDrawing();
            void     PreRenderFrameBuffer();
            ::Result PreRenderContents();
            ::Result InitializeNativeWindow();
            void     DisplayPreRenderedFrame();   
            //bool     WaitForVolumeButton();       

        public:            
            void setTransferMemory(u8*, u64 size);
            ::Result ShowScreenTimeout();

        private:
            ViDisplay m_display;
            ViLayer m_layer;
            NWindow m_win;
            NvMap m_map;
            u8* g_nv_transfer_memory = nullptr;
            u64 nv_transfer_memory_size = 0;
    };
    
}