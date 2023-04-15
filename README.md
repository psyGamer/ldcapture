<h1 align="center">ldcapture - Frame perfect recording</h1>

ldcapture is a frame perfect recording tool by injecting itself into the target application.

## Important note about Anti-Cheats

Since this hijacks some internal functions, some Anti-Cheats may detect this a cheating. Use this tool at your own risk and make sure the target application doesn't use an Anti-Cheat or is fine with this.

## Compiling

```bash
$ git clone https://github.com/psyGamer/ldcapture
$ cd ldcapture
$ make release
```
The resulting shared library is located under `./bin/ldcapture.so`

## Usage

Currently there are no prebuild binaries, so you'll have to compile from source.

### Linux

`$ LD_PRELOAD="path/to/ldcapture.so" ./your-program`

## Controling ldcapture from the target application

In order to control ldcapture you can use the API functions prowided.
To get a function pointer for those API functions you can call `dlsym(_, "apiFunctionName")`. The first parameter is ignored. The second parameter is the API function name.

### List of all API functions

- `void ldcapture_StartRecording()`: Starts the recording
- `void ldcapture_StopRecording()`: Stops the recording
- `void ldcapture_StopRecordingAfter(i32)`: Stops the recording after the specifed amount of frames
- `void ldcapture_ReloadConfig()`: Reloads the config file.

## Configuring ldcapture

If a file called `ldcapture.conf` exists in your working directory, it will try to use it as a config file.

The following example config is intended for use with Celeste, but can be adapted to work with everything:

```ini
fps = 60
video_width = 1920
video_height = 1080
video_bitrate = 6500000
audio_bitrate = 128000
audio_samplerate = 48000
container_type= "mp4"
video_codec = "libx264"
audio_codec = "aac"
video_codec_options = {
    preset = "ultrafast"
}
```

- `fps`:The framerate of the recording
- `video_width`: The output video width of the recording
- `video_height`: The output video height of the recording
- `video_bitrate`: The output video bitrate of the recording
- `audio_bitrate`: The output audio bitrate of the recording
- `audio_samplerate`: The output audio samplerate of the recording
- `container_type`: Output container type as the file extension. For example: `mp4`. See: [Video container formats - Wikipedia](https://en.wikipedia.org/wiki/Comparison_of_video_container_formats#General_information)
- `video_codec`: If not empty, overwrites the output video codec. See: [Video codecs - Wikipedia](https://en.wikipedia.org/wiki/Comparison_of_video_container_formats#Video_coding_formats_support)
- `audio_codec`: If not empty, overwrites the output audio codec. See: [Audio codecs - Wikipedia](https://en.wikipedia.org/wiki/Comparison_of_video_container_formats#Audio_coding_formats_support)
- `video_codec_options`: Set of codec options specific it the specified codec. See the codecs documentation for the options and the above example for the formatting.
- `audio_codec_options`: Set of codec options specific it the specified codec. See the codecs documentation for the options and the above example for the formatting.

## Supported platforms

Currently only Linux with OpenGL X11 and FMOD is supported.

## Using ldcapture with Celeste

If you want to use ldcapture with Celeste specifically you should use the [.NET Core version of Everest](https://github.com/Popax21/Everest) since the .NET Framework version causes some issues with performance and audio desyncs.

Additionally, you should install the [ldcapture-CelesteInterop](https://github.com/Popax21/Everest) mod, which provides easy access to the API functions via the debug console.

### Linux

When starting Celeste, you should specify the `LD_LIBRARY_PATH` to avoid Celeste restarting, which might cause issues:

`$ LD_PRELOAD="path/to/ldcapture.so" LD_LIBRARY_PATH="./lib64-linux" ./Celeste`

## Suport

If you need help setting this up, have a features request or a bug report, please open an issue here. Alternativly, you can DM me on Discord or ping me in the [Celeste Discord](https://discord.gg/celeste): psyGamer#8442

## Credits

This project was mainly inspired by [.kkapture](https://github.com/DemoJameson/kkapture).
