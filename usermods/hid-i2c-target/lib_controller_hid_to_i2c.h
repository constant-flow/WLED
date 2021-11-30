// This library converts hid signals sent via I2C as HID events
// on the i2c controller side.
// The protocol is not related to https://docs.microsoft.com/en-us/windows-hardware/drivers/hid/hid-over-i2c-guide

#if !defined(HEADER_HID_I2C_HID)
#define HEADER_HID_I2C_HID

#define REQ_HID_GET_STATUS 0x07
#define HID_CHANNELS 6
#define EVENT_LIST_LENGTH 6

#include "hid.h"
#include <Wire.h>

const int I2C_ADD = 0x50;

static struct
{
    int i2c_addr;
    uint8_t pressedSlots[EVENT_LIST_LENGTH];
    uint8_t releasedSlots[EVENT_LIST_LENGTH];
    uint8_t pressedModifier;
    uint8_t releasedModifier;
} context;

struct hid_map_event
{
    uint8_t modifier;
    uint8_t slots[HID_CHANNELS];
} hid_current, hid_last;

uint8_t *hidGetPressedKeys()
{
    return context.pressedSlots;
}

uint8_t *hidGetReleasedKeys()
{
    return context.releasedSlots;
}

uint8_t hidGetReleasedModifierKey()
{
    return context.releasedModifier;
}

uint8_t hidGetPressedModifierKey()
{
    return context.pressedModifier;
}

bool hidIsShiftPressed()
{
    return (hid_current.modifier & KEYBOARD_MODIFIER_LEFTSHIFT) | (hid_current.modifier & KEYBOARD_MODIFIER_RIGHTSHIFT);
}

void hidInit(uint8_t i2c_addr)
{
    context.i2c_addr = i2c_addr;
    Wire.begin(13,16);
    // Wire.begin();
    // Wire.setClock(100000);
}

void getElementsOfANotInB(hid_map_event a, hid_map_event b, uint8_t *slots)
{
    uint8_t notFoundId = 0;

    for (int ia = 0; ia < HID_CHANNELS; ia++)
    {
        bool notFoundInB = true;
        uint8_t toFind = a.slots[ia];
        for (int ib = 0; ib < HID_CHANNELS; ib++)
        {
            if (toFind == b.slots[ib])
            {
                notFoundInB = false;
                break;
            }
        }

        if (notFoundInB)
        {
            slots[notFoundId++] = toFind;
        }
    }

    //clear all other not used
    for (int i = notFoundId; i < EVENT_LIST_LENGTH; i++)
    {
        slots[i] = 0;
    }
}

bool hidInvalidStatus(hid_map_event hid)
{
    for (int i = 0; i < HID_CHANNELS; i++)
    {
        if (hid.slots[i] == 1)
            return true;
    }
    return false;
}

void hidCheckForChanges()
{
    if (hidInvalidStatus(hid_last) || hidInvalidStatus(hid_current))
        return;

    getElementsOfANotInB(hid_last, hid_current, context.releasedSlots);
    getElementsOfANotInB(hid_current, hid_last, context.pressedSlots);

    uint8_t modifier_changes = hid_current.modifier ^ hid_last.modifier;

    context.pressedModifier = hid_current.modifier & modifier_changes;
    context.releasedModifier = hid_last.modifier & modifier_changes;
}

void hidScan()
{
    Wire.beginTransmission(context.i2c_addr);
    Wire.write(REQ_HID_GET_STATUS);
    Wire.endTransmission(false);
    Wire.requestFrom(context.i2c_addr, 7);

    if (hid_current.slots[0] != 0x01)
    { //0x01 = invalid, e.g. if there are too many pressed
        hid_last = hid_current;
    }

    int i = 0;
    while (Wire.available()) // slave may send less than requested
    {
        uint8_t value = Wire.read(); // receive a byte as character
        if (i == 0)
            hid_current.modifier = value;
        else if (i < 7)
            hid_current.slots[i - 1] = value;
        else
            Serial.println("Error: no hid slot that high existing");
        i++;
    }

    hidCheckForChanges();
}

void hidPrintTypedKeys()
{
    uint8_t *pressed = hidGetPressedKeys();

    for (int i = 0; i < EVENT_LIST_LENGTH; i++)
    {
        if (pressed[i] != 0 && pressed[i] != 1)
        {
            char keyAscii = keycode2ascii[pressed[i]][hidIsShiftPressed()];
            Serial.print(keyAscii);
            Serial.flush();
        }
    }
}

void hidPrintEvents()
{
    uint8_t *pressed = hidGetPressedKeys();
    uint8_t *released = hidGetReleasedKeys();

    for (int i = 0; i < EVENT_LIST_LENGTH; i++)
    {
        if (pressed[i] != 0 && pressed[i] != 1)
        {
            char keyAscii = keycode2ascii[pressed[i]][hidIsShiftPressed()];
            Serial.print("⬇   Pressed: ");
            Serial.println(keyAscii);
        }
    }

    for (int i = 0; i < EVENT_LIST_LENGTH; i++)
    {
        if (released[i] != 0 && released[i] != 1)
        {
            char keyAscii = keycode2ascii[released[i]][hidIsShiftPressed()];
            Serial.print(" ⬆ Released: ");
            Serial.println(keyAscii);
        }
    }
}

void hidDebugRawStatus()
{
    Wire.beginTransmission(context.i2c_addr);
    Wire.write(REQ_HID_GET_STATUS);
    Wire.endTransmission(false);
    Wire.requestFrom(context.i2c_addr, 7);

    Serial.print("HID (REQ_HID_GET_STATUS): ");
    while (Wire.available()) // slave may send less than requested
    {
        char c = Wire.read(); // receive a byte as character
        Serial.print((int)c); // print the character
        Serial.print(" ");
    }

    Serial.println("");
}

// Request the I2C device to identify
void hidDebugTest()
{
    Wire.beginTransmission(context.i2c_addr);
    Wire.write(0x00);
    Wire.endTransmission();
    Wire.requestFrom(context.i2c_addr, 20);

    Serial.print("Test (0x00): ");
    for (int i = 0; i < 20; i++)
    {
        char read = Wire.read();
        Serial.print(read);
    }

    Serial.println("");
}

void hidDebugUnknownCommand()
{
    Wire.beginTransmission(context.i2c_addr);
    Wire.write(12);
    Wire.endTransmission();
    Wire.requestFrom(context.i2c_addr, 2);

    uint8_t read = Wire.read();
    Serial.print("response on unknown request: ");
    Serial.println(read);
}

#endif // HEADER_HID_I2C_HID