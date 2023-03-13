#include "api.h"
#include "timing.h"

void ldcapture_StartRecording()
{
    timing_start();
}

void ldcapture_StopRecording()
{
    timing_stop();
}