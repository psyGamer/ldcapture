#pragma once

#include "base.h"

void ldcapture_StartRecording();
void ldcapture_StopRecording();
void ldcapture_StopRecordingAfter(i32 extensionFrames);
void ldcapture_ReloadConfig();