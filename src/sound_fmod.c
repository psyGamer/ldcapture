#include "base.h"

#include <pthread.h>

#include <fmod_common.h>

#include "hook.h"
#include "timing.h"
#include "encoder.h"
#include "util/math.h"

static FMOD_RESULT (*orig_FMOD_System_init)(FMOD_SYSTEM* system, i32 maxchannels, FMOD_INITFLAGS flags, void* extradriverdata) = NULL;
static FMOD_RESULT (*orig_FMOD_System_release)(FMOD_SYSTEM* system)                                                            = NULL;

static FMOD_RESULT (*fn_FMOD_System_CreateDSP)(FMOD_SYSTEM* system, const FMOD_DSP_DESCRIPTION* description, FMOD_DSP** dsp) = NULL;
static FMOD_RESULT (*fn_FMOD_System_GetMasterChannelGroup)(FMOD_SYSTEM* system, FMOD_CHANNELGROUP** channelgroup)            = NULL;
static FMOD_RESULT (*fn_FMOD_ChannelGroup_AddDSP)(FMOD_CHANNELGROUP* channelgroup, i32 index, FMOD_DSP* dsp)                 = NULL;
static FMOD_RESULT (*fn_FMOD_ChannelGroup_RemoveDSP)(FMOD_CHANNELGROUP* channelgroup, FMOD_DSP* dsp)                         = NULL;
static FMOD_RESULT (*fn_FMOD_ChannelGroup_SetPaused)(FMOD_CHANNELGROUP* channelgroup, FMOD_BOOL paused)                      = NULL;
static FMOD_RESULT (*fn_FMOD_ChannelGroup_GetVolume)(FMOD_CHANNELGROUP* channelgroup, f32* volume)                           = NULL;
static FMOD_RESULT (*fn_FMOD_ChannelGroup_SetVolume)(FMOD_CHANNELGROUP* channelgroup, f32 volume)                            = NULL;
static FMOD_RESULT (*fn_FMOD_DSP_Release)(FMOD_DSP* dsp)                                                                     = NULL;

static void load_symbols()
{
    library_handle_t libfmod       = shared_library_open("fmod");

    orig_FMOD_System_init    = shared_library_get_symbol(libfmod, "_ZN4FMOD6System4initEijPv");
    orig_FMOD_System_release = shared_library_get_symbol(libfmod, "_ZN4FMOD6System7releaseEv");

    fn_FMOD_System_CreateDSP             = shared_library_get_symbol(libfmod, "FMOD_System_CreateDSP");
    fn_FMOD_System_GetMasterChannelGroup = shared_library_get_symbol(libfmod, "FMOD_System_GetMasterChannelGroup");
    fn_FMOD_ChannelGroup_AddDSP          = shared_library_get_symbol(libfmod, "FMOD_ChannelGroup_AddDSP");
    fn_FMOD_ChannelGroup_RemoveDSP       = shared_library_get_symbol(libfmod, "FMOD_ChannelGroup_RemoveDSP");
    fn_FMOD_ChannelGroup_SetPaused       = shared_library_get_symbol(libfmod, "FMOD_ChannelGroup_SetPaused");
    fn_FMOD_ChannelGroup_GetVolume       = shared_library_get_symbol(libfmod, "FMOD_ChannelGroup_GetVolume");
    fn_FMOD_ChannelGroup_SetVolume       = shared_library_get_symbol(libfmod, "FMOD_ChannelGroup_SetVolume");
    fn_FMOD_DSP_Release                  = shared_library_get_symbol(libfmod, "FMOD_DSP_Release");

    shared_library_close(libfmod);
}

static FMOD_DSP* dsp;
static FMOD_CHANNELGROUP* master_group;

static i32 total_recoded_samples_error = 0;
static i32 target_recorded_samples = 48000 / 60;
static i32 recorded_samples = 0;

FMOD_RESULT F_CALLBACK dsp_read_callback(FMOD_DSP_STATE* dspState, f32* inBuffer, f32* outBuffer, u32 length, i32 inChannels, i32* outChannels) 
{
    if (inChannels == *outChannels)
    {
        memcpy(outBuffer, inBuffer, inChannels * length * sizeof(f32));
    }
    else if (inChannels > *outChannels)
    {
        // Cut the remaining channels off
        for (i32 sample = 0; sample < length; sample++)
        {
            // ? Would mcmcpy be faster here
            for (i32 channel = 0; channel < *outChannels; channel++)
            {
                outBuffer[sample * *outChannels + channel] = inBuffer[sample * inChannels + channel];
            }
        }
    }
    else
    {
        // Repeat the last channel to fill the remaining ones
        for (i32 sample = 0; sample < length; sample++)
        {
            // ? Would mcmcpy be faster here
            i32 channel = 0;
            for (; channel < inChannels; channel++)
            {
                outBuffer[sample * *outChannels + channel] = inBuffer[sample * inChannels + channel];
            }
            for (; channel < *outChannels; channel++)
            {
                outBuffer[sample * *outChannels + channel] = inBuffer[sample * inChannels + inChannels - 1];
            }
        }
    }

    if (!timing_is_running()) return FMOD_OK;

    encoder_t* encoder = encoder_get_current();
    encoder_prepare_sound(encoder, inChannels, length, ENCODER_SOUND_FORMAT_PCM_F32);
    memcpy(encoder->sound_data, inBuffer, inChannels * length * sizeof(f32));
    encoder_flush_sound(encoder);

    recorded_samples += length;

    return FMOD_OK;
}

FMOD_RESULT hook_FMOD_System_init(FMOD_SYSTEM* system, i32 maxchannels, FMOD_INITFLAGS flags, void* extradriverdata)
{
    if (orig_FMOD_System_init == NULL) load_symbols();

    FMOD_RESULT result = orig_FMOD_System_init(system, maxchannels, flags, extradriverdata);

    if (result != FMOD_OK) return result;

    if (fn_FMOD_System_CreateDSP == NULL ||
        fn_FMOD_System_GetMasterChannelGroup == NULL ||
        fn_FMOD_ChannelGroup_AddDSP == NULL ||
        fn_FMOD_ChannelGroup_RemoveDSP == NULL ||
        fn_FMOD_ChannelGroup_SetPaused == NULL ||
        fn_FMOD_DSP_Release == NULL) load_symbols();

    FMOD_DSP_DESCRIPTION desc = { 0 };
    strncpy(desc.name, "ldcatpure DSP recording", sizeof(desc.name));
    desc.version          = 0x00010000;
    desc.numinputbuffers  = 1;
    desc.numoutputbuffers = 1;
    desc.read             = dsp_read_callback; 

    fn_FMOD_System_GetMasterChannelGroup(system, &master_group);
    fn_FMOD_System_CreateDSP(system, &desc, &dsp);
    fn_FMOD_ChannelGroup_AddDSP(master_group, 0, dsp);

    return result;
}

FMOD_RESULT hook_FMOD_System_release(FMOD_SYSTEM* system)
{
    if (orig_FMOD_System_release == NULL) load_symbols();

    FMOD_RESULT result = orig_FMOD_System_release(system);

    if (result != FMOD_OK) return result;

    fn_FMOD_ChannelGroup_RemoveDSP(master_group, dsp);
    fn_FMOD_DSP_Release(dsp);

    return result;
}

static bool run_sound_worker = true;
static pthread_t sound_worker_thread = (pthread_t)NULL;

static bool sound_paused = false;
static f32 sound_volume = -1;

// NOTE: This thread might be a bit difficult to understand since it communicates over timing.h with the video capture.
//       It's probably a good idea to look at a video capture implemenation, for example video_opengl_x11.c, too.
static void* sound_worker(void* _)
{
    TRACE("Started FMOD sound thread");

    while (run_sound_worker)
    {
        // We spin while waiting since we need to respond quickly
        if (!timing_is_running())
        {
            if (sound_paused)
            {
                if (sound_volume != -1)
                    fn_FMOD_ChannelGroup_SetVolume(master_group, sound_volume);
                fn_FMOD_ChannelGroup_SetPaused(master_group, false);
                sound_paused = false;
            }
            continue;
        }

        // Wait until the next frame is started first
        while (timing_is_running() && run_sound_worker && timing_is_sound_done());
        while (timing_is_running() && run_sound_worker && !timing_is_video_ready());

frameStart:
        // Wait a frame to sync again with the video
        if (total_recoded_samples_error >= target_recorded_samples)
        {
            // If the sound wasn't pause we don't want to get data while skipping
            if (!sound_paused)
            {
                fn_FMOD_ChannelGroup_GetVolume(master_group, &sound_volume);
                fn_FMOD_ChannelGroup_SetVolume(master_group, 0.0f);
                fn_FMOD_ChannelGroup_SetPaused(master_group, true);
                sound_paused = true;
            }

            total_recoded_samples_error -= target_recorded_samples;
            
            // The next frame might not've started yet
            while (timing_is_running() && run_sound_worker && timing_is_sound_done());
            // Since we skip the frame we're already done
            timing_mark_sound_done();
            while (timing_is_running() && run_sound_worker && !timing_is_video_ready());

            continue;
        }

        if (sound_paused)
        {
            if (sound_volume != -1)
                fn_FMOD_ChannelGroup_SetVolume(master_group, sound_volume);
            fn_FMOD_ChannelGroup_SetPaused(master_group, false);
            sound_paused = false;
        }
        
        // Wait for data
        while (timing_is_running() && run_sound_worker && recorded_samples < target_recorded_samples);
        // Usually we record more than one frame of data
        total_recoded_samples_error += recorded_samples - target_recorded_samples;
        recorded_samples = 0;

        // Only pause if the next frame isn't ready
        if (!timing_is_video_ready())
        {
            // Setting volume to 0 should reduce sound artifacts
            fn_FMOD_ChannelGroup_GetVolume(master_group, &sound_volume);
            fn_FMOD_ChannelGroup_SetVolume(master_group, 0.0f);
            fn_FMOD_ChannelGroup_SetPaused(master_group, true);
            sound_paused = true;
        }
        else
        {
            timing_mark_sound_done();
            goto frameStart; // Skip syncing with the video since we know it's already ready
        }

        timing_mark_sound_done();
    }

    if (sound_paused)
    {
        if (sound_volume != -1)
            fn_FMOD_ChannelGroup_SetVolume(master_group, sound_volume);
        fn_FMOD_ChannelGroup_SetPaused(master_group, false);
        sound_paused = false;
    }

    TRACE("Stopped FMOD sound thread");
    return NULL;
}

void init_sound_fmod()
{
    hook_symbol(hook_FMOD_System_init, (void**)&orig_FMOD_System_init, "_ZN4FMOD6System4initEijPv");
    hook_symbol(hook_FMOD_System_release, (void**)&orig_FMOD_System_release, "_ZN4FMOD6System7releaseEv");

    // We don't actually hook anything, just getting the original function
    hook_symbol(NULL, (void**)&fn_FMOD_System_CreateDSP, "FMOD_System_CreateDSP");
    hook_symbol(NULL, (void**)&fn_FMOD_System_GetMasterChannelGroup, "FMOD_System_GetMasterChannelGroup");
    hook_symbol(NULL, (void**)&fn_FMOD_ChannelGroup_AddDSP, "FMOD_ChannelGroup_AddDSP");
    hook_symbol(NULL, (void**)&fn_FMOD_ChannelGroup_RemoveDSP, "FMOD_ChannelGroup_RemoveDSP");
    hook_symbol(NULL, (void**)&fn_FMOD_ChannelGroup_SetPaused, "FMOD_ChannelGroup_SetPaused");
    hook_symbol(NULL, (void**)&fn_FMOD_DSP_Release, "FMOD_DSP_Release");

    run_sound_worker = true;
    pthread_create(&sound_worker_thread, NULL, sound_worker, NULL);
}

void shutdown_sound_fmod()
{
    run_sound_worker = false;
    
    if (sound_worker_thread)
        pthread_join(sound_worker_thread, NULL);
}

// libfmod.so (SHA1: 043c7a0c10705679f29f42b0f44e51245e7f8b65)
FMOD_RESULT _ZN4FMOD6System4initEijPv(FMOD_SYSTEM* system, i32 maxchannels, FMOD_INITFLAGS flags, void* extradriverdata)
{
    TRACE("FMOD init");
    return hook_FMOD_System_init(system, maxchannels, flags, extradriverdata);
}
FMOD_RESULT _ZN4FMOD6System7releaseEv(FMOD_SYSTEM* system)
{
    TRACE("FMOD release");
    return hook_FMOD_System_release(system);
}