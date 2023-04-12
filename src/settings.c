#include "base.h"
#include "settings.h"
#include "filesystem.h"

#include <libconfig.h>

static config_t config;

static i32 fps = 60;
static i32 video_width = 1920;
static i32 video_height = 1080;
static i32 video_bitrate = 1000000;
static i32 audio_bitrate = 64000;
static i32 audio_samplerate = 48000;

static bool overwrite_video_codec = false;
static bool overwrite_audio_codec = false;
static const char* video_codec = "";
static const char* audio_codec = "";
static const char* container_type = "mp4";

static codec_option_t* video_codec_options = NULL;
static codec_option_t* audio_codec_options = NULL;
static i32 video_codec_options_length = 0;
static i32 audio_codec_options_length = 0;

i32 settings_fps() { return fps; }
i32 settings_video_width() { return video_width; }
i32 settings_video_height() { return video_height; }
i32 settings_video_bitrate() { return video_bitrate; }
i32 settings_audio_bitrate() { return audio_bitrate; }
i32 settings_audio_samplerate() { return audio_samplerate; }

bool settings_overwrite_video_codec() { return overwrite_video_codec;}
bool settings_overwrite_audio_codec() { return overwrite_audio_codec; }
const char* settings_video_codec() { return video_codec; }
const char* settings_audio_codec() { return audio_codec; }
const char* settings_container_type() { return container_type; }

codec_option_t* settings_video_codec_options() { return video_codec_options; }
codec_option_t* settings_audio_codec_options() { return audio_codec_options; }
i32 settings_video_codec_options_length() { return video_codec_options_length; }
i32 settings_audio_codec_options_length() { return audio_codec_options_length; }

static void readCodecOptions(config_setting_t* configSetting, codec_option_t** codecOptions, i32* codecOptionsLength)
{
    *codecOptionsLength = config_setting_length(configSetting);
    free(*codecOptions);
    *codecOptions = malloc(sizeof(codec_option_t) * *codecOptionsLength);

    for (i32 i = 0; i < *codecOptionsLength; i++)
    {
        config_setting_t* option = config_setting_get_elem(configSetting, i);
        (*codecOptions)[i] = (codec_option_t){
            .name = strdup(option->name),
            .value = strdup(option->value.sval),
        };
    }
}

void settings_reload()
{
    if (file_exists(LDCAPTURE_CONFIG_FILE))
    {
        config_read_file(&config, LDCAPTURE_CONFIG_FILE);

        config_lookup_int(&config, "fps", &fps);
        config_lookup_int(&config, "video_width", &video_width);
        config_lookup_int(&config, "video_height", &video_height);
        config_lookup_int(&config, "video_bitrate", &video_bitrate);
        config_lookup_int(&config, "audio_bitrate", &audio_bitrate);
        config_lookup_int(&config, "audio_samplerate", &audio_samplerate);

        config_lookup_string(&config, "video_codec", &video_codec);
        config_lookup_string(&config, "audio_codec", &audio_codec);
        config_lookup_string(&config, "container_type", &container_type);

        if (video_codec)
            overwrite_audio_codec = strlen(video_codec) != 0;
        if (audio_codec)
            overwrite_audio_codec = strlen(audio_codec) != 0;

        config_setting_t* videoCodecOptions = config_lookup(&config, "video_codec_options");
        config_setting_t* audioCodecOptions = config_lookup(&config, "audio_codec_options");

        if (videoCodecOptions)
            readCodecOptions(videoCodecOptions, &video_codec_options, &video_codec_options_length);
        if (audioCodecOptions)
            readCodecOptions(audioCodecOptions, &audio_codec_options, &audio_codec_options_length);
    }
}

void init_settings()
{
    config_init(&config);
    settings_reload();
}

void shutdown_settings()
{
    config_destroy(&config);
}