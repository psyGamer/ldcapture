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
static const char* outfile_type = "mp4";
static codec_option_t* codec_options;
static i32 codec_options_length = 0;

i32 settings_fps() { return fps; }
i32 settings_video_width() { return video_width; }
i32 settings_video_height() { return video_height; }
i32 settings_video_bitrate() { return video_bitrate; }
i32 settings_audio_bitrate() { return audio_bitrate; }
i32 settings_audio_samplerate() { return audio_samplerate; }
const char* settings_outfile_type() { return outfile_type; }
codec_option_t* settings_codec_options() { return codec_options; }
i32 settings_codec_options_length() { return codec_options_length; }

void init_settings()
{
    if (file_exists(LDCAPTURE_CONFIG_FILE))
    {
        config_init(&config);
        config_read_file(&config, LDCAPTURE_CONFIG_FILE);

        config_lookup_int(&config, "fps", &fps);
        config_lookup_int(&config, "video_width", &video_width);
        config_lookup_int(&config, "video_height", &video_height);
        config_lookup_int(&config, "video_bitrate", &video_bitrate);
        config_lookup_int(&config, "audio_bitrate", &audio_bitrate);
        config_lookup_int(&config, "audio_samplerate", &audio_samplerate);
        config_lookup_string(&config, "outfile_type", &outfile_type);

        config_setting_t* codecOptions = config_lookup(&config, "codec_options");
        if (!codecOptions) return;

        codec_options_length = config_setting_length(codecOptions);
        codec_options = malloc(sizeof(codec_option_t) * codec_options_length);

        for (i32 i = 0; i < codec_options_length; i++)
        {
            config_setting_t* option = config_setting_get_elem(codecOptions, i);
            codec_options[i] = (codec_option_t){
                .name = strdup(option->name),
                .value = strdup(option->value.sval),
            };
        }
    }
}

void shutdown_settings()
{
    config_destroy(&config);
}