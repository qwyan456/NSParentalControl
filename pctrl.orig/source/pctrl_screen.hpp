/*
 * Copyright (c) Atmosphère-NX
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once
#include <stratosphere.hpp>

namespace alefbet::pctrl::srv {

    //using AmsResult = ::ams::Result;       

    #define TRY_AND_RETURN(res_expr, message)                                 \
    {                                                                         \
        if (const auto _tmp_r_try_rc = (res_expr); _tmp_r_try_rc != 0) {      \
            alefbet::pctrl::logger::logToFile(message);                       \
            return 1;                                                         \
        }                                                                     \
    }                                                                         \
    

    class GuiController {            
        private:
            void InitializeFrameBufferPointer();
            ::Result SetupDisplayInternal();
            ::Result SetupDisplayExternal();
            ::Result PrepareScreenForDrawing();
            void   PreRenderFrameBuffer();
            ::Result PreRenderContents();
            ::Result InitializeNativeWindow();
            void   DisplayPreRenderedFrame();           

        public:            
            ::Result ShowScreenTimeout();
            ::Result ShowRemainingTime();
            ::Result ShowScreenWarning();
            ::Result UpdateRemainingTime(u8 remaining_time_in_minutes);
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