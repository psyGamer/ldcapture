#define _XOPEN_SOURCE 700
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <pthread.h>

#include <fmod_studio_common.h>

#include "hook.h"
#include "timing.h"
#include "util/math.h"

static FMOD_RESULT (*orig_FMOD_System_init)(FMOD_SYSTEM *system, int maxchannels, FMOD_INITFLAGS flags, void *extradriverdata) = NULL;
static FMOD_RESULT (*orig_FMOD_System_release)(FMOD_SYSTEM *system)                                                            = NULL;

static FMOD_RESULT (*fn_FMOD_System_CreateDSP)(FMOD_SYSTEM *system, const FMOD_DSP_DESCRIPTION *description, FMOD_DSP **dsp) = NULL;
static FMOD_RESULT (*fn_FMOD_System_GetMasterChannelGroup)(FMOD_SYSTEM *system, FMOD_CHANNELGROUP **channelgroup)            = NULL;
static FMOD_RESULT (*fn_FMOD_ChannelGroup_AddDSP)(FMOD_CHANNELGROUP *channelgroup, int index, FMOD_DSP *dsp)                 = NULL;
static FMOD_RESULT (*fn_FMOD_ChannelGroup_RemoveDSP)(FMOD_CHANNELGROUP *channelgroup, FMOD_DSP *dsp)                         = NULL;
static FMOD_RESULT (*fn_FMOD_ChannelGroup_SetPaused)(FMOD_CHANNELGROUP *channelgroup, FMOD_BOOL paused)                      = NULL;
static FMOD_RESULT (*fn_FMOD_DSP_Release)(FMOD_DSP *dsp)                                                                     = NULL;

typedef struct dsp_data_t
{
    float *buffer;
    float volumeLinear;
    int lengthSamples;
    int channels;
} dsp_data_t;

static FMOD_DSP *dsp;
static FMOD_CHANNELGROUP *masterGroup;

static FILE *outFile;

static int totalRecodedSamplesError = 0;
static int targetRecordedSamples = 48000 / 60;
static int recordedSamples = 0;

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
    fn_FMOD_DSP_Release                  = shared_library_get_symbol(libfmod, "FMOD_DSP_Release");

    shared_library_close(libfmod);
}

FMOD_RESULT F_CALLBACK dsp_create_callback(FMOD_DSP_STATE *dspState)
{
    unsigned int blocksize;
    FMOD_RESULT result;

    result = dspState->functions->getblocksize(dspState, &blocksize);

    dsp_data_t *data = calloc(sizeof(dsp_data_t), 1);
    if (!data)
    {
        return FMOD_ERR_MEMORY;
    }
    dspState->plugindata = data;
    data->volumeLinear = 0.5f;
    data->lengthSamples = blocksize;

    // *8 = maximum size allowing room for 7.1.   Could ask dsp_state->functions->getspeakermode for the right speakermode to get real speaker count.
    data->buffer = malloc(blocksize * 8 * sizeof(float));
    if (!data->buffer)
    {
        return FMOD_ERR_MEMORY;
    }

    return FMOD_OK;
}

FMOD_RESULT F_CALLBACK dsp_release_callback(FMOD_DSP_STATE *dspState)
{
    if (dspState->plugindata)
    {
        dsp_data_t *data = (dsp_data_t *)dspState->plugindata;

        if (data->buffer) free(data->buffer);
        free(data);
    }

    return FMOD_OK;
}

FMOD_RESULT F_CALLBACK dsp_read_callback(FMOD_DSP_STATE *dspState, float *inBuffer, float *outBuffer, unsigned int length, int inChannels, int *outChannels) 
{
    dsp_data_t *data = dspState->plugindata;

    if (outFile == NULL) outFile = fopen("./audio-fmod.dat", "wb");

    int size = max(inChannels, *outChannels) * length;

    memcpy(outBuffer, inBuffer, size * sizeof(float));
    recordedSamples += length;

    if (inChannels != 2 || *outChannels != 2) return FMOD_OK;

    fwrite(inBuffer, sizeof(float), size, outFile);
    fflush(outFile);

    return FMOD_OK;
}

FMOD_RESULT hook_FMOD_System_init(FMOD_SYSTEM *system, int maxchannels, FMOD_INITFLAGS flags, void *extradriverdata)
{
    if (orig_FMOD_System_init == NULL) load_symbols();
    FMOD_RESULT result = orig_FMOD_System_init(system, maxchannels, flags, extradriverdata);

    if (result != FMOD_OK) return result;

    if (fn_FMOD_System_CreateDSP == NULL ||
        fn_FMOD_System_GetMasterChannelGroup == NULL ||
        fn_FMOD_ChannelGroup_AddDSP == NULL ||
        fn_FMOD_ChannelGroup_RemoveDSP == NULL ||
        fn_FMOD_DSP_Release == NULL) load_symbols();

    FMOD_DSP_DESCRIPTION desc = { 0 };
    strncpy(desc.name, "ldcatpure DSP recording", sizeof(desc.name));
    desc.version          = 0x00010000;
    desc.numinputbuffers  = 1;
    desc.numoutputbuffers = 1;
    desc.read             = dsp_read_callback; 
    desc.create           = dsp_create_callback;
    desc.release          = dsp_release_callback;

    fn_FMOD_System_GetMasterChannelGroup(system, &masterGroup);
    fn_FMOD_System_CreateDSP(system, &desc, &dsp);
    fn_FMOD_ChannelGroup_AddDSP(masterGroup, 0, dsp);

    return result;
}

FMOD_RESULT hook_FMOD_System_release(FMOD_SYSTEM *system)
{
    if (orig_FMOD_System_release == NULL) load_symbols();
    FMOD_RESULT result = orig_FMOD_System_release(system);

    if (result != FMOD_OK) return result;

    FMOD_ChannelGroup_RemoveDSP(masterGroup, dsp);
    FMOD_DSP_Release(dsp);

    return result;
}

static bool runSoundWorker = true;
static pthread_t soundWorkerThread;

static void *sound_worker(void *_)
{
    printf("Starting sound thread\n"); fflush(stdout);
    while (runSoundWorker)
    {
        // We spin while waiting since we need to respond quickly
        if (!timing_is_running()) continue;

        printf("Sound 1\n"); fflush(stdout);
        while (timing_is_sound_done());

        // Wait a frame to sync again with the video
        if (totalRecodedSamplesError >= targetRecordedSamples)
        {
            while (!timing_is_realtime_frame_done());
            totalRecodedSamplesError -= targetRecordedSamples;
            timing_sound_done();
            continue;
        }

        recordedSamples = 0;

        fn_FMOD_ChannelGroup_SetPaused(masterGroup, false);
        while (recordedSamples == 0); // Wait for data
        fn_FMOD_ChannelGroup_SetPaused(masterGroup, true);

        totalRecodedSamplesError += recordedSamples - targetRecordedSamples;

        printf("Sound Done: %i | Error: %i\n", recordedSamples, totalRecodedSamplesError); fflush(stdout);
        timing_sound_done();
    }

    fn_FMOD_ChannelGroup_SetPaused(masterGroup, false);

    return NULL;
}

void init_sound_fmod5()
{
    hook_symbol(hook_FMOD_System_init, (void **)&orig_FMOD_System_init, "_ZN4FMOD6System4initEijPv");
    hook_symbol(hook_FMOD_System_release, (void **)&orig_FMOD_System_release, "_ZN4FMOD6System7releaseEv");

    // We don't actually hook anything, just getting the original function
    hook_symbol(NULL, (void **)&fn_FMOD_System_CreateDSP, "FMOD_System_CreateDSP");
    hook_symbol(NULL, (void **)&fn_FMOD_System_GetMasterChannelGroup, "FMOD_System_GetMasterChannelGroup");
    hook_symbol(NULL, (void **)&fn_FMOD_ChannelGroup_AddDSP, "FMOD_ChannelGroup_AddDSP");
    hook_symbol(NULL, (void **)&fn_FMOD_ChannelGroup_RemoveDSP, "FMOD_ChannelGroup_RemoveDSP");
    hook_symbol(NULL, (void **)&fn_FMOD_ChannelGroup_SetPaused, "FMOD_ChannelGroup_SetPaused");
    hook_symbol(NULL, (void **)&fn_FMOD_DSP_Release, "FMOD_DSP_Release");


    pthread_create(&soundWorkerThread, NULL, sound_worker, NULL);
}

void shutdown_soundsys_fmod5()
{
    runSoundWorker = false;
    pthread_join(soundWorkerThread, NULL);
}

// libfmod.so (SHA1: 043c7a0c10705679f29f42b0f44e51245e7f8b65)
FMOD_RESULT _ZN4FMOD6System4initEijPv(FMOD_SYSTEM *system, int maxchannels, FMOD_INITFLAGS flags, void *extradriverdata)
{
    return hook_FMOD_System_init(system, maxchannels, flags, extradriverdata);
}
FMOD_RESULT _ZN4FMOD6System7releaseEv(FMOD_SYSTEM *system)
{
    return hook_FMOD_System_release(system);
}