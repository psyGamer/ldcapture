#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "hook.h"

static const uint32_t SECONDS_TO_TICKS = 1000000000;
static const uint32_t TARGET_FPS = 60;
static const uint64_t TARGET_TIMESTEP_INC = (uint64_t)((1.0 / TARGET_FPS) * SECONDS_TO_TICKS);

static int currentFrame = 0;
static uint64_t currentTimestamp = -1;
static uint64_t currentRealTimestamp = -1;

static bool fixedFPS = false;

static bool videoDone = false;
static bool soundDone = false;

SYM_HOOK(uint64_t, SystemNative_GetTimestamp, (void),
{
    if (!fixedFPS) return orig_SystemNative_GetTimestamp();
    if (currentTimestamp == -1) currentTimestamp = orig_SystemNative_GetTimestamp();
    return currentTimestamp;
})

void timing_start()
{
    fixedFPS = true;
    currentFrame = 0;
    currentTimestamp = -1;
    currentRealTimestamp = -1;
    videoDone = false;
    soundDone = false;
}

void timing_stop()
{
    fixedFPS = false;
}

void timing_next_frame()
{
    currentFrame++;

    if (currentTimestamp == -1) currentTimestamp = orig_SystemNative_GetTimestamp();
    currentTimestamp += TARGET_TIMESTEP_INC;
    currentRealTimestamp = orig_SystemNative_GetTimestamp();

    videoDone = false;
    soundDone = false;
    printf("NEXT FRAME!\n");
}

bool timing_is_running()
{
    return fixedFPS;
}

void timing_video_done()
{
    videoDone = true;
    printf("VIDEO DONE!\n");
}

void timing_sound_done()
{
    soundDone = true;
    printf("SOUND DONE!\n");
}

bool timing_is_video_done()
{
    return videoDone;
}

bool timing_is_sound_done()
{
    return soundDone;
}

bool timing_is_realtime_frame_done()
{
    if (orig_SystemNative_GetTimestamp == NULL) init_ldcapture();
    return (orig_SystemNative_GetTimestamp() - currentRealTimestamp) > TARGET_TIMESTEP_INC;

    return currentRealTimestamp == -1 || ((orig_SystemNative_GetTimestamp() - currentRealTimestamp) > TARGET_TIMESTEP_INC);
}

int timing_get_target_fps()
{
    return TARGET_FPS;
}

int timing_get_current_frame()
{
    return currentFrame;
}

void init_timing_linux()
{
    LOAD_SYM_HOOK(SystemNative_GetTimestamp);
}