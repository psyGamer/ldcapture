#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "hook.h"

static const uint32_t SECONDS_TO_TICKS = 1000000000;
static const uint32_t TARGET_FPS = 60;
static const double TARGET_TIMESTEP_INC = (1.0 / TARGET_FPS) * SECONDS_TO_TICKS;

static uint64_t currentTimestamp = -1;

static bool fixedFPS = false;
static bool nextFrame = false;

SYM_HOOK(uint64_t, SystemNative_GetTimestamp, (void),
{
    if (!fixedFPS) return orig_SystemNative_GetTimestamp();

    if (currentTimestamp == -1) currentTimestamp = orig_SystemNative_GetTimestamp();

    if (!nextFrame) return currentTimestamp;
    currentTimestamp += TARGET_TIMESTEP_INC;
    nextFrame = false;

    return currentTimestamp;
})

void timing_start_fixed_fps()
{
    fixedFPS = true;
}

bool timing_is_fixed_fps()
{
    return fixedFPS;
}

void timing_next_frame()
{
    nextFrame = true;
}

void init_timing_linux()
{
    LOAD_SYM_HOOK(SystemNative_GetTimestamp);
}