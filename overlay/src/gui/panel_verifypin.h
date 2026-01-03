#pragma once

#include "switch.h"
#include <tesla.hpp>

typedef enum {
    PanelSetupMenu = 1,
    PanelSetPin = 2
} NextPanel;

class VerifyPinPanel : public tsl::Gui {
public:
    VerifyPinPanel(const NextPanel& nextPanel);
    ~VerifyPinPanel();

    tsl::elm::Element* createUI() override;
    void rebuildUI();
    void update() override;
    void closeAndClean();

    // Called once every frame to handle inputs not handled by other UI elements
    virtual bool handleInput(u64 keysDown, u64 keysHeld, const HidTouchState &touchPos, HidAnalogStickState joyStickPosLeft, HidAnalogStickState joyStickPosRight) override;

private:
    void goToNextPanel() const;

private:    
    tsl::elm::OverlayFrame* rootFrame_ = nullptr;
    tsl::elm::List* rootList_ = nullptr;
    std::vector<u64> verifyPin_;
    bool error_ = false;
    NextPanel nextPanel_ = PanelSetupMenu;
};