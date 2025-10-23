#pragma once
#include <switch.h>

class GuiController {
    public:
        void showScreenTimeout();
        void hideScreenTimeout();
        void showRemainingTimePanel();
        void updateRemainingTimePanel(const u16& remaining_time_in_minutes, const u16& limit_in_minutes);
        void hideRemainingTimePanel();
        
        inline bool isRemainingTimePanelVisible() {
            return remaining_time_visible_;
        }

    private:
        void showOverlay(u16 width, u16 height, u16 posX, u16 posY);
        void clearScreen();
        //static Result hidsysEnableAppletToGetInput(bool enable, u64 aruid);
        //static void requestForeground(bool enabled);    
        
    private:
        u16 width_ = 0;
        u16 height_ = 0;
        bool remaining_time_visible_ = false;
};
