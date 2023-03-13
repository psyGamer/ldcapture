<h1 align="center">ldcapture - Frame perfect recording</h1>

ldcapture is a frame perfect recording tool by injecting itself into the target application.

## Important note about Anti-Cheats

Since this hijacks some internal functions, some Anti-Cheats may detect this a cheating. Use this tool at your own risk and make sure the target application doesn't use an Anti-Cheat or is fine with this.

## Compiling

```bash
$ git clone https://github.com/psyGamer/ldcapture
$ cd ldcapture
$ make
```
The resulting shared library is located under `./bin/ldcapture.so`

## Usage

Currently there are no prebuild binaries, so you'll have to compile from source.

### Linux

`$ LD_PRELOAD="path/to/ldcapture.so" ./your-program`

## Controling ldcapture from the target application

In order to control ldcapture you can use the API functions prowided.
To get a function pointer for those API functions you can call `dlsym(_, apiFunctionName)` the first parameter is ignored. The second parameter is the API function name.

If you can't use this function pointer to invoke it, you can prefix the `apiFunctionName` with `INVOKE_` in order to invoke the funcion directly. This will return `NULL` from `dlsym`.

### List of all API functions
- `void ldcapture_StartRecording()`
- `void ldcapture_StopRecording()`

## Supported platforms

Currently only Linux with OpenGL X11 and FMOD is supported.


## Using ldcapture with Celeste

If you want to use ldcapture with Celeste specifically you should use the [.NET Core version of Everest](https://github.com/Popax21/Everest) since the .NET Framework version causes some issues with performance and audio desyncs.

### Starting Celeste

You should specify the `LD_LIBRARY_PATH` to avoid Celeste restarting, which might cause issues.

#### Linux

`$ LD_PRELOAD="path/to/ldcapture.so" LD_LIBRARY_PATH="./lib64-linux ./Celeste"`

### Starting a recording

Currently the only way to start a recording is by specifying using the [C# Debug Console]() to use the API functions.
Enter the following in the normal debug console or within a TAS start it with `console`.

#### Start the recording

Console: `evalcs System.Runtime.InteropServices.NativeLibrary.GetExport(new IntPtr(1), "INVOKE_ldcapture_StartRecording");`

TAS: `console evalcs System.Runtime.InteropServices.NativeLibrary.GetExport(new IntPtr(1), "INVOKE_ldcapture_StartRecording");`

#### Stop the recording

Console: `evalcs System.Runtime.InteropServices.NativeLibrary.GetExport(new IntPtr(1), "INVOKE_ldcapture_StopRecording");`

TAS: `console evalcs System.Runtime.InteropServices.NativeLibrary.GetExport(new IntPtr(1), "INVOKE_ldcapture_StopRecording");`

## Credits

This project was mainly inspired by [.kkapture](https://github.com/DemoJameson/kkapture).
