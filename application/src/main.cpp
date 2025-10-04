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
#include <switch.h>
#include <stdio.h>
#include <string>
#include "logger.h"

void testPctl() {
    bool isPctlEnabled = false;
    Result rc = pctlIsRestrictionEnabled(&isPctlEnabled);
    if(R_FAILED(rc)) {
        printf("Error #1\n");            
    }
    
    if(isPctlEnabled) {
        pctlauthChangePasscode();
    }
}

void main(int argc, char** argv) {
    consoleInit(NULL);

    printf("Démarrage\n");
    consoleUpdate(NULL);

    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    PadState pad;
    padInitializeDefault(&pad);
    padUpdate(&pad);

    bool pingSent(false);

    pctlInitialize();

    while(appletMainLoop()) {
        u64 kDown = padGetButtonsDown(&pad);        
        u64 kHeld = padGetButtons(&pad);

        // Quit with B
        if (kDown & HidNpadButton_A) testPctl();
        if (kDown & HidNpadButton_B) break;        

        bool is_pctl_enabled = false;
        Result rc = pctlIsRestrictionEnabled(&is_pctl_enabled);
        if(R_FAILED(rc)) {
            printf("Error #1: %i:%i", R_MODULE(rc), R_DESCRIPTION(rc));
        } else {
            printf("Restriction enabled: %s\n", is_pctl_enabled ? "true" : "false");
        }

        u32 safety_level = 0;
        rc = pctlGetSafetyLevel(&safety_level);
        if(R_FAILED(rc)) {
            printf("Error #2: %i:%i", R_MODULE(rc), R_DESCRIPTION(rc));
        } else {
            printf("Safety level: %i\n", safety_level);
        }

        PctlRestrictionSettings settings;
        rc = pctlGetCurrentSettings(&settings);
        if(R_FAILED(rc)) {
            printf("Error #3: %i:%i", R_MODULE(rc), R_DESCRIPTION(rc));
        } else {            
            printf("rating_age=%i\n", settings.rating_age);
            printf("sns_post_restriction=%i\n", settings.sns_post_restriction);
            printf("free_communication_restriction=%i\n", settings.free_communication_restriction);
            printf("\n");
        }

        bool is_pairing_active = false;
        rc = pctlIsPairingActive(&is_pairing_active);
        if(R_FAILED(rc)) {
            printf("Error #4: %i:%i", R_MODULE(rc), R_DESCRIPTION(rc));
        } else {
            printf("Pairing active: %s\n", is_pairing_active ? "true" : "false");
        }

        bool is_play_timer_alarm_disabled = false;
        rc = pctlIsPlayTimerAlarmDisabled(&is_play_timer_alarm_disabled);
        if(R_FAILED(rc)) {
            printf("Error #5: %i:%i", R_MODULE(rc), R_DESCRIPTION(rc));
        } else {
            printf("Play timer alarm disabled: %s\n", is_play_timer_alarm_disabled ? "true" : "false");
        }

        //pctlauthRegisterPasscode();

        

        consoleUpdate(NULL);
        svcSleepThread(100'000);
    }

    pctlExit();

    printf("Fin\n");

    consoleExit(NULL);
}