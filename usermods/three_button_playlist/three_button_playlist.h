#pragma once

#include "wled.h"

/*
 * This usermod allows to use three physical momentary buttons to skip forward and backward through a list of presets.
 * A Play buttons allows to retrigger the currently selected playlist item. 
 * The play button has only an effect for non-looping presets see: https://github.com/constant-flow/WLED/tree/tpm2-recording-playback
 * A longpress on the play button will turn off the LEDs, any other press will turn them on again.
 * 
 * Prev button ID : Button id to switch to the previous preset (define buttons via `config -> LED pref-> Button X GPIO`)
 * Play button ID : Button id to switch to the next preset
 * Next button ID : Button id to apply the preset again
 * Playlist items : The length of the playlist (first index is 1)
 * Playlist start index : preset to start on boot, first index is 1, must be positive
 */

class ThreeButtonPlaylist : public Usermod
{
private:
    int16_t selected_index; // currently selected item
    uint8_t button_id_prev; 
    uint8_t button_id_play;
    uint8_t button_id_next;
    uint8_t playlist_length;

    uint64_t playLastPressed;
    bool alreadyToggled;

    bool handlePlayLongPress(uint8_t button) {
        if(button != button_id_play) return false;
        if(!buttonPressedBefore[button]) return false;

        if (millis() - playLastPressed > 1000 && !alreadyToggled){
            alreadyToggled = true;
            
            toggleOnOff();
            colorUpdated(CALL_MODE_BUTTON);
            Serial.println("Three button Playlist: Toggle off");
            return true;
        }
        return false;
    }

public:
    void setup()
    {
        Serial.println("Three button playlist: Prev, Play/Pause, Next");
        alreadyToggled = false;
    }

    bool handleButton(uint8_t button)
    {
        bool handledLongPress = handlePlayLongPress(button);
        if(handledLongPress) return true;

        if (!isButtonPressed(button))
        {
            buttonPressedBefore[button] = false;
            return true;
        }

        if (buttonPressedBefore[button])
        {
            return true;
        }

        buttonPressedBefore[button] = true;

        if (button == button_id_prev)
        {
            selected_index--;
        };
        if (button == button_id_play)
        {
            playLastPressed = millis();
            alreadyToggled = false;
        };
        if (button == button_id_next)
        {
            selected_index++;
        };

        if (selected_index <= 0) selected_index += playlist_length;
        if (selected_index >= playlist_length) selected_index -= playlist_length;

        Serial.print("Three Button Playlist: Selected preset nr: ");
        Serial.println(selected_index + 1); //index in UI is 1 higher than internal index
        applyPreset(selected_index + 1);
        colorUpdated(CALL_MODE_BUTTON);
        return true; // other functions are conflicting. Take full ctrl over buttons.
    }

    void loop()
    {
    }

    void addToConfig(JsonObject &root)
    {
        JsonObject top = root.createNestedObject("Three Button Playlist");
        top["Prev button ID"] = button_id_prev;
        top["Play button ID"] = button_id_play;
        top["Next button ID"] = button_id_next;
        top["Playlist items"] = playlist_length;
        top["Playlist start index"] = selected_index + 1; // index in UI is 1 higher than internal index
    }

    bool readFromConfig(JsonObject &root)
    {
        //set default values
        button_id_prev = 0;
        button_id_play = 1;
        button_id_next = 2;
        playlist_length = 8;
        selected_index = 1;

        JsonObject top = root["Three Button Playlist"];
        bool configComplete = !top.isNull();

        configComplete &= getJsonValue(top["Prev button ID"], button_id_prev);
        configComplete &= getJsonValue(top["Play button ID"], button_id_play);
        configComplete &= getJsonValue(top["Next button ID"], button_id_next);
        configComplete &= getJsonValue(top["Playlist items"], playlist_length);
        configComplete &= getJsonValue(top["Playlist start index"], selected_index);
        selected_index--; // index in UI is 1 higher than internal index

        return configComplete;
    }
};