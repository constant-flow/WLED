# Playback recordings

This usermod can play recorded animations

## Software
modify `platformio.ini` and add to the `build_flags` of your configuration the following

```
-D USERMOD_PLAYBACK_RECORDINGS
```

Enable the SD (only one at a time)
1. via `-D WLED_USE_SD_MMC` when connected via MMC or
1. via `-D WLED_USE_SD_SPI` when connected via SPI (use usermod page to setup SPI pins)

## Setup a playback of a recording on a preset

Go to `[WLED-IP]/edit` and upload a recording file to flash memory (LittleFS), e.g. `record.tpm2` or any other file with a supported file format.
The file extension encodes the format used to decode the animation.
Go to `Presets` and create a new preset.
Edit `API Command` and add this to playback the animation on segment 2.
```
{"playback":{"file":"/record.tpm2","seg":2}}
```
In case you don't care about segments remove the `,"seg":2` part.

If SD (`WLED_USE_SD_MMC` or `WLED_USE_SD_SPI`) is enabled, the system will prefer `SD` over `Flash` stored recordings, to add an animation add it via the SD card on a host computer (no WLED interface to do that right now).