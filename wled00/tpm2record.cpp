#include "wled.h"

// --- CONSTANTS ---
#define TPM2_START      0xC9
#define TPM2_DATA_FRAME 0xDA
#define TPM2_COMMAND    0xC0
#define TPM2_END        0x36

// infinite loop of animation
#define RECORDING_REPEAT_LOOP -1

// Default repeat count, when not specified by preset (-1=loop, 0=play once, 2=repeat two times)
#define RECORDING_REPEAT_DEFAULT 0

// --- Recording playback related ---
File     recordingFile;
uint8_t  colorData[4];
uint8_t  colorChannels    = 3;
uint32_t msFrameDelay     = 33; // time between frames
int32_t  recordingRepeats = RECORDING_REPEAT_LOOP;
unsigned long lastFrame   = 0;

// scrolls to next frame, returns false when no further data is available
bool ignoreDataUntilNextFrame()
{
  uint8_t rb = 0;
  do
  {
    rb = recordingFile.read();
    DEBUG_PRINT("ignore until next frame: ");
    DEBUG_PRINTLN(rb);
  } while (recordingFile.available() && rb != TPM2_START);
  if (!recordingFile.available())
    return false;
  return true;
}

void gotoEndOfFrame()
{
  uint8_t rb = 0;
  do
  {
    if (rb != 0)
      DEBUG_PRINTLN("WARNING: Frame was expected to be at end");
    rb = recordingFile.read();
    DEBUG_PRINT("Go to end of frame: ");
    DEBUG_PRINT(rb);
  } while (recordingFile.available() && rb != TPM2_END);
  DEBUG_PRINT(" (Data left: ");
  DEBUG_PRINT(recordingFile.available());
  DEBUG_PRINTLN(")");
}

void readCommandData()
{
  DEBUG_PRINTLN("readCommandData: not implemented yet");
  ignoreDataUntilNextFrame();
}

void getNextColorData(uint8_t data[])
{
  data[0] = recordingFile.read();
  data[1] = recordingFile.read();
  data[2] = recordingFile.read();
  DEBUG_PRINT("read color: ");
  DEBUG_PRINT(data[0]);
  DEBUG_PRINT(" ");
  DEBUG_PRINT(data[1]);
  DEBUG_PRINT(" ");
  DEBUG_PRINT(data[2]);
  DEBUG_PRINTLN("");
  data[3] = 0; // TODO add RGBW mode to TPM2: colorChannels = 4
}

uint16_t getNextFrameLength()
{
  if (!recordingFile.available())
    return 0;
  uint8_t highbyte_size = recordingFile.read();
  uint8_t lowbyte_size = recordingFile.read();
  uint16_t size = highbyte_size << 8 | lowbyte_size;
  DEBUG_PRINT("Payload size: ");
  DEBUG_PRINT(size);
  DEBUG_PRINT(" (");
  DEBUG_PRINT(highbyte_size);
  DEBUG_PRINT(",");
  DEBUG_PRINT(lowbyte_size);
  DEBUG_PRINTLN(")");
  return size;
}

void playNextRecordingFrame()
{
  DEBUG_PRINT("=== Load next Frame (Data left: ");
  DEBUG_PRINT(recordingFile.available());
  DEBUG_PRINTLN(")");

  //If recording reached end loop or stop playback
  if (!recordingFile.available())
  {
    if (recordingRepeats == RECORDING_REPEAT_LOOP)
    {
      recordingFile.seek(0);
      DEBUG_PRINTLN("Repeat (loop)");
    }
    else if (recordingRepeats > 0)
    {
      recordingFile.seek(0);
      recordingRepeats--;
      DEBUG_PRINT("Repeat for ");
      DEBUG_PRINT(recordingRepeats);
      DEBUG_PRINTLN(" loops");
    }
    else
    {
      DEBUG_PRINT("Repeats: ");
      DEBUG_PRINTLN(recordingRepeats);
      DEBUG_PRINTLN("STOP RECORDING PLAYBACK");
      uint8_t mode = REALTIME_MODE_INACTIVE;
      realtimeLock(0, mode);
      recordingFile.close();
      return;
    }
  }

  // scan to next TPM2 Frame start, should be the first attempt
  uint8_t rb = 0;
  do
  {
    rb = recordingFile.read();
    DEBUG_PRINT("Read new frame: ");
    DEBUG_PRINTLN(rb);
  } while (recordingFile.available() && rb != TPM2_START);
  if (!recordingFile.available())
    return;

  rb = recordingFile.read();
  DEBUG_PRINT("check if data or command: ");
  DEBUG_PRINTLN(rb);

  while (rb == TPM2_COMMAND)
  {
    readCommandData(); //it's assumed all control data is stored in the beginning
    rb = recordingFile.read();
    DEBUG_PRINT("scan for command or data: ");
    DEBUG_PRINTLN(rb);
  }

  if (rb != TPM2_DATA_FRAME)
  {
    DEBUG_PRINTLN("WARNING: RECEIVED UNEXPECTED DATA");
    DEBUG_PRINTLN(rb);
    return;
  }

  uint16_t frameLength = getNextFrameLength(); // opt-TODO maybe stretch recording to available leds

  for (uint16_t i = 0; i < frameLength / colorChannels; i++)
  {
    getNextColorData(colorData);
    setRealtimePixel(i, colorData[0], colorData[1], colorData[2], colorData[3]);
  }

  gotoEndOfFrame();

  strip.show();
  // tell ui we are playing the recording right now
  uint8_t mode = REALTIME_MODE_TPM2RECORD;
  realtimeLock(realtimeTimeoutMs, mode);

  lastFrame = millis();
}

void handlePlayRecording()
{
  if (realtimeMode != REALTIME_MODE_TPM2RECORD)
    return;
  if ( millis() - lastFrame < msFrameDelay)
    return; // TODO limit to correct framerate
  DEBUG_PRINT("recording time passed: ");
  DEBUG_PRINTLN( millis() - lastFrame);
  playNextRecordingFrame();
}

void printWholeRecording() {
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

void loadRecording(const char *filepath)
{
  //close any potentially open file
  if(recordingFile.available()) recordingFile.close();

  recordingFile = WLED_FS.open(filepath, "rb");

  printWholeRecording();

  if (realtimeOverride == REALTIME_OVERRIDE_ONCE)
  {
      realtimeOverride = REALTIME_OVERRIDE_NONE;
  }

  recordingRepeats = RECORDING_REPEAT_DEFAULT;
  recordingFile = WLED_FS.open(filepath, "rb");
  playNextRecordingFrame();
}
