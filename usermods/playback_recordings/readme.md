# Playback recordings

This usermod can play recorded animations

## Software
modify `platformio.ini` and add to the `build_flags` of your configuration the following

Enable the SD 
1. via `WLED_USE_SD_MMC` when connected via MMC or
1. via `WLED_USE_SD_SPI` connected via SPI (use usermod page to setup SPI pins)
```
-D USERMOD_PLAYBACK_RECORDINGS
```