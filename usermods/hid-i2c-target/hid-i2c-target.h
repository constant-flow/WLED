#pragma once

#include "wled.h"
#include "lib_controller_hid_to_i2c.h"

// Author: Constantin Wolf
// Receives HID signals via I2C: WLED can trigger events based on events of connected HID devices (keyboard, mouse, gamepad)
//
// Implemented Features:
// =====================
//   - Keyboard: Trigger Preset `X` on Number `X` [0-9]
//
// Future Features:
// ================
//   - Keyboard: Adjust brightness (+/-)
//   - Keyboard: Toggle LED on/off (ESC)
//   - Mouse: set color by x/y-position

class HidI2cTarget : public Usermod
{
private:
public:
    void setup()
    {
        hidInit(0x50);
        DEBUG_PRINTLN("Init Usermod: HID-I2C-Target");
    }

    void loop()
    {
        hidScan();
        // hidPrintEvents();
        // hidDebugTest();

        uint8_t *pressed = hidGetPressedKeys();

        for (int i = 0; i < EVENT_LIST_LENGTH; i++)
        {
            if (pressed[i] != 0 && pressed[i] != 1)
            {
                char keyAscii = keycode2ascii[pressed[i]][hidIsShiftPressed()];
                DEBUG_PRINT("key event: ");
                DEBUG_PRINTLN(keyAscii);

                if (keyAscii >= '0' && keyAscii <= '9')
                {
                    // number pressed
                    applyPreset(keyAscii - '1');
                    DEBUG_PRINTLN(keyAscii);
                }
            }
        }
    }
};