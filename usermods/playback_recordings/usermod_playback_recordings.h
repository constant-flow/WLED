#pragma once

#include "wled.h"
#include "recording_tpm2.h"

// This usermod can play recorded animations

// {"playback":{"file":"/record.tpm2","format":"tpm2"}}
// {"playback":{"file":"/record.tpm2","format":"tpm2","seg":2}}

class PlaybackRecordings : public Usermod
{
private:
  String jsonKeyRecording = "playback";
  String jsonKeyFilePath = "file";
  String jsonKeyPlaybackSegment = "seg";
  String jsonKeyPlaybackSegmentId = "id";
  String jsonKeyRecordingFormat = "format";
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
    if (jsonRecordingEntry.is<JsonObject>())
    {
      const char *recording_path = jsonRecordingEntry[jsonKeyFilePath].as<const char *>();
      if (recording_path)
      {
        int id = -1;

        JsonVariant jsonPlaybackSegment = jsonRecordingEntry[jsonKeyPlaybackSegment];
        if (jsonPlaybackSegment)
        { // playback on segments
          if      (jsonPlaybackSegment.is<JsonObject>())  { id = jsonPlaybackSegment[jsonKeyPlaybackSegmentId] | -1; }
          else if (jsonPlaybackSegment.is<JsonInteger>()) { id = jsonPlaybackSegment; } 
          else { DEBUG_PRINTLN("TPM2: 'seg' either as integer or as json with 'id':'integer'"); }
        };       

        WS2812FX::Segment sg = strip.getSegment(id);

        JsonVariant recordingFormat = jsonRecordingEntry[jsonKeyRecordingFormat];
        if(recordingFormat == formatTpm2) {
          loadRecording(recording_path, sg.start, sg.stop);
          DEBUG_PRINTLN("Playback: play recording");
        } else {
          DEBUG_PRINTLN("Playback: unknown format ... if you read that, you can code the format you need XD");
        }
      }
    } else {
      DEBUG_PRINTLN("Playback: no json object / wrong format");
    }

  }

    uint16_t getId()
    {
      return USERMOD_ID_PLAYBACK_RECORDINGS;
    }
  };