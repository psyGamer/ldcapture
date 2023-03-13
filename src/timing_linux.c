#include "timing.h"

#include <time.h>

#include "hook.h"

static const u32 SECONDS_TO_NANOSECONDS = 1000000000; // 10^9

static const u32 TARGET_FPS = 60;
static const u64 TARGET_TIMESTEP_INC = (u64)((1.0 / TARGET_FPS) * SECONDS_TO_NANOSECONDS);

static u32 currentFrame = 0;
static u64 currentTimestamp = -1;
static u64 currentRealTimestamp = -1;

static bool fixedFPS = false;

static bool videoDone = false;
static bool soundDone = false;

// .NET Core
SYM_HOOK(u64, SystemNative_GetTimestamp, (void),
{
    if (!fixedFPS) return orig_SystemNative_GetTimestamp();
    if (currentTimestamp == -1) currentTimestamp = orig_SystemNative_GetTimestamp();
    return currentTimestamp;
})

// System Wide
SYM_HOOK(i32, clock_gettime, (clockid_t clockID, struct timespec *ts),
{
    if (!fixedFPS) return orig_clock_gettime(clockID, ts);
    // Use more specific hook if available
    if (orig_SystemNative_GetTimestamp != NULL) return orig_clock_gettime(clockID, ts);

    if (currentTimestamp == -1)
    {
        i32 result = orig_clock_gettime(clockID, ts);
        currentTimestamp = ts->tv_sec * SECONDS_TO_NANOSECONDS + ts->tv_nsec;
        return result;
    }
    else
    {
        ts->tv_sec  = currentTimestamp / SECONDS_TO_NANOSECONDS;
        ts->tv_nsec = currentTimestamp % SECONDS_TO_NANOSECONDS;
        return 0;
    }
})

static u64 get_current_timestamp()
{
    // Use hooks if available
    if (orig_SystemNative_GetTimestamp) return orig_SystemNative_GetTimestamp();

    struct timespec ts;
    if (orig_clock_gettime != NULL)
        orig_clock_gettime(CLOCK_MONOTONIC, &ts);
    else
        clock_gettime(CLOCK_MONOTONIC, &ts);
    
    return ts.tv_sec * SECONDS_TO_NANOSECONDS + ts.tv_nsec;;
}

void timing_start()
{
    INFO("Recording started");
    fixedFPS = true;
    currentFrame = 0;
    currentTimestamp = -1;
    currentRealTimestamp = -1;
    videoDone = false;
    soundDone = false;
}

void timing_stop()
{
    INFO("Recording stopped");
    fixedFPS = false;
}

void timing_next_frame()
{
    currentFrame++;

    if (currentTimestamp == -1) currentTimestamp = get_current_timestamp();
    currentTimestamp += TARGET_TIMESTEP_INC;
    currentRealTimestamp = get_current_timestamp();

    videoDone = false;
    soundDone = false;

    TRACE("Current frame: %i", currentFrame);
}

bool timing_is_running()
{
    return fixedFPS;
}

void timing_video_done()
{
    videoDone = true;
}

void timing_sound_done()
{
    soundDone = true;
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
    return currentRealTimestamp == -1 || ((get_current_timestamp() - currentRealTimestamp) > TARGET_TIMESTEP_INC);
}

i32 timing_get_target_fps()
{
    return TARGET_FPS;
}

i32 timing_get_current_frame()
{
    return currentFrame;
}

void init_timing_linux()
{
    LOAD_SYM_HOOK(clock_gettime);
    LOAD_SYM_HOOK(SystemNative_GetTimestamp);
}