#pragma once

#include "wled.h"

// predefs
void handlePlayRecording();
void loadRecording(const char *filepath, uint16_t startLed, uint16_t stopLed);
void playFrame();
void tpm2_playNextRecordingFrame();

//      ##     ##  ######  ######## ########  ##     ##  #######  ########  
//      ##     ## ##    ## ##       ##     ## ###   ### ##     ## ##     ## 
//      ##     ## ##       ##       ##     ## #### #### ##     ## ##     ## 
//      ##     ##  ######  ######   ########  ## ### ## ##     ## ##     ## 
//      ##     ##       ## ##       ##   ##   ##     ## ##     ## ##     ## 
//      ##     ## ##    ## ##       ##    ##  ##     ## ##     ## ##     ## 
//       #######   ######  ######## ##     ## ##     ##  #######  ########  


// This usermod can play recorded animations
// 
// What formats are supported:
//   - *.TPM2
//
// What does it mean:
//   You can now store short recorded animations on the ESP32 (in the ROM: no SD required)
//
// How to transfer the animation:
//   WLED offers a web file manager under <IP_OF_WLED>/edit here you can upload a recorded file
//
// How to create a recording:
//   You can record with tools like Jinx
//
// How to load the animation:
//   You have to specify a preset to playback this recording with an API command
//   {"playback":{"file":"/record.tpm2"}}
//
//   You can specify a preset to playback this recording on a specific segment
//   {"playback":{"file":"/record.tpm2","seg":2}}
//   {"playback":{"file":"/record.tpm2","seg":{"id":2}}
//
// How to trigger the animation:
//   Presets can be triggered multiple interfaces e.g. via the json API, via the web interface or with a connected IR remote
//
// What next:
//   - Playback and Recording of RGBW animations, as right now only RGB recordings are supported by WLED

enum PLAYBACK_FORMAT {
  TPM2=0, 
  FSEQ,
  FORMAT_UNKNOWN,
  COUNT_PLAYBACK_FORMATS
  };

enum PLAYBACK_FORMAT currentPlaybackFormat = PLAYBACK_FORMAT::FORMAT_UNKNOWN;

static const String playback_formats[] = {"tpm2", "fseq","   "};

class PlaybackRecordings : public Usermod
{
private:
  String jsonKeyRecording = "playback";
  String jsonKeyFilePath = "file";
  String jsonKeyPlaybackSegment = "seg";
  String jsonKeyPlaybackSegmentId = "id";
  String formatTpm2 = "tpm2";

  String recordingFileToLoad = "";

public:
  void setup()
  {
    DEBUG_PRINTLN("Playback: usermod loaded");
  }

  void loop()
  {
    handlePlayRecording();
  }

  void readFromJsonState(JsonObject &root)
  {
    DEBUG_PRINTLN("Playback: load json");
    recordingFileToLoad = root[jsonKeyRecording] | recordingFileToLoad;

    JsonVariant jsonRecordingEntry = root[jsonKeyRecording];
    if (!jsonRecordingEntry.is<JsonObject>()) {
      DEBUG_PRINTLN("Playback: no json object / wrong format");
      return;
    }
    
    const char *recording_path = jsonRecordingEntry[jsonKeyFilePath].as<const char *>();
    String pathToRecording = recording_path;
    if (!recording_path)
    {
      DEBUG_PRINTLN("Playback: 'recording_path' not defined");
      return;
    }

    // retrieve the segment(id) to play the recording on
    int id = -1; // segment id
    JsonVariant jsonPlaybackSegment = jsonRecordingEntry[jsonKeyPlaybackSegment];
    if (jsonPlaybackSegment)
    { // playback on segments
      if      (jsonPlaybackSegment.is<JsonObject>())  { id = jsonPlaybackSegment[jsonKeyPlaybackSegmentId] | -1; }
      else if (jsonPlaybackSegment.is<JsonInteger>()) { id = jsonPlaybackSegment; } 
      else { DEBUG_PRINTLN("Playback: 'seg' either as integer or as json with 'id':'integer'");}
    };       

    // retrieve the recording format from the file extension    
    for(int i=0; i<PLAYBACK_FORMAT::COUNT_PLAYBACK_FORMATS; i++){
      if(pathToRecording.endsWith(playback_formats[i])) {
        currentPlaybackFormat = (PLAYBACK_FORMAT) i;
        break;
      }
    }

    if(currentPlaybackFormat == PLAYBACK_FORMAT::FORMAT_UNKNOWN) {
      DEBUG_PRINTLN("Playback: unknown format ... if you read that, you can code the format you need XD");
      return;
    }
    
    WS2812FX::Segment sg = strip.getSegment(id);
    loadRecording(recording_path, sg.start, sg.stop);
    DEBUG_PRINTLN("Playback: play recording");    
  }

    uint16_t getId()
    {
      return USERMOD_ID_PLAYBACK_RECORDINGS;
    }
  };

//        ######## #### ##       ######## 
//        ##        ##  ##       ##       
//        ##        ##  ##       ##       
//        ######    ##  ##       ######   
//        ##        ##  ##       ##       
//        ##        ##  ##       ##       
//        ##       #### ######## ######## 

// Recording format agnostic functions to load and skim thru a recording file

// SD connected via MMC 
#ifdef WLED_USE_SD_MMC
  #define USED_STORAGE_FILESYSTEMS "SD MMC, LittleFS"
  #define SD_ADAPTER SD_MMC
  #include "SD_MMC.h"
// SD connected via SPI (adjustable via usermod config)
#elif defined(WLED_USE_SD_SPI)
  #define SD_ADAPTER SD
  #define USED_STORAGE_FILESYSTEMS "SD SPI, LittleFS"
  #include "SD.h"
  #include "SPI.h"
// no SD card support only use records from Flash (LittleFS)
#else
  #define USED_STORAGE_FILESYSTEMS "LittleFS"
#endif

#define REALTIME_MODE_PLAYBACK REALTIME_MODE_GENERIC

// infinite loop of animation
#define RECORDING_REPEAT_LOOP -1

// Default repeat count, when not specified by preset (-1=loop, 0=play once, 2=repeat two times)
#define RECORDING_REPEAT_DEFAULT 0


File recordingFile;
uint16_t playbackLedStart = 0; // first led to play animation on
uint16_t playbackLedStop  = 0; // led after the last led to play animation on
uint8_t  colorData[4];
uint8_t  colorChannels    = 3;
uint32_t msFrameDelay     = 33; // time between frames
int32_t  recordingRepeats = RECORDING_REPEAT_LOOP;
unsigned long lastFrame   = 0;

#ifdef SD_ADAPTER
//checks if the file is available on SD card
bool fileOnSD(const char *filepath)
{
  if(!SD_ADAPTER.begin()) return false; // mounting the card failed

  uint8_t cardType = SD_ADAPTER.cardType();
  if(cardType == CARD_NONE) return false; // no SD card attached  
  if(cardType == CARD_MMC || cardType == CARD_SD || cardType == CARD_SDHC)
  {
    return SD_ADAPTER.exists(filepath);       
  } 

  return false; // unknown card type
}
#endif

//checks if the file is available on LittleFS
bool fileOnFS(const char *filepath)
{
  return WLED_FS.exists(filepath);
}

void clearLastPlayback() { 

  for (uint16_t i = playbackLedStart; i < playbackLedStop; i++)
  {
    // tpm2_GetNextColorData(colorData);
    setRealtimePixel(i, 0,0,0,0);
  }
}

void loadRecording(const char *filepath, uint16_t startLed, uint16_t stopLed)
{  
  //close any potentially open file  
  if(recordingFile.available()) {
    clearLastPlayback();
    recordingFile.close();
  }

  playbackLedStart = startLed;
  playbackLedStop = stopLed;

  // No start/stop defined
  if(playbackLedStart == uint16_t(-1) || playbackLedStop == uint16_t(-1)) {
    WS2812FX::Segment sg = strip.getSegment(-1);
    playbackLedStart = sg.start;
    playbackLedStop = sg.stop;
  }

  DEBUG_PRINTF("Playback: Load animation on LED %d to %d\n", playbackLedStart, playbackLedStop);         

  #ifdef SD_ADAPTER
  if(fileOnSD(filepath)){
    DEBUG_PRINTF("Read file from SD: %s\n", filepath);
    recordingFile = SD_ADAPTER.open(filepath, "rb");  
  } else 
  #endif
  if(fileOnFS(filepath)) {
    DEBUG_PRINTF("Read file from FS: %s\n", filepath);
    recordingFile = WLED_FS.open(filepath, "rb");
  } else {
    DEBUG_PRINTF("File %s not found (%s)\n", filepath, USED_STORAGE_FILESYSTEMS);
    return;
  }

  if (realtimeOverride == REALTIME_OVERRIDE_ONCE)
  {
      realtimeOverride = REALTIME_OVERRIDE_NONE;
  }

  recordingRepeats = RECORDING_REPEAT_DEFAULT;
  playFrame();
}

// skips until a specific byte comes up
void skipUntil(uint8_t byteToStopAt)
{
  uint8_t rb = 0;
  do { rb = recordingFile.read(); }
  while (recordingFile.available() && rb != byteToStopAt);
}

bool stopBecauseAtTheEnd()
{
  //If recording reached end loop or stop playback
  if (!recordingFile.available())
  {
    if (recordingRepeats == RECORDING_REPEAT_LOOP)
    {
      recordingFile.seek(0); // go back the beginning of the recording
    }
    else if (recordingRepeats > 0)
    {
      recordingFile.seek(0); // go back the beginning of the recording
      recordingRepeats--;
      DEBUG_PRINT("Repeat recordind again for:");
      DEBUG_PRINTLN(recordingRepeats);      
    }
    else
    {      
      exitRealtime();
      recordingFile.close();
      clearLastPlayback();
      return true;
    }
  }

  return false;
}

void playFrame() {
  switch (currentPlaybackFormat)
  {
    case PLAYBACK_FORMAT::TPM2:  tpm2_playNextRecordingFrame(); break;
    // Add case for each format
    default: break;
  }
}

void handlePlayRecording()
{
  if (realtimeMode != REALTIME_MODE_PLAYBACK) return;
  if ( millis() - lastFrame < msFrameDelay)   return;

  playFrame();
}

//      ######## ########  ##     ##  #######  
//         ##    ##     ## ###   ### ##     ## 
//         ##    ##     ## #### ####        ## 
//         ##    ########  ## ### ##  #######  
//         ##    ##        ##     ## ##        
//         ##    ##        ##     ## ##        
//         ##    ##        ##     ## ######### 

// reference spec of TPM2: https://gist.github.com/jblang/89e24e2655be6c463c56
// - A packet contains any data of the TPM2 protocol, it
//     starts with `TPM2_START` and ends with `TPM2_END`
// - A frame contains the visual data (the LEDs color's) of one moment

// --- CONSTANTS ---
#define TPM2_START      0xC9
#define TPM2_DATA_FRAME 0xDA
#define TPM2_COMMAND    0xC0
#define TPM2_END        0x36
#define TPM2_RESPONSE   0xAA

// --- Recording playback related ---

void tpm2_SkipUntilNextPacket()  { skipUntil(TPM2_START); }

void tpm2_SkipUntilEndOfPacket() { skipUntil(TPM2_END); }

void tpm2_GetNextColorData(uint8_t data[])
{
  data[0] = recordingFile.read();
  data[1] = recordingFile.read();
  data[2] = recordingFile.read();
  data[3] = 0; // TODO add RGBW mode to TPM2
}

uint16_t tpm2_getNextPacketLength()
{
  if (!recordingFile.available()) { return 0; }
  uint8_t highbyte_size = recordingFile.read();
  uint8_t lowbyte_size = recordingFile.read();
  uint16_t size = highbyte_size << 8 | lowbyte_size;
  return size;
}

void tpm2_processCommandData()
{
  DEBUG_PRINTLN("tpm2_processCommandData: not implemented yet");
  tpm2_SkipUntilNextPacket();
}

void processResponseData()
{
  DEBUG_PRINTLN("processResponseData: not implemented yet");
  tpm2_SkipUntilNextPacket();
}

void tpm2_processFrameData()
{
  uint16_t packetLength = tpm2_getNextPacketLength(); // opt-TODO maybe stretch recording to available leds
  uint16_t lastLed = min(playbackLedStop, uint16_t(playbackLedStart + packetLength));

  for (uint16_t i = playbackLedStart; i < lastLed; i++)
  {
    tpm2_GetNextColorData(colorData);
    setRealtimePixel(i, colorData[0], colorData[1], colorData[2], colorData[3]);
  }

  tpm2_SkipUntilEndOfPacket();

  strip.show();
  // tell ui we are playing the recording right now
  realtimeLock(realtimeTimeoutMs, REALTIME_MODE_PLAYBACK);

  lastFrame = millis();
}

void tpm2_processUnknownData(uint8_t data)
{
  DEBUG_PRINT("tpm2_processUnknownData - received:");
  DEBUG_PRINTLN(data);
  tpm2_SkipUntilNextPacket();
}

// scan and forward until next frame was read (this will process commands)
void tpm2_playNextRecordingFrame()
{
  if(stopBecauseAtTheEnd()) return;

  uint8_t rb = 0; // last read byte from file

  // scan to next TPM2 packet start, should be the first attempt
  do { rb = recordingFile.read(); } 
  while (recordingFile.available() && rb != TPM2_START);  
  if (!recordingFile.available()) { return; }

  // process everything until (including) the next frame data
  while(true)
  {
    rb = recordingFile.read();
    if     (rb == TPM2_COMMAND)    tpm2_processCommandData();     
    else if(rb == TPM2_RESPONSE)   processResponseData();
    else if(rb != TPM2_DATA_FRAME) tpm2_processUnknownData(rb);
    else {
      tpm2_processFrameData();
      break;      
    }
  }
}

void tpm2_printWholeRecording()
{
  while (recordingFile.available())
  {
    uint8_t rb = recordingFile.read();

    switch (rb)
    {
    case 0xC9:
      DEBUG_PRINTLN("");
      DEBUG_PRINT(rb);
      DEBUG_PRINT(" ");
      break;
    default:
    {
      DEBUG_PRINT(rb);
      DEBUG_PRINT(" ");
    }
    }
  }

  recordingFile.close();
}
