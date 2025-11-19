#pragma once

namespace alefbet::pctrl::srv {

    //using AmsResult = ::ams::Result;       

    #define TRY_AND_RETURN(res_expr, message)                                 \
    {                                                                         \
        if (const auto _tmp_r_try_rc = (res_expr); _tmp_r_try_rc != 0) {      \
            alefbet::pctrl::logger::logToFile(message);                       \
            alefbet::pctrl::logger::logToFile("Error %i\n", _tmp_r_try_rc);   \
            return 1;                                                         \
        }                                                                     \
    }                                                                         \
    
    class GuiController {            
        private:
            void     InitializeFrameBufferPointer();
            ::Result SetupDisplayInternal();
            ::Result SetupDisplayExternal();
            ::Result PrepareScreenForDrawing();
            void     PreRenderFrameBuffer();
            ::Result PreRenderContents();
            ::Result InitializeNativeWindow();
            void     DisplayPreRenderedFrame();   
            bool     WaitForVolumeButton();       

        public:            
            ::Result ShowScreenTimeout();
            //::Result ShowRemainingTime();
            //::Result ShowScreenWarning();
            //::Result UpdateRemainingTime(u8 remaining_time_in_minutes);
            void HideScreen();            

        private:
            ViDisplay m_display;
            ViLayer m_layer;
            NWindow m_win;
            NvMap m_map;    
            //u8* heap_pointer_ = nullptr;
            u8 m_remaining_time_in_minutes = 0;
    };
    
}