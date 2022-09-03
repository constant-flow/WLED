#pragma once

#include "wled.h"

// predefs
void file_handlePlayRecording();
void file_loadRecording(const char *filepath, uint16_t startLed, uint16_t stopLed);
void file_playFrame();
void tpm2_playNextRecordingFrame();


#ifndef USED_STORAGE_FILESYSTEMS
  #define USED_STORAGE_FILESYSTEMS "LittleFS (SD_CARD mod not active)"
#endif


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

public:
  static const char _name[];

  void setup()
  { 
    DEBUG_PRINTF("[%s] usermod loaded (storage: %s)\n", _name, USED_STORAGE_FILESYSTEMS); 
  }

  void loop()
  {
    file_handlePlayRecording();
  }

  void readFromJsonState(JsonObject &root)
  {
    DEBUG_PRINTF("[%s] load json\n", _name);

    // check if recording keyword is contained in API-command (json)
    // when a preset is fired it's normal to receive first the preset-firing ("ps":"<nr>]","time":"<UTC>")
    // followed by the specified API-Command of this preset
    JsonVariant jsonRecordingEntry = root[jsonKeyRecording];
    if (!jsonRecordingEntry.is<JsonObject>()) {
      String debugOut;
      serializeJson(root, debugOut);
      DEBUG_PRINTF("[%s] no '%s' key or wrong format: \"%s\"\n", _name, jsonKeyRecording.c_str(), debugOut.c_str());
      return;
    }
    
    // check if a mandatory path to the playback file exists in the API-command
    const char *recording_path = jsonRecordingEntry[jsonKeyFilePath].as<const char *>();
    String pathToRecording = recording_path;
    if (!recording_path)
    {
      DEBUG_PRINTF("[%s] '%s' not defined\n", _name, jsonKeyFilePath.c_str());
      return;
    }

    // retrieve the segment(id) to play the recording on
    int id = -1; // segment id
    JsonVariant jsonPlaybackSegment = jsonRecordingEntry[jsonKeyPlaybackSegment];
    if (jsonPlaybackSegment)
    { // playback on segments
      if      (jsonPlaybackSegment.is<JsonObject>())  { id = jsonPlaybackSegment[jsonKeyPlaybackSegmentId] | -1; }
      else if (jsonPlaybackSegment.is<JsonInteger>()) { id = jsonPlaybackSegment; } 
      else { DEBUG_PRINTF("[%s] '%s' either as integer or as json with 'id':'integer'\n", _name, jsonKeyPlaybackSegment.c_str());};       
    }

    // retrieve the recording format from the file extension    
    for(int i=0; i<PLAYBACK_FORMAT::COUNT_PLAYBACK_FORMATS; i++){
      if(pathToRecording.endsWith(playback_formats[i])) {
        currentPlaybackFormat = (PLAYBACK_FORMAT) i;
        break;
      }
    }

    // stop here if the format is unknown
    if(currentPlaybackFormat == PLAYBACK_FORMAT::FORMAT_UNKNOWN) {
      DEBUG_PRINTF("[%s] unknown format ... if you read that, you can code the format you need XD\n", _name);      
      return;
    }
    
    // load playback to defined segment on strip (file_loadRecording handles the different formats within (file_playFrame))
    WS2812FX::Segment sg = strip.getSegment(id);
    file_loadRecording(recording_path, sg.start, sg.stop);
    DEBUG_PRINTF("[%s] play recording\n", _name);      
  }

  uint16_t getId()
  {
    return USERMOD_ID_PLAYBACK_RECORDINGS;
  }
};

const char PlaybackRecordings::_name[] PROGMEM = "Playback Recordings";


//        ######## #### ##       ######## 
//        ##        ##  ##       ##       
//        ##        ##  ##       ##       
//        ######    ##  ##       ######   
//        ##        ##  ##       ##       
//        ##        ##  ##       ##       
//        ##       #### ######## ######## 

// Recording format agnostic functions to load and skim thru a 
// recording/playback file and control its related entities

//TODO: maybe add as custom RT_MODE in `const.h` and `json.cpp`
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


// clear the segment used by the playback
void file_clearLastPlayback() { 
  for (uint16_t i = playbackLedStart; i < playbackLedStop; i++)
  {
    // tpm2_GetNextColorData(colorData);
    setRealtimePixel(i, 0,0,0,0);
  }
}

//checks if the file is available on LittleFS
bool file_onFS(const char *filepath)
{
  return WLED_FS.exists(filepath);
}

void file_loadRecording(const char *filepath, uint16_t startLed, uint16_t stopLed)
{  
  //close any potentially open file  
  if(recordingFile.available()) {
    file_clearLastPlayback();
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

  DEBUG_PRINTF("[%s] Load animation on LED %d to %d\n", PlaybackRecordings::_name, playbackLedStart, playbackLedStop);         

  #ifdef SD_ADAPTER  
  if(file_onSD(filepath)){
    DEBUG_PRINTF("[%s] Read file from SD: %s\n", PlaybackRecordings::_name, filepath);
    recordingFile = SD_ADAPTER.open(filepath, "rb");  
  } else 
  #endif
  if(file_onFS(filepath)) {
    DEBUG_PRINTF("[%s] Read file from FS: %s\n", PlaybackRecordings::_name, filepath);
    recordingFile = WLED_FS.open(filepath, "rb");
  } else {
    DEBUG_PRINTF("[%s] File %s not found (%s)\n", PlaybackRecordings::_name, filepath, USED_STORAGE_FILESYSTEMS);
    return;
  }

  if (realtimeOverride == REALTIME_OVERRIDE_ONCE)
  {
      realtimeOverride = REALTIME_OVERRIDE_NONE;
  }

  recordingRepeats = RECORDING_REPEAT_DEFAULT;
  file_playFrame();
}

// skips until a specific byte comes up
void file_skipUntil(uint8_t byteToStopAt)
{
  uint8_t rb = 0;
  do { rb = recordingFile.read(); }
  while (recordingFile.available() && rb != byteToStopAt);
}

bool file_stopBecauseAtTheEnd()
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
      DEBUG_PRINTF("[%s] Repeat recordind again for: %d\n", PlaybackRecordings::_name, recordingRepeats);  
    }
    else
    {      
      exitRealtime();
      recordingFile.close();
      file_clearLastPlayback();
      return true;
    }
  }

  return false;
}

void file_playFrame() {
  switch (currentPlaybackFormat)
  {
    case PLAYBACK_FORMAT::TPM2:  tpm2_playNextRecordingFrame(); break;
    // Add case for each format
    default: break;
  }
}

void file_handlePlayRecording()
{
  if (realtimeMode != REALTIME_MODE_PLAYBACK) return;
  if ( millis() - lastFrame < msFrameDelay)   return;

  file_playFrame();
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

void tpm2_SkipUntilNextPacket()  { file_skipUntil(TPM2_START); }

void tpm2_SkipUntilEndOfPacket() { file_skipUntil(TPM2_END); }

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
  DEBUG_PRINTF("[%s] tpm2_processCommandData: not implemented yet\n", PlaybackRecordings::_name);
  tpm2_SkipUntilNextPacket();
}

void tpm2_processResponseData()
{
  DEBUG_PRINTF("[%s] tpm2_processResponseData: not implemented yet\n", PlaybackRecordings::_name);
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
  DEBUG_PRINTF("[%s] tpm2_processUnknownData - received: %d\n", PlaybackRecordings::_name, data);
  tpm2_SkipUntilNextPacket();
}

// scan and forward until next frame was read (this will process commands)
void tpm2_playNextRecordingFrame()
{
  if(file_stopBecauseAtTheEnd()) return;

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
    else if(rb == TPM2_RESPONSE)   tpm2_processResponseData();
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
    case TPM2_START:
      DEBUG_PRINTF("\n%02x ", rb);
      break;
    default:
    {
      DEBUG_PRINTF("%02x ", rb);
    }
    }
  }

  recordingFile.close();
}
