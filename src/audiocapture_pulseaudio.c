#ifdef DEPRECATED_SOUND_PULSEAUDIO

#include "base.h"
#include "timing.h"
#include "util/pulse_wrapper.h"

typedef struct combine_module_t
{
    u32 moduleIndex;
    u32 ownerModuleIndex;
} combine_module_t;

#define i_key u32
#define i_val combine_module_t*
#define i_tag module
#include <stc/cmap.h>

#define UNUSED_PARAMETER(param) (void)param

#define POTENTIAL_CLIENT_INDICES_LENGTH 32

typedef struct pulse_data_t
{
    pa_stream* stream;

    /* client info */
    char* clientName;
    u32 clientIndex;
    size_t currentPotentialClientIndex;
    u32 potentialClientIndices[POTENTIAL_CLIENT_INDICES_LENGTH];
    // u32 clientIndex;
    // char *client;

    /* sink input info */
    u32 sinkInputIndex;
    u32 sinkInputSinkIndex;
    char* sinkInputSinkName;

    i32 moveSuccess;

    /* combine module info */
    // maps from sink input sink index to combine module index
    cmap_module* combineModules;
    i32 loadModuleSuccess;
    u32 nextOwnerModuleIndex;

    /* server info */
    pa_sample_format_t format;
    u32 samplesPerSecond;
    u32 bytesPerFrame;
    u8 channels;
    u64 firstTimestamp;

    /* statistics */
    u32 packets;
    u64 frames;
} pulse_data_t;

static void load_new_module(pulse_data_t* data);
static bool move_to_combine_module(pulse_data_t* data);
static void pulse_stop_recording(pulse_data_t* data);

void update_sink_input_info_callback(pa_context* c, const pa_sink_input_info* i, i32 eol, void* userdata)
{
    pulse_data_t* data = (pulse_data_t*)userdata;

    if (eol || i->index == PA_INVALID_INDEX)
    {
        pulse_signal(0);
        return;
    }
    TRACE("update sink input: Name:%s client:%i index:%i ClientIndex:%i", i->name, i->client, i->index, data->clientIndex);
    // Checking if new sink corresponds to our client
    if (data->clientIndex != PA_INVALID_INDEX && data->clientIndex == i->client)
    {
        data->sinkInputIndex = i->index;

        if (data->sinkInputSinkIndex == PA_INVALID_INDEX)
        {
            data->sinkInputSinkIndex = i->sink;
        }

        // Move new sink-input to the module
        TRACE("   load_new_module");
        load_new_module(data);
        TRACE("   move_to_combine_module");
        move_to_combine_module(data);
    }

    pulse_signal(0);
}

static void sink_event_callback(pa_context* c, pa_subscription_event_type_t t, u32 idx, void* userdata)
{
    pulse_data_t* data = (pulse_data_t*)userdata;

    if ((t & PA_SUBSCRIPTION_EVENT_FACILITY_MASK) == PA_SUBSCRIPTION_EVENT_SINK_INPUT)
    {
        if ((t & PA_SUBSCRIPTION_EVENT_TYPE_MASK) == PA_SUBSCRIPTION_EVENT_NEW)
        {
            TRACE("new sink-input added %d", idx);

            // Directly calling pulse function because pulse calls
            // this callback on a helper thread and not the main
            // thread
            // Check if sink-input corresponds to the client
            pa_operation* op = pa_context_get_sink_input_info(c, idx, update_sink_input_info_callback, data);
            if (!op)
            {
                pulse_signal(0);
                return;
            }

            pa_operation_unref(op);

        }
        else if ((t & PA_SUBSCRIPTION_EVENT_TYPE_MASK) == PA_SUBSCRIPTION_EVENT_REMOVE)
        {
            TRACE("sink-input removed %d\n", idx);

            // Check if sink-input has been removed
            if (data->sinkInputIndex == idx)
            {
                data->sinkInputIndex = PA_INVALID_INDEX;
            }
        }
    }

    pulse_signal(0);
}

static void move_sink_input_callback(pa_context* c, i32 success, void* userdata)
{
    pulse_data_t* data = (pulse_data_t*)userdata;
    data->moveSuccess = success;

    pulse_signal(0);
}

static void get_client_index_callback(pa_context* c, const pa_client_info* i, i32 eol, void* userdata)
{
    pulse_data_t* data = (pulse_data_t*)userdata;

    if (eol || i->index == PA_INVALID_INDEX)
    {
        pulse_signal(0);
        return;
    }
    TRACE("name: %s, index: %i", i->name, i->index);
    if (strcmp(data->clientName, i->name) == 0)
    {
        TRACE("Found potential client index: %d for %s", i->index, i->name);
        data->potentialClientIndices[data->currentPotentialClientIndex++] = i->index;
    }
}

static void get_sink_input_callback(pa_context* c, const pa_sink_input_info* info, i32 eol, void* userdata)
{
    pulse_data_t* data = (pulse_data_t*)userdata;

    if (eol || info->index == PA_INVALID_INDEX || data->sinkInputIndex != PA_INVALID_INDEX)
    {
        pulse_signal(0);
        return;
    }
    TRACE("Client: %s, ClientIndex: %i", data->clientName, data->clientIndex);
    TRACE("Name: %s | Index: %i", info->name, info->client);
    for (size_t i = 0; i < data->currentPotentialClientIndex; i++)
    {
        if (data->potentialClientIndices[i] == info->client)
        {
            TRACE("found sink-input %s with index %d and sink index %d", info->name, info->index, info->sink);
            data->clientIndex = data->potentialClientIndices[i];
            data->sinkInputIndex = info->index;
            data->sinkInputSinkIndex = info->sink;
        }
    }
}

static void get_sink_name_by_index_callback(pa_context* c, const pa_sink_info* i, i32 eol, void* userdata)
{
    pulse_data_t* data = (pulse_data_t*)userdata;

    if (eol || i->index == PA_INVALID_INDEX)
    {
        pulse_signal(0);
    } else
    {
        data->sinkInputSinkName = strdup(i->name);
    }
}

static void load_new_module_callback(pa_context* c, u32 idx, void* userdata)
{
    pulse_data_t* data = (pulse_data_t*)userdata;

    TRACE("new module idx %d", idx);
    if (idx != PA_INVALID_INDEX)
    {
        data->nextOwnerModuleIndex = idx;
    }
    else
    {
        data->loadModuleSuccess = 0;
    }

    pulse_signal(0);
}

static void get_sink_id_by_owner_callback(pa_context* c, const pa_sink_info* i, i32 eol, void* userdata)
{
    pulse_data_t* data = (pulse_data_t*)userdata;

    if (eol || i->index == PA_INVALID_INDEX)
    {
        pulse_signal(0);
        return;
    }
    TRACE("Combine module callback: i->index:%i sinkInputSinkIndex:%i nextOwnerModuleIndex:%i", i->index, data->sinkInputSinkIndex, data->nextOwnerModuleIndex);
    if (data->nextOwnerModuleIndex == i->owner_module)
    {
        combine_module_t* newModule = malloc(sizeof(combine_module_t));
        newModule->moduleIndex = i->index;
        newModule->ownerModuleIndex = data->nextOwnerModuleIndex;
        TRACE("New combine module: moduleIndex:%i ownerModuleIndex:%i sinkInputSinkIndex:%i", newModule->moduleIndex, newModule->ownerModuleIndex, data->sinkInputSinkIndex);
        cmap_module_insert(data->combineModules, data->sinkInputSinkIndex, newModule);
    }
}

static void unload_module_callback(pa_context* c, i32 success, void* userdata)
{
    TRACE("module unload success: %d", success);
    pulse_signal(0);
}

static bool restore_sink(pulse_data_t* data)
{
    // Move existing sink-input back to old sink
    if (data->sinkInputIndex != PA_INVALID_INDEX && data->sinkInputSinkIndex != PA_INVALID_INDEX)
    {
        TRACE("moving sink input %d to sink %d", data->sinkInputIndex, data->sinkInputSinkIndex);
        pulse_move_sink_input(data->sinkInputIndex, data->sinkInputSinkIndex, move_sink_input_callback, data);
        TRACE("move done.");

        if (!data->moveSuccess)
        {
            TRACE("move not successful");
            return false;
        }

        return true;
    }
    return true;
}


static bool get_sink_input(pulse_data_t* data)
{
    // Find sink-input with corresponding client
    data->sinkInputIndex = PA_INVALID_INDEX;
    data->sinkInputSinkIndex = PA_INVALID_INDEX;
    TRACE("finding sink-input for the corresponding client");
    pulse_get_sink_input_info_list(get_sink_input_callback, data);

    if (data->sinkInputIndex == PA_INVALID_INDEX)
    {
        TRACE("sink-input not found");
        return false;
    }
    return true;
}

static void load_new_module(pulse_data_t* data)
{
    TRACE("looking for %d", data->sinkInputSinkIndex);

    if (!cmap_module_contains(data->combineModules, data->sinkInputSinkIndex)) {
        // A module for this sink has not yet been loaded
        TRACE("module for sink input sink index %d has not been loaded", data->sinkInputSinkIndex);

        TRACE("getting sink name");
        // Get sink name from index
        pulse_get_sink_name_by_index(data->sinkInputSinkIndex, get_sink_name_by_index_callback, data);

        // Load the new module
        data->loadModuleSuccess = 1;
        char args[1024];
        sprintf(args, "sink_name=ldcaptureRecord%s slaves=%s", data->sinkInputSinkName, data->sinkInputSinkName);

        TRACE("loading new module with args %s", args);
        data->nextOwnerModuleIndex = PA_INVALID_INDEX;
        pulse_load_new_module("module-combine-sink", args, load_new_module_callback, data);

        if (!data->loadModuleSuccess)
        {
            TRACE("Unable to load module");
            return;
        }
        TRACE("successfully loaded new module");

        // Lookup module id with owner_module id
        TRACE("looking up new module id with owner module id");
        pulse_get_sink_list(get_sink_id_by_owner_callback, data);
    }
}

static bool move_to_combine_module(pulse_data_t* data)
{
    // Move sink-input to new module
    data->moveSuccess = 1;
    cmap_module_iter iter = cmap_module_find(data->combineModules, data->sinkInputSinkIndex);

    if (!cmap_module_contains(data->combineModules, data->sinkInputSinkIndex))
    {
        TRACE("no combine module found");
        return false;
    }

    u32 combineModuleIndex = iter.ref->second->moduleIndex;
    TRACE("moving sink input %d to sink index %d", data->sinkInputIndex, combineModuleIndex);
    pulse_move_sink_input(data->sinkInputIndex, combineModuleIndex, move_sink_input_callback, data);

    if (!data->moveSuccess) 
    {
        TRACE("Unable to move sink input to combine module");
        return false;
    }

    return true;
}

static FILE* out_file = NULL;

/**
 * Callback for pulse which gets executed when new audio data is available
 *
 * @warning The function may be called even after disconnecting the stream
 */
static void pulse_stream_read(pa_stream* p, size_t nbytes, void* userdata)
{
    UNUSED_PARAMETER(p);
    UNUSED_PARAMETER(nbytes);
    pulse_data_t* data = (pulse_data_t*)userdata;

    TRACE("Getting data");

    int16_t* paData = NULL;
    size_t bytes = 0;
    if (pa_stream_peek(data->stream, (const void**)&paData, &bytes) != 0)
    {
        TRACE("Failed to peek at stream data");
        return;
    }

    // No data
    if (paData == NULL && bytes == 0) 
    {
        TRACE("No data");
        return;
    }
    // Hole in buffer
    if (paData == NULL && bytes > 0)
    {
        TRACE("Hole in buffer");
        if (pa_stream_drop(data->stream) != 0) {
            TRACE("Failed to drop a hole! (Sounds weird, doesn't it?)");
            return;
        }
    }

    TRACE("Got %zu bytes", bytes);

    if (out_file == NULL) out_file = fopen("./audio.dat", "wb");

    if (timing_is_running())
    {
        fwrite(paData, 1, bytes, out_file);
        fflush(out_file);
    }

    TRACE("Dropping");
    if (pa_stream_drop(data->stream) != 0)
    {
        TRACE("Failed to drop data after peeking.");
        return;
    }

    // const void* frames;
    // size_t bytes;

    // if (!data->stream)
    //     goto exit;

    // pa_stream_peek(data->stream, &frames, &bytes);

    // // check if we got data
    // if (!bytes)
    //     goto exit;

    // if (!frames)
    // {
    //     TRACE("Got audio hole of %u bytes", (unsigned int)bytes);
    //     pa_stream_drop(data->stream);
    //     goto exit;
    // }

    // // TODO: Use data
    // TRACE("GOT DATA FROM PULSEAUDIO!!!!!!");

    // struct obs_source_audio out;
    // out.speakers = data->speakers;
    // out.samples_per_sec = data->samples_per_sec;
    // out.format = pulse_to_obs_audio_format(data->format);
    // out.data[0] = (uint8_t*)frames;
    // out.frames = bytes / data->bytes_per_frame;
    // out.timestamp = get_sample_time(out.frames, out.samples_per_sec);

    // if (!data->first_ts)
    // 	data->first_ts = out.timestamp + STARTUP_TIMEOUT_NS;

    // if (out.timestamp > data->first_ts)
    // 	obs_source_output_audio(data->source, &out);

    // data->packets++;
    // data->frames += out.frames;

    // pa_stream_drop(data->stream);
// exit:
//     pulse_signal(0);
}

static pa_channel_map pulse_channel_map()
{
    pa_channel_map ret;

    ret.map[0] = PA_CHANNEL_POSITION_FRONT_LEFT;
    ret.map[1] = PA_CHANNEL_POSITION_FRONT_RIGHT;
    ret.map[2] = PA_CHANNEL_POSITION_FRONT_CENTER;
    ret.map[3] = PA_CHANNEL_POSITION_LFE;
    ret.map[4] = PA_CHANNEL_POSITION_REAR_LEFT;
    ret.map[5] = PA_CHANNEL_POSITION_REAR_RIGHT;
    ret.map[6] = PA_CHANNEL_POSITION_SIDE_LEFT;
    ret.map[7] = PA_CHANNEL_POSITION_SIDE_RIGHT;

    // TODO: Not just assume stereo
    ret.channels = 2;

    // switch (layout) {
    // case SPEAKERS_MONO:
    // 	ret.channels = 1;
    // 	ret.map[0] = PA_CHANNEL_POSITION_MONO;
    // 	break;

    // case SPEAKERS_STEREO:
    // 	ret.channels = 2;
    // 	break;

    // case SPEAKERS_2POINT1:
    // 	ret.channels = 3;
    // 	ret.map[2] = PA_CHANNEL_POSITION_LFE;
    // 	break;

    // case SPEAKERS_4POINT0:
    // 	ret.channels = 4;
    // 	ret.map[3] = PA_CHANNEL_POSITION_REAR_CENTER;
    // 	break;

    // case SPEAKERS_4POINT1:
    // 	ret.channels = 5;
    // 	ret.map[4] = PA_CHANNEL_POSITION_REAR_CENTER;
    // 	break;

    // case SPEAKERS_5POINT1:
    // 	ret.channels = 6;
    // 	break;

    // case SPEAKERS_7POINT1:
    // 	ret.channels = 8;
    // 	break;

    // case SPEAKERS_UNKNOWN:
    // default:
    // 	ret.channels = 0;
    // 	break;
    // }

    return ret;
}

static void pulse_source_info(pa_context* c, const pa_source_info* i, i32 eol, void* userdata)
{
    UNUSED_PARAMETER(c);
    pulse_data_t* data = (pulse_data_t*)userdata;
    // An error occured
    if (eol < 0)
    {
        data->format = PA_SAMPLE_INVALID;
        pulse_signal(0);
        return;
    }
    // Terminating call for multi instance callbacks
    if (eol > 0)
    {
        pulse_signal(0);
        return;
    }

    pa_proplist* proplist = i->proplist;

    TRACE("Audio format: %s, %u Hz, %u channels",
        pa_sample_format_to_string(i->sample_spec.format),
        i->sample_spec.rate, i->sample_spec.channels);

    pa_sample_format_t format = i->sample_spec.format;
    // if (pulse_to_obs_audio_format(format) == AUDIO_FORMAT_UNKNOWN)
    // {
    // 	format = PA_SAMPLE_FLOAT32LE;

    // 	TRACE("Sample format %s not supported by OBS, using %s instead for recording",
    // 	     pa_sample_format_to_string(i->sample_spec.format),
    // 	     pa_sample_format_to_string(format));
    // }

    uint8_t channels = i->sample_spec.channels;
    // if (pulse_channels_to_obs_speakers(channels) == SPEAKERS_UNKNOWN)
    // {
    // 	channels = 2;

    // 	TRACE("%c channels not supported by OBS, using %c instead for recording", i->sample_spec.channels, channels);
    // }

    data->format = format;
    data->samplesPerSecond = i->sample_spec.rate;
    data->channels = channels;
}

static int_fast32_t pulse_start_recording(pulse_data_t* data)
{
    char monitorName[1024];
    sprintf(monitorName, "ldcaptureRecord%s.monitor", data->sinkInputSinkName);

    if (pulse_get_source_info_by_name(pulse_source_info, monitorName, data) < 0)
    {
        TRACE("Unable to get client info !");
        return -1;
    }

    if (data->format == PA_SAMPLE_INVALID)
    {
        TRACE("An error occurred while getting the client info!");
        return -1;
    }

    pa_sample_spec spec;
    spec.format = data->format;
    spec.rate = data->samplesPerSecond;
    spec.channels = data->channels;

    if (!pa_sample_spec_valid(&spec))
    {
        TRACE("Sample spec is not valid");
        return -1;
    }

    // data->speakers = pulse_channels_to_obs_speakers(spec.channels);
    data->bytesPerFrame = pa_frame_size(&spec);

    pa_channel_map channelMap = pulse_channel_map();

    data->stream = pulse_stream_new("ldcaptureStream", &spec, &channelMap);
    if (!data->stream)
    {
        TRACE("Unable to create stream");
        return -1;
    }

    pulse_lock();
    pa_stream_set_read_callback(data->stream, pulse_stream_read, (void*)data);
    pulse_unlock();

    pa_buffer_attr attr;
    attr.fragsize = pa_usec_to_bytes(25000, &spec);
    attr.maxlength = (u32)-1;
    attr.minreq = (u32)-1;
    attr.prebuf = (u32)-1;
    attr.tlength = (u32)-1;

    pa_stream_flags_t flags = PA_STREAM_ADJUST_LATENCY;

    pulse_lock();
    int_fast32_t ret = pa_stream_connect_record(data->stream, monitorName, &attr, flags);
    pulse_unlock();
    if (ret < 0)
    {
        pulse_stop_recording(data);
        TRACE("Unable to connect to stream");
        return -1;
    }

    TRACE("Started recording from '%s'", data->clientName);
    return 0;
}

static void pulse_stop_recording(pulse_data_t* data)
{
    if (data->stream)
    {
        pulse_lock();
        pa_stream_disconnect(data->stream);
        pa_stream_unref(data->stream);
        data->stream = NULL;
        pulse_unlock();
    }

    TRACE("Stopped recording from '%s'", data->clientName);
    TRACE("Got %lu packets with %lu frames", data->packets, data->frames);

    data->firstTimestamp = 0;
    data->packets = 0;
    data->frames = 0;
}

static pulse_data_t* gdata = NULL;
static bool init1 = false;
static bool init2 = false;
static bool shut1 = false;
static bool shut2 = false;

static void audiocapture_pulseaudio_update(void* userdata)
{
    if (init2) return;

    pulse_data_t* data = (pulse_data_t*)userdata;
    bool restart = false;

    restart = true;

    data->clientName = "Celeste";
    // data->clientName = "Firefox";

    /*
    const char* newClient;

    newClient = obs_data_get_string(settings, "client");
    printf("new client: %s\n", new_client);
    if (newClient && (!data->client || strcmp(data->client, newClient) != 0)) {
        printf("need to restart\n");
        if (data->client)
            bfree(data->client);
        data->client = bstrdup(new_client);
        data->clientIndex = PA_INVALID_INDEX;

        restart = true;
    }
    */

    if (!restart)
        return;

    if (!restore_sink(data))
    {
        TRACE("FAILED UPDATE: restore_sink");
        return;
    }

    // Find client idx
    data->clientIndex = PA_INVALID_INDEX;
    data->currentPotentialClientIndex = 0;
    for (i32 i = 0; i < POTENTIAL_CLIENT_INDICES_LENGTH; i++) data->potentialClientIndices[i] = PA_INVALID_INDEX;
    pulse_get_client_info_list(get_client_index_callback, data);
    TRACE("searching for client index");

    if (data->currentPotentialClientIndex == 0)
    {
        TRACE("client not found");
        return;
    }

    if (!get_sink_input(data))
    {
        TRACE("FAILED UPDATE: get_sink_input");
        return;
    }

    load_new_module(data);

    if (!move_to_combine_module(data))
    {
        TRACE("FAILED UPDATE: move_to_combine_module");
        return;
    }

    if (data->stream)
    {
        TRACE("stopping recording");
        pulse_stop_recording(data);
    }

    // Start recording from the combine module
    TRACE("starting recording");
    pulse_start_recording(data);

    TRACE("SUCCESS UPDATE");
    init2 = true;
}

void init_audiocapture_pulseaudio()
{
    TRACE("-------------");
    if (!init1)
    {
        gdata = malloc(sizeof(pulse_data_t));
        memset(gdata, 0, sizeof(pulse_data_t));
        gdata->clientIndex = PA_INVALID_INDEX;
        gdata->sinkInputIndex = PA_INVALID_INDEX;
        gdata->sinkInputSinkIndex = PA_INVALID_INDEX;
        gdata->combineModules = malloc(sizeof(cmap_module));
        memset(gdata->combineModules, 0, sizeof(cmap_module));
        
        TRACE("initting from create");
        pulse_init();
        pulse_subscribe_sink_input_events(sink_event_callback, gdata);
        TRACE("finished initting from create now calling update");
        init1 = true;
    }
    TRACE("calling update");
    audiocapture_pulseaudio_update(gdata);
    TRACE("finished updating from create");
}

void shutdown_audiocapture_pulseaudio()
{
    TRACE("Trying to shut down");
    if (shut1 || shut2) return;
    TRACE("Starting shut down");
    shut1 = true;

    pulse_data_t* data = (pulse_data_t*)gdata;

    if (data->stream) pulse_stop_recording(data);

    // Move existing sink-input back to old sink
    if (data->sinkInputIndex != PA_INVALID_INDEX && data->sinkInputSinkIndex != PA_INVALID_INDEX)
    {
        if (get_sink_input(data))
        {
            TRACE("moving sink input %d to sink %d", data->sinkInputIndex, data->sinkInputSinkIndex);
            pulse_move_sink_input(data->sinkInputIndex, data->sinkInputSinkIndex, move_sink_input_callback, data);

            if (!data->moveSuccess)
            {
                TRACE("move not successful");
            }
        }
    }

    // Unload loaded modules
    for (cmap_module_iter it = cmap_module_begin(data->combineModules); it.ref; cmap_module_next(&it))
    {
        pulse_unload_module(it.ref->second->ownerModuleIndex, unload_module_callback, 0);
        free(it.ref->second);
    }

    pulse_unref();

    cmap_module_drop(data->combineModules);
    free(data->combineModules);

    // if (data->client) free(data->client);
    if (data->sinkInputSinkName) free(data->sinkInputSinkName);

    free(data);
    shut2 = true;
    TRACE("Successful shut down");
}

bool is_shutdown_done_audiocapture_pulseaudio()
{
    return shut2;
}

#endif // DEPRECATED_SOUND_PULSEAUDIO