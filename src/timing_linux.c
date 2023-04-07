#include "timing.h"

#include <time.h>

#include "settings.h"
#include "encoder.h"
#include "hook.h"
#include "api.h"

static const u32 SECONDS_TO_NANOSECONDS = 1000000000; // 10^9

static u32 timestep_inc = 0;

static u32 currentFrame = 0;
static u64 currentTimestamp = -1;
static u64 currentRealTimestamp = -1;

static bool fixedFPS = false;
static bool use_extension_frames = false;
static i32 extension_frames = 0;

static bool videoReady = false;
static bool soundDone = false;

static bool video_finished = false;
static bool sound_finished = false;
static bool pending_finish = false;

// .NET Core
SYM_HOOK(u64, SystemNative_GetTimestamp, (void),
{
    if (!timing_is_running()) return orig_SystemNative_GetTimestamp();
    if (currentTimestamp == -1) currentTimestamp = orig_SystemNative_GetTimestamp();
    return currentTimestamp;
})

// System Wide
SYM_HOOK(i32, clock_gettime, (clockid_t clockID, struct timespec* ts),
{
    if (!timing_is_running()) return orig_clock_gettime(clockID, ts);
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
    videoReady = false;
    soundDone = false;
}

void timing_stop()
{
    INFO("Recording stopped");
    video_finished = false;
    sound_finished = false;
    pending_finish = true;
    fixedFPS = false;
}
void timing_stop_after(i32 extensionFrames)
{
    INFO("Stopping recording after: %i", extensionFrames);
    extension_frames = extensionFrames;
    use_extension_frames = true;
}

void timing_next_frame()
{
    currentFrame++;

    if (currentTimestamp == -1) currentTimestamp = get_current_timestamp();
    currentTimestamp += timestep_inc;
    currentRealTimestamp = get_current_timestamp();

    // Order is important here, since clearing sound first might cause a race condition,
    // when first wait for sound clear and than for video ready in another thread.
    videoReady = false;
    soundDone = false;

    if (use_extension_frames)
    {
        extension_frames--;

        if (extension_frames <= 0)
        {
            use_extension_frames = false;
            timing_stop();
        }
    }

    TRACE("Current frame: %i", currentFrame);
}

bool timing_is_first_frame() { return currentFrame == 0; }
bool timing_is_running() { return fixedFPS; }

void timing_mark_video_ready() { videoReady = true; }
void timing_mark_sound_done() { soundDone = true; }

static void end_encoding() {
    video_finished = false;
    sound_finished = false;
    pending_finish = false;
    encoder_destroy(encoder_get_current()); 
}
void timing_video_finished() { video_finished = true; if (pending_finish && sound_finished) end_encoding(); }
void timing_sound_finished() { sound_finished = true; if (pending_finish && video_finished) end_encoding(); }

bool timing_is_video_ready() { return videoReady; }
bool timing_is_sound_done() { return soundDone; }

bool timing_is_realtime_frame_done()
{
    return currentRealTimestamp == -1 || ((get_current_timestamp() - currentRealTimestamp) > timestep_inc);
}

i32 timing_get_current_frame() { return currentFrame; }

void init_timing_linux()
{
    LOAD_SYM_HOOK(clock_gettime);
    LOAD_SYM_HOOK(SystemNative_GetTimestamp);

    timestep_inc = (u64)((1.0 / settings_fps())*  SECONDS_TO_NANOSECONDS);
}