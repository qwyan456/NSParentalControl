#include "panel_setpin.h"
#include "logger.h"
#include <tesla.hpp>
#include <switch.h>
#include <mutex>
#include "Command.hpp"
#include "AppContext.h"
#include "helpers.h"

using namespace alefbet::pctrl;

static std::mutex updateMutex_;

SetupPinPanel::SetupPinPanel() {}

SetupPinPanel::~SetupPinPanel() {  
    closeAndClean();
}

void SetupPinPanel::closeAndClean() {
}

tsl::elm::Element* SetupPinPanel::createUI() {
    rootFrame_ = new tsl::elm::OverlayFrame("Parental Control", "Setup PIN");
    rootList_ = new tsl::elm::List();
    
    rebuildUI();

    return rootFrame_;
}

void SetupPinPanel::rebuildUI() {    
    rootList_->addItem(new tsl::elm::CustomDrawer([this](tsl::gfx::Renderer* renderer, s32 x, s32 y, s32 w, s32 h) {
        renderer->drawString("Use all pad keys except B", false, 0, 0, 20, renderer->a(0xff0f));
    }));

    rootList_->addItem(new tsl::elm::CategoryHeader("New PIN"));    
    rootList_->addItem(new tsl::elm::CustomDrawer([this](tsl::gfx::Renderer* renderer, s32 x, s32 y, s32 w, s32 h) {
        u16 spacing = w/5;
        bool active = false;

        for(size_t i = 1 ; i < 5 ; i++) {
            active = currentPin_.size() >= i;
            renderer->drawCircle(x + i*spacing, y + 15, 15, active, tsl::Color(0xffff));
        }
    }), 30);

    rootList_->addItem(new tsl::elm::CategoryHeader("New PIN"));
    rootList_->addItem(new tsl::elm::CustomDrawer([this](tsl::gfx::Renderer* renderer, s32 x, s32 y, s32 w, s32 h) {
        u16 spacing = w/5;
        bool active = false;

        for(size_t i = 1 ; i < 5 ; i++) {
            active = newPin_.size() >= i;
            renderer->drawCircle(x + i*spacing, y + 15, 15, active, tsl::Color(0xffff));
        }
    }), 30);

    rootList_->addItem(new tsl::elm::CategoryHeader("Verify PIN"));
    rootList_->addItem(new tsl::elm::CustomDrawer([this](tsl::gfx::Renderer* renderer, s32 x, s32 y, s32 w, s32 h) {
        u16 spacing = w/5;
        bool active = false;

        for(size_t i = 1 ; i < 5 ; i++) {
            active = verifyPin_.size() >= i;
            renderer->drawCircle(x + i*spacing, y + 15, 15, active, tsl::Color(0xffff));
        }
    }), 30);

    if(currentPin_.size() == 4 && newPin_.size() == 4 && verifyPin_.size() == 4) {
        rootList_->addItem(new tsl::elm::CustomDrawer([this](tsl::gfx::Renderer* renderer, s32 x, s32 y, s32 w, s32 h) {
            bool verified = newPin_ == verifyPin_;
            renderer->drawString(verified ? "PINs match" : "PINs do not match", false, 0, 0, 20, verified ? renderer->a(0x161f) : renderer->a(0x611f));
        }), 20);
    }
    
    rootFrame_->setContent(rootList_);
}

void SetupPinPanel::update() {    
    std::lock_guard<std::mutex> lock(updateMutex_);
    if(updateOvl_) {        
        rebuildUI();
        updateOvl_ = false;
    } 
}

// Called once every frame to handle inputs not handled by other UI elements
bool SetupPinPanel::handleInput(u64 keysDown, u64 keysHeld, const HidTouchState &touchPos, HidAnalogStickState joyStickPosLeft, HidAnalogStickState joyStickPosRight) {
    // If user clicks B we get back
    if (keysDown & HidNpadButton_B) {
        tsl::goBack();
        return true;
    }

    // Otherwise he is programming a PIN
    {
        std::lock_guard<std::mutex> lock(updateMutex_);
        if(currentPin_.size() < (size_t)4) {
            currentPin_.push_back(keysDown);
            updateOvl_ = true;        
        } else if(newPin_.size() < (size_t)4) {
            newPin_.push_back(keysDown);
            updateOvl_ = true;
        } else if(verifyPin_.size() < (size_t)4) {
            verifyPin_.push_back(keysDown);
            updateOvl_ = true;
        }
    }

    return false;
}