#include "api.h"
#include "timing.h"
#include "encoder.h"

void ldcapture_StartRecording()
{
    encoder_create(encoder_get_current(), ENCODER_TYPE_FFMPEG);
    timing_start();
}

void ldcapture_StopRecording()
{
    timing_stop();
}

void ldcapture_StopRecordingAfter(i32 extensionFrames)
{
    timing_stop_after(extensionFrames);
}