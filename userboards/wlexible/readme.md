# Wlexible board

- 2 LED strips
- 3 Button playlist control: prev / play / next
- IR-Remote input
- HID-Keyboard input (when RP2040 board connected)
- SD Cards to store TPM2 - animations

## Setup 
- Flash this `wlexible` config via platformio (see `platformio.ini`): VSCode add platformio add-on + `CMD+shift+P`:`PlatformIO: Upload`
- Access WLED access point via WiFi (`WLEXIBLE-AP`)
- Setup WLED to an available accessible Wifi (if there is no network, create an AP with your phone).
- Visit `http://poe.local/edit` and upload the `cfg.json` from this folder (`Choose File` -> pick `cfg.json` -> press `Upload`)
- Reset / Reboot the board
- The board should show up as `wlexible`, if you need multiple boards in the network, change its hostname

## HID Support
- The board can trigger the presets 1 - 10 with the numbered keys 0 - 9 (Gitlab: tools/hardware/pico-hid-host)
- USB keyboard can be connected via OTG converter

## TPM2 Recording
- Recordings can be done with Jinx! or the LedToolkit (Gitlab: tools/hardware/led-toolkit/-/tree/main/ScreenRecStreamCut)
- Bigger files can be added to SD card
- Smaller files can be added to flash via `http://poe.local/edit` this doesn't require an SD card

## Assign recordings to presets: 
- visit `wlexible.local/`
- go to `Presets` 
- press `Create Preset`
- untick `Use current state`
- Add to API-command: `{"tpm2":{"file":"/[recording].tpm2"}}`

### play recording on segment
- API-command to play `rec.tpm2` on segment `2` : `{"tpm2":{"file":"/rec.tpm2","seg":2}}`
- you can alter the segments with each preset via the [API](https://kno.wled.ge/interfaces/json-api/)

