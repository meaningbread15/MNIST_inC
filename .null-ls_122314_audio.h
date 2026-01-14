/*
Audio playback and capture library. Choice of public domain or MIT-0. See license statements at the end of this file.
miniaudio - v0.11.23 - 2025-09-11

David Reid - mackron@gmail.com

Website:       https://miniaud.io
Documentation: https://miniaud.io/docs
GitHub:        https://github.com/mackron/miniaudio
*/

/*
1. Introduction
===============
To use miniaudio, just include "miniaudio.h" like any other header and add "miniaudio.c" to your
source tree. If you don't want to add it to your source tree you can compile and link to it like
any other library. Note that ABI compatibility is not guaranteed between versions, even with bug
fix releases, so take care if compiling as a shared object.

miniaudio includes both low level and high level APIs. The low level API is good for those who want
to do all of their mixing themselves and only require a light weight interface to the underlying
audio device. The high level API is good for those who have complex mixing and effect requirements.

In miniaudio, objects are transparent structures. Unlike many other libraries, there are no handles
to opaque objects which means you need to allocate memory for objects yourself. In the examples
presented in this documentation you will often see objects declared on the stack. You need to be
careful when translating these examples to your own code so that you don't accidentally declare
your objects on the stack and then cause them to become invalid once the function returns. In
addition, you must ensure the memory address of your objects remain the same throughout their
lifetime. You therefore cannot be making copies of your objects.

A config/init pattern is used throughout the entire library. The idea is that you set up a config
object and pass that into the initialization routine. The advantage to this system is that the
config object can be initialized with logical defaults and new properties added to it without
breaking the API. The config object can be allocated on the stack and does not need to be
maintained after initialization of the corresponding object.


1.1. Low Level API
------------------
The low level API gives you access to the raw audio data of an audio device. It supports playback,
capture, full-duplex and loopback (WASAPI only). You can enumerate over devices to determine which
physical device(s) you want to connect to.

The low level API uses the concept of a "device" as the abstraction for physical devices. The idea
is that you choose a physical device to emit or capture audio from, and then move data to/from the
device when miniaudio tells you to. Data is delivered to and from devices asynchronously via a
callback which you specify when initializing the device.

When initializing the device you first need to configure it. The device configuration allows you to
specify things like the format of the data delivered via the callback, the size of the internal
buffer and the ID of the device you want to emit or capture audio from.

Once you have the device configuration set up you can initialize the device. When initializing a
device you need to allocate memory for the device object beforehand. This gives the application
complete control over how the memory is allocated. In the example below we initialize a playback
device on the stack, but you could allocate it on the heap if that suits your situation better.

    ```c
    void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
    {
        // In playback mode copy data to pOutput. In capture mode read data from pInput. In full-duplex mode, both
        // pOutput and pInput will be valid and you can move data from pInput into pOutput. Never process more than
        // frameCount frames.
    }

    int main()
    {
        ma_device_config config = ma_device_config_init(ma_device_type_playback);
        config.playback.format   = ma_format_f32;   // Set to ma_format_unknown to use the device's native format.
        config.playback.channels = 2;               // Set to 0 to use the device's native channel count.
        config.sampleRate        = 48000;           // Set to 0 to use the device's native sample rate.
        config.dataCallback      = data_callback;   // This function will be called when miniaudio needs more data.
        config.pUserData         = pMyCustomData;   // Can be accessed from the device object (device.pUserData).

        ma_device device;
        if (ma_device_init(NULL, &config, &device) != MA_SUCCESS) {
            return -1;  // Failed to initialize the device.
        }

        ma_device_start(&device);     // The device is sleeping by default so you'll need to start it manually.

        // Do something here. Probably your program's main loop.

        ma_device_uninit(&device);
        return 0;
    }
    ```

In the example above, `data_callback()` is where audio data is written and read from the device.
The idea is in playback mode you cause sound to be emitted from the speakers by writing audio data
to the output buffer (`pOutput` in the example). In capture mode you read data from the input
buffer (`pInput`) to extract sound captured by the microphone. The `frameCount` parameter tells you
how many frames can be written to the output buffer and read from the input buffer. A "frame" is
one sample for each channel. For example, in a stereo stream (2 channels), one frame is 2
samples: one for the left, one for the right. The channel count is defined by the device config.
The size in bytes of an individual sample is defined by the sample format which is also specified
in the device config. Multi-channel audio data is always interleaved, which means the samples for
each frame are stored next to each other in memory. For example, in a stereo stream the first pair
of samples will be the left and right samples for the first frame, the second pair of samples will
be the left and right samples for the second frame, etc.

The configuration of the device is defined by the `ma_device_config` structure. The config object
is always initialized with `ma_device_config_init()`. It's important to always initialize the
config with this function as it initializes it with logical defaults and ensures your program
doesn't break when new members are added to the `ma_device_config` structure. The example above
uses a fairly simple and standard device configuration. The call to `ma_device_config_init()` takes
a single parameter, which is whether or not the device is a playback, capture, duplex or loopback
device (loopback devices are not supported on all backends). The `config.playback.format` member
sets the sample format which can be one of the following (all formats are native-endian):

    +---------------+----------------------------------------+---------------------------+
    | Symbol        | Description                            | Range                     |
    +---------------+----------------------------------------+---------------------------+
    | ma_format_f32 | 32-bit floating point                  | [-1, 1]                   |
    | ma_format_s16 | 16-bit signed integer                  | [-32768, 32767]           |
    | ma_format_s24 | 24-bit signed integer (tightly packed) | [-8388608, 8388607]       |
    | ma_format_s32 | 32-bit signed integer                  | [-2147483648, 2147483647] |
    | ma_format_u8  | 8-bit unsigned integer                 | [0, 255]                  |
    +---------------+----------------------------------------+---------------------------+

The `config.playback.channels` member sets the number of channels to use with the device. The
channel count cannot exceed MA_MAX_CHANNELS. The `config.sampleRate` member sets the sample rate
(which must be the same for both playback and capture in full-duplex configurations). This is
usually set to 44100 or 48000, but can be set to anything. It's recommended to keep this between
8000 and 384000, however.

Note that leaving the format, channel count and/or sample rate at their default values will result
in the internal device's native configuration being used which is useful if you want to avoid the
overhead of miniaudio's automatic data conversion.

In addition to the sample format, channel count and sample rate, the data callback and user data
pointer are also set via the config. The user data pointer is not passed into the callback as a
parameter, but is instead set to the `pUserData` member of `ma_device` which you can access
directly since all miniaudio structures are transparent.

Initializing the device is done with `ma_device_init()`. This will return a result code telling you
what went wrong, if anything. On success it will return `MA_SUCCESS`. After initialization is
complete the device will be in a stopped state. To start it, use `ma_device_start()`.
Uninitializing the device will stop it, which is what the example above does, but you can also stop
the device with `ma_device_stop()`. To resume the device simply call `ma_device_start()` again.
Note that it's important to never stop or start the device from inside the callback. This will
result in a deadlock. Instead you set a variable or signal an event indicating that the device
needs to stop and handle it in a different thread. The following APIs must never be called inside
the callback:

    ```c
    ma_device_init()
    ma_device_init_ex()
    ma_device_uninit()
    ma_device_start()
    ma_device_stop()
    ```

You must never try uninitializing and reinitializing a device inside the callback. You must also
never try to stop and start it from inside the callback. There are a few other things you shouldn't
do in the callback depending on your requirements, however this isn't so much a thread-safety
thing, but rather a real-time processing thing which is beyond the scope of this introduction.

The example above demonstrates the initialization of a playback device, but it works exactly the
same for capture. All you need to do is change the device type from `ma_device_type_playback` to
`ma_device_type_capture` when setting up the config, like so:

    ```c
    ma_device_config config = ma_device_config_init(ma_device_type_capture);
    config.capture.format   = MY_FORMAT;
    config.capture.channels = MY_CHANNEL_COUNT;
    ```

In the data callback you just read from the input buffer (`pInput` in the example above) and leave
the output buffer alone (it will be set to NULL when the device type is set to
`ma_device_type_capture`).

These are the available device types and how you should handle the buffers in the callback:

    +-------------------------+--------------------------------------------------------+
    | Device Type             | Callback Behavior                                      |
    +-------------------------+--------------------------------------------------------+
    | ma_device_type_playback | Write to output buffer, leave input buffer untouched.  |
    | ma_device_type_capture  | Read from input buffer, leave output buffer untouched. |
    | ma_device_type_duplex   | Read from input buffer, write to output buffer.        |
    | ma_device_type_loopback | Read from input buffer, leave output buffer untouched. |
    +-------------------------+--------------------------------------------------------+

You will notice in the example above that the sample format and channel count is specified
separately for playback and capture. This is to support different data formats between the playback
and capture devices in a full-duplex system. An example may be that you want to capture audio data
as a monaural stream (one channel), but output sound to a stereo speaker system. Note that if you
use different formats between playback and capture in a full-duplex configuration you will need to
convert the data yourself. There are functions available to help you do this which will be
explained later.

The example above did not specify a physical device to connect to which means it will use the
operating system's default device. If you have multiple physical devices connected and you want to
use a specific one you will need to specify the device ID in the configuration, like so:

    ```c
    config.playback.pDeviceID = pMyPlaybackDeviceID;    // Only if requesting a playback or duplex device.
    config.capture.pDeviceID = pMyCaptureDeviceID;      // Only if requesting a capture, duplex or loopback device.
    ```

To retrieve the device ID you will need to perform device enumeration, however this requires the
use of a new concept called the "context". Conceptually speaking the context sits above the device.
There is one context to many devices. The purpose of the context is to represent the backend at a
more global level and to perform operations outside the scope of an individual device. Mainly it is
used for performing run-time linking against backend libraries, initializing backends and
enumerating devices. The example below shows how to enumerate devices.

    ```c
    ma_context context;
    if (ma_context_init(NULL, 0, NULL, &context) != MA_SUCCESS) {
        // Error.
    }

    ma_device_info* pPlaybackInfos;
    ma_uint32 playbackCount;
    ma_device_info* pCaptureInfos;
    ma_uint32 captureCount;
    if (ma_context_get_devices(&context, &pPlaybackInfos, &playbackCount, &pCaptureInfos, &captureCount) != MA_SUCCESS) {
        // Error.
    }

    // Loop over each device info and do something with it. Here we just print the name with their index. You may want
    // to give the user the opportunity to choose which device they'd prefer.
    for (ma_uint32 iDevice = 0; iDevice < playbackCount; iDevice += 1) {
        printf("%d - %s\n", iDevice, pPlaybackInfos[iDevice].name);
    }

    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.playback.pDeviceID = &pPlaybackInfos[chosenPlaybackDeviceIndex].id;
    config.playback.format    = MY_FORMAT;
    config.playback.channels  = MY_CHANNEL_COUNT;
    config.sampleRate         = MY_SAMPLE_RATE;
    config.dataCallback       = data_callback;
    config.pUserData          = pMyCustomData;

    ma_device device;
    if (ma_device_init(&context, &config, &device) != MA_SUCCESS) {
        // Error
    }

    ...

    ma_device_uninit(&device);
    ma_context_uninit(&context);
    ```

The first thing we do in this example is initialize a `ma_context` object with `ma_context_init()`.
The first parameter is a pointer to a list of `ma_backend` values which are used to override the
default backend priorities. When this is NULL, as in this example, miniaudio's default priorities
are used. The second parameter is the number of backends listed in the array pointed to by the
first parameter. The third parameter is a pointer to a `ma_context_config` object which can be
NULL, in which case defaults are used. The context configuration is used for setting the logging
callback, custom memory allocation callbacks, user-defined data and some backend-specific
configurations.

Once the context has been initialized you can enumerate devices. In the example above we use the
simpler `ma_context_get_devices()`, however you can also use a callback for handling devices by
using `ma_context_enumerate_devices()`. When using `ma_context_get_devices()` you provide a pointer
to a pointer that will, upon output, be set to a pointer to a buffer containing a list of
`ma_device_info` structures. You also provide a pointer to an unsigned integer that will receive
the number of items in the returned buffer. Do not free the returned buffers as their memory is
managed internally by miniaudio.

The `ma_device_info` structure contains an `id` member which is the ID you pass to the device
config. It also contains the name of the device which is useful for presenting a list of devices
to the user via the UI.

When creating your own context you will want to pass it to `ma_device_init()` when initializing the
device. Passing in NULL, like we do in the first example, will result in miniaudio creating the
context for you, which you don't want to do since you've already created a context. Note that
internally the context is only tracked by it's pointer which means you must not change the location
of the `ma_context` object. If this is an issue, consider using `malloc()` to allocate memory for
the context.


1.2. High Level API
-------------------
The high level API consists of three main parts:

  * Resource management for loading and streaming sounds.
  * A node graph for advanced mixing and effect processing.
  * A high level "engine" that wraps around the resource manager and node graph.

The resource manager (`ma_resource_manager`) is used for loading sounds. It supports loading sounds
fully into memory and also streaming. It will also deal with reference counting for you which
avoids the same sound being loaded multiple times.

The node graph is used for mixing and effect processing. The idea is that you connect a number of
nodes into the graph by connecting each node's outputs to another node's inputs. Each node can
implement its own effect. By chaining nodes together, advanced mixing and effect processing can
be achieved.

The engine encapsulates both the resource manager and the node graph to create a simple, easy to
use high level API. The resource manager and node graph APIs are covered in more later sections of
this manual.

The code below shows how you can initialize an engine using its default configuration.

    ```c
    ma_result result;
    ma_engine engine;

    result = ma_engine_init(NULL, &engine);
    if (result != MA_SUCCESS) {
        return result;  // Failed to initialize the engine.
    }
    ```

This creates an engine instance which will initialize a device internally which you can access with
`ma_engine_get_device()`. It will also initialize a resource manager for you which can be accessed
with `ma_engine_get_resource_manager()`. The engine itself is a node graph (`ma_node_graph`) which
means you can pass a pointer to the engine object into any of the `ma_node_graph` APIs (with a
cast). Alternatively, you can use `ma_engine_get_node_graph()` instead of a cast.

Note that all objects in miniaudio, including the `ma_engine` object in the example above, are
transparent structures. There are no handles to opaque structures in miniaudio which means you need
to be mindful of how you declare them. In the example above we are declaring it on the stack, but
this will result in the struct being invalidated once the function encapsulating it returns. If
allocating the engine on the heap is more appropriate, you can easily do so with a standard call
to `malloc()` or whatever heap allocation routine you like:

    ```c
    ma_engine* pEngine = malloc(sizeof(*pEngine));
    ```

The `ma_engine` API uses the same config/init pattern used all throughout miniaudio. To configure
an engine, you can fill out a `ma_engine_config` object and pass it into the first parameter of
`ma_engine_init()`:

    ```c
    ma_result result;
    ma_engine engine;
    ma_engine_config engineConfig;

    engineConfig = ma_engine_config_init();
    engineConfig.pResourceManager = &myCustomResourceManager;   // <-- Initialized as some earlier stage.

    result = ma_engine_init(&engineConfig, &engine);
    if (result != MA_SUCCESS) {
        return result;
    }
    ```

This creates an engine instance using a custom config. In this particular example it's showing how
you can specify a custom resource manager rather than having the engine initialize one internally.
This is particularly useful if you want to have multiple engine's share the same resource manager.

The engine must be uninitialized with `ma_engine_uninit()` when it's no longer needed.

By default the engine will be started, but nothing will be playing because no sounds have been
initialized. The easiest but least flexible way of playing a sound is like so:

    ```c
    ma_engine_play_sound(&engine, "my_sound.wav", NULL);
    ```

This plays what miniaudio calls an "inline" sound. It plays the sound once, and then puts the
internal sound up for recycling. The last parameter is used to specify which sound group the sound
should be associated with which will be explained later. This particular way of playing a sound is
simple, but lacks flexibility and features. A more flexible way of playing a sound is to first
initialize a sound:

    ```c
    ma_result result;
    ma_sound sound;

    result = ma_sound_init_from_file(&engine, "my_sound.wav", 0, NULL, NULL, &sound);
    if (result != MA_SUCCESS) {
        return result;
    }

    ma_sound_start(&sound);
    ```

This returns a `ma_sound` object which represents a single instance of the specified sound file. If
you want to play the same file multiple times simultaneously, you need to create one sound for each
instance.

Sounds should be uninitialized with `ma_sound_uninit()`.

Sounds are not started by default. Start a sound with `ma_sound_start()` and stop it with
`ma_sound_stop()`. When a sound is stopped, it is not rewound to the start. Use
`ma_sound_seek_to_pcm_frame(&sound, 0)` to seek back to the start of a sound. By default, starting
and stopping sounds happens immediately, but sometimes it might be convenient to schedule the sound
to be started and/or stopped at a specific time. This can be done with the following functions:

    ```c
    ma_sound_set_start_time_in_pcm_frames()
    ma_sound_set_start_time_in_milliseconds()
    ma_sound_set_stop_time_in_pcm_frames()
    ma_sound_set_stop_time_in_milliseconds()
    ```

The start/stop time needs to be specified based on the absolute timer which is controlled by the
engine. The current global time in PCM frames can be retrieved with
`ma_engine_get_time_in_pcm_frames()`. The engine's global time can be changed with
`ma_engine_set_time_in_pcm_frames()` for synchronization purposes if required. Note that scheduling
a start time still requires an explicit call to `ma_sound_start()` before anything will play:

    ```c
    ma_sound_set_start_time_in_pcm_frames(&sound, ma_engine_get_time_in_pcm_frames(&engine) + (ma_engine_get_sample_rate(&engine) * 2);
    ma_sound_start(&sound);
    ```

The third parameter of `ma_sound_init_from_file()` is a set of flags that control how the sound be
loaded and a few options on which features should be enabled for that sound. By default, the sound
is synchronously loaded fully into memory straight from the file system without any kind of
decoding. If you want to decode the sound before storing it in memory, you need to specify the
`MA_SOUND_FLAG_DECODE` flag. This is useful if you want to incur the cost of decoding at an earlier
stage, such as a loading stage. Without this option, decoding will happen dynamically at mixing
time which might be too expensive on the audio thread.

If you want to load the sound asynchronously, you can specify the `MA_SOUND_FLAG_ASYNC` flag. This
will result in `ma_sound_init_from_file()` returning quickly, but the sound will not start playing
until the sound has had some audio decoded.

The fourth parameter is a pointer to sound group. A sound group is used as a mechanism to organise
sounds into groups which have their own effect processing and volume control. An example is a game
which might have separate groups for sfx, voice and music. Each of these groups have their own
independent volume control. Use `ma_sound_group_init()` or `ma_sound_group_init_ex()` to initialize
a sound group.

Sounds and sound groups are nodes in the engine's node graph and can be plugged into any `ma_node`
API. This makes it possible to connect sounds and sound groups to effect nodes to produce complex
effect chains.

A sound can have its volume changed with `ma_sound_set_volume()`. If you prefer decibel volume
control you can use `ma_volume_db_to_linear()` to convert from decibel representation to linear.

Panning and pitching is supported with `ma_sound_set_pan()` and `ma_sound_set_pitch()`. If you know
a sound will never have its pitch changed with `ma_sound_set_pitch()` or via the doppler effect,
you can specify the `MA_SOUND_FLAG_NO_PITCH` flag when initializing the sound for an optimization.

By default, sounds and sound groups have spatialization enabled. If you don't ever want to
spatialize your sounds, initialize the sound with the `MA_SOUND_FLAG_NO_SPATIALIZATION` flag. The
spatialization model is fairly simple and is roughly on feature parity with OpenAL. HRTF and
environmental occlusion are not currently supported, but planned for the future. The supported
features include:

  * Sound and listener positioning and orientation with cones
  * Attenuation models: none, inverse, linear and exponential
  * Doppler effect

Sounds can be faded in and out with `ma_sound_set_fade_in_pcm_frames()`.

To check if a sound is currently playing, you can use `ma_sound_is_playing()`. To check if a sound
is at the end, use `ma_sound_at_end()`. Looping of a sound can be controlled with
`ma_sound_set_looping()`. Use `ma_sound_is_looping()` to check whether or not the sound is looping.



2. Building
===========
miniaudio should work cleanly out of the box without the need to download or install any
dependencies. See below for platform-specific details.

This library has been designed to be added directly to your source tree which is the preferred way
of using it, but you can compile it as a normal library if that's your preference. Be careful if
compiling as a shared object because miniaudio is not ABI compatible between any release, including
bug fix releases. It's recommended you link statically.

Note that GCC and Clang require `-msse2`, `-mavx2`, etc. for SIMD optimizations.

If you get errors about undefined references to `__sync_val_compare_and_swap_8`, `__atomic_load_8`,
etc. you need to link with `-latomic`.


2.1. Windows
------------
The Windows build should compile cleanly on all popular compilers without the need to configure any
include paths nor link to any libraries.

The UWP build may require linking to mmdevapi.lib if you get errors about an unresolved external
symbol for `ActivateAudioInterfaceAsync()`.


2.2. macOS and iOS
------------------
The macOS build should compile cleanly without the need to download any dependencies nor link to
any libraries or frameworks. The iOS build needs to be compiled as Objective-C and will need to
link the relevant frameworks but should compile cleanly out of the box with Xcode. Compiling
through the command line requires linking to `-lpthread` and `-lm`.

Due to the way miniaudio links to frameworks at runtime, your application may not pass Apple's
notarization process. To fix this there are two options. The first is to compile with
`-DMA_NO_RUNTIME_LINKING` which in turn will require linking with
`-framework CoreFoundation -framework CoreAudio -framework AudioToolbox`. If you get errors about
AudioToolbox, try with `-framework AudioUnit` instead. You may get this when using older versions
of iOS. Alternatively, if you would rather keep using runtime linking you can add the following to
your entitlements.xcent file:

    ```
    <key>com.apple.security.cs.allow-dyld-environment-variables</key>
    <true/>
    <key>com.apple.security.cs.allow-unsigned-executable-memory</key>
    <true/>
    ```

See this discussion for more info: https://github.com/mackron/miniaudio/issues/203.


2.3. Linux
----------
The Linux build only requires linking to `-ldl`, `-lpthread` and `-lm`. You do not need any
development packages. You may need to link with `-latomic` if you're compiling for 32-bit ARM.


2.4. BSD
--------
The BSD build only requires linking to `-lpthread` and `-lm`. NetBSD uses audio(4), OpenBSD uses
sndio and FreeBSD uses OSS. You may need to link with `-latomic` if you're compiling for 32-bit
ARM.


2.5. Android
------------
AAudio is the highest priority backend on Android. This should work out of the box without needing
any kind of compiler configuration. Support for AAudio starts with Android 8 which means older
versions will fall back to OpenSL|ES which requires API level 16+.

There have been reports that the OpenSL|ES backend fails to initialize on some Android based
devices due to `dlopen()` failing to open "libOpenSLES.so". If this happens on your platform
you'll need to disable run-time linking with `MA_NO_RUNTIME_LINKING` and link with -lOpenSLES.


2.6. Emscripten
---------------
The Emscripten build emits Web Audio JavaScript directly and should compile cleanly out of the box.
You cannot use `-std=c*` compiler flags, nor `-ansi`.

You can enable the use of AudioWorklets by defining `MA_ENABLE_AUDIO_WORKLETS` and then compiling
with the following options:

    -sAUDIO_WORKLET=1 -sWASM_WORKERS=1 -sASYNCIFY

An example for compiling with AudioWorklet support might look like this:

    emcc program.c -o bin/program.html -DMA_ENABLE_AUDIO_WORKLETS -sAUDIO_WORKLET=1 -sWASM_WORKERS=1 -sASYNCIFY

To run locally, you'll need to use emrun:

    emrun bin/program.html



2.7. Build Options
------------------
`#define` these options before including miniaudio.c, or pass them as compiler flags:

    +----------------------------------+--------------------------------------------------------------------+
    | Option                           | Description                                                        |
    +----------------------------------+--------------------------------------------------------------------+
    | MA_NO_WASAPI                     | Disables the WASAPI backend.                                       |
    +----------------------------------+--------------------------------------------------------------------+
    | MA_NO_DSOUND                     | Disables the DirectSound backend.                                  |
    +----------------------------------+--------------------------------------------------------------------+
    | MA_NO_WINMM                      | Disables the WinMM backend.                                        |
    +----------------------------------+--------------------------------------------------------------------+
    | MA_NO_ALSA                       | Disables the ALSA backend.                                         |
    +----------------------------------+--------------------------------------------------------------------+
    | MA_NO_PULSEAUDIO                 | Disables the PulseAudio backend.                                   |
    +----------------------------------+--------------------------------------------------------------------+
    | MA_NO_JACK                       | Disables the JACK backend.                                         |
    +----------------------------------+--------------------------------------------------------------------+
    | MA_NO_COREAUDIO                  | Disables the Core Audio backend.                                   |
    +----------------------------------+--------------------------------------------------------------------+
    | MA_NO_SNDIO                      | Disables the sndio backend.                                        |
    +----------------------------------+--------------------------------------------------------------------+
    | MA_NO_AUDIO4                     | Disables the audio(4) backend.                                     |
    +----------------------------------+--------------------------------------------------------------------+
    | MA_NO_OSS                        | Disables the OSS backend.                                          |
    +----------------------------------+--------------------------------------------------------------------+
    | MA_NO_AAUDIO                     | Disables the AAudio backend.                                       |
    +----------------------------------+--------------------------------------------------------------------+
    | MA_NO_OPENSL                     | Disables the OpenSL|ES backend.                                    |
    +----------------------------------+--------------------------------------------------------------------+
    | MA_NO_WEBAUDIO                   | Disables the Web Audio backend.                                    |
    +----------------------------------+--------------------------------------------------------------------+
    | MA_NO_CUSTOM                     | Disables support for custom backends.                              |
    +----------------------------------+--------------------------------------------------------------------+
    | MA_NO_NULL                       | Disables the null backend.                                         |
    +----------------------------------+--------------------------------------------------------------------+
    | MA_ENABLE_ONLY_SPECIFIC_BACKENDS | Disables all backends by default and requires `MA_ENABLE_*` to     |
    |                                  | enable specific backends.                                          |
    +----------------------------------+--------------------------------------------------------------------+
    | MA_ENABLE_WASAPI                 | Used in conjunction with MA_ENABLE_ONLY_SPECIFIC_BACKENDS to       |
    |                                  | enable the WASAPI backend.                                         |
    +----------------------------------+--------------------------------------------------------------------+
    | MA_ENABLE_DSOUND                 | Used in conjunction with MA_ENABLE_ONLY_SPECIFIC_BACKENDS to       |
    |                                  | enable the DirectSound backend.                                    |
    +----------------------------------+--------------------------------------------------------------------+
    | MA_ENABLE_WINMM                  | Used in conjunction with MA_ENABLE_ONLY_SPECIFIC_BACKENDS to       |
    |                                  | enable the WinMM backend.                                          |
    +----------------------------------+--------------------------------------------------------------------+
    | MA_ENABLE_ALSA                   | Used in conjunction with MA_ENABLE_ONLY_SPECIFIC_BACKENDS to       |
    |                                  | enable the ALSA backend.                                           |
    +----------------------------------+--------------------------------------------------------------------+
    | MA_ENABLE_PULSEAUDIO             | Used in conjunction with MA_ENABLE_ONLY_SPECIFIC_BACKENDS to       |
    |                                  | enable the PulseAudio backend.                                     |
    +----------------------------------+--------------------------------------------------------------------+
    | MA_ENABLE_JACK                   | Used in conjunction with MA_ENABLE_ONLY_SPECIFIC_BACKENDS to       |
    |                                  | enable the JACK backend.                                           |
    +----------------------------------+--------------------------------------------------------------------+
    | MA_ENABLE_COREAUDIO              | Used in conjunction with MA_ENABLE_ONLY_SPECIFIC_BACKENDS to       |
    |                                  | enable the Core Audio backend.                                     |
    +----------------------------------+--------------------------------------------------------------------+
    | MA_ENABLE_SNDIO                  | Used in conjunction with MA_ENABLE_ONLY_SPECIFIC_BACKENDS to       |
    |                                  | enable the sndio backend.                                          |
    +----------------------------------+--------------------------------------------------------------------+
    | MA_ENABLE_AUDIO4                 | Used in conjunction with MA_ENABLE_ONLY_SPECIFIC_BACKENDS to       |
    |                                  | enable the audio(4) backend.                                       |
    +----------------------------------+--------------------------------------------------------------------+
    | MA_ENABLE_OSS                    | Used in conjunction with MA_ENABLE_ONLY_SPECIFIC_BACKENDS to       |
    |                                  | enable the OSS backend.                                            |
    +----------------------------------+--------------------------------------------------------------------+
    | MA_ENABLE_AAUDIO                 | Used in conjunction with MA_ENABLE_ONLY_SPECIFIC_BACKENDS to       |
    |                                  | enable the AAudio backend.                                         |
    +----------------------------------+--------------------------------------------------------------------+
    | MA_ENABLE_OPENSL                 | Used in conjunction with MA_ENABLE_ONLY_SPECIFIC_BACKENDS to       |
    |                                  | enable the OpenSL|ES backend.                                      |
    +----------------------------------+--------------------------------------------------------------------+
    | MA_ENABLE_WEBAUDIO               | Used in conjunction with MA_ENABLE_ONLY_SPECIFIC_BACKENDS to       |
    |                                  | enable the Web Audio backend.                                      |
    +----------------------------------+--------------------------------------------------------------------+
    | MA_ENABLE_CUSTOM                 | Used in conjunction with MA_ENABLE_ONLY_SPECIFIC_BACKENDS to       |
    |                                  | enable custom backends.                                            |
    +----------------------------------+--------------------------------------------------------------------+
    | MA_ENABLE_NULL                   | Used in conjunction with MA_ENABLE_ONLY_SPECIFIC_BACKENDS to       |
    |                                  | enable the null backend.                                           |
    +----------------------------------+--------------------------------------------------------------------+
    | MA_NO_DECODING                   | Disables decoding APIs.                                            |
    +----------------------------------+--------------------------------------------------------------------+
    | MA_NO_ENCODING                   | Disables encoding APIs.                                            |
    +----------------------------------+--------------------------------------------------------------------+
    | MA_NO_WAV                        | Disables the built-in WAV decoder and encoder.                     |
    +----------------------------------+--------------------------------------------------------------------+
    | MA_NO_FLAC                       | Disables the built-in FLAC decoder.                                |
    +----------------------------------+--------------------------------------------------------------------+
    | MA_NO_MP3                        | Disables the built-in MP3 decoder.                                 |
    +----------------------------------+--------------------------------------------------------------------+
    | MA_NO_DEVICE_IO                  | Disables playback and recording. This will disable `ma_context`    |
    |                                  | and `ma_device` APIs. This is useful if you only want to use       |
    |                                  | miniaudio's data conversion and/or decoding APIs.                  |
    +----------------------------------+--------------------------------------------------------------------+
    | MA_NO_RESOURCE_MANAGER           | Disables the resource manager. When using the engine this will     |
    |                                  | also disable the following functions:                              |
    |                                  |                                                                    |
    |                                  | ```                                                                |
    |                                  | ma_sound_init_from_file()                                          |
    |                                  | ma_sound_init_from_file_w()                                        |
    |                                  | ma_sound_init_copy()                                               |
    |                                  | ma_engine_play_sound_ex()                                          |
    |                                  | ma_engine_play_sound()                                             |
    |                                  | ```                                                                |
    |                                  |                                                                    |
    |                                  | The only way to initialize a `ma_sound` object is to initialize it |
    |                                  | from a data source.                                                |
    +----------------------------------+--------------------------------------------------------------------+
    | MA_NO_NODE_GRAPH                 | Disables the node graph API. This will also disable the engine API |
    |                                  | because it depends on the node graph.                              |
    +----------------------------------+--------------------------------------------------------------------+
    | MA_NO_ENGINE                     | Disables the engine API.                                           |
    +----------------------------------+--------------------------------------------------------------------+
    | MA_NO_THREADING                  | Disables the `ma_thread`, `ma_mutex`, `ma_semaphore` and           |
    |                                  | `ma_event` APIs. This option is useful if you only need to use     |
    |                                  | miniaudio for data conversion, decoding and/or encoding. Some      |
    |                                  | families of APIs require threading which means the following       |
    |                                  | options must also be set:                                          |
    |                                  |                                                                    |
    |                                  |     ```                                                            |
    |                                  |     MA_NO_DEVICE_IO                                                |
    |                                  |     ```                                                            |
    +----------------------------------+--------------------------------------------------------------------+
    | MA_NO_GENERATION                 | Disables generation APIs such a `ma_waveform` and `ma_noise`.      |
    +----------------------------------+--------------------------------------------------------------------+
    | MA_NO_SSE2                       | Disables SSE2 optimizations.                                       |
    +----------------------------------+--------------------------------------------------------------------+
    | MA_NO_AVX2                       | Disables AVX2 optimizations.                                       |
    +----------------------------------+--------------------------------------------------------------------+
    | MA_NO_NEON                       | Disables NEON optimizations.                                       |
    +----------------------------------+--------------------------------------------------------------------+
    | MA_NO_RUNTIME_LINKING            | Disables runtime linking. This is useful for passing Apple's       |
    |                                  | notarization process. When enabling this, you may need to avoid    |
    |                                  | using `-std=c89` or `-std=c99` on Linux builds or else you may end |
    |                                  | up with compilation errors due to conflicts with `timespec` and    |
    |                                  | `timeval` data types.                                              |
    |                                  |                                                                    |
    |                                  | You may need to enable this if your target platform does not allow |
    |                                  | runtime linking via `dlopen()`.                                    |
    +----------------------------------+--------------------------------------------------------------------+
    | MA_USE_STDINT                    | (Pass this in as a compiler flag. Do not `#define` this before     |
    |                                  | miniaudio.c) Forces the use of stdint.h for sized types.           |
    +----------------------------------+--------------------------------------------------------------------+
    | MA_DEBUG_OUTPUT                  | Enable `printf()` output of debug logs (`MA_LOG_LEVEL_DEBUG`).     |
    +----------------------------------+--------------------------------------------------------------------+
    | MA_COINIT_VALUE                  | Windows only. The value to pass to internal calls to               |
    |                                  | `CoInitializeEx()`. Defaults to `COINIT_MULTITHREADED`.            |
    +----------------------------------+--------------------------------------------------------------------+
    | MA_FORCE_UWP                     | Windows only. Affects only the WASAPI backend. Will force the      |
    |                                  | WASAPI backend to use the UWP code path instead of the regular     |
    |                                  | desktop path. This is normally auto-detected and should rarely be  |
    |                                  | needed to be used explicitly, but can be useful for debugging.     |
    +----------------------------------+--------------------------------------------------------------------+
    | MA_ON_THREAD_ENTRY               | Defines some code that will be executed as soon as an internal     |
    |                                  | miniaudio-managed thread is created. This will be the first thing  |
    |                                  | to be executed by the thread entry point.                          |
    +----------------------------------+--------------------------------------------------------------------+
    | MA_ON_THREAD_EXIT                | Defines some code that will be executed from the entry point of an |
    |                                  | internal miniaudio-managed thread upon exit. This will be the last |
    |                                  | thing to be executed before the thread's entry point exits.        |
    +----------------------------------+--------------------------------------------------------------------+
    | MA_THREAD_DEFAULT_STACK_SIZE     | If set, specifies the default stack size used by miniaudio-managed |
    |                                  | threads.                                                           |
    +----------------------------------+--------------------------------------------------------------------+
    | MA_API                           | Controls how public APIs should be decorated. Default is `extern`. |
    +----------------------------------+--------------------------------------------------------------------+


3. Definitions
==============
This section defines common terms used throughout miniaudio. Unfortunately there is often ambiguity
in the use of terms throughout the audio space, so this section is intended to clarify how miniaudio
uses each term.

3.1. Sample
-----------
A sample is a single unit of audio data. If the sample format is f32, then one sample is one 32-bit
floating point number.

3.2. Frame / PCM Frame
----------------------
A frame is a group of samples equal to the number of channels. For a stereo stream a frame is 2
samples, a mono frame is 1 sample, a 5.1 surround sound frame is 6 samples, etc. The terms "frame"
and "PCM frame" are the same thing in miniaudio. Note that this is different to a compressed frame.
If ever miniaudio needs to refer to a compressed frame, such as a FLAC frame, it will always
clarify what it's referring to with something like "FLAC frame".

3.3. Channel
------------
A stream of monaural audio that is emitted from an individual speaker in a speaker system, or
received from an individual microphone in a microphone system. A stereo stream has two channels (a
left channel, and a right channel), a 5.1 surround sound system has 6 channels, etc. Some audio
systems refer to a channel as a complex audio stream that's mixed with other channels to produce
the final mix - this is completely different to miniaudio's use of the term "channel" and should
not be confused.

3.4. Sample Rate
----------------
The sample rate in miniaudio is always expressed in Hz, such as 44100, 48000, etc. It's the number
of PCM frames that are processed per second.

3.5. Formats
------------
Throughout miniaudio you will see references to different sample formats:

    +---------------+----------------------------------------+---------------------------+
    | Symbol        | Description                            | Range                     |
    +---------------+----------------------------------------+---------------------------+
    | ma_format_f32 | 32-bit floating point                  | [-1, 1]                   |
    | ma_format_s16 | 16-bit signed integer                  | [-32768, 32767]           |
    | ma_format_s24 | 24-bit signed integer (tightly packed) | [-8388608, 8388607]       |
    | ma_format_s32 | 32-bit signed integer                  | [-2147483648, 2147483647] |
    | ma_format_u8  | 8-bit unsigned integer                 | [0, 255]                  |
    +---------------+----------------------------------------+---------------------------+

All formats are native-endian.



4. Data Sources
===============
The data source abstraction in miniaudio is used for retrieving audio data from some source. A few
examples include `ma_decoder`, `ma_noise` and `ma_waveform`. You will need to be familiar with data
sources in order to make sense of some of the higher level concepts in miniaudio.

The `ma_data_source` API is a generic interface for reading from a data source. Any object that
implements the data source interface can be plugged into any `ma_data_source` function.

To read data from a data source:

    ```c
    ma_result result;
    ma_uint64 framesRead;

    result = ma_data_source_read_pcm_frames(pDataSource, pFramesOut, frameCount, &framesRead);
    if (result != MA_SUCCESS) {
        return result;  // Failed to read data from the data source.
    }
    ```

If you don't need the number of frames that were successfully read you can pass in `NULL` to the
`pFramesRead` parameter. If this returns a value less than the number of frames requested it means
the end of the file has been reached. `MA_AT_END` will be returned only when the number of frames
read is 0.

When calling any data source function, with the exception of `ma_data_source_init()` and
`ma_data_source_uninit()`, you can pass in any object that implements a data source. For example,
you could plug in a decoder like so:

    ```c
    ma_result result;
    ma_uint64 framesRead;
    ma_decoder decoder;   // <-- This would be initialized with `ma_decoder_init_*()`.

    result = ma_data_source_read_pcm_frames(&decoder, pFramesOut, frameCount, &framesRead);
    if (result != MA_SUCCESS) {
        return result;  // Failed to read data from the decoder.
    }
    ```

If you want to seek forward you can pass in `NULL` to the `pFramesOut` parameter. Alternatively you
can use `ma_data_source_seek_pcm_frames()`.

To seek to a specific PCM frame:

    ```c
    result = ma_data_source_seek_to_pcm_frame(pDataSource, frameIndex);
    if (result != MA_SUCCESS) {
        return result;  // Failed to seek to PCM frame.
    }
    ```

You can retrieve the total length of a data source in PCM frames, but note that some data sources
may not have the notion of a length, such as noise and waveforms, and others may just not have a
way of determining the length such as some decoders. To retrieve the length:

    ```c
    ma_uint64 length;

    result = ma_data_source_get_length_in_pcm_frames(pDataSource, &length);
    if (result != MA_SUCCESS) {
        return result;  // Failed to retrieve the length.
    }
    ```

Care should be taken when retrieving the length of a data source where the underlying decoder is
pulling data from a data stream with an undefined length, such as internet radio or some kind of
broadcast. If you do this, `ma_data_source_get_length_in_pcm_frames()` may never return.

The current position of the cursor in PCM frames can also be retrieved:

    ```c
    ma_uint64 cursor;

    result = ma_data_source_get_cursor_in_pcm_frames(pDataSource, &cursor);
    if (result != MA_SUCCESS) {
        return result;  // Failed to retrieve the cursor.
    }
    ```

You will often need to know the data format that will be returned after reading. This can be
retrieved like so:

    ```c
    ma_format format;
    ma_uint32 channels;
    ma_uint32 sampleRate;
    ma_channel channelMap[MA_MAX_CHANNELS];

    result = ma_data_source_get_data_format(pDataSource, &format, &channels, &sampleRate, channelMap, MA_MAX_CHANNELS);
    if (result != MA_SUCCESS) {
        return result;  // Failed to retrieve data format.
    }
    ```

If you do not need a specific data format property, just pass in NULL to the respective parameter.

There may be cases where you want to implement something like a sound bank where you only want to
read data within a certain range of the underlying data. To do this you can use a range:

    ```c
    result = ma_data_source_set_range_in_pcm_frames(pDataSource, rangeBegInFrames, rangeEndInFrames);
    if (result != MA_SUCCESS) {
        return result;  // Failed to set the range.
    }
    ```

This is useful if you have a sound bank where many sounds are stored in the same file and you want
the data source to only play one of those sub-sounds. Note that once the range is set, everything
that takes a position, such as cursors and loop points, should always be relative to the start of
the range. When the range is set, any previously defined loop point will be reset.

Custom loop points can also be used with data sources. By default, data sources will loop after
they reach the end of the data source, but if you need to loop at a specific location, you can do
the following:

    ```c
    result = ma_data_source_set_loop_point_in_pcm_frames(pDataSource, loopBegInFrames, loopEndInFrames);
    if (result != MA_SUCCESS) {
        return result;  // Failed to set the loop point.
    }
    ```

The loop point is relative to the current range.

It's sometimes useful to chain data sources together so that a seamless transition can be achieved.
To do this, you can use chaining:

    ```c
    ma_decoder decoder1;
    ma_decoder decoder2;

    // ... initialize decoders with ma_decoder_init_*() ...

    result = ma_data_source_set_next(&decoder1, &decoder2);
    if (result != MA_SUCCESS) {
        return result;  // Failed to set the next data source.
    }

    result = ma_data_source_read_pcm_frames(&decoder1, pFramesOut, frameCount, pFramesRead);
    if (result != MA_SUCCESS) {
        return result;  // Failed to read from the decoder.
    }
    ```

In the example above we're using decoders. When reading from a chain, you always want to read from
the top level data source in the chain. In the example above, `decoder1` is the top level data
source in the chain. When `decoder1` reaches the end, `decoder2` will start seamlessly without any
gaps.

Note that when looping is enabled, only the current data source will be looped. You can loop the
entire chain by linking in a loop like so:

    ```c
    ma_data_source_set_next(&decoder1, &decoder2);  // decoder1 -> decoder2
    ma_data_source_set_next(&decoder2, &decoder1);  // decoder2 -> decoder1 (loop back to the start).
    ```

Note that setting up chaining is not thread safe, so care needs to be taken if you're dynamically
changing links while the audio thread is in the middle of reading.

Do not use `ma_decoder_seek_to_pcm_frame()` as a means to reuse a data source to play multiple
instances of the same sound simultaneously. This can be extremely inefficient depending on the type
of data source and can result in glitching due to subtle changes to the state of internal filters.
Instead, initialize multiple data sources for each instance.


4.1. Custom Data Sources
------------------------
You can implement a custom data source by implementing the functions in `ma_data_source_vtable`.
Your custom object must have `ma_data_source_base` as it's first member:

    ```c
    struct my_data_source
    {
        ma_data_source_base base;
        ...
    };
    ```

In your initialization routine, you need to call `ma_data_source_init()` in order to set up the
base object (`ma_data_source_base`):

    ```c
    static ma_result my_data_source_read(ma_data_source* pDataSource, void* pFramesOut, ma_uint64 frameCount, ma_uint64* pFramesRead)
    {
        // Read data here. Output in the same format returned by my_data_source_get_data_format().
    }

    static ma_result my_data_source_seek(ma_data_source* pDataSource, ma_uint64 frameIndex)
    {
        // Seek to a specific PCM frame here. Return MA_NOT_IMPLEMENTED if seeking is not supported.
    }

    static ma_result my_data_source_get_data_format(ma_data_source* pDataSource, ma_format* pFormat, ma_uint32* pChannels, ma_uint32* pSampleRate, ma_channel* pChannelMap, size_t channelMapCap)
    {
        // Return the format of the data here.
    }

    static ma_result my_data_source_get_cursor(ma_data_source* pDataSource, ma_uint64* pCursor)
    {
        // Retrieve the current position of the cursor here. Return MA_NOT_IMPLEMENTED and set *pCursor to 0 if there is no notion of a cursor.
    }

    static ma_result my_data_source_get_length(ma_data_source* pDataSource, ma_uint64* pLength)
    {
        // Retrieve the length in PCM frames here. Return MA_NOT_IMPLEMENTED and set *pLength to 0 if there is no notion of a length or if the length is unknown.
    }

    static ma_data_source_vtable g_my_data_source_vtable =
    {
        my_data_source_read,
        my_data_source_seek,
        my_data_source_get_data_format,
        my_data_source_get_cursor,
        my_data_source_get_length
    };

    ma_result my_data_source_init(my_data_source* pMyDataSource)
    {
        ma_result result;
        ma_data_source_config baseConfig;

        baseConfig = ma_data_source_config_init();
        baseConfig.vtable = &g_my_data_source_vtable;

        result = ma_data_source_init(&baseConfig, &pMyDataSource->base);
        if (result != MA_SUCCESS) {
            return result;
        }

        // ... do the initialization of your custom data source here ...

        return MA_SUCCESS;
    }

    void my_data_source_uninit(my_data_source* pMyDataSource)
    {
        // ... do the uninitialization of your custom data source here ...

        // You must uninitialize the base data source.
        ma_data_source_uninit(&pMyDataSource->base);
    }
    ```

Note that `ma_data_source_init()` and `ma_data_source_uninit()` are never called directly outside
of the custom data source. It's up to the custom data source itself to call these within their own
init/uninit functions.



5. Engine
=========
The `ma_engine` API is a high level API for managing and mixing sounds and effect processing. The
`ma_engine` object encapsulates a resource manager and a node graph, both of which will be
explained in more detail later.

Sounds are called `ma_sound` and are created from an engine. Sounds can be associated with a mixing
group called `ma_sound_group` which are also created from the engine. Both `ma_sound` and
`ma_sound_group` objects are nodes within the engine's node graph.

When the engine is initialized, it will normally create a device internally. If you would rather
manage the device yourself, you can do so and just pass a pointer to it via the engine config when
you initialize the engine. You can also just use the engine without a device, which again can be
configured via the engine config.

The most basic way to initialize the engine is with a default config, like so:

    ```c
    ma_result result;
    ma_engine engine;

    result = ma_engine_init(NULL, &engine);
    if (result != MA_SUCCESS) {
        return result;  // Failed to initialize the engine.
    }
    ```

This will result in the engine initializing a playback device using the operating system's default
device. This will be sufficient for many use cases, but if you need more flexibility you'll want to
configure the engine with an engine config:

    ```c
    ma_result result;
    ma_engine engine;
    ma_engine_config engineConfig;

    engineConfig = ma_engine_config_init();
    engineConfig.pDevice = &myDevice;

    result = ma_engine_init(&engineConfig, &engine);
    if (result != MA_SUCCESS) {
        return result;  // Failed to initialize the engine.
    }
    ```

In the example above we're passing in a pre-initialized device. Since the caller is the one in
control of the device's data callback, it's their responsibility to manually call
`ma_engine_read_pcm_frames()` from inside their data callback:

    ```c
    void playback_data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
    {
        ma_engine_read_pcm_frames(&g_Engine, pOutput, frameCount, NULL);
    }
    ```

You can also use the engine independent of a device entirely:

    ```c
    ma_result result;
    ma_engine engine;
    ma_engine_config engineConfig;

    engineConfig = ma_engine_config_init();
    engineConfig.noDevice   = MA_TRUE;
    engineConfig.channels   = 2;        // Must be set when not using a device.
    engineConfig.sampleRate = 48000;    // Must be set when not using a device.

    result = ma_engine_init(&engineConfig, &engine);
    if (result != MA_SUCCESS) {
        return result;  // Failed to initialize the engine.
    }
    ```

Note that when you're not using a device, you must set the channel count and sample rate in the
config or else miniaudio won't know what to use (miniaudio will use the device to determine this
normally). When not using a device, you need to use `ma_engine_read_pcm_frames()` to process audio
data from the engine. This kind of setup is useful if you want to do something like offline
processing or want to use a different audio system for playback such as SDL.

When a sound is loaded it goes through a resource manager. By default the engine will initialize a
resource manager internally, but you can also specify a pre-initialized resource manager:

    ```c
    ma_result result;
    ma_engine engine1;
    ma_engine engine2;
    ma_engine_config engineConfig;

    engineConfig = ma_engine_config_init();
    engineConfig.pResourceManager = &myResourceManager;

    ma_engine_init(&engineConfig, &engine1);
    ma_engine_init(&engineConfig, &engine2);
    ```

In this example we are initializing two engines, both of which are sharing the same resource
manager. This is especially useful for saving memory when loading the same file across multiple
engines. If you were not to use a shared resource manager, each engine instance would use their own
which would result in any sounds that are used between both engine's being loaded twice. By using
a shared resource manager, it would only be loaded once. Using multiple engine's is useful when you
need to output to multiple playback devices, such as in a local multiplayer game where each player
is using their own set of headphones.

By default an engine will be in a started state. To make it so the engine is not automatically
started you can configure it as such:

    ```c
    engineConfig.noAutoStart = MA_TRUE;

    // The engine will need to be started manually.
    ma_engine_start(&engine);

    // Later on the engine can be stopped with ma_engine_stop().
    ma_engine_stop(&engine);
    ```

The concept of starting or stopping an engine is only relevant when using the engine with a
device. Attempting to start or stop an engine that is not associated with a device will result in
`MA_INVALID_OPERATION`.

The master volume of the engine can be controlled with `ma_engine_set_volume()` which takes a
linear scale, with 0 resulting in silence and anything above 1 resulting in amplification. If you
prefer decibel based volume control, use `ma_volume_db_to_linear()` to convert from dB to linear.

When a sound is spatialized, it is done so relative to a listener. An engine can be configured to
have multiple listeners which can be configured via the config:

    ```c
    engineConfig.listenerCount = 2;
    ```

The maximum number of listeners is restricted to `MA_ENGINE_MAX_LISTENERS`. By default, when a
sound is spatialized, it will be done so relative to the closest listener. You can also pin a sound
to a specific listener which will be explained later. Listener's have a position, direction, cone,
and velocity (for doppler effect). A listener is referenced by an index, the meaning of which is up
to the caller (the index is 0 based and cannot go beyond the listener count, minus 1). The
position, direction and velocity are all specified in absolute terms:

    ```c
    ma_engine_listener_set_position(&engine, listenerIndex, worldPosX, worldPosY, worldPosZ);
    ```

The direction of the listener represents it's forward vector. The listener's up vector can also be
specified and defaults to +1 on the Y axis.

    ```c
    ma_engine_listener_set_direction(&engine, listenerIndex, forwardX, forwardY, forwardZ);
    ma_engine_listener_set_world_up(&engine, listenerIndex, 0, 1, 0);
    ```

The engine supports directional attenuation. The listener can have a cone the controls how sound is
attenuated based on the listener's direction. When a sound is between the inner and outer cones, it
will be attenuated between 1 and the cone's outer gain:

    ```c
    ma_engine_listener_set_cone(&engine, listenerIndex, innerAngleInRadians, outerAngleInRadians, outerGain);
    ```

When a sound is inside the inner code, no directional attenuation is applied. When the sound is
outside of the outer cone, the attenuation will be set to `outerGain` in the example above. When
the sound is in between the inner and outer cones, the attenuation will be interpolated between 1
and the outer gain.

The engine's coordinate system follows the OpenGL coordinate system where positive X points right,
positive Y points up and negative Z points forward.

The simplest and least flexible way to play a sound is like so:

    ```c
    ma_engine_play_sound(&engine, "my_sound.wav", pGroup);
    ```

This is a "fire and forget" style of function. The engine will manage the `ma_sound` object
internally. When the sound finishes playing, it'll be put up for recycling. For more flexibility
you'll want to initialize a sound object:

    ```c
    ma_sound sound;

    result = ma_sound_init_from_file(&engine, "my_sound.wav", flags, pGroup, NULL, &sound);
    if (result != MA_SUCCESS) {
        return result;  // Failed to load sound.
    }
    ```

Sounds need to be uninitialized with `ma_sound_uninit()`.

The example above loads a sound from a file. If the resource manager has been disabled you will not
be able to use this function and instead you'll need to initialize a sound directly from a data
source:

    ```c
    ma_sound sound;

    result = ma_sound_init_from_data_source(&engine, &dataSource, flags, pGroup, &sound);
    if (result != MA_SUCCESS) {
        return result;
    }
    ```

Each `ma_sound` object represents a single instance of the sound. If you want to play the same
sound multiple times at the same time, you need to initialize a separate `ma_sound` object.

For the most flexibility when initializing sounds, use `ma_sound_init_ex()`. This uses miniaudio's
standard config/init pattern:

    ```c
    ma_sound sound;
    ma_sound_config soundConfig;

    soundConfig = ma_sound_config_init();
    soundConfig.pFilePath   = NULL; // Set this to load from a file path.
    soundConfig.pDataSource = NULL; // Set this to initialize from an existing data source.
    soundConfig.pInitialAttachment = &someNodeInTheNodeGraph;
    soundConfig.initialAttachmentInputBusIndex = 0;
    soundConfig.channelsIn  = 1;
    soundConfig.channelsOut = 0;    // Set to 0 to use the engine's native channel count.

    result = ma_sound_init_ex(&soundConfig, &sound);
    if (result != MA_SUCCESS) {
        return result;
    }
    ```

In the example above, the sound is being initialized without a file nor a data source. This is
valid, in which case the sound acts as a node in the middle of the node graph. This means you can
connect other sounds to this sound and allow it to act like a sound group. Indeed, this is exactly
what a `ma_sound_group` is.

When loading a sound, you specify a set of flags that control how the sound is loaded and what
features are enabled for that sound. When no flags are set, the sound will be fully loaded into
memory in exactly the same format as how it's stored on the file system. The resource manager will
allocate a block of memory and then load the file directly into it. When reading audio data, it
will be decoded dynamically on the fly. In order to save processing time on the audio thread, it
might be beneficial to pre-decode the sound. You can do this with the `MA_SOUND_FLAG_DECODE` flag:

    ```c
    ma_sound_init_from_file(&engine, "my_sound.wav", MA_SOUND_FLAG_DECODE, pGroup, NULL, &sound);
    ```

By default, sounds will be loaded synchronously, meaning `ma_sound_init_*()` will not return until
the sound has been fully loaded. If this is prohibitive you can instead load sounds asynchronously
by specifying the `MA_SOUND_FLAG_ASYNC` flag:

    ```c
    ma_sound_init_from_file(&engine, "my_sound.wav", MA_SOUND_FLAG_DECODE | MA_SOUND_FLAG_ASYNC, pGroup, NULL, &sound);
    ```

This will result in `ma_sound_init_*()` returning quickly, but the sound won't yet have been fully
loaded. When you start the sound, it won't output anything until some sound is available. The sound
will start outputting audio before the sound has been fully decoded when the `MA_SOUND_FLAG_DECODE`
is specified.

If you need to wait for an asynchronously loaded sound to be fully loaded, you can use a fence. A
fence in miniaudio is a simple synchronization mechanism which simply blocks until it's internal
counter hit's zero. You can specify a fence like so:

    ```c
    ma_result result;
    ma_fence fence;
    ma_sound sounds[4];

    result = ma_fence_init(&fence);
    if (result != MA_SUCCESS) {
        return result;
    }

    // Load some sounds asynchronously.
    for (int iSound = 0; iSound < 4; iSound += 1) {
        ma_sound_init_from_file(&engine, mySoundFilesPaths[iSound], MA_SOUND_FLAG_DECODE | MA_SOUND_FLAG_ASYNC, pGroup, &fence, &sounds[iSound]);
    }

    // ... do some other stuff here in the mean time ...

    // Wait for all sounds to finish loading.
    ma_fence_wait(&fence);
    ```

If loading the entire sound into memory is prohibitive, you can also configure the engine to stream
the audio data:

    ```c
    ma_sound_init_from_file(&engine, "my_sound.wav", MA_SOUND_FLAG_STREAM, pGroup, NULL, &sound);
    ```

When streaming sounds, 2 seconds worth of audio data is stored in memory. Although it should work
fine, it's inefficient to use streaming for short sounds. Streaming is useful for things like music
tracks in games.

When loading a sound from a file path, the engine will reference count the file to prevent it from
being loaded if it's already in memory. When you uninitialize a sound, the reference count will be
decremented, and if it hits zero, the sound will be unloaded from memory. This reference counting
system is not used for streams. The engine will use a 64-bit hash of the file name when comparing
file paths which means there's a small chance you might encounter a name collision. If this is an
issue, you'll need to use a different name for one of the colliding file paths, or just not load
from files and instead load from a data source.

You can use `ma_sound_init_copy()` to initialize a copy of another sound. Note, however, that this
only works for sounds that were initialized with `ma_sound_init_from_file()` and without the
`MA_SOUND_FLAG_STREAM` flag.

When you initialize a sound, if you specify a sound group the sound will be attached to that group
automatically. If you set it to NULL, it will be automatically attached to the engine's endpoint.
If you would instead rather leave the sound unattached by default, you can specify the
`MA_SOUND_FLAG_NO_DEFAULT_ATTACHMENT` flag. This is useful if you want to set up a complex node
graph.

Sounds are not started by default. To start a sound, use `ma_sound_start()`. Stop a sound with
`ma_sound_stop()`.

Sounds can have their volume controlled with `ma_sound_set_volume()` in the same way as the
engine's master volume.

Sounds support stereo panning and pitching. Set the pan with `ma_sound_set_pan()`. Setting the pan
to 0 will result in an unpanned sound. Setting it to -1 will shift everything to the left, whereas
+1 will shift it to the right. The pitch can be controlled with `ma_sound_set_pitch()`. A larger
value will result in a higher pitch. The pitch must be greater than 0.

The engine supports 3D spatialization of sounds. By default sounds will have spatialization
enabled, but if a sound does not need to be spatialized it's best to disable it. There are two ways
to disable spatialization of a sound:

    ```c
    // Disable spatialization at initialization time via a flag:
    ma_sound_init_from_file(&engine, "my_sound.wav", MA_SOUND_FLAG_NO_SPATIALIZATION, NULL, NULL, &sound);

    // Dynamically disable or enable spatialization post-initialization:
    ma_sound_set_spatialization_enabled(&sound, isSpatializationEnabled);
    ```

By default sounds will be spatialized based on the closest listener. If a sound should always be
spatialized relative to a specific listener it can be pinned to one:

    ```c
    ma_sound_set_pinned_listener_index(&sound, listenerIndex);
    ```

Like listeners, sounds have a position. By default, the position of a sound is in absolute space,
but it can be changed to be relative to a listener:

    ```c
    ma_sound_set_positioning(&sound, ma_positioning_relative);
    ```

Note that relative positioning of a sound only makes sense if there is either only one listener, or
the sound is pinned to a specific listener. To set the position of a sound:

    ```c
    ma_sound_set_position(&sound, posX, posY, posZ);
    ```

The direction works the same way as a listener and represents the sound's forward direction:

    ```c
    ma_sound_set_direction(&sound, forwardX, forwardY, forwardZ);
    ```

Sound's also have a cone for controlling directional attenuation. This works exactly the same as
listeners:

    ```c
    ma_sound_set_cone(&sound, innerAngleInRadians, outerAngleInRadians, outerGain);
    ```

The velocity of a sound is used for doppler effect and can be set as such:

    ```c
    ma_sound_set_velocity(&sound, velocityX, velocityY, velocityZ);
    ```

The engine supports different attenuation models which can be configured on a per-sound basis. By
default the attenuation model is set to `ma_attenuation_model_inverse` which is the equivalent to
OpenAL's `AL_INVERSE_DISTANCE_CLAMPED`. Configure the attenuation model like so:

    ```c
    ma_sound_set_attenuation_model(&sound, ma_attenuation_model_inverse);
    ```

The supported attenuation models include the following:

    +----------------------------------+----------------------------------------------+
    | ma_attenuation_model_none        | No distance attenuation.                     |
    +----------------------------------+----------------------------------------------+
    | ma_attenuation_model_inverse     | Equivalent to `AL_INVERSE_DISTANCE_CLAMPED`. |
    +----------------------------------+----------------------------------------------+
    | ma_attenuation_model_linear      | Linear attenuation.                          |
    +----------------------------------+----------------------------------------------+
    | ma_attenuation_model_exponential | Exponential attenuation.                     |
    +----------------------------------+----------------------------------------------+

To control how quickly a sound rolls off as it moves away from the listener, you need to configure
the rolloff:

    ```c
    ma_sound_set_rolloff(&sound, rolloff);
    ```

You can control the minimum and maximum gain to apply from spatialization:

    ```c
    ma_sound_set_min_gain(&sound, minGain);
    ma_sound_set_max_gain(&sound, maxGain);
    ```

Likewise, in the calculation of attenuation, you can control the minimum and maximum distances for
the attenuation calculation. This is useful if you want to ensure sounds don't drop below a certain
volume after the listener moves further away and to have sounds play a maximum volume when the
listener is within a certain distance:

    ```c
    ma_sound_set_min_distance(&sound, minDistance);
    ma_sound_set_max_distance(&sound, maxDistance);
    ```

The engine's spatialization system supports doppler effect. The doppler factor can be configure on
a per-sound basis like so:

    ```c
    ma_sound_set_doppler_factor(&sound, dopplerFactor);
    ```

You can fade sounds in and out with `ma_sound_set_fade_in_pcm_frames()` and
`ma_sound_set_fade_in_milliseconds()`. Set the volume to -1 to use the current volume as the
starting volume:

    ```c
    // Fade in over 1 second.
    ma_sound_set_fade_in_milliseconds(&sound, 0, 1, 1000);

    // ... sometime later ...

    // Fade out over 1 second, starting from the current volume.
    ma_sound_set_fade_in_milliseconds(&sound, -1, 0, 1000);
    ```

By default sounds will start immediately, but sometimes for timing and synchronization purposes it
can be useful to schedule a sound to start or stop:

    ```c
    // Start the sound in 1 second from now.
    ma_sound_set_start_time_in_pcm_frames(&sound, ma_engine_get_time_in_pcm_frames(&engine) + (ma_engine_get_sample_rate(&engine) * 1));

    // Stop the sound in 2 seconds from now.
    ma_sound_set_stop_time_in_pcm_frames(&sound, ma_engine_get_time_in_pcm_frames(&engine) + (ma_engine_get_sample_rate(&engine) * 2));
    ```

Note that scheduling a start time still requires an explicit call to `ma_sound_start()` before
anything will play.

The time is specified in global time which is controlled by the engine. You can get the engine's
current time with `ma_engine_get_time_in_pcm_frames()`. The engine's global time is incremented
automatically as audio data is read, but it can be reset with `ma_engine_set_time_in_pcm_frames()`
in case it needs to be resynchronized for some reason.

To determine whether or not a sound is currently playing, use `ma_sound_is_playing()`. This will
take the scheduled start and stop times into account.

Whether or not a sound should loop can be controlled with `ma_sound_set_looping()`. Sounds will not
be looping by default. Use `ma_sound_is_looping()` to determine whether or not a sound is looping.

Use `ma_sound_at_end()` to determine whether or not a sound is currently at the end. For a looping
sound this should never return true. Alternatively, you can configure a callback that will be fired
when the sound reaches the end. Note that the callback is fired from the audio thread which means
you cannot be uninitializing sound from the callback. To set the callback you can use
`ma_sound_set_end_callback()`. Alternatively, if you're using `ma_sound_init_ex()`, you can pass it
into the config like so:

    ```c
    soundConfig.endCallback = my_end_callback;
    soundConfig.pEndCallbackUserData = pMyEndCallbackUserData;
    ```

The end callback is declared like so:

    ```c
    void my_end_callback(void* pUserData, ma_sound* pSound)
    {
        ...
    }
    ```

Internally a sound wraps around a data source. Some APIs exist to control the underlying data
source, mainly for convenience:

    ```c
    ma_sound_seek_to_pcm_frame(&sound, frameIndex);
    ma_sound_get_data_format(&sound, &format, &channels, &sampleRate, pChannelMap, channelMapCapacity);
    ma_sound_get_cursor_in_pcm_frames(&sound, &cursor);
    ma_sound_get_length_in_pcm_frames(&sound, &length);
    ```

Sound groups have the same API as sounds, only they are called `ma_sound_group`, and since they do
not have any notion of a data source, anything relating to a data source is unavailable.

Internally, sound data is loaded via the `ma_decoder` API which means by default it only supports
file formats that have built-in support in miniaudio. You can extend this to support any kind of
file format through the use of custom decoders. To do this you'll need to use a self-managed
resource manager and configure it appropriately. See the "Resource Management" section below for
details on how to set this up.


6. Resource Management
======================
Many programs will want to manage sound resources for things such as reference counting and
streaming. This is supported by miniaudio via the `ma_resource_manager` API.

The resource manager is mainly responsible for the following:

  * Loading of sound files into memory with reference counting.
  * Streaming of sound data.

When loading a sound file, the resource manager will give you back a `ma_data_source` compatible
object called `ma_resource_manager_data_source`. This object can be passed into any
`ma_data_source` API which is how you can read and seek audio data. When loading a sound file, you
specify whether or not you want the sound to be fully loaded into memory (and optionally
pre-decoded) or streamed. When loading into memory, you can also specify whether or not you want
the data to be loaded asynchronously.

The example below is how you can initialize a resource manager using it's default configuration:

    ```c
    ma_resource_manager_config config;
    ma_resource_manager resourceManager;

    config = ma_resource_manager_config_init();
    result = ma_resource_manager_init(&config, &resourceManager);
    if (result != MA_SUCCESS) {
        ma_device_uninit(&device);
        printf("Failed to initialize the resource manager.");
        return -1;
    }
    ```

You can configure the format, channels and sample rate of the decoded audio data. By default it
will use the file's native data format, but you can configure it to use a consistent format. This
is useful for offloading the cost of data conversion to load time rather than dynamically
converting at mixing time. To do this, you configure the decoded format, channels and sample rate
like the code below:

    ```c
    config = ma_resource_manager_config_init();
    config.decodedFormat     = device.playback.format;
    config.decodedChannels   = device.playback.channels;
    config.decodedSampleRate = device.sampleRate;
    ```

In the code above, the resource manager will be configured so that any decoded audio data will be
pre-converted at load time to the device's native data format. If instead you used defaults and
the data format of the file did not match the device's data format, you would need to convert the
data at mixing time which may be prohibitive in high-performance and large scale scenarios like
games.

Internally the resource manager uses the `ma_decoder` API to load sounds. This means by default it
only supports decoders that are built into miniaudio. It's possible to support additional encoding
formats through the use of custom decoders. To do so, pass in your `ma_decoding_backend_vtable`
vtables into the resource manager config:

    ```c
    ma_decoding_backend_vtable* pCustomBackendVTables[] =
    {
        &g_ma_decoding_backend_vtable_libvorbis,
        &g_ma_decoding_backend_vtable_libopus
    };

    ...

    resourceManagerConfig.ppCustomDecodingBackendVTables = pCustomBackendVTables;
    resourceManagerConfig.customDecodingBackendCount     = sizeof(pCustomBackendVTables) / sizeof(pCustomBackendVTables[0]);
    resourceManagerConfig.pCustomDecodingBackendUserData = NULL;
    ```

This system can allow you to support any kind of file format. See the "Decoding" section for
details on how to implement custom decoders. The miniaudio repository includes examples for Opus
via libopus and libopusfile and Vorbis via libvorbis and libvorbisfile.

Asynchronicity is achieved via a job system. When an operation needs to be performed, such as the
decoding of a page, a job will be posted to a queue which will then be processed by a job thread.
By default there will be only one job thread running, but this can be configured, like so:

    ```c
    config = ma_resource_manager_config_init();
    config.jobThreadCount = MY_JOB_THREAD_COUNT;
    ```

By default job threads are managed internally by the resource manager, however you can also self
manage your job threads if, for example, you want to integrate the job processing into your
existing job infrastructure, or if you simply don't like the way the resource manager does it. To
do this, just set the job thread count to 0 and process jobs manually. To process jobs, you first
need to retrieve a job using `ma_resource_manager_next_job()` and then process it using
`ma_job_process()`:

    ```c
    config = ma_resource_manager_config_init();
    config.jobThreadCount = 0;                            // Don't manage any job threads internally.
    config.flags = MA_RESOURCE_MANAGER_FLAG_NON_BLOCKING; // Optional. Makes `ma_resource_manager_next_job()` non-blocking.

    // ... Initialize your custom job threads ...

    void my_custom_job_thread(...)
    {
        for (;;) {
            ma_job job;
            ma_result result = ma_resource_manager_next_job(pMyResourceManager, &job);
            if (result != MA_SUCCESS) {
                if (result == MA_NO_DATA_AVAILABLE) {
                    // No jobs are available. Keep going. Will only get this if the resource manager was initialized
                    // with MA_RESOURCE_MANAGER_FLAG_NON_BLOCKING.
                    continue;
                } else if (result == MA_CANCELLED) {
                    // MA_JOB_TYPE_QUIT was posted. Exit.
                    break;
                } else {
                    // Some other error occurred.
                    break;
                }
            }

            ma_job_process(&job);
        }
    }
    ```

In the example above, the `MA_JOB_TYPE_QUIT` event is the used as the termination
indicator, but you can use whatever you would like to terminate the thread. The call to
`ma_resource_manager_next_job()` is blocking by default, but can be configured to be non-blocking
by initializing the resource manager with the `MA_RESOURCE_MANAGER_FLAG_NON_BLOCKING` configuration
flag. Note that the `MA_JOB_TYPE_QUIT` will never be removed from the job queue. This
is to give every thread the opportunity to catch the event and terminate naturally.

When loading a file, it's sometimes convenient to be able to customize how files are opened and
read instead of using standard `fopen()`, `fclose()`, etc. which is what miniaudio will use by
default. This can be done by setting `pVFS` member of the resource manager's config:

    ```c
    // Initialize your custom VFS object. See documentation for VFS for information on how to do this.
    my_custom_vfs vfs = my_custom_vfs_init();

    config = ma_resource_manager_config_init();
    config.pVFS = &vfs;
    ```

This is particularly useful in programs like games where you want to read straight from an archive
rather than the normal file system. If you do not specify a custom VFS, the resource manager will
use the operating system's normal file operations.

To load a sound file and create a data source, call `ma_resource_manager_data_source_init()`. When
loading a sound you need to specify the file path and options for how the sounds should be loaded.
By default a sound will be loaded synchronously. The returned data source is owned by the caller
which means the caller is responsible for the allocation and freeing of the data source. Below is
an example for initializing a data source:

    ```c
    ma_resource_manager_data_source dataSource;
    ma_result result = ma_resource_manager_data_source_init(pResourceManager, pFilePath, flags, &dataSource);
    if (result != MA_SUCCESS) {
        // Error.
    }

    // ...

    // A ma_resource_manager_data_source object is compatible with the `ma_data_source` API. To read data, just call
    // the `ma_data_source_read_pcm_frames()` like you would with any normal data source.
    result = ma_data_source_read_pcm_frames(&dataSource, pDecodedData, frameCount, &framesRead);
    if (result != MA_SUCCESS) {
        // Failed to read PCM frames.
    }

    // ...

    ma_resource_manager_data_source_uninit(&dataSource);
    ```

The `flags` parameter specifies how you want to perform loading of the sound file. It can be a
combination of the following flags:

    ```
    MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_STREAM
    MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_DECODE
    MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_ASYNC
    MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_WAIT_INIT
    MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_LOOPING
    ```

When no flags are specified (set to 0), the sound will be fully loaded into memory, but not
decoded, meaning the raw file data will be stored in memory, and then dynamically decoded when
`ma_data_source_read_pcm_frames()` is called. To instead decode the audio data before storing it in
memory, use the `MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_DECODE` flag. By default, the sound file will
be loaded synchronously, meaning `ma_resource_manager_data_source_init()` will only return after
the entire file has been loaded. This is good for simplicity, but can be prohibitively slow. You
can instead load the sound asynchronously using the `MA_RESOURCE_MANAGER_DATA_SOURCE_ASYNC` flag.
This will result in `ma_resource_manager_data_source_init()` returning quickly, but no data will be
returned by `ma_data_source_read_pcm_frames()` until some data is available. When no data is
available because the asynchronous decoding hasn't caught up, `MA_BUSY` will be returned by
`ma_data_source_read_pcm_frames()`.

For large sounds, it's often prohibitive to store the entire file in memory. To mitigate this, you
can instead stream audio data which you can do by specifying the
`MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_STREAM` flag. When streaming, data will be decoded in 1
second pages. When a new page needs to be decoded, a job will be posted to the job queue and then
subsequently processed in a job thread.

The `MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_LOOPING` flag can be used so that the sound will loop
when it reaches the end by default. It's recommended you use this flag when you want to have a
looping streaming sound. If you try loading a very short sound as a stream, you will get a glitch.
This is because the resource manager needs to pre-fill the initial buffer at initialization time,
and if you don't specify the `MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_LOOPING` flag, the resource
manager will assume the sound is not looping and will stop filling the buffer when it reaches the
end, therefore resulting in a discontinuous buffer.

For in-memory sounds, reference counting is used to ensure the data is loaded only once. This means
multiple calls to `ma_resource_manager_data_source_init()` with the same file path will result in
the file data only being loaded once. Each call to `ma_resource_manager_data_source_init()` must be
matched up with a call to `ma_resource_manager_data_source_uninit()`. Sometimes it can be useful
for a program to register self-managed raw audio data and associate it with a file path. Use the
`ma_resource_manager_register_*()` and `ma_resource_manager_unregister_*()` APIs to do this.
`ma_resource_manager_register_decoded_data()` is used to associate a pointer to raw, self-managed
decoded audio data in the specified data format with the specified name. Likewise,
`ma_resource_manager_register_encoded_data()` is used to associate a pointer to raw self-managed
encoded audio data (the raw file data) with the specified name. Note that these names need not be
actual file paths. When `ma_resource_manager_data_source_init()` is called (without the
`MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_STREAM` flag), the resource manager will look for these
explicitly registered data buffers and, if found, will use it as the backing data for the data
source. Note that the resource manager does *not* make a copy of this data so it is up to the
caller to ensure the pointer stays valid for its lifetime. Use
`ma_resource_manager_unregister_data()` to unregister the self-managed data. You can also use
`ma_resource_manager_register_file()` and `ma_resource_manager_unregister_file()` to register and
unregister a file. It does not make sense to use the `MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_STREAM`
flag with a self-managed data pointer.


6.1. Asynchronous Loading and Synchronization
---------------------------------------------
When loading asynchronously, it can be useful to poll whether or not loading has finished. Use
`ma_resource_manager_data_source_result()` to determine this. For in-memory sounds, this will
return `MA_SUCCESS` when the file has been *entirely* decoded. If the sound is still being decoded,
`MA_BUSY` will be returned. Otherwise, some other error code will be returned if the sound failed
to load. For streaming data sources, `MA_SUCCESS` will be returned when the first page has been
decoded and the sound is ready to be played. If the first page is still being decoded, `MA_BUSY`
will be returned. Otherwise, some other error code will be returned if the sound failed to load.

In addition to polling, you can also use a simple synchronization object called a "fence" to wait
for asynchronously loaded sounds to finish. This is called `ma_fence`. The advantage to using a
fence is that it can be used to wait for a group of sounds to finish loading rather than waiting
for sounds on an individual basis. There are two stages to loading a sound:

  * Initialization of the internal decoder; and
  * Completion of decoding of the file (the file is fully decoded)

You can specify separate fences for each of the different stages. Waiting for the initialization
of the internal decoder is important for when you need to know the sample format, channels and
sample rate of the file.

The example below shows how you could use a fence when loading a number of sounds:

    ```c
    // This fence will be released when all sounds are finished loading entirely.
    ma_fence fence;
    ma_fence_init(&fence);

    // This will be passed into the initialization routine for each sound.
    ma_resource_manager_pipeline_notifications notifications = ma_resource_manager_pipeline_notifications_init();
    notifications.done.pFence = &fence;

    // Now load a bunch of sounds:
    for (iSound = 0; iSound < soundCount; iSound += 1) {
        ma_resource_manager_data_source_init(pResourceManager, pSoundFilePaths[iSound], flags, &notifications, &pSoundSources[iSound]);
    }

    // ... DO SOMETHING ELSE WHILE SOUNDS ARE LOADING ...

    // Wait for loading of sounds to finish.
    ma_fence_wait(&fence);
    ```

In the example above we used a fence for waiting until the entire file has been fully decoded. If
you only need to wait for the initialization of the internal decoder to complete, you can use the
`init` member of the `ma_resource_manager_pipeline_notifications` object:

    ```c
    notifications.init.pFence = &fence;
    ```

If a fence is not appropriate for your situation, you can instead use a callback that is fired on
an individual sound basis. This is done in a very similar way to fences:

    ```c
    typedef struct
    {
        ma_async_notification_callbacks cb;
        void* pMyData;
    } my_notification;

    void my_notification_callback(ma_async_notification* pNotification)
    {
        my_notification* pMyNotification = (my_notification*)pNotification;

        // Do something in response to the sound finishing loading.
    }

    ...

    my_notification myCallback;
    myCallback.cb.onSignal = my_notification_callback;
    myCallback.pMyData     = pMyData;

    ma_resource_manager_pipeline_notifications notifications = ma_resource_manager_pipeline_notifications_init();
    notifications.done.pNotification = &myCallback;

    ma_resource_manager_data_source_init(pResourceManager, "my_sound.wav", flags, &notifications, &mySound);
    ```

In the example above we just extend the `ma_async_notification_callbacks` object and pass an
instantiation into the `ma_resource_manager_pipeline_notifications` in the same way as we did with
the fence, only we set `pNotification` instead of `pFence`. You can set both of these at the same
time and they should both work as expected. If using the `pNotification` system, you need to ensure
your `ma_async_notification_callbacks` object stays valid.



6.2. Resource Manager Implementation Details
--------------------------------------------
Resources are managed in two main ways:

  * By storing the entire sound inside an in-memory buffer (referred to as a data buffer)
  * By streaming audio data on the fly (referred to as a data stream)

A resource managed data source (`ma_resource_manager_data_source`) encapsulates a data buffer or
data stream, depending on whether or not the data source was initialized with the
`MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_STREAM` flag. If so, it will make use of a
`ma_resource_manager_data_stream` object. Otherwise it will use a `ma_resource_manager_data_buffer`
object. Both of these objects are data sources which means they can be used with any
`ma_data_source_*()` API.

Another major feature of the resource manager is the ability to asynchronously decode audio files.
This relieves the audio thread of time-consuming decoding which can negatively affect scalability
due to the audio thread needing to complete it's work extremely quickly to avoid glitching.
Asynchronous decoding is achieved through a job system. There is a central multi-producer,
multi-consumer, fixed-capacity job queue. When some asynchronous work needs to be done, a job is
posted to the queue which is then read by a job thread. The number of job threads can be
configured for improved scalability, and job threads can all run in parallel without needing to
worry about the order of execution (how this is achieved is explained below).

When a sound is being loaded asynchronously, playback can begin before the sound has been fully
decoded. This enables the application to start playback of the sound quickly, while at the same
time allowing to resource manager to keep loading in the background. Since there may be less
threads than the number of sounds being loaded at a given time, a simple scheduling system is used
to keep decoding time balanced and fair. The resource manager solves this by splitting decoding
into chunks called pages. By default, each page is 1 second long. When a page has been decoded, a
new job will be posted to start decoding the next page. By dividing up decoding into pages, an
individual sound shouldn't ever delay every other sound from having their first page decoded. Of
course, when loading many sounds at the same time, there will always be an amount of time required
to process jobs in the queue so in heavy load situations there will still be some delay. To
determine if a data source is ready to have some frames read, use
`ma_resource_manager_data_source_get_available_frames()`. This will return the number of frames
available starting from the current position.


6.2.1. Job Queue
----------------
The resource manager uses a job queue which is multi-producer, multi-consumer, and fixed-capacity.
This job queue is not currently lock-free, and instead uses a spinlock to achieve thread-safety.
Only a fixed number of jobs can be allocated and inserted into the queue which is done through a
lock-free data structure for allocating an index into a fixed sized array, with reference counting
for mitigation of the ABA problem. The reference count is 32-bit.

For many types of jobs it's important that they execute in a specific order. In these cases, jobs
are executed serially. For the resource manager, serial execution of jobs is only required on a
per-object basis (per data buffer or per data stream). Each of these objects stores an execution
counter. When a job is posted it is associated with an execution counter. When the job is
processed, it checks if the execution counter of the job equals the execution counter of the
owning object and if so, processes the job. If the counters are not equal, the job will be posted
back onto the job queue for later processing. When the job finishes processing the execution order
of the main object is incremented. This system means the no matter how many job threads are
executing, decoding of an individual sound will always get processed serially. The advantage to
having multiple threads comes into play when loading multiple sounds at the same time.

The resource manager's job queue is not 100% lock-free and will use a spinlock to achieve
thread-safety for a very small section of code. This is only relevant when the resource manager
uses more than one job thread. If only using a single job thread, which is the default, the
lock should never actually wait in practice. The amount of time spent locking should be quite
short, but it's something to be aware of for those who have pedantic lock-free requirements and
need to use more than one job thread. There are plans to remove this lock in a future version.

In addition, posting a job will release a semaphore, which on Win32 is implemented with
`ReleaseSemaphore` and on POSIX platforms via a condition variable:

    ```c
    pthread_mutex_lock(&pSemaphore->lock);
    {
        pSemaphore->value += 1;
        pthread_cond_signal(&pSemaphore->cond);
    }
    pthread_mutex_unlock(&pSemaphore->lock);
    ```

Again, this is relevant for those with strict lock-free requirements in the audio thread. To avoid
this, you can use non-blocking mode (via the `MA_JOB_QUEUE_FLAG_NON_BLOCKING`
flag) and implement your own job processing routine (see the "Resource Manager" section above for
details on how to do this).



6.2.2. Data Buffers
-------------------
When the `MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_STREAM` flag is excluded at initialization time, the
resource manager will try to load the data into an in-memory data buffer. Before doing so, however,
it will first check if the specified file is already loaded. If so, it will increment a reference
counter and just use the already loaded data. This saves both time and memory. When the data buffer
is uninitialized, the reference counter will be decremented. If the counter hits zero, the file
will be unloaded. This is a detail to keep in mind because it could result in excessive loading and
unloading of a sound. For example, the following sequence will result in a file be loaded twice,
once after the other:

    ```c
    ma_resource_manager_data_source_init(pResourceManager, "my_file", ..., &myDataBuffer0); // Refcount = 1. Initial load.
    ma_resource_manager_data_source_uninit(&myDataBuffer0);                                 // Refcount = 0. Unloaded.

    ma_resource_manager_data_source_init(pResourceManager, "my_file", ..., &myDataBuffer1); // Refcount = 1. Reloaded because previous uninit() unloaded it.
    ma_resource_manager_data_source_uninit(&myDataBuffer1);                                 // Refcount = 0. Unloaded.
    ```

A binary search tree (BST) is used for storing data buffers as it has good balance between
efficiency and simplicity. The key of the BST is a 64-bit hash of the file path that was passed
into `ma_resource_manager_data_source_init()`. The advantage of using a hash is that it saves
memory over storing the entire path, has faster comparisons, and results in a mostly balanced BST
due to the random nature of the hash. The disadvantages are that file names are case-sensitive and
there's a small chance of name collisions. If case-sensitivity is an issue, you should normalize
your file names to upper- or lower-case before initializing your data sources. If name collisions
become an issue, you'll need to change the name of one of the colliding names or just not use the
resource manager.

When a sound file has not already been loaded and the `MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_ASYNC`
flag is excluded, the file will be decoded synchronously by the calling thread. There are two
options for controlling how the audio is stored in the data buffer - encoded or decoded. When the
`MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_DECODE` option is excluded, the raw file data will be stored
in memory. Otherwise the sound will be decoded before storing it in memory. Synchronous loading is
a very simple and standard process of simply adding an item to the BST, allocating a block of
memory and then decoding (if `MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_DECODE` is specified).

When the `MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_ASYNC` flag is specified, loading of the data buffer
is done asynchronously. In this case, a job is posted to the queue to start loading and then the
function immediately returns, setting an internal result code to `MA_BUSY`. This result code is
returned when the program calls `ma_resource_manager_data_source_result()`. When decoding has fully
completed `MA_SUCCESS` will be returned. This can be used to know if loading has fully completed.

When loading asynchronously, a single job is posted to the queue of the type
`MA_JOB_TYPE_RESOURCE_MANAGER_LOAD_DATA_BUFFER_NODE`. This involves making a copy of the file path and
associating it with job. When the job is processed by the job thread, it will first load the file
using the VFS associated with the resource manager. When using a custom VFS, it's important that it
be completely thread-safe because it will be used from one or more job threads at the same time.
Individual files should only ever be accessed by one thread at a time, however. After opening the
file via the VFS, the job will determine whether or not the file is being decoded. If not, it
simply allocates a block of memory and loads the raw file contents into it and returns. On the
other hand, when the file is being decoded, it will first allocate a decoder on the heap and
initialize it. Then it will check if the length of the file is known. If so it will allocate a
block of memory to store the decoded output and initialize it to silence. If the size is unknown,
it will allocate room for one page. After memory has been allocated, the first page will be
decoded. If the sound is shorter than a page, the result code will be set to `MA_SUCCESS` and the
completion event will be signalled and loading is now complete. If, however, there is more to
decode, a job with the code `MA_JOB_TYPE_RESOURCE_MANAGER_PAGE_DATA_BUFFER_NODE` is posted. This job
will decode the next page and perform the same process if it reaches the end. If there is more to
decode, the job will post another `MA_JOB_TYPE_RESOURCE_MANAGER_PAGE_DATA_BUFFER_NODE` job which will
keep on happening until the sound has been fully decoded. For sounds of an unknown length, each
page will be linked together as a linked list. Internally this is implemented via the
`ma_paged_audio_buffer` object.


6.2.3. Data Streams
-------------------
Data streams only ever store two pages worth of data for each instance. They are most useful for
large sounds like music tracks in games that would consume too much memory if fully decoded in
memory. After every frame from a page has been read, a job will be posted to load the next page
which is done from the VFS.

For data streams, the `MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_ASYNC` flag will determine whether or
not initialization of the data source waits until the two pages have been decoded. When unset,
`ma_resource_manager_data_source_init()` will wait until the two pages have been loaded, otherwise
it will return immediately.

When frames are read from a data stream using `ma_resource_manager_data_source_read_pcm_frames()`,
`MA_BUSY` will be returned if there are no frames available. If there are some frames available,
but less than the number requested, `MA_SUCCESS` will be returned, but the actual number of frames
read will be less than the number requested. Due to the asynchronous nature of data streams,
seeking is also asynchronous. If the data stream is in the middle of a seek, `MA_BUSY` will be
returned when trying to read frames.

When `ma_resource_manager_data_source_read_pcm_frames()` results in a page getting fully consumed
a job is posted to load the next page. This will be posted from the same thread that called
`ma_resource_manager_data_source_read_pcm_frames()`.

Data streams are uninitialized by posting a job to the queue, but the function won't return until
that job has been processed. The reason for this is that the caller owns the data stream object and
therefore miniaudio needs to ensure everything completes before handing back control to the caller.
Also, if the data stream is uninitialized while pages are in the middle of decoding, they must
complete before destroying any underlying object and the job system handles this cleanly.

Note that when a new page needs to be loaded, a job will be posted to the resource manager's job
thread from the audio thread. You must keep in mind the details mentioned in the "Job Queue"
section above regarding locking when posting an event if you require a strictly lock-free audio
thread.



7. Node Graph
=============
miniaudio's routing infrastructure follows a node graph paradigm. The idea is that you create a
node whose outputs are attached to inputs of another node, thereby creating a graph. There are
different types of nodes, with each node in the graph processing input data to produce output,
which is then fed through the chain. Each node in the graph can apply their own custom effects. At
the start of the graph will usually be one or more data source nodes which have no inputs and
instead pull their data from a data source. At the end of the graph is an endpoint which represents
the end of the chain and is where the final output is ultimately extracted from.

Each node has a number of input buses and a number of output buses. An output bus from a node is
attached to an input bus of another. Multiple nodes can connect their output buses to another
node's input bus, in which case their outputs will be mixed before processing by the node. Below is
a diagram that illustrates a hypothetical node graph setup:

    ```
    >>>>>>>>>>>>>>>>>>>>>>>>>>>>>> Data flows left to right >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

    +---------------+                              +-----------------+
    | Data Source 1 =----+    +----------+    +----= Low Pass Filter =----+
    +---------------+    |    |          =----+    +-----------------+    |    +----------+
                         +----= Splitter |                                +----= ENDPOINT |
    +---------------+    |    |          =----+    +-----------------+    |    +----------+
    | Data Source 2 =----+    +----------+    +----=  Echo / Delay   =----+
    +---------------+                              +-----------------+
    ```

In the above graph, it starts with two data sources whose outputs are attached to the input of a
splitter node. It's at this point that the two data sources are mixed. After mixing, the splitter
performs it's processing routine and produces two outputs which is simply a duplication of the
input stream. One output is attached to a low pass filter, whereas the other output is attached to
a echo/delay. The outputs of the low pass filter and the echo are attached to the endpoint, and
since they're both connected to the same input bus, they'll be mixed.

Each input bus must be configured to accept the same number of channels, but the number of channels
used by input buses can be different to the number of channels for output buses in which case
miniaudio will automatically convert the input data to the output channel count before processing.
The number of channels of an output bus of one node must match the channel count of the input bus
it's attached to. The channel counts cannot be changed after the node has been initialized. If you
attempt to attach an output bus to an input bus with a different channel count, attachment will
fail.

To use a node graph, you first need to initialize a `ma_node_graph` object. This is essentially a
container around the entire graph. The `ma_node_graph` object is required for some thread-safety
issues which will be explained later. A `ma_node_graph` object is initialized using miniaudio's
standard config/init system:

    ```c
    ma_node_graph_config nodeGraphConfig = ma_node_graph_config_init(myChannelCount);

    result = ma_node_graph_init(&nodeGraphConfig, NULL, &nodeGraph);    // Second parameter is a pointer to allocation callbacks.
    if (result != MA_SUCCESS) {
        // Failed to initialize node graph.
    }
    ```

When you initialize the node graph, you're specifying the channel count of the endpoint. The
endpoint is a special node which has one input bus and one output bus, both of which have the
same channel count, which is specified in the config. Any nodes that connect directly to the
endpoint must be configured such that their output buses have the same channel count. When you read
audio data from the node graph, it'll have the channel count you specified in the config. To read
data from the graph:

    ```c
    ma_uint32 framesRead;
    result = ma_node_graph_read_pcm_frames(&nodeGraph, pFramesOut, frameCount, &framesRead);
    if (result != MA_SUCCESS) {
        // Failed to read data from the node graph.
    }
    ```

When you read audio data, miniaudio starts at the node graph's endpoint node which then pulls in
data from its input attachments, which in turn recursively pull in data from their inputs, and so
on. At the start of the graph there will be some kind of data source node which will have zero
inputs and will instead read directly from a data source. The base nodes don't literally need to
read from a `ma_data_source` object, but they will always have some kind of underlying object that
sources some kind of audio. The `ma_data_source_node` node can be used to read from a
`ma_data_source`. Data is always in floating-point format and in the number of channels you
specified when the graph was initialized. The sample rate is defined by the underlying data sources.
It's up to you to ensure they use a consistent and appropriate sample rate.

The `ma_node` API is designed to allow custom nodes to be implemented with relative ease, but
miniaudio includes a few stock nodes for common functionality. This is how you would initialize a
node which reads directly from a data source (`ma_data_source_node`) which is an example of one
of the stock nodes that comes with miniaudio:

    ```c
    ma_data_source_node_config config = ma_data_source_node_config_init(pMyDataSource);

    ma_data_source_node dataSourceNode;
    result = ma_data_source_node_init(&nodeGraph, &config, NULL, &dataSourceNode);
    if (result != MA_SUCCESS) {
        // Failed to create data source node.
    }
    ```

The data source node will use the output channel count to determine the channel count of the output
bus. There will be 1 output bus and 0 input buses (data will be drawn directly from the data
source). The data source must output to floating-point (`ma_format_f32`) or else an error will be
returned from `ma_data_source_node_init()`.

By default the node will not be attached to the graph. To do so, use `ma_node_attach_output_bus()`:

    ```c
    result = ma_node_attach_output_bus(&dataSourceNode, 0, ma_node_graph_get_endpoint(&nodeGraph), 0);
    if (result != MA_SUCCESS) {
        // Failed to attach node.
    }
    ```

The code above connects the data source node directly to the endpoint. Since the data source node
has only a single output bus, the index will always be 0. Likewise, the endpoint only has a single
input bus which means the input bus index will also always be 0.

To detach a specific output bus, use `ma_node_detach_output_bus()`. To detach all output buses, use
`ma_node_detach_all_output_buses()`. If you want to just move the output bus from one attachment to
another, you do not need to detach first. You can just call `ma_node_attach_output_bus()` and it'll
deal with it for you.

Less frequently you may want to create a specialized node. This will be a node where you implement
your own processing callback to apply a custom effect of some kind. This is similar to initializing
one of the stock node types, only this time you need to specify a pointer to a vtable containing a
pointer to the processing function and the number of input and output buses. Example:

    ```c
    static void my_custom_node_process_pcm_frames(ma_node* pNode, const float** ppFramesIn, ma_uint32* pFrameCountIn, float** ppFramesOut, ma_uint32* pFrameCountOut)
    {
        // Do some processing of ppFramesIn (one stream of audio data per input bus)
        const float* pFramesIn_0 = ppFramesIn[0]; // Input bus @ index 0.
        const float* pFramesIn_1 = ppFramesIn[1]; // Input bus @ index 1.
        float* pFramesOut_0 = ppFramesOut[0];     // Output bus @ index 0.

        // Do some processing. On input, `pFrameCountIn` will be the number of input frames in each
        // buffer in `ppFramesIn` and `pFrameCountOut` will be the capacity of each of the buffers
        // in `ppFramesOut`. On output, `pFrameCountIn` should be set to the number of input frames
        // your node consumed and `pFrameCountOut` should be set the number of output frames that
        // were produced.
        //
        // You should process as many frames as you can. If your effect consumes input frames at the
        // same rate as output frames (always the case, unless you're doing resampling), you need
        // only look at `ppFramesOut` and process that exact number of frames. If you're doing
        // resampling, you'll need to be sure to set both `pFrameCountIn` and `pFrameCountOut`
        // properly.
    }

    static ma_node_vtable my_custom_node_vtable =
    {
        my_custom_node_process_pcm_frames, // The function that will be called to process your custom node. This is where you'd implement your effect processing.
        NULL,   // Optional. A callback for calculating the number of input frames that are required to process a specified number of output frames.
        2,      // 2 input buses.
        1,      // 1 output bus.
        0       // Default flags.
    };

    ...

    // Each bus needs to have a channel count specified. To do this you need to specify the channel
    // counts in an array and then pass that into the node config.
    ma_uint32 inputChannels[2];     // Equal in size to the number of input channels specified in the vtable.
    ma_uint32 outputChannels[1];    // Equal in size to the number of output channels specified in the vtable.

    inputChannels[0]  = channelsIn;
    inputChannels[1]  = channelsIn;
    outputChannels[0] = channelsOut;

    ma_node_config nodeConfig = ma_node_config_init();
    nodeConfig.vtable          = &my_custom_node_vtable;
    nodeConfig.pInputChannels  = inputChannels;
    nodeConfig.pOutputChannels = outputChannels;

    ma_node_base node;
    result = ma_node_init(&nodeGraph, &nodeConfig, NULL, &node);
    if (result != MA_SUCCESS) {
        // Failed to initialize node.
    }
    ```

When initializing a custom node, as in the code above, you'll normally just place your vtable in
static space. The number of input and output buses are specified as part of the vtable. If you need
a variable number of buses on a per-node bases, the vtable should have the relevant bus count set
to `MA_NODE_BUS_COUNT_UNKNOWN`. In this case, the bus count should be set in the node config:

    ```c
    static ma_node_vtable my_custom_node_vtable =
    {
        my_custom_node_process_pcm_frames, // The function that will be called process your custom node. This is where you'd implement your effect processing.
        NULL,   // Optional. A callback for calculating the number of input frames that are required to process a specified number of output frames.
        MA_NODE_BUS_COUNT_UNKNOWN,  // The number of input buses is determined on a per-node basis.
        1,      // 1 output bus.
        0       // Default flags.
    };

    ...

    ma_node_config nodeConfig = ma_node_config_init();
    nodeConfig.vtable          = &my_custom_node_vtable;
    nodeConfig.inputBusCount   = myBusCount;        // <-- Since the vtable specifies MA_NODE_BUS_COUNT_UNKNOWN, the input bus count should be set here.
    nodeConfig.pInputChannels  = inputChannels;     // <-- Make sure there are nodeConfig.inputBusCount elements in this array.
    nodeConfig.pOutputChannels = outputChannels;    // <-- The vtable specifies 1 output bus, so there must be 1 element in this array.
    ```

In the above example it's important to never set the `inputBusCount` and `outputBusCount` members
to anything other than their defaults if the vtable specifies an explicit count. They can only be
set if the vtable specifies MA_NODE_BUS_COUNT_UNKNOWN in the relevant bus count.

Most often you'll want to create a structure to encapsulate your node with some extra data. You
need to make sure the `ma_node_base` object is your first member of the structure:

    ```c
    typedef struct
    {
        ma_node_base base; // <-- Make sure this is always the first member.
        float someCustomData;
    } my_custom_node;
    ```

By doing this, your object will be compatible with all `ma_node` APIs and you can attach it to the
graph just like any other node.

In the custom processing callback (`my_custom_node_process_pcm_frames()` in the example above), the
number of channels for each bus is what was specified by the config when the node was initialized
with `ma_node_init()`. In addition, all attachments to each of the input buses will have been
pre-mixed by miniaudio. The config allows you to specify different channel counts for each
individual input and output bus. It's up to the effect to handle it appropriate, and if it can't,
return an error in it's initialization routine.

Custom nodes can be assigned some flags to describe their behaviour. These are set via the vtable
and include the following:

    +-----------------------------------------+---------------------------------------------------+
    | Flag Name                               | Description                                       |
    +-----------------------------------------+---------------------------------------------------+
    | MA_NODE_FLAG_PASSTHROUGH                | Useful for nodes that do not do any kind of audio |
    |                                         | processing, but are instead used for tracking     |
    |                                         | time, handling events, etc. Also used by the      |
    |                                         | internal endpoint node. It reads directly from    |
    |                                         | the input bus to the output bus. Nodes with this  |
    |                                         | flag must have exactly 1 input bus and 1 output   |
    |                                         | bus, and both buses must have the same channel    |
    |                                         | counts.                                           |
    +-----------------------------------------+---------------------------------------------------+
    | MA_NODE_FLAG_CONTINUOUS_PROCESSING      | Causes the processing callback to be called even  |
    |                                         | when no data is available to be read from input   |
    |                                         | attachments. When a node has at least one input   |
    |                                         | bus, but there are no inputs attached or the      |
    |                                         | inputs do not deliver any data, the node's        |
    |                                         | processing callback will not get fired. This flag |
    |                                         | will make it so the callback is always fired      |
    |                                         | regardless of whether or not any input data is    |
    |                                         | received. This is useful for effects like         |
    |                                         | echos where there will be a tail of audio data    |
    |                                         | that still needs to be processed even when the    |
    |                                         | original data sources have reached their ends. It |
    |                                         | may also be useful for nodes that must always     |
    |                                         | have their processing callback fired when there   |
    |                                         | are no inputs attached.                           |
    +-----------------------------------------+---------------------------------------------------+
    | MA_NODE_FLAG_ALLOW_NULL_INPUT           | Used in conjunction with                          |
    |                                         | `MA_NODE_FLAG_CONTINUOUS_PROCESSING`. When this   |
    |                                         | is set, the `ppFramesIn` parameter of the         |
    |                                         | processing callback will be set to NULL when      |
    |                                         | there are no input frames are available. When     |
    |                                         | this is unset, silence will be posted to the      |
    |                                         | processing callback.                              |
    +-----------------------------------------+---------------------------------------------------+
    | MA_NODE_FLAG_DIFFERENT_PROCESSING_RATES | Used to tell miniaudio that input and output      |
    |                                         | frames are processed at different rates. You      |
    |                                         | should set this for any nodes that perform        |
    |                                         | resampling.                                       |
    +-----------------------------------------+---------------------------------------------------+
    | MA_NODE_FLAG_SILENT_OUTPUT              | Used to tell miniaudio that a node produces only  |
    |                                         | silent output. This is useful for nodes where you |
    |                                         | don't want the output to contribute to the final  |
    |                                         | mix. An example might be if you want split your   |
    |                                         | stream and have one branch be output to a file.   |
    |                                         | When using this flag, you should avoid writing to |
    |                                         | the output buffer of the node's processing        |
    |                                         | callback because miniaudio will ignore it anyway. |
    +-----------------------------------------+---------------------------------------------------+


If you need to make a copy of an audio stream for effect processing you can use a splitter node
called `ma_splitter_node`. This takes has 1 input bus and splits the stream into 2 output buses.
You can use it like this:

    ```c
    ma_splitter_node_config splitterNodeConfig = ma_splitter_node_config_init(channels);

    ma_splitter_node splitterNode;
    result = ma_splitter_node_init(&nodeGraph, &splitterNodeConfig, NULL, &splitterNode);
    if (result != MA_SUCCESS) {
        // Failed to create node.
    }

    // Attach your output buses to two different input buses (can be on two different nodes).
    ma_node_attach_output_bus(&splitterNode, 0, ma_node_graph_get_endpoint(&nodeGraph), 0); // Attach directly to the endpoint.
    ma_node_attach_output_bus(&splitterNode, 1, &myEffectNode,                          0); // Attach to input bus 0 of some effect node.
    ```

The volume of an output bus can be configured on a per-bus basis:

    ```c
    ma_node_set_output_bus_volume(&splitterNode, 0, 0.5f);
    ma_node_set_output_bus_volume(&splitterNode, 1, 0.5f);
    ```

In the code above we're using the splitter node from before and changing the volume of each of the
copied streams.

You can start and stop a node with the following:

    ```c
    ma_node_set_state(&splitterNode, ma_node_state_started);    // The default state.
    ma_node_set_state(&splitterNode, ma_node_state_stopped);
    ```

By default the node is in a started state, but since it won't be connected to anything won't
actually be invoked by the node graph until it's connected. When you stop a node, data will not be
read from any of its input connections. You can use this property to stop a group of sounds
atomically.

You can configure the initial state of a node in it's config:

    ```c
    nodeConfig.initialState = ma_node_state_stopped;
    ```

Note that for the stock specialized nodes, all of their configs will have a `nodeConfig` member
which is the config to use with the base node. This is where the initial state can be configured
for specialized nodes:

    ```c
    dataSourceNodeConfig.nodeConfig.initialState = ma_node_state_stopped;
    ```

When using a specialized node like `ma_data_source_node` or `ma_splitter_node`, be sure to not
modify the `vtable` member of the `nodeConfig` object.


7.1. Timing
-----------
The node graph supports starting and stopping nodes at scheduled times. This is especially useful
for data source nodes where you want to get the node set up, but only start playback at a specific
time. There are two clocks: local and global.

A local clock is per-node, whereas the global clock is per graph. Scheduling starts and stops can
only be done based on the global clock because the local clock will not be running while the node
is stopped. The global clocks advances whenever `ma_node_graph_read_pcm_frames()` is called. On the
other hand, the local clock only advances when the node's processing callback is fired, and is
advanced based on the output frame count.

To retrieve the global time, use `ma_node_graph_get_time()`. The global time can be set with
`ma_node_graph_set_time()` which might be useful if you want to do seeking on a global timeline.
Getting and setting the local time is similar. Use `ma_node_get_time()` to retrieve the local time,
and `ma_node_set_time()` to set the local time. The global and local times will be advanced by the
audio thread, so care should be taken to avoid data races. Ideally you should avoid calling these
outside of the node processing callbacks which are always run on the audio thread.

There is basic support for scheduling the starting and stopping of nodes. You can only schedule one
start and one stop at a time. This is mainly intended for putting nodes into a started or stopped
state in a frame-exact manner. Without this mechanism, starting and stopping of a node is limited
to the resolution of a call to `ma_node_graph_read_pcm_frames()` which would typically be in blocks
of several milliseconds. The following APIs can be used for scheduling node states:

    ```c
    ma_node_set_state_time()
    ma_node_get_state_time()
    ```

The time is absolute and must be based on the global clock. An example is below:

    ```c
    ma_node_set_state_time(&myNode, ma_node_state_started, sampleRate*1);   // Delay starting to 1 second.
    ma_node_set_state_time(&myNode, ma_node_state_stopped, sampleRate*5);   // Delay stopping to 5 seconds.
    ```

An example for changing the state using a relative time.

    ```c
    ma_node_set_state_time(&myNode, ma_node_state_started, sampleRate*1 + ma_node_graph_get_time(&myNodeGraph));
    ma_node_set_state_time(&myNode, ma_node_state_stopped, sampleRate*5 + ma_node_graph_get_time(&myNodeGraph));
    ```

Note that due to the nature of multi-threading the times may not be 100% exact. If this is an
issue, consider scheduling state changes from within a processing callback. An idea might be to
have some kind of passthrough trigger node that is used specifically for tracking time and handling
events.



7.2. Thread Safety and Locking
------------------------------
When processing audio, it's ideal not to have any kind of locking in the audio thread. Since it's
expected that `ma_node_graph_read_pcm_frames()` would be run on the audio thread, it does so
without the use of any locks. This section discusses the implementation used by miniaudio and goes
over some of the compromises employed by miniaudio to achieve this goal. Note that the current
implementation may not be ideal - feedback and critiques are most welcome.

The node graph API is not *entirely* lock-free. Only `ma_node_graph_read_pcm_frames()` is expected
to be lock-free. Attachment, detachment and uninitialization of nodes use locks to simplify the
implementation, but are crafted in a way such that such locking is not required when reading audio
data from the graph. Locking in these areas are achieved by means of spinlocks.

The main complication with keeping `ma_node_graph_read_pcm_frames()` lock-free stems from the fact
that a node can be uninitialized, and it's memory potentially freed, while in the middle of being
processed on the audio thread. There are times when the audio thread will be referencing a node,
which means the uninitialization process of a node needs to make sure it delays returning until the
audio thread is finished so that control is not handed back to the caller thereby giving them a
chance to free the node's memory.

When the audio thread is processing a node, it does so by reading from each of the output buses of
the node. In order for a node to process data for one of its output buses, it needs to read from
each of its input buses, and so on an so forth. It follows that once all output buses of a node
are detached, the node as a whole will be disconnected and no further processing will occur unless
it's output buses are reattached, which won't be happening when the node is being uninitialized.
By having `ma_node_detach_output_bus()` wait until the audio thread is finished with it, we can
simplify a few things, at the expense of making `ma_node_detach_output_bus()` a bit slower. By
doing this, the implementation of `ma_node_uninit()` becomes trivial - just detach all output
nodes, followed by each of the attachments to each of its input nodes, and then do any final clean
up.

With the above design, the worst-case scenario is `ma_node_detach_output_bus()` taking as long as
it takes to process the output bus being detached. This will happen if it's called at just the
wrong moment where the audio thread has just iterated it and has just started processing. The
caller of `ma_node_detach_output_bus()` will stall until the audio thread is finished, which
includes the cost of recursively processing its inputs. This is the biggest compromise made with
the approach taken by miniaudio for its lock-free processing system. The cost of detaching nodes
earlier in the pipeline (data sources, for example) will be cheaper than the cost of detaching
higher level nodes, such as some kind of final post-processing endpoint. If you need to do mass
detachments, detach starting from the lowest level nodes and work your way towards the final
endpoint node (but don't try detaching the node graph's endpoint). If the audio thread is not
running, detachment will be fast and detachment in any order will be the same. The reason nodes
need to wait for their input attachments to complete is due to the potential for desyncs between
data sources. If the node was to terminate processing mid way through processing its inputs,
there's a chance that some of the underlying data sources will have been read, but then others not.
That will then result in a potential desynchronization when detaching and reattaching higher-level
nodes. A possible solution to this is to have an option when detaching to terminate processing
before processing all input attachments which should be fairly simple.

Another compromise, albeit less significant, is locking when attaching and detaching nodes. This
locking is achieved by means of a spinlock in order to reduce memory overhead. A lock is present
for each input bus and output bus. When an output bus is connected to an input bus, both the output
bus and input bus is locked. This locking is specifically for attaching and detaching across
different threads and does not affect `ma_node_graph_read_pcm_frames()` in any way. The locking and
unlocking is mostly self-explanatory, but a slightly less intuitive aspect comes into it when
considering that iterating over attachments must not break as a result of attaching or detaching a
node while iteration is occurring.

Attaching and detaching are both quite simple. When an output bus of a node is attached to an input
bus of another node, it's added to a linked list. Basically, an input bus is a linked list, where
each item in the list is and output bus. We have some intentional (and convenient) restrictions on
what can done with the linked list in order to simplify the implementation. First of all, whenever
something needs to iterate over the list, it must do so in a forward direction. Backwards iteration
is not supported. Also, items can only be added to the start of the list.

The linked list is a doubly-linked list where each item in the list (an output bus) holds a pointer
to the next item in the list, and another to the previous item. A pointer to the previous item is
only required for fast detachment of the node - it is never used in iteration. This is an
important property because it means from the perspective of iteration, attaching and detaching of
an item can be done with a single atomic assignment. This is exploited by both the attachment and
detachment process. When attaching the node, the first thing that is done is the setting of the
local "next" and "previous" pointers of the node. After that, the item is "attached" to the list
by simply performing an atomic exchange with the head pointer. After that, the node is "attached"
to the list from the perspective of iteration. Even though the "previous" pointer of the next item
hasn't yet been set, from the perspective of iteration it's been attached because iteration will
only be happening in a forward direction which means the "previous" pointer won't actually ever get
used. The same general process applies to detachment. See `ma_node_attach_output_bus()` and
`ma_node_detach_output_bus()` for the implementation of this mechanism.



8. Decoding
===========
The `ma_decoder` API is used for reading audio files. Decoders are completely decoupled from
devices and can be used independently. Built-in support is included for the following formats:

    +---------+
    | Format  |
    +---------+
    | WAV     |
    | MP3     |
    | FLAC    |
    +---------+

You can disable the built-in decoders by specifying one or more of the following options before the
miniaudio implementation:

    ```c
    #define MA_NO_WAV
    #define MA_NO_MP3
    #define MA_NO_FLAC
    ```

miniaudio supports the ability to plug in custom decoders. See the section below for details on how
to use custom decoders.

A decoder can be initialized from a file with `ma_decoder_init_file()`, a block of memory with
`ma_decoder_init_memory()`, or from data delivered via callbacks with `ma_decoder_init()`. Here is
an example for loading a decoder from a file:

    ```c
    ma_decoder decoder;
    ma_result result = ma_decoder_init_file("MySong.mp3", NULL, &decoder);
    if (result != MA_SUCCESS) {
        return false;   // An error occurred.
    }

    ...

    ma_decoder_uninit(&decoder);
    ```

When initializing a decoder, you can optionally pass in a pointer to a `ma_decoder_config` object
(the `NULL` argument in the example above) which allows you to configure the output format, channel
count, sample rate and channel map:

    ```c
    ma_decoder_config config = ma_decoder_config_init(ma_format_f32, 2, 48000);
    ```

When passing in `NULL` for decoder config in `ma_decoder_init*()`, the output format will be the
same as that defined by the decoding backend.

Data is read from the decoder as PCM frames. This will output the number of PCM frames actually
read. If this is less than the requested number of PCM frames it means you've reached the end. The
return value will be `MA_AT_END` if no samples have been read and the end has been reached.

    ```c
    ma_result result = ma_decoder_read_pcm_frames(pDecoder, pFrames, framesToRead, &framesRead);
    if (framesRead < framesToRead) {
        // Reached the end.
    }
    ```

You can also seek to a specific frame like so:

    ```c
    ma_result result = ma_decoder_seek_to_pcm_frame(pDecoder, targetFrame);
    if (result != MA_SUCCESS) {
        return false;   // An error occurred.
    }
    ```

If you want to loop back to the start, you can simply seek back to the first PCM frame:

    ```c
    ma_decoder_seek_to_pcm_frame(pDecoder, 0);
    ```

When loading a decoder, miniaudio uses a trial and error technique to find the appropriate decoding
backend. This can be unnecessarily inefficient if the type is already known. In this case you can
use `encodingFormat` variable in the device config to specify a specific encoding format you want
to decode:

    ```c
    decoderConfig.encodingFormat = ma_encoding_format_wav;
    ```

See the `ma_encoding_format` enum for possible encoding formats.

The `ma_decoder_init_file()` API will try using the file extension to determine which decoding
backend to prefer.


8.1. Custom Decoders
--------------------
It's possible to implement a custom decoder and plug it into miniaudio. This is extremely useful
when you want to use the `ma_decoder` API, but need to support an encoding format that's not one of
the stock formats supported by miniaudio. This can be put to particularly good use when using the
`ma_engine` and/or `ma_resource_manager` APIs because they use `ma_decoder` internally. If, for
example, you wanted to support Opus, you can do so with a custom decoder (there if a reference
Opus decoder in the "extras" folder of the miniaudio repository which uses libopus + libopusfile).

A custom decoder must implement a data source. A vtable called `ma_decoding_backend_vtable` needs
to be implemented which is then passed into the decoder config:

    ```c
    ma_decoding_backend_vtable* pCustomBackendVTables[] =
    {
        &g_ma_decoding_backend_vtable_libvorbis,
        &g_ma_decoding_backend_vtable_libopus
    };

    ...

    decoderConfig = ma_decoder_config_init_default();
    decoderConfig.pCustomBackendUserData = NULL;
    decoderConfig.ppCustomBackendVTables = pCustomBackendVTables;
    decoderConfig.customBackendCount     = sizeof(pCustomBackendVTables) / sizeof(pCustomBackendVTables[0]);
    ```

The `ma_decoding_backend_vtable` vtable has the following functions:

    ```
    onInit
    onInitFile
    onInitFileW
    onInitMemory
    onUninit
    ```

There are only two functions that must be implemented - `onInit` and `onUninit`. The other
functions can be implemented for a small optimization for loading from a file path or memory. If
these are not specified, miniaudio will deal with it for you via a generic implementation.

When you initialize a custom data source (by implementing the `onInit` function in the vtable) you
will need to output a pointer to a `ma_data_source` which implements your custom decoder. See the
section about data sources for details on how to implement this. Alternatively, see the
"custom_decoders" example in the miniaudio repository.

The `onInit` function takes a pointer to some callbacks for the purpose of reading raw audio data
from some arbitrary source. You'll use these functions to read from the raw data and perform the
decoding. When you call them, you will pass in the `pReadSeekTellUserData` pointer to the relevant
parameter.

The `pConfig` parameter in `onInit` can be used to configure the backend if appropriate. It's only
used as a hint and can be ignored. However, if any of the properties are relevant to your decoder,
an optimal implementation will handle the relevant properties appropriately.

If memory allocation is required, it should be done so via the specified allocation callbacks if
possible (the `pAllocationCallbacks` parameter).

If an error occurs when initializing the decoder, you should leave `ppBackend` unset, or set to
NULL, and make sure everything is cleaned up appropriately and an appropriate result code returned.
When multiple custom backends are specified, miniaudio will cycle through the vtables in the order
they're listed in the array that's passed into the decoder config so it's important that your
initialization routine is clean.

When a decoder is uninitialized, the `onUninit` callback will be fired which will give you an
opportunity to clean up and internal data.



9. Encoding
===========
The `ma_encoding` API is used for writing audio files. The only supported output format is WAV.
This can be disabled by specifying the following option before the implementation of miniaudio:

    ```c
    #define MA_NO_WAV
    ```

An encoder can be initialized to write to a file with `ma_encoder_init_file()` or from data
delivered via callbacks with `ma_encoder_init()`. Below is an example for initializing an encoder
to output to a file.

    ```c
    ma_encoder_config config = ma_encoder_config_init(ma_encoding_format_wav, FORMAT, CHANNELS, SAMPLE_RATE);
    ma_encoder encoder;
    ma_result result = ma_encoder_init_file("my_file.wav", &config, &encoder);
    if (result != MA_SUCCESS) {
        // Error
    }

    ...

    ma_encoder_uninit(&encoder);
    ```

When initializing an encoder you must specify a config which is initialized with
`ma_encoder_config_init()`. Here you must specify the file type, the output sample format, output
channel count and output sample rate. The following file types are supported:

    +------------------------+-------------+
    | Enum                   | Description |
    +------------------------+-------------+
    | ma_encoding_format_wav | WAV         |
    +------------------------+-------------+

If the format, channel count or sample rate is not supported by the output file type an error will
be returned. The encoder will not perform data conversion so you will need to convert it before
outputting any audio data. To output audio data, use `ma_encoder_write_pcm_frames()`, like in the
example below:

    ```c
    ma_uint64 framesWritten;
    result = ma_encoder_write_pcm_frames(&encoder, pPCMFramesToWrite, framesToWrite, &framesWritten);
    if (result != MA_SUCCESS) {
        ... handle error ...
    }
    ```

The `framesWritten` variable will contain the number of PCM frames that were actually written. This
is optionally and you can pass in `NULL` if you need this.

Encoders must be uninitialized with `ma_encoder_uninit()`.



10. Data Conversion
===================
A data conversion API is included with miniaudio which supports the majority of data conversion
requirements. This supports conversion between sample formats, channel counts (with channel
mapping) and sample rates.


10.1. Sample Format Conversion
------------------------------
Conversion between sample formats is achieved with the `ma_pcm_*_to_*()`, `ma_pcm_convert()` and
`ma_convert_pcm_frames_format()` APIs. Use `ma_pcm_*_to_*()` to convert between two specific
formats. Use `ma_pcm_convert()` to convert based on a `ma_format` variable. Use
`ma_convert_pcm_frames_format()` to convert PCM frames where you want to specify the frame count
and channel count as a variable instead of the total sample count.


10.1.1. Dithering
-----------------
Dithering can be set using the ditherMode parameter.

The different dithering modes include the following, in order of efficiency:

    +-----------+--------------------------+
    | Type      | Enum Token               |
    +-----------+--------------------------+
    | None      | ma_dither_mode_none      |
    | Rectangle | ma_dither_mode_rectangle |
    | Triangle  | ma_dither_mode_triangle  |
    +-----------+--------------------------+

Note that even if the dither mode is set to something other than `ma_dither_mode_none`, it will be
ignored for conversions where dithering is not needed. Dithering is available for the following
conversions:

    ```
    s16 -> u8
    s24 -> u8
    s32 -> u8
    f32 -> u8
    s24 -> s16
    s32 -> s16
    f32 -> s16
    ```

Note that it is not an error to pass something other than ma_dither_mode_none for conversions where
dither is not used. It will just be ignored.



10.2. Channel Conversion
------------------------
Channel conversion is used for channel rearrangement and conversion from one channel count to
another. The `ma_channel_converter` API is used for channel conversion. Below is an example of
initializing a simple channel converter which converts from mono to stereo.

    ```c
    ma_channel_converter_config config = ma_channel_converter_config_init(
        ma_format,                      // Sample format
        1,                              // Input channels
        NULL,                           // Input channel map
        2,                              // Output channels
        NULL,                           // Output channel map
        ma_channel_mix_mode_default);   // The mixing algorithm to use when combining channels.

    result = ma_channel_converter_init(&config, NULL, &converter);
    if (result != MA_SUCCESS) {
        // Error.
    }
    ```

To perform the conversion simply call `ma_channel_converter_process_pcm_frames()` like so:

    ```c
    ma_result result = ma_channel_converter_process_pcm_frames(&converter, pFramesOut, pFramesIn, frameCount);
    if (result != MA_SUCCESS) {
        // Error.
    }
    ```

It is up to the caller to ensure the output buffer is large enough to accommodate the new PCM
frames.

Input and output PCM frames are always interleaved. Deinterleaved layouts are not supported.


10.2.1. Channel Mapping
-----------------------
In addition to converting from one channel count to another, like the example above, the channel
converter can also be used to rearrange channels. When initializing the channel converter, you can
optionally pass in channel maps for both the input and output frames. If the channel counts are the
same, and each channel map contains the same channel positions with the exception that they're in
a different order, a simple shuffling of the channels will be performed. If, however, there is not
a 1:1 mapping of channel positions, or the channel counts differ, the input channels will be mixed
based on a mixing mode which is specified when initializing the `ma_channel_converter_config`
object.

When converting from mono to multi-channel, the mono channel is simply copied to each output
channel. When going the other way around, the audio of each output channel is simply averaged and
copied to the mono channel.

In more complicated cases blending is used. The `ma_channel_mix_mode_simple` mode will drop excess
channels and silence extra channels. For example, converting from 4 to 2 channels, the 3rd and 4th
channels will be dropped, whereas converting from 2 to 4 channels will put silence into the 3rd and
4th channels.

The `ma_channel_mix_mode_rectangle` mode uses spacial locality based on a rectangle to compute a
simple distribution between input and output. Imagine sitting in the middle of a room, with
speakers on the walls representing channel positions. The `MA_CHANNEL_FRONT_LEFT` position can be
thought of as being in the corner of the front and left walls.

Finally, the `ma_channel_mix_mode_custom_weights` mode can be used to use custom user-defined
weights. Custom weights can be passed in as the last parameter of
`ma_channel_converter_config_init()`.

Predefined channel maps can be retrieved with `ma_channel_map_init_standard()`. This takes a
`ma_standard_channel_map` enum as its first parameter, which can be one of the following:

    +-----------------------------------+-----------------------------------------------------------+
    | Name                              | Description                                               |
    +-----------------------------------+-----------------------------------------------------------+
    | ma_standard_channel_map_default   | Default channel map used by miniaudio. See below.         |
    | ma_standard_channel_map_microsoft | Channel map used by Microsoft's bitfield channel maps.    |
    | ma_standard_channel_map_alsa      | Default ALSA channel map.                                 |
    | ma_standard_channel_map_rfc3551   | RFC 3551. Based on AIFF.                                  |
    | ma_standard_channel_map_flac      | FLAC channel map.                                         |
    | ma_standard_channel_map_vorbis    | Vorbis channel map.                                       |
    | ma_standard_channel_map_sound4    | FreeBSD's sound(4).                                       |
    | ma_standard_channel_map_sndio     | sndio channel map. http://www.sndio.org/tips.html.        |
    | ma_standard_channel_map_webaudio  | https://webaudio.github.io/web-audio-api/#ChannelOrdering |
    +-----------------------------------+-----------------------------------------------------------+

Below are the channel maps used by default in miniaudio (`ma_standard_channel_map_default`):

    +---------------+---------------------------------+
    | Channel Count | Mapping                         |
    +---------------+---------------------------------+
    | 1 (Mono)      | 0: MA_CHANNEL_MONO              |
    +---------------+---------------------------------+
    | 2 (Stereo)    | 0: MA_CHANNEL_FRONT_LEFT   <br> |
    |               | 1: MA_CHANNEL_FRONT_RIGHT       |
    +---------------+---------------------------------+
    | 3             | 0: MA_CHANNEL_FRONT_LEFT   <br> |
    |               | 1: MA_CHANNEL_FRONT_RIGHT  <br> |
    |               | 2: MA_CHANNEL_FRONT_CENTER      |
    +---------------+---------------------------------+
    | 4 (Surround)  | 0: MA_CHANNEL_FRONT_LEFT   <br> |
    |               | 1: MA_CHANNEL_FRONT_RIGHT  <br> |
    |               | 2: MA_CHANNEL_FRONT_CENTER <br> |
    |               | 3: MA_CHANNEL_BACK_CENTER       |
    +---------------+---------------------------------+
    | 5             | 0: MA_CHANNEL_FRONT_LEFT   <br> |
    |               | 1: MA_CHANNEL_FRONT_RIGHT  <br> |
    |               | 2: MA_CHANNEL_FRONT_CENTER <br> |
    |               | 3: MA_CHANNEL_BACK_LEFT    <br> |
    |               | 4: MA_CHANNEL_BACK_RIGHT        |
    +---------------+---------------------------------+
    | 6 (5.1)       | 0: MA_CHANNEL_FRONT_LEFT   <br> |
    |               | 1: MA_CHANNEL_FRONT_RIGHT  <br> |
    |               | 2: MA_CHANNEL_FRONT_CENTER <br> |
    |               | 3: MA_CHANNEL_LFE          <br> |
    |               | 4: MA_CHANNEL_SIDE_LEFT    <br> |
    |               | 5: MA_CHANNEL_SIDE_RIGHT        |
    +---------------+---------------------------------+
    | 7             | 0: MA_CHANNEL_FRONT_LEFT   <br> |
    |               | 1: MA_CHANNEL_FRONT_RIGHT  <br> |
    |               | 2: MA_CHANNEL_FRONT_CENTER <br> |
    |               | 3: MA_CHANNEL_LFE          <br> |
    |               | 4: MA_CHANNEL_BACK_CENTER  <br> |
    |               | 4: MA_CHANNEL_SIDE_LEFT    <br> |
    |               | 5: MA_CHANNEL_SIDE_RIGHT        |
    +---------------+---------------------------------+
    | 8 (7.1)       | 0: MA_CHANNEL_FRONT_LEFT   <br> |
    |               | 1: MA_CHANNEL_FRONT_RIGHT  <br> |
    |               | 2: MA_CHANNEL_FRONT_CENTER <br> |
    |               | 3: MA_CHANNEL_LFE          <br> |
    |               | 4: MA_CHANNEL_BACK_LEFT    <br> |
    |               | 5: MA_CHANNEL_BACK_RIGHT   <br> |
    |               | 6: MA_CHANNEL_SIDE_LEFT    <br> |
    |               | 7: MA_CHANNEL_SIDE_RIGHT        |
    +---------------+---------------------------------+
    | Other         | All channels set to 0. This     |
    |               | is equivalent to the same       |
    |               | mapping as the device.          |
    +---------------+---------------------------------+



10.3. Resampling
----------------
Resampling is achieved with the `ma_resampler` object. To create a resampler object, do something
like the following:

    ```c
    ma_resampler_config config = ma_resampler_config_init(
        ma_format_s16,
        channels,
        sampleRateIn,
        sampleRateOut,
        ma_resample_algorithm_linear);

    ma_resampler resampler;
    ma_result result = ma_resampler_init(&config, NULL, &resampler);
    if (result != MA_SUCCESS) {
        // An error occurred...
    }
    ```

Do the following to uninitialize the resampler:

    ```c
    ma_resampler_uninit(&resampler);
    ```

The following example shows how data can be processed

    ```c
    ma_uint64 frameCountIn  = 1000;
    ma_uint64 frameCountOut = 2000;
    ma_result result = ma_resampler_process_pcm_frames(&resampler, pFramesIn, &frameCountIn, pFramesOut, &frameCountOut);
    if (result != MA_SUCCESS) {
        // An error occurred...
    }

    // At this point, frameCountIn contains the number of input frames that were consumed and frameCountOut contains the
    // number of output frames written.
    ```

To initialize the resampler you first need to set up a config (`ma_resampler_config`) with
`ma_resampler_config_init()`. You need to specify the sample format you want to use, the number of
channels, the input and output sample rate, and the algorithm.

The sample format can be either `ma_format_s16` or `ma_format_f32`. If you need a different format
you will need to perform pre- and post-conversions yourself where necessary. Note that the format
is the same for both input and output. The format cannot be changed after initialization.

The resampler supports multiple channels and is always interleaved (both input and output). The
channel count cannot be changed after initialization.

The sample rates can be anything other than zero, and are always specified in hertz. They should be
set to something like 44100, etc. The sample rate is the only configuration property that can be
changed after initialization.

The miniaudio resampler has built-in support for the following algorithms:

    +-----------+------------------------------+
    | Algorithm | Enum Token                   |
    +-----------+------------------------------+
    | Linear    | ma_resample_algorithm_linear |
    | Custom    | ma_resample_algorithm_custom |
    +-----------+------------------------------+

The algorithm cannot be changed after initialization.

Processing always happens on a per PCM frame basis and always assumes interleaved input and output.
De-interleaved processing is not supported. To process frames, use
`ma_resampler_process_pcm_frames()`. On input, this function takes the number of output frames you
can fit in the output buffer and the number of input frames contained in the input buffer. On
output these variables contain the number of output frames that were written to the output buffer
and the number of input frames that were consumed in the process. You can pass in NULL for the
input buffer in which case it will be treated as an infinitely large buffer of zeros. The output
buffer can also be NULL, in which case the processing will be treated as seek.

The sample rate can be changed dynamically on the fly. You can change this with explicit sample
rates with `ma_resampler_set_rate()` and also with a decimal ratio with
`ma_resampler_set_rate_ratio()`. The ratio is in/out.

Sometimes it's useful to know exactly how many input frames will be required to output a specific
number of frames. You can calculate this with `ma_resampler_get_required_input_frame_count()`.
Likewise, it's sometimes useful to know exactly how many frames would be output given a certain
number of input frames. You can do this with `ma_resampler_get_expected_output_frame_count()`.

Due to the nature of how resampling works, the resampler introduces some latency. This can be
retrieved in terms of both the input rate and the output rate with
`ma_resampler_get_input_latency()` and `ma_resampler_get_output_latency()`.


10.3.1. Resampling Algorithms
-----------------------------
The choice of resampling algorithm depends on your situation and requirements.


10.3.1.1. Linear Resampling
---------------------------
The linear resampler is the fastest, but comes at the expense of poorer quality. There is, however,
some control over the quality of the linear resampler which may make it a suitable option depending
on your requirements.

The linear resampler performs low-pass filtering before or after downsampling or upsampling,
depending on the sample rates you're converting between. When decreasing the sample rate, the
low-pass filter will be applied before downsampling. When increasing the rate it will be performed
after upsampling. By default a fourth order low-pass filter will be applied. This can be configured
via the `lpfOrder` configuration variable. Setting this to 0 will disable filtering.

The low-pass filter has a cutoff frequency which defaults to half the sample rate of the lowest of
the input and output sample rates (Nyquist Frequency).

The API for the linear resampler is the same as the main resampler API, only it's called
`ma_linear_resampler`.


10.3.2. Custom Resamplers
-------------------------
You can implement a custom resampler by using the `ma_resample_algorithm_custom` resampling
algorithm and setting a vtable in the resampler config:

    ```c
    ma_resampler_config config = ma_resampler_config_init(..., ma_resample_algorithm_custom);
    config.pBackendVTable = &g_customResamplerVTable;
    ```

Custom resamplers are useful if the stock algorithms are not appropriate for your use case. You
need to implement the required functions in `ma_resampling_backend_vtable`. Note that not all
functions in the vtable need to be implemented, but if it's possible to implement, they should be.

You can use the `ma_linear_resampler` object for an example on how to implement the vtable. The
`onGetHeapSize` callback is used to calculate the size of any internal heap allocation the custom
resampler will need to make given the supplied config. When you initialize the resampler via the
`onInit` callback, you'll be given a pointer to a heap allocation which is where you should store
the heap allocated data. You should not free this data in `onUninit` because miniaudio will manage
it for you.

The `onProcess` callback is where the actual resampling takes place. On input, `pFrameCountIn`
points to a variable containing the number of frames in the `pFramesIn` buffer and
`pFrameCountOut` points to a variable containing the capacity in frames of the `pFramesOut` buffer.
On output, `pFrameCountIn` should be set to the number of input frames that were fully consumed,
whereas `pFrameCountOut` should be set to the number of frames that were written to `pFramesOut`.

The `onSetRate` callback is optional and is used for dynamically changing the sample rate. If
dynamic rate changes are not supported, you can set this callback to NULL.

The `onGetInputLatency` and `onGetOutputLatency` functions are used for retrieving the latency in
input and output rates respectively. These can be NULL in which case latency calculations will be
assumed to be NULL.

The `onGetRequiredInputFrameCount` callback is used to give miniaudio a hint as to how many input
frames are required to be available to produce the given number of output frames. Likewise, the
`onGetExpectedOutputFrameCount` callback is used to determine how many output frames will be
produced given the specified number of input frames. miniaudio will use these as a hint, but they
are optional and can be set to NULL if you're unable to implement them.



10.4. General Data Conversion
-----------------------------
The `ma_data_converter` API can be used to wrap sample format conversion, channel conversion and
resampling into one operation. This is what miniaudio uses internally to convert between the format
requested when the device was initialized and the format of the backend's native device. The API
for general data conversion is very similar to the resampling API. Create a `ma_data_converter`
object like this:

    ```c
    ma_data_converter_config config = ma_data_converter_config_init(
        inputFormat,
        outputFormat,
        inputChannels,
        outputChannels,
        inputSampleRate,
        outputSampleRate
    );

    ma_data_converter converter;
    ma_result result = ma_data_converter_init(&config, NULL, &converter);
    if (result != MA_SUCCESS) {
        // An error occurred...
    }
    ```

In the example above we use `ma_data_converter_config_init()` to initialize the config, however
there's many more properties that can be configured, such as channel maps and resampling quality.
Something like the following may be more suitable depending on your requirements:

    ```c
    ma_data_converter_config config = ma_data_converter_config_init_default();
    config.formatIn = inputFormat;
    config.formatOut = outputFormat;
    config.channelsIn = inputChannels;
    config.channelsOut = outputChannels;
    config.sampleRateIn = inputSampleRate;
    config.sampleRateOut = outputSampleRate;
    ma_channel_map_init_standard(ma_standard_channel_map_flac, config.channelMapIn, sizeof(config.channelMapIn)/sizeof(config.channelMapIn[0]), config.channelCountIn);
    config.resampling.linear.lpfOrder = MA_MAX_FILTER_ORDER;
    ```

Do the following to uninitialize the data converter:

    ```c
    ma_data_converter_uninit(&converter, NULL);
    ```

The following example shows how data can be processed

    ```c
    ma_uint64 frameCountIn  = 1000;
    ma_uint64 frameCountOut = 2000;
    ma_result result = ma_data_converter_process_pcm_frames(&converter, pFramesIn, &frameCountIn, pFramesOut, &frameCountOut);
    if (result != MA_SUCCESS) {
        // An error occurred...
    }

    // At this point, frameCountIn contains the number of input frames that were consumed and frameCountOut contains the number
    // of output frames written.
    ```

The data converter supports multiple channels and is always interleaved (both input and output).
The channel count cannot be changed after initialization.

Sample rates can be anything other than zero, and are always specified in hertz. They should be set
to something like 44100, etc. The sample rate is the only configuration property that can be
changed after initialization, but only if the `resampling.allowDynamicSampleRate` member of
`ma_data_converter_config` is set to `MA_TRUE`. To change the sample rate, use
`ma_data_converter_set_rate()` or `ma_data_converter_set_rate_ratio()`. The ratio must be in/out.
The resampling algorithm cannot be changed after initialization.

Processing always happens on a per PCM frame basis and always assumes interleaved input and output.
De-interleaved processing is not supported. To process frames, use
`ma_data_converter_process_pcm_frames()`. On input, this function takes the number of output frames
you can fit in the output buffer and the number of input frames contained in the input buffer. On
output these variables contain the number of output frames that were written to the output buffer
and the number of input frames that were consumed in the process. You can pass in NULL for the
input buffer in which case it will be treated as an infinitely large
buffer of zeros. The output buffer can also be NULL, in which case the processing will be treated
as seek.

Sometimes it's useful to know exactly how many input frames will be required to output a specific
number of frames. You can calculate this with `ma_data_converter_get_required_input_frame_count()`.
Likewise, it's sometimes useful to know exactly how many frames would be output given a certain
number of input frames. You can do this with `ma_data_converter_get_expected_output_frame_count()`.

Due to the nature of how resampling works, the data converter introduces some latency if resampling
is required. This can be retrieved in terms of both the input rate and the output rate with
`ma_data_converter_get_input_latency()` and `ma_data_converter_get_output_latency()`.



11. Filtering
=============

11.1. Biquad Filtering
----------------------
Biquad filtering is achieved with the `ma_biquad` API. Example:

    ```c
    ma_biquad_config config = ma_biquad_config_init(ma_format_f32, channels, b0, b1, b2, a0, a1, a2);
    ma_result result = ma_biquad_init(&config, NULL, &biquad);
    if (result != MA_SUCCESS) {
        // Error.
    }

    ...

    ma_biquad_process_pcm_frames(&biquad, pFramesOut, pFramesIn, frameCount);
    ```

Biquad filtering is implemented using transposed direct form 2. The numerator coefficients are b0,
b1 and b2, and the denominator coefficients are a0, a1 and a2. The a0 coefficient is required and
coefficients must not be pre-normalized.

Supported formats are `ma_format_s16` and `ma_format_f32`. If you need to use a different format
you need to convert it yourself beforehand. When using `ma_format_s16` the biquad filter will use
fixed point arithmetic. When using `ma_format_f32`, floating point arithmetic will be used.

Input and output frames are always interleaved.

Filtering can be applied in-place by passing in the same pointer for both the input and output
buffers, like so:

    ```c
    ma_biquad_process_pcm_frames(&biquad, pMyData, pMyData, frameCount);
    ```

If you need to change the values of the coefficients, but maintain the values in the registers you
can do so with `ma_biquad_reinit()`. This is useful if you need to change the properties of the
filter while keeping the values of registers valid to avoid glitching. Do not use
`ma_biquad_init()` for this as it will do a full initialization which involves clearing the
registers to 0. Note that changing the format or channel count after initialization is invalid and
will result in an error.


11.2. Low-Pass Filtering
------------------------
Low-pass filtering is achieved with the following APIs:

    +---------+------------------------------------------+
    | API     | Description                              |
    +---------+------------------------------------------+
    | ma_lpf1 | First order low-pass filter              |
    | ma_lpf2 | Second order low-pass filter             |
    | ma_lpf  | High order low-pass filter (Butterworth) |
    +---------+------------------------------------------+

Low-pass filter example:

    ```c
    ma_lpf_config config = ma_lpf_config_init(ma_format_f32, channels, sampleRate, cutoffFrequency, order);
    ma_result result = ma_lpf_init(&config, &lpf);
    if (result != MA_SUCCESS) {
        // Error.
    }

    ...

    ma_lpf_process_pcm_frames(&lpf, pFramesOut, pFramesIn, frameCount);
    ```

Supported formats are `ma_format_s16` and` ma_format_f32`. If you need to use a different format
you need to convert it yourself beforehand. Input and output frames are always interleaved.

Filtering can be applied in-place by passing in the same pointer for both the input and output
buffers, like so:

    ```c
    ma_lpf_process_pcm_frames(&lpf, pMyData, pMyData, frameCount);
    ```

The maximum filter order is limited to `MA_MAX_FILTER_ORDER` which is set to 8. If you need more,
you can chain first and second order filters together.

    ```c
    for (iFilter = 0; iFilter < filterCount; iFilter += 1) {
        ma_lpf2_process_pcm_frames(&lpf2[iFilter], pMyData, pMyData, frameCount);
    }
    ```

If you need to change the configuration of the filter, but need to maintain the state of internal
registers you can do so with `ma_lpf_reinit()`. This may be useful if you need to change the sample
rate and/or cutoff frequency dynamically while maintaining smooth transitions. Note that changing the
format or channel count after initialization is invalid and will result in an error.

The `ma_lpf` object supports a configurable order, but if you only need a first order filter you
may want to consider using `ma_lpf1`. Likewise, if you only need a second order filter you can use
`ma_lpf2`. The advantage of this is that they're lighter weight and a bit more efficient.

If an even filter order is specified, a series of second order filters will be processed in a
chain. If an odd filter order is specified, a first order filter will be applied, followed by a
series of second order filters in a chain.


11.3. High-Pass Filtering
-------------------------
High-pass filtering is achieved with the following APIs:

    +---------+-------------------------------------------+
    | API     | Description                               |
    +---------+-------------------------------------------+
    | ma_hpf1 | First order high-pass filter              |
    | ma_hpf2 | Second order high-pass filter             |
    | ma_hpf  | High order high-pass filter (Butterworth) |
    +---------+-------------------------------------------+

High-pass filters work exactly the same as low-pass filters, only the APIs are called `ma_hpf1`,
`ma_hpf2` and `ma_hpf`. See example code for low-pass filters for example usage.


11.4. Band-Pass Filtering
-------------------------
Band-pass filtering is achieved with the following APIs:

    +---------+-------------------------------+
    | API     | Description                   |
    +---------+-------------------------------+
    | ma_bpf2 | Second order band-pass filter |
    | ma_bpf  | High order band-pass filter   |
    +---------+-------------------------------+

Band-pass filters work exactly the same as low-pass filters, only the APIs are called `ma_bpf2` and
`ma_hpf`. See example code for low-pass filters for example usage. Note that the order for
band-pass filters must be an even number which means there is no first order band-pass filter,
unlike low-pass and high-pass filters.


11.5. Notch Filtering
---------------------
Notch filtering is achieved with the following APIs:

    +-----------+------------------------------------------+
    | API       | Description                              |
    +-----------+------------------------------------------+
    | ma_notch2 | Second order notching filter             |
    +-----------+------------------------------------------+


11.6. Peaking EQ Filtering
-------------------------
Peaking filtering is achieved with the following APIs:

    +----------+------------------------------------------+
    | API      | Description                              |
    +----------+------------------------------------------+
    | ma_peak2 | Second order peaking filter              |
    +----------+------------------------------------------+


11.7. Low Shelf Filtering
-------------------------
Low shelf filtering is achieved with the following APIs:

    +-------------+------------------------------------------+
    | API         | Description                              |
    +-------------+------------------------------------------+
    | ma_loshelf2 | Second order low shelf filter            |
    +-------------+------------------------------------------+

Where a high-pass filter is used to eliminate lower frequencies, a low shelf filter can be used to
just turn them down rather than eliminate them entirely.


11.8. High Shelf Filtering
--------------------------
High shelf filtering is achieved with the following APIs:

    +-------------+------------------------------------------+
    | API         | Description                              |
    +-------------+------------------------------------------+
    | ma_hishelf2 | Second order high shelf filter           |
    +-------------+------------------------------------------+

The high shelf filter has the same API as the low shelf filter, only you would use `ma_hishelf`
instead of `ma_loshelf`. Where a low shelf filter is used to adjust the volume of low frequencies,
the high shelf filter does the same thing for high frequencies.




12. Waveform and Noise Generation
=================================

12.1. Waveforms
---------------
miniaudio supports generation of sine, square, triangle and sawtooth waveforms. This is achieved
with the `ma_waveform` API. Example:

    ```c
    ma_waveform_config config = ma_waveform_config_init(
        FORMAT,
        CHANNELS,
        SAMPLE_RATE,
        ma_waveform_type_sine,
        amplitude,
        frequency);

    ma_waveform waveform;
    ma_result result = ma_waveform_init(&config, &waveform);
    if (result != MA_SUCCESS) {
        // Error.
    }

    ...

    ma_waveform_read_pcm_frames(&waveform, pOutput, frameCount);
    ```

The amplitude, frequency, type, and sample rate can be changed dynamically with
`ma_waveform_set_amplitude()`, `ma_waveform_set_frequency()`, `ma_waveform_set_type()`, and
`ma_waveform_set_sample_rate()` respectively.

You can invert the waveform by setting the amplitude to a negative value. You can use this to
control whether or not a sawtooth has a positive or negative ramp, for example.

Below are the supported waveform types:

    +---------------------------+
    | Enum Name                 |
    +---------------------------+
    | ma_waveform_type_sine     |
    | ma_waveform_type_square   |
    | ma_waveform_type_triangle |
    | ma_waveform_type_sawtooth |
    +---------------------------+



12.2. Noise
-----------
miniaudio supports generation of white, pink and Brownian noise via the `ma_noise` API. Example:

    ```c
    ma_noise_config config = ma_noise_config_init(
        FORMAT,
        CHANNELS,
        ma_noise_type_white,
        SEED,
        amplitude);

    ma_noise noise;
    ma_result result = ma_noise_init(&config, &noise);
    if (result != MA_SUCCESS) {
        // Error.
    }

    ...

    ma_noise_read_pcm_frames(&noise, pOutput, frameCount);
    ```

The noise API uses simple LCG random number generation. It supports a custom seed which is useful
for things like automated testing requiring reproducibility. Setting the seed to zero will default
to `MA_DEFAULT_LCG_SEED`.

The amplitude and seed can be changed dynamically with `ma_noise_set_amplitude()` and
`ma_noise_set_seed()` respectively.

By default, the noise API will use different values for different channels. So, for example, the
left side in a stereo stream will be different to the right side. To instead have each channel use
the same random value, set the `duplicateChannels` member of the noise config to true, like so:

    ```c
    config.duplicateChannels = MA_TRUE;
    ```

Below are the supported noise types.

    +------------------------+
    | Enum Name              |
    +------------------------+
    | ma_noise_type_white    |
    | ma_noise_type_pink     |
    | ma_noise_type_brownian |
    +------------------------+



13. Audio Buffers
=================
miniaudio supports reading from a buffer of raw audio data via the `ma_audio_buffer` API. This can
read from memory that's managed by the application, but can also handle the memory management for
you internally. Memory management is flexible and should support most use cases.

Audio buffers are initialized using the standard configuration system used everywhere in miniaudio:

    ```c
    ma_audio_buffer_config config = ma_audio_buffer_config_init(
        format,
        channels,
        sizeInFrames,
        pExistingData,
        &allocationCallbacks);

    ma_audio_buffer buffer;
    result = ma_audio_buffer_init(&config, &buffer);
    if (result != MA_SUCCESS) {
        // Error.
    }

    ...

    ma_audio_buffer_uninit(&buffer);
    ```

In the example above, the memory pointed to by `pExistingData` will *not* be copied and is how an
application can do self-managed memory allocation. If you would rather make a copy of the data, use
`ma_audio_buffer_init_copy()`. To uninitialize the buffer, use `ma_audio_buffer_uninit()`.

Sometimes it can be convenient to allocate the memory for the `ma_audio_buffer` structure and the
raw audio data in a contiguous block of memory. That is, the raw audio data will be located
immediately after the `ma_audio_buffer` structure. To do this, use
`ma_audio_buffer_alloc_and_init()`:

    ```c
    ma_audio_buffer_config config = ma_audio_buffer_config_init(
        format,
        channels,
        sizeInFrames,
        pExistingData,
        &allocationCallbacks);

    ma_audio_buffer* pBuffer
    result = ma_audio_buffer_alloc_and_init(&config, &pBuffer);
    if (result != MA_SUCCESS) {
        // Error
    }

    ...

    ma_audio_buffer_uninit_and_free(&buffer);
    ```

If you initialize the buffer with `ma_audio_buffer_alloc_and_init()` you should uninitialize it
with `ma_audio_buffer_uninit_and_free()`. In the example above, the memory pointed to by
`pExistingData` will be copied into the buffer, which is contrary to the behavior of
`ma_audio_buffer_init()`.

An audio buffer has a playback cursor just like a decoder. As you read frames from the buffer, the
cursor moves forward. The last parameter (`loop`) can be used to determine if the buffer should
loop. The return value is the number of frames actually read. If this is less than the number of
frames requested it means the end has been reached. This should never happen if the `loop`
parameter is set to true. If you want to manually loop back to the start, you can do so with with
`ma_audio_buffer_seek_to_pcm_frame(pAudioBuffer, 0)`. Below is an example for reading data from an
audio buffer.

    ```c
    ma_uint64 framesRead = ma_audio_buffer_read_pcm_frames(pAudioBuffer, pFramesOut, desiredFrameCount, isLooping);
    if (framesRead < desiredFrameCount) {
        // If not looping, this means the end has been reached. This should never happen in looping mode with valid input.
    }
    ```

Sometimes you may want to avoid the cost of data movement between the internal buffer and the
output buffer. Instead you can use memory mapping to retrieve a pointer to a segment of data:

    ```c
    void* pMappedFrames;
    ma_uint64 frameCount = frameCountToTryMapping;
    ma_result result = ma_audio_buffer_map(pAudioBuffer, &pMappedFrames, &frameCount);
    if (result == MA_SUCCESS) {
        // Map was successful. The value in frameCount will be how many frames were _actually_ mapped, which may be
        // less due to the end of the buffer being reached.
        ma_copy_pcm_frames(pFramesOut, pMappedFrames, frameCount, pAudioBuffer->format, pAudioBuffer->channels);

        // You must unmap the buffer.
        ma_audio_buffer_unmap(pAudioBuffer, frameCount);
    }
    ```

When you use memory mapping, the read cursor is increment by the frame count passed in to
`ma_audio_buffer_unmap()`. If you decide not to process every frame you can pass in a value smaller
than the value returned by `ma_audio_buffer_map()`. The disadvantage to using memory mapping is
that it does not handle looping for you. You can determine if the buffer is at the end for the
purpose of looping with `ma_audio_buffer_at_end()` or by inspecting the return value of
`ma_audio_buffer_unmap()` and checking if it equals `MA_AT_END`. You should not treat `MA_AT_END`
as an error when returned by `ma_audio_buffer_unmap()`.



14. Ring Buffers
================
miniaudio supports lock free (single producer, single consumer) ring buffers which are exposed via
the `ma_rb` and `ma_pcm_rb` APIs. The `ma_rb` API operates on bytes, whereas the `ma_pcm_rb`
operates on PCM frames. They are otherwise identical as `ma_pcm_rb` is just a wrapper around
`ma_rb`.

Unlike most other APIs in miniaudio, ring buffers support both interleaved and deinterleaved
streams. The caller can also allocate their own backing memory for the ring buffer to use
internally for added flexibility. Otherwise the ring buffer will manage it's internal memory for
you.

The examples below use the PCM frame variant of the ring buffer since that's most likely the one
you will want to use. To initialize a ring buffer, do something like the following:

    ```c
    ma_pcm_rb rb;
    ma_result result = ma_pcm_rb_init(FORMAT, CHANNELS, BUFFER_SIZE_IN_FRAMES, NULL, NULL, &rb);
    if (result != MA_SUCCESS) {
        // Error
    }
    ```

The `ma_pcm_rb_init()` function takes the sample format and channel count as parameters because
it's the PCM variant of the ring buffer API. For the regular ring buffer that operates on bytes you
would call `ma_rb_init()` which leaves these out and just takes the size of the buffer in bytes
instead of frames. The fourth parameter is an optional pre-allocated buffer and the fifth parameter
is a pointer to a `ma_allocation_callbacks` structure for custom memory allocation routines.
Passing in `NULL` for this results in `MA_MALLOC()` and `MA_FREE()` being used.

Use `ma_pcm_rb_init_ex()` if you need a deinterleaved buffer. The data for each sub-buffer is
offset from each other based on the stride. To manage your sub-buffers you can use
`ma_pcm_rb_get_subbuffer_stride()`, `ma_pcm_rb_get_subbuffer_offset()` and
`ma_pcm_rb_get_subbuffer_ptr()`.

Use `ma_pcm_rb_acquire_read()` and `ma_pcm_rb_acquire_write()` to retrieve a pointer to a section
of the ring buffer. You specify the number of frames you need, and on output it will set to what
was actually acquired. If the read or write pointer is positioned such that the number of frames
requested will require a loop, it will be clamped to the end of the buffer. Therefore, the number
of frames you're given may be less than the number you requested.

After calling `ma_pcm_rb_acquire_read()` or `ma_pcm_rb_acquire_write()`, you do your work on the
buffer and then "commit" it with `ma_pcm_rb_commit_read()` or `ma_pcm_rb_commit_write()`. This is
where the read/write pointers are updated. When you commit you need to pass in the buffer that was
returned by the earlier call to `ma_pcm_rb_acquire_read()` or `ma_pcm_rb_acquire_write()` and is
only used for validation. The number of frames passed to `ma_pcm_rb_commit_read()` and
`ma_pcm_rb_commit_write()` is what's used to increment the pointers, and can be less that what was
originally requested.

If you want to correct for drift between the write pointer and the read pointer you can use a
combination of `ma_pcm_rb_pointer_distance()`, `ma_pcm_rb_seek_read()` and
`ma_pcm_rb_seek_write()`. Note that you can only move the pointers forward, and you should only
move the read pointer forward via the consumer thread, and the write pointer forward by the
producer thread. If there is too much space between the pointers, move the read pointer forward. If
there is too little space between the pointers, move the write pointer forward.

You can use a ring buffer at the byte level instead of the PCM frame level by using the `ma_rb`
API. This is exactly the same, only you will use the `ma_rb` functions instead of `ma_pcm_rb` and
instead of frame counts you will pass around byte counts.

The maximum size of the buffer in bytes is `0x7FFFFFFF-(MA_SIMD_ALIGNMENT-1)` due to the most
significant bit being used to encode a loop flag and the internally managed buffers always being
aligned to `MA_SIMD_ALIGNMENT`.

Note that the ring buffer is only thread safe when used by a single consumer thread and single
producer thread.



15. Backends
============
The following backends are supported by miniaudio. These are listed in order of default priority.
When no backend is specified when initializing a context or device, miniaudio will attempt to use
each of these backends in the order listed in the table below.

Note that backends that are not usable by the build target will not be included in the build. For
example, ALSA, which is specific to Linux, will not be included in the Windows build.

    +-------------+-----------------------+--------------------------------------------------------+
    | Name        | Enum Name             | Supported Operating Systems                            |
    +-------------+-----------------------+--------------------------------------------------------+
    | WASAPI      | ma_backend_wasapi     | Windows Vista+                                         |
    | DirectSound | ma_backend_dsound     | Windows XP+                                            |
    | WinMM       | ma_backend_winmm      | Windows 95+                                            |
    | Core Audio  | ma_backend_coreaudio  | macOS, iOS                                             |
    | sndio       | ma_backend_sndio      | OpenBSD                                                |
    | audio(4)    | ma_backend_audio4     | NetBSD, OpenBSD                                        |
    | OSS         | ma_backend_oss        | FreeBSD                                                |
    | PulseAudio  | ma_backend_pulseaudio | Cross Platform (disabled on Windows, BSD and Android)  |
    | ALSA        | ma_backend_alsa       | Linux                                                  |
    | JACK        | ma_backend_jack       | Cross Platform (disabled on BSD and Android)           |
    | AAudio      | ma_backend_aaudio     | Android 8+                                             |
    | OpenSL ES   | ma_backend_opensl     | Android (API level 16+)                                |
    | Web Audio   | ma_backend_webaudio   | Web (via Emscripten)                                   |
    | Custom      | ma_backend_custom     | Cross Platform                                         |
    | Null        | ma_backend_null       | Cross Platform (not used on Web)                       |
    +-------------+-----------------------+--------------------------------------------------------+

Some backends have some nuance details you may want to be aware of.

15.1. WASAPI
------------
- Low-latency shared mode will be disabled when using an application-defined sample rate which is
  different to the device's native sample rate. To work around this, set `wasapi.noAutoConvertSRC`
  to true in the device config. This is due to IAudioClient3_InitializeSharedAudioStream() failing
  when the `AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM` flag is specified. Setting wasapi.noAutoConvertSRC
  will result in miniaudio's internal resampler being used instead which will in turn enable the
  use of low-latency shared mode.

15.2. PulseAudio
----------------
- If you experience bad glitching/noise on Arch Linux, consider this fix from the Arch wiki:
  https://wiki.archlinux.org/index.php/PulseAudio/Troubleshooting#Glitches,_skips_or_crackling.
  Alternatively, consider using a different backend such as ALSA.

15.3. Android
-------------
- To capture audio on Android, remember to add the RECORD_AUDIO permission to your manifest:
  `<uses-permission android:name="android.permission.RECORD_AUDIO" />`
- With OpenSL|ES, only a single ma_context can be active at any given time. This is due to a
  limitation with OpenSL|ES.
- With AAudio, only default devices are enumerated. This is due to AAudio not having an enumeration
  API (devices are enumerated through Java). You can however perform your own device enumeration
  through Java and then set the ID in the ma_device_id structure (ma_device_id.aaudio) and pass it
  to ma_device_init().
- The backend API will perform resampling where possible. The reason for this as opposed to using
  miniaudio's built-in resampler is to take advantage of any potential device-specific
  optimizations the driver may implement.

BSD
---
- The sndio backend is currently only enabled on OpenBSD builds.
- The audio(4) backend is supported on OpenBSD, but you may need to disable sndiod before you can
  use it.

15.4. UWP
---------
- UWP only supports default playback and capture devices.
- UWP requires the Microphone capability to be enabled in the application's manifest (Package.appxmanifest):

    ```
    <Package ...>
        ...
        <Capabilities>
            <DeviceCapability Name="microphone" />
        </Capabilities>
    </Package>
    ```

15.5. Web Audio / Emscripten
----------------------------
- You cannot use `-std=c*` compiler flags, nor `-ansi`. This only applies to the Emscripten build.
- The first time a context is initialized it will create a global object called "miniaudio" whose
  primary purpose is to act as a factory for device objects.
- Currently the Web Audio backend uses ScriptProcessorNode's, but this may need to change later as
  they've been deprecated.
- Google has implemented a policy in their browsers that prevent automatic media output without
  first receiving some kind of user input. The following web page has additional details:
  https://developers.google.com/web/updates/2017/09/autoplay-policy-changes. Starting the device
  may fail if you try to start playback without first handling some kind of user input.



16. Optimization Tips
=====================
See below for some tips on improving performance.

16.1. Low Level API
-------------------
- In the data callback, if your data is already clipped prior to copying it into the output buffer,
  set the `noClip` config option in the device config to true. This will disable miniaudio's built
  in clipping function.
- By default, miniaudio will pre-silence the data callback's output buffer. If you know that you
  will always write valid data to the output buffer you can disable pre-silencing by setting the
  `noPreSilence` config option in the device config to true.

16.2. High Level API
--------------------
- If a sound does not require doppler or pitch shifting, consider disabling pitching by
  initializing the sound with the `MA_SOUND_FLAG_NO_PITCH` flag.
- If a sound does not require spatialization, disable it by initializing the sound with the
  `MA_SOUND_FLAG_NO_SPATIALIZATION` flag. It can be re-enabled again post-initialization with
  `ma_sound_set_spatialization_enabled()`.
- If you know all of your sounds will always be the same sample rate, set the engine's sample
  rate to match that of the sounds. Likewise, if you're using a self-managed resource manager,
  consider setting the decoded sample rate to match your sounds. By configuring everything to
  use a consistent sample rate, sample rate conversion can be avoided.



17. Miscellaneous Notes
=======================
- Automatic stream routing is enabled on a per-backend basis. Support is explicitly enabled for
  WASAPI and Core Audio, however other backends such as PulseAudio may naturally support it, though
  not all have been tested.
- When compiling with VC6 and earlier, decoding is restricted to files less than 2GB in size. This
  is due to 64-bit file APIs not being available.
*/

#ifndef miniaudio_h
#define miniaudio_h

#ifdef __cplusplus
extern "C" {
#endif

#define MA_STRINGIFY(x)     #x
#define MA_XSTRINGIFY(x)    MA_STRINGIFY(x)

#define MA_VERSION_MAJOR    0
#define MA_VERSION_MINOR    11
#define MA_VERSION_REVISION 23
#define MA_VERSION_STRING   MA_XSTRINGIFY(MA_VERSION_MAJOR) "." MA_XSTRINGIFY(MA_VERSION_MINOR) "." MA_XSTRINGIFY(MA_VERSION_REVISION)

#if defined(_MSC_VER) && !defined(__clang__)
    #pragma warning(push)
    #pragma warning(disable:4201)   /* nonstandard extension used: nameless struct/union */
    #pragma warning(disable:4214)   /* nonstandard extension used: bit field types other than int */
    #pragma warning(disable:4324)   /* structure was padded due to alignment specifier */
#elif defined(__clang__) || (defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 8)))
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wpedantic" /* For ISO C99 doesn't support unnamed structs/unions [-Wpedantic] */
    #if defined(__clang__)
        #pragma GCC diagnostic ignored "-Wc11-extensions"   /* anonymous unions are a C11 extension */
    #endif
#endif


#if defined(__LP64__) || defined(_WIN64) || (defined(__x86_64__) && !defined(__ILP32__)) || defined(_M_X64) || defined(__ia64) || defined(_M_IA64) || defined(__aarch64__) || defined(_M_ARM64) || defined(__powerpc64__) || defined(__ppc64__)
    #define MA_SIZEOF_PTR   8
#else
    #define MA_SIZEOF_PTR   4
#endif

#include <stddef.h> /* For size_t. */

/* Sized types. */
#if defined(MA_USE_STDINT)
    #include <stdint.h>
    typedef int8_t   ma_int8;
    typedef uint8_t  ma_uint8;
    typedef int16_t  ma_int16;
    typedef uint16_t ma_uint16;
    typedef int32_t  ma_int32;
    typedef uint32_t ma_uint32;
    typedef int64_t  ma_int64;
    typedef uint64_t ma_uint64;
#else
    typedef   signed char           ma_int8;
    typedef unsigned char           ma_uint8;
    typedef   signed short          ma_int16;
    typedef unsigned short          ma_uint16;
    typedef   signed int            ma_int32;
    typedef unsigned int            ma_uint32;
    #if defined(_MSC_VER) && !defined(__clang__)
        typedef   signed __int64    ma_int64;
        typedef unsigned __int64    ma_uint64;
    #else
        #if defined(__clang__) || (defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)))
            #pragma GCC diagnostic push
            #pragma GCC diagnostic ignored "-Wlong-long"
            #if defined(__clang__)
                #pragma GCC diagnostic ignored "-Wc++11-long-long"
            #endif
        #endif
        typedef   signed long long  ma_int64;
        typedef unsigned long long  ma_uint64;
        #if defined(__clang__) || (defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)))
            #pragma GCC diagnostic pop
        #endif
    #endif
#endif  /* MA_USE_STDINT */

#if MA_SIZEOF_PTR == 8
    typedef ma_uint64           ma_uintptr;
#else
    typedef ma_uint32           ma_uintptr;
#endif

typedef ma_uint8    ma_bool8;
typedef ma_uint32   ma_bool32;
#define MA_TRUE     1
#define MA_FALSE    0

/* These float types are not used universally by miniaudio. It's to simplify some macro expansion for atomic types. */
typedef float       ma_float;
typedef double      ma_double;

typedef void* ma_handle;
typedef void* ma_ptr;

/*
ma_proc is annoying because when compiling with GCC we get pedantic warnings about converting
between `void*` and `void (*)()`. We can't use `void (*)()` with MSVC however, because we'll get
warning C4191 about "type cast between incompatible function types". To work around this I'm going
to use a different data type depending on the compiler.
*/
#if defined(__GNUC__)
typedef void (*ma_proc)(void);
#else
typedef void* ma_proc;
#endif

#if defined(_MSC_VER) && !defined(_WCHAR_T_DEFINED)
typedef ma_uint16 wchar_t;
#endif

/* Define NULL for some compilers. */
#ifndef NULL
#define NULL 0
#endif

#if defined(SIZE_MAX)
    #define MA_SIZE_MAX    SIZE_MAX
#else
    #define MA_SIZE_MAX    0xFFFFFFFF  /* When SIZE_MAX is not defined by the standard library just default to the maximum 32-bit unsigned integer. */
#endif

#define MA_UINT64_MAX      (((ma_uint64)0xFFFFFFFF << 32) | (ma_uint64)0xFFFFFFFF)   /* Weird shifting syntax is for VC6 compatibility. */


/* Platform/backend detection. */
#if defined(_WIN32) || defined(__COSMOPOLITAN__)
    #define MA_WIN32
    #if defined(MA_FORCE_UWP) || (defined(WINAPI_FAMILY) && ((defined(WINAPI_FAMILY_PC_APP) && WINAPI_FAMILY == WINAPI_FAMILY_PC_APP) || (defined(WINAPI_FAMILY_PHONE_APP) && WINAPI_FAMILY == WINAPI_FAMILY_PHONE_APP)))
        #define MA_WIN32_UWP
    #elif defined(WINAPI_FAMILY) && (defined(WINAPI_FAMILY_GAMES) && WINAPI_FAMILY == WINAPI_FAMILY_GAMES)
        #define MA_WIN32_GDK
    #elif defined(NXDK)
        #define MA_WIN32_NXDK
    #else
        #define MA_WIN32_DESKTOP
    #endif

    /* The original Xbox. */
    #if defined(NXDK)   /* <-- Add other Xbox compiler toolchains here, and then add a toolchain-specific define in case we need to discriminate between them later. */
        #define MA_XBOX

        #if defined(NXDK)
            #define MA_XBOX_NXDK
        #endif
    #endif
#endif
#if defined(__MSDOS__) || defined(MSDOS) || defined(_MSDOS) || defined(__DOS__)
    #define MA_DOS

    /* No threading allowed on DOS. */
    #ifndef MA_NO_THREADING
    #define MA_NO_THREADING
    #endif

    /* No runtime linking allowed on DOS. */
    #ifndef MA_NO_RUNTIME_LINKING
    #define MA_NO_RUNTIME_LINKING
    #endif
#endif
#if !defined(MA_WIN32) && !defined(MA_DOS)    /* If it's not Win32, assume POSIX. */
    #define MA_POSIX

    #if !defined(MA_NO_THREADING)
        /*
        Use the MA_NO_PTHREAD_IN_HEADER option at your own risk. This is intentionally undocumented.
        You can use this to avoid including pthread.h in the header section. The downside is that it
        results in some fixed sized structures being declared for the various types that are used in
        miniaudio. The risk here is that these types might be too small for a given platform. This
        risk is yours to take and no support will be offered if you enable this option.
        */
        #ifndef MA_NO_PTHREAD_IN_HEADER
            #include <pthread.h>    /* Unfortunate #include, but needed for pthread_t, pthread_mutex_t and pthread_cond_t types. */
            typedef pthread_t       ma_pthread_t;
            typedef pthread_mutex_t ma_pthread_mutex_t;
            typedef pthread_cond_t  ma_pthread_cond_t;
        #else
            typedef ma_uintptr      ma_pthread_t;
            typedef union           ma_pthread_mutex_t { char __data[40]; ma_uint64 __alignment; } ma_pthread_mutex_t;
            typedef union           ma_pthread_cond_t  { char __data[48]; ma_uint64 __alignment; } ma_pthread_cond_t;
        #endif
    #endif

    #if defined(__unix__)
        #define MA_UNIX
    #endif
    #if defined(__linux__)
        #define MA_LINUX
    #endif
    #if defined(__APPLE__)
        #define MA_APPLE
    #endif
    #if defined(__DragonFly__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
        #define MA_BSD
    #endif
    #if defined(__ANDROID__)
        #define MA_ANDROID
    #endif
    #if defined(__EMSCRIPTEN__)
        #define MA_EMSCRIPTEN
    #endif
    #if defined(__ORBIS__)
        #define MA_ORBIS
    #endif
    #if defined(__PROSPERO__)
        #define MA_PROSPERO
    #endif
    #if defined(__3DS__)
        #define MA_3DS
    #endif
    #if defined(__SWITCH__) || defined(__NX__)
        #define MA_SWITCH
    #endif
    #if defined(__BEOS__) || defined(__HAIKU__)
        #define MA_BEOS
    #endif
    #if defined(__HAIKU__)
        #define MA_HAIKU
    #endif
#endif

#if !defined(MA_FALLTHROUGH) && defined(__cplusplus) && __cplusplus >= 201703L
    #define MA_FALLTHROUGH [[fallthrough]]
#endif
#if !defined(MA_FALLTHROUGH) && defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202000L
    #define MA_FALLTHROUGH [[fallthrough]]
#endif
#if !defined(MA_FALLTHROUGH) && defined(__has_attribute)
    #if __has_attribute(fallthrough)
        #define MA_FALLTHROUGH __attribute__((fallthrough))
    #endif
#endif
#if !defined(MA_FALLTHROUGH)
    #define MA_FALLTHROUGH ((void)0)
#endif

#ifdef _MSC_VER
    #define MA_INLINE __forceinline

    /* noinline was introduced in Visual Studio 2005. */
    #if _MSC_VER >= 1400
        #define MA_NO_INLINE __declspec(noinline)
    #else
        #define MA_NO_INLINE
    #endif
#elif defined(__GNUC__)
    /*
    I've had a bug report where GCC is emitting warnings about functions possibly not being inlineable. This warning happens when
    the __attribute__((always_inline)) attribute is defined without an "inline" statement. I think therefore there must be some
    case where "__inline__" is not always defined, thus the compiler emitting these warnings. When using -std=c89 or -ansi on the
    command line, we cannot use the "inline" keyword and instead need to use "__inline__". In an attempt to work around this issue
    I am using "__inline__" only when we're compiling in strict ANSI mode.
    */
    #if defined(__STRICT_ANSI__)
        #define MA_GNUC_INLINE_HINT __inline__
    #else
        #define MA_GNUC_INLINE_HINT inline
    #endif

    #if (__GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 2)) || defined(__clang__)
        #define MA_INLINE MA_GNUC_INLINE_HINT __attribute__((always_inline))
        #define MA_NO_INLINE __attribute__((noinline))
    #else
        #define MA_INLINE MA_GNUC_INLINE_HINT
        #define MA_NO_INLINE
    #endif
#elif defined(__WATCOMC__)
    #define MA_INLINE __inline
    #define MA_NO_INLINE
#else
    #define MA_INLINE
    #define MA_NO_INLINE
#endif

/* MA_DLL is not officially supported. You're on your own if you want to use this. */
#if defined(MA_DLL)
    #if defined(_WIN32)
        #define MA_DLL_IMPORT  __declspec(dllimport)
        #define MA_DLL_EXPORT  __declspec(dllexport)
        #define MA_DLL_PRIVATE static
    #else
        #if defined(__GNUC__) && __GNUC__ >= 4
            #define MA_DLL_IMPORT  __attribute__((visibility("default")))
            #define MA_DLL_EXPORT  __attribute__((visibility("default")))
            #define MA_DLL_PRIVATE __attribute__((visibility("hidden")))
        #else
            #define MA_DLL_IMPORT
            #define MA_DLL_EXPORT
            #define MA_DLL_PRIVATE static
        #endif
    #endif
#endif

#if !defined(MA_API)
    #if defined(MA_DLL)
        #if defined(MINIAUDIO_IMPLEMENTATION) || defined(MA_IMPLEMENTATION)
            #define MA_API  MA_DLL_EXPORT
        #else
            #define MA_API  MA_DLL_IMPORT
        #endif
    #else
        #define MA_API extern
    #endif
#endif

#if !defined(MA_STATIC)
    #if defined(MA_DLL)
        #define MA_PRIVATE MA_DLL_PRIVATE
    #else
        #define MA_PRIVATE static
    #endif
#endif


/* SIMD alignment in bytes. Currently set to 32 bytes in preparation for future AVX optimizations. */
#define MA_SIMD_ALIGNMENT  32

/*
Special wchar_t type to ensure any structures in the public sections that reference it have a
consistent size across all platforms.

On Windows, wchar_t is 2 bytes, whereas everywhere else it's 4 bytes. Since Windows likes to use
wchar_t for its IDs, we need a special explicitly sized wchar type that is always 2 bytes on all
platforms.
*/
#if !defined(MA_POSIX) && defined(MA_WIN32)
typedef wchar_t     ma_wchar_win32;
#else
typedef ma_uint16   ma_wchar_win32;
#endif



/*
Logging Levels
==============
Log levels are only used to give logging callbacks some context as to the severity of a log message
so they can do filtering. All log levels will be posted to registered logging callbacks. If you
don't want to output a certain log level you can discriminate against the log level in the callback.

MA_LOG_LEVEL_DEBUG
    Used for debugging. Useful for debug and test builds, but should be disabled in release builds.

MA_LOG_LEVEL_INFO
    Informational logging. Useful for debugging. This will never be called from within the data
    callback.

MA_LOG_LEVEL_WARNING
    Warnings. You should enable this in you development builds and action them when encountered. These
    logs usually indicate a potential problem or misconfiguration, but still allow you to keep
    running. This will never be called from within the data callback.

MA_LOG_LEVEL_ERROR
    Error logging. This will be fired when an operation fails and is subsequently aborted. This can
    be fired from within the data callback, in which case the device will be stopped. You should
    always have this log level enabled.
*/
typedef enum
{
    MA_LOG_LEVEL_DEBUG   = 4,
    MA_LOG_LEVEL_INFO    = 3,
    MA_LOG_LEVEL_WARNING = 2,
    MA_LOG_LEVEL_ERROR   = 1
} ma_log_level;

/*
Variables needing to be accessed atomically should be declared with this macro for two reasons:

    1) It allows people who read the code to identify a variable as such; and
    2) It forces alignment on platforms where it's required or optimal.

Note that for x86/64, alignment is not strictly necessary, but does have some performance
implications. Where supported by the compiler, alignment will be used, but otherwise if the CPU
architecture does not require it, it will simply leave it unaligned. This is the case with old
versions of Visual Studio, which I've confirmed with at least VC6.
*/
#if !defined(_MSC_VER) && defined (__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
    #include <stdalign.h>
    #define MA_ATOMIC(alignment, type)            _Alignas(alignment) type
#else
    #if defined(__GNUC__)
        /* GCC-style compilers. */
        #define MA_ATOMIC(alignment, type)        type __attribute__((aligned(alignment)))
    #elif defined(_MSC_VER) && _MSC_VER > 1200  /* 1200 = VC6. Alignment not supported, but not necessary because x86 is the only supported target. */
        /* MSVC. */
        #define MA_ATOMIC(alignment, type)        __declspec(align(alignment)) type
    #else
        /* Other compilers. */
        #define MA_ATOMIC(alignment, type)        type
    #endif
#endif

typedef struct ma_context ma_context;
typedef struct ma_device ma_device;

typedef ma_uint8 ma_channel;
typedef enum
{
    MA_CHANNEL_NONE               = 0,
    MA_CHANNEL_MONO               = 1,
    MA_CHANNEL_FRONT_LEFT         = 2,
    MA_CHANNEL_FRONT_RIGHT        = 3,
    MA_CHANNEL_FRONT_CENTER       = 4,
    MA_CHANNEL_LFE                = 5,
    MA_CHANNEL_BACK_LEFT          = 6,
    MA_CHANNEL_BACK_RIGHT         = 7,
    MA_CHANNEL_FRONT_LEFT_CENTER  = 8,
    MA_CHANNEL_FRONT_RIGHT_CENTER = 9,
    MA_CHANNEL_BACK_CENTER        = 10,
    MA_CHANNEL_SIDE_LEFT          = 11,
    MA_CHANNEL_SIDE_RIGHT         = 12,
    MA_CHANNEL_TOP_CENTER         = 13,
    MA_CHANNEL_TOP_FRONT_LEFT     = 14,
    MA_CHANNEL_TOP_FRONT_CENTER   = 15,
    MA_CHANNEL_TOP_FRONT_RIGHT    = 16,
    MA_CHANNEL_TOP_BACK_LEFT      = 17,
    MA_CHANNEL_TOP_BACK_CENTER    = 18,
    MA_CHANNEL_TOP_BACK_RIGHT     = 19,
    MA_CHANNEL_AUX_0              = 20,
    MA_CHANNEL_AUX_1              = 21,
    MA_CHANNEL_AUX_2              = 22,
    MA_CHANNEL_AUX_3              = 23,
    MA_CHANNEL_AUX_4              = 24,
    MA_CHANNEL_AUX_5              = 25,
    MA_CHANNEL_AUX_6              = 26,
    MA_CHANNEL_AUX_7              = 27,
    MA_CHANNEL_AUX_8              = 28,
    MA_CHANNEL_AUX_9              = 29,
    MA_CHANNEL_AUX_10             = 30,
    MA_CHANNEL_AUX_11             = 31,
    MA_CHANNEL_AUX_12             = 32,
    MA_CHANNEL_AUX_13             = 33,
    MA_CHANNEL_AUX_14             = 34,
    MA_CHANNEL_AUX_15             = 35,
    MA_CHANNEL_AUX_16             = 36,
    MA_CHANNEL_AUX_17             = 37,
    MA_CHANNEL_AUX_18             = 38,
    MA_CHANNEL_AUX_19             = 39,
    MA_CHANNEL_AUX_20             = 40,
    MA_CHANNEL_AUX_21             = 41,
    MA_CHANNEL_AUX_22             = 42,
    MA_CHANNEL_AUX_23             = 43,
    MA_CHANNEL_AUX_24             = 44,
    MA_CHANNEL_AUX_25             = 45,
    MA_CHANNEL_AUX_26             = 46,
    MA_CHANNEL_AUX_27             = 47,
    MA_CHANNEL_AUX_28             = 48,
    MA_CHANNEL_AUX_29             = 49,
    MA_CHANNEL_AUX_30             = 50,
    MA_CHANNEL_AUX_31             = 51,
    MA_CHANNEL_LEFT               = MA_CHANNEL_FRONT_LEFT,
    MA_CHANNEL_RIGHT              = MA_CHANNEL_FRONT_RIGHT,
    MA_CHANNEL_POSITION_COUNT     = (MA_CHANNEL_AUX_31 + 1)
} _ma_channel_position; /* Do not use `_ma_channel_position` directly. Use `ma_channel` instead. */

typedef enum
{
    MA_SUCCESS                        =  0,
    MA_ERROR                          = -1,  /* A generic error. */
    MA_INVALID_ARGS                   = -2,
    MA_INVALID_OPERATION              = -3,
    MA_OUT_OF_MEMORY                  = -4,
    MA_OUT_OF_RANGE                   = -5,
    MA_ACCESS_DENIED                  = -6,
    MA_DOES_NOT_EXIST                 = -7,
    MA_ALREADY_EXISTS                 = -8,
    MA_TOO_MANY_OPEN_FILES            = -9,
    MA_INVALID_FILE                   = -10,
    MA_TOO_BIG                        = -11,
    MA_PATH_TOO_LONG                  = -12,
    MA_NAME_TOO_LONG                  = -13,
    MA_NOT_DIRECTORY                  = -14,
    MA_IS_DIRECTORY                   = -15,
    MA_DIRECTORY_NOT_EMPTY            = -16,
    MA_AT_END                         = -17,
    MA_NO_SPACE                       = -18,
    MA_BUSY                           = -19,
    MA_IO_ERROR                       = -20,
    MA_INTERRUPT                      = -21,
    MA_UNAVAILABLE                    = -22,
    MA_ALREADY_IN_USE                 = -23,
    MA_BAD_ADDRESS                    = -24,
    MA_BAD_SEEK                       = -25,
    MA_BAD_PIPE                       = -26,
    MA_DEADLOCK                       = -27,
    MA_TOO_MANY_LINKS                 = -28,
    MA_NOT_IMPLEMENTED                = -29,
    MA_NO_MESSAGE                     = -30,
    MA_BAD_MESSAGE                    = -31,
    MA_NO_DATA_AVAILABLE              = -32,
    MA_INVALID_DATA                   = -33,
    MA_TIMEOUT                        = -34,
    MA_NO_NETWORK                     = -35,
    MA_NOT_UNIQUE                     = -36,
    MA_NOT_SOCKET                     = -37,
    MA_NO_ADDRESS                     = -38,
    MA_BAD_PROTOCOL                   = -39,
    MA_PROTOCOL_UNAVAILABLE           = -40,
    MA_PROTOCOL_NOT_SUPPORTED         = -41,
    MA_PROTOCOL_FAMILY_NOT_SUPPORTED  = -42,
    MA_ADDRESS_FAMILY_NOT_SUPPORTED   = -43,
    MA_SOCKET_NOT_SUPPORTED           = -44,
    MA_CONNECTION_RESET               = -45,
    MA_ALREADY_CONNECTED              = -46,
    MA_NOT_CONNECTED                  = -47,
    MA_CONNECTION_REFUSED             = -48,
    MA_NO_HOST                        = -49,
    MA_IN_PROGRESS                    = -50,
    MA_CANCELLED                      = -51,
    MA_MEMORY_ALREADY_MAPPED          = -52,

    /* General non-standard errors. */
    MA_CRC_MISMATCH                   = -100,

    /* General miniaudio-specific errors. */
    MA_FORMAT_NOT_SUPPORTED           = -200,
    MA_DEVICE_TYPE_NOT_SUPPORTED      = -201,
    MA_SHARE_MODE_NOT_SUPPORTED       = -202,
    MA_NO_BACKEND                     = -203,
    MA_NO_DEVICE                      = -204,
    MA_API_NOT_FOUND                  = -205,
    MA_INVALID_DEVICE_CONFIG          = -206,
    MA_LOOP                           = -207,
    MA_BACKEND_NOT_ENABLED            = -208,

    /* State errors. */
    MA_DEVICE_NOT_INITIALIZED         = -300,
    MA_DEVICE_ALREADY_INITIALIZED     = -301,
    MA_DEVICE_NOT_STARTED             = -302,
    MA_DEVICE_NOT_STOPPED             = -303,

    /* Operation errors. */
    MA_FAILED_TO_INIT_BACKEND         = -400,
    MA_FAILED_TO_OPEN_BACKEND_DEVICE  = -401,
    MA_FAILED_TO_START_BACKEND_DEVICE = -402,
    MA_FAILED_TO_STOP_BACKEND_DEVICE  = -403
} ma_result;


#define MA_MIN_CHANNELS                 1
#ifndef MA_MAX_CHANNELS
#define MA_MAX_CHANNELS                 254
#endif

#ifndef MA_MAX_FILTER_ORDER
#define MA_MAX_FILTER_ORDER             8
#endif

typedef enum
{
    ma_stream_format_pcm = 0
} ma_stream_format;

typedef enum
{
    ma_stream_layout_interleaved = 0,
    ma_stream_layout_deinterleaved
} ma_stream_layout;

typedef enum
{
    ma_dither_mode_none = 0,
    ma_dither_mode_rectangle,
    ma_dither_mode_triangle
} ma_dither_mode;

typedef enum
{
    /*
    I like to keep these explicitly defined because they're used as a key into a lookup table. When items are
    added to this, make sure there are no gaps and that they're added to the lookup table in ma_get_bytes_per_sample().
    */
    ma_format_unknown = 0,     /* Mainly used for indicating an error, but also used as the default for the output format for decoders. */
    ma_format_u8      = 1,
    ma_format_s16     = 2,     /* Seems to be the most widely supported format. */
    ma_format_s24     = 3,     /* Tightly packed. 3 bytes per sample. */
    ma_format_s32     = 4,
    ma_format_f32     = 5,
    ma_format_count
} ma_format;

typedef enum
{
    /* Standard rates need to be in priority order. */
    ma_standard_sample_rate_48000  = 48000,     /* Most common */
    ma_standard_sample_rate_44100  = 44100,

    ma_standard_sample_rate_32000  = 32000,     /* Lows */
    ma_standard_sample_rate_24000  = 24000,
    ma_standard_sample_rate_22050  = 22050,

    ma_standard_sample_rate_88200  = 88200,     /* Highs */
    ma_standard_sample_rate_96000  = 96000,
    ma_standard_sample_rate_176400 = 176400,
    ma_standard_sample_rate_192000 = 192000,

    ma_standard_sample_rate_16000  = 16000,     /* Extreme lows */
    ma_standard_sample_rate_11025  = 11025,
    ma_standard_sample_rate_8000   = 8000,

    ma_standard_sample_rate_352800 = 352800,    /* Extreme highs */
    ma_standard_sample_rate_384000 = 384000,

    ma_standard_sample_rate_min    = ma_standard_sample_rate_8000,
    ma_standard_sample_rate_max    = ma_standard_sample_rate_384000,
    ma_standard_sample_rate_count  = 14         /* Need to maintain the count manually. Make sure this is updated if items are added to enum. */
} ma_standard_sample_rate;


typedef enum
{
    ma_channel_mix_mode_rectangular = 0,   /* Simple averaging based on the plane(s) the channel is sitting on. */
    ma_channel_mix_mode_simple,            /* Drop excess channels; zeroed out extra channels. */
    ma_channel_mix_mode_custom_weights,    /* Use custom weights specified in ma_channel_converter_config. */
    ma_channel_mix_mode_default = ma_channel_mix_mode_rectangular
} ma_channel_mix_mode;

typedef enum
{
    ma_standard_channel_map_microsoft,
    ma_standard_channel_map_alsa,
    ma_standard_channel_map_rfc3551,   /* Based off AIFF. */
    ma_standard_channel_map_flac,
    ma_standard_channel_map_vorbis,
    ma_standard_channel_map_sound4,    /* FreeBSD's sound(4). */
    ma_standard_channel_map_sndio,     /* www.sndio.org/tips.html */
    ma_standard_channel_map_webaudio = ma_standard_channel_map_flac, /* https://webaudio.github.io/web-audio-api/#ChannelOrdering. Only 1, 2, 4 and 6 channels are defined, but can fill in the gaps with logical assumptions. */
    ma_standard_channel_map_default = ma_standard_channel_map_microsoft
} ma_standard_channel_map;

typedef enum
{
    ma_performance_profile_low_latency = 0,
    ma_performance_profile_conservative
} ma_performance_profile;


typedef struct
{
    void* pUserData;
    void* (* onMalloc)(size_t sz, void* pUserData);
    void* (* onRealloc)(void* p, size_t sz, void* pUserData);
    void  (* onFree)(void* p, void* pUserData);
} ma_allocation_callbacks;

typedef struct
{
    ma_uint32 state;
} ma_lcg;


/*
Atomics.

These are typesafe structures to prevent errors as a result of forgetting to reference variables atomically. It's too
easy to introduce subtle bugs where you accidentally do a regular assignment instead of an atomic load/store, etc. By
using a struct we can enforce the use of atomics at compile time.

These types are declared in the header section because we need to reference them in structs below, but functions for
using them are only exposed in the implementation section. I do not want these to be part of the public API.

There's a few downsides to this system. The first is that you need to declare a new struct for each type. Below are
some macros to help with the declarations. They will be named like so:

    ma_atomic_uint32 - atomic ma_uint32
    ma_atomic_int32  - atomic ma_int32
    ma_atomic_uint64 - atomic ma_uint64
    ma_atomic_float  - atomic float
    ma_atomic_bool32 - atomic ma_bool32

The other downside is that atomic pointers are extremely messy. You need to declare a new struct for each specific
type of pointer you need to make atomic. For example, an atomic ma_node* will look like this:

    MA_ATOMIC_SAFE_TYPE_IMPL_PTR(node)

Which will declare a type struct that's named like so:

    ma_atomic_ptr_node

Functions to use the atomic types are declared in the implementation section. All atomic functions are prefixed with
the name of the struct. For example:

    ma_atomic_uint32_set() - Atomic store of ma_uint32
    ma_atomic_uint32_get() - Atomic load of ma_uint32
    etc.

For pointer types it's the same, which makes them a bit messy to use due to the length of each function name, but in
return you get type safety and enforcement of atomic operations.
*/
#define MA_ATOMIC_SAFE_TYPE_DECL(c89TypeExtension, typeSize, type) \
    typedef struct \
    { \
        MA_ATOMIC(typeSize, ma_##type) value; \
    } ma_atomic_##type; \

#define MA_ATOMIC_SAFE_TYPE_DECL_PTR(type) \
    typedef struct \
    { \
        MA_ATOMIC(MA_SIZEOF_PTR, ma_##type*) value; \
    } ma_atomic_ptr_##type; \

MA_ATOMIC_SAFE_TYPE_DECL(32,  4, uint32)
MA_ATOMIC_SAFE_TYPE_DECL(i32, 4, int32)
MA_ATOMIC_SAFE_TYPE_DECL(64,  8, uint64)
MA_ATOMIC_SAFE_TYPE_DECL(f32, 4, float)
MA_ATOMIC_SAFE_TYPE_DECL(32,  4, bool32)


/* Spinlocks are 32-bit for compatibility reasons. */
typedef ma_uint32 ma_spinlock;

#ifndef MA_NO_THREADING
    /* Thread priorities should be ordered such that the default priority of the worker thread is 0. */
    typedef enum
    {
        ma_thread_priority_idle     = -5,
        ma_thread_priority_lowest   = -4,
        ma_thread_priority_low      = -3,
        ma_thread_priority_normal   = -2,
        ma_thread_priority_high     = -1,
        ma_thread_priority_highest  =  0,
        ma_thread_priority_realtime =  1,
        ma_thread_priority_default  =  0
    } ma_thread_priority;

    #if defined(MA_POSIX)
        typedef ma_pthread_t ma_thread;
    #elif defined(MA_WIN32)
        typedef ma_handle ma_thread;
    #endif

    #if defined(MA_POSIX)
        typedef ma_pthread_mutex_t ma_mutex;
    #elif defined(MA_WIN32)
        typedef ma_handle ma_mutex;
    #endif

    #if defined(MA_POSIX)
        typedef struct
        {
            ma_uint32 value;
            ma_pthread_mutex_t lock;
            ma_pthread_cond_t cond;
        } ma_event;
    #elif defined(MA_WIN32)
        typedef ma_handle ma_event;
    #endif

    #if defined(MA_POSIX)
        typedef struct
        {
            int value;
            ma_pthread_mutex_t lock;
            ma_pthread_cond_t cond;
        } ma_semaphore;
    #elif defined(MA_WIN32)
        typedef ma_handle ma_semaphore;
    #endif
#else
    /* MA_NO_THREADING is set which means threading is disabled. Threading is required by some API families. If any of these are enabled we need to throw an error. */
    #ifndef MA_NO_DEVICE_IO
        #error "MA_NO_THREADING cannot be used without MA_NO_DEVICE_IO";
    #endif
#endif  /* MA_NO_THREADING */


/*
Retrieves the version of miniaudio as separated integers. Each component can be NULL if it's not required.
*/
MA_API void ma_version(ma_uint32* pMajor, ma_uint32* pMinor, ma_uint32* pRevision);

/*
Retrieves the version of miniaudio as a string which can be useful for logging purposes.
*/
MA_API const char* ma_version_string(void);


/**************************************************************************************************************************************************************

Logging

**************************************************************************************************************************************************************/
#include <stdarg.h> /* For va_list. */

#if defined(__has_attribute)
    #if __has_attribute(format)
        #define MA_ATTRIBUTE_FORMAT(fmt, va) __attribute__((format(printf, fmt, va)))
    #endif
#endif
#ifndef MA_ATTRIBUTE_FORMAT
#define MA_ATTRIBUTE_FORMAT(fmt, va)
#endif

#ifndef MA_MAX_LOG_CALLBACKS
#define MA_MAX_LOG_CALLBACKS    4
#endif


/*
The callback for handling log messages.


Parameters
----------
pUserData (in)
    The user data pointer that was passed into ma_log_register_callback().

logLevel (in)
    The log level. This can be one of the following:

    +----------------------+
    | Log Level            |
    +----------------------+
    | MA_LOG_LEVEL_DEBUG   |
    | MA_LOG_LEVEL_INFO    |
    | MA_LOG_LEVEL_WARNING |
    | MA_LOG_LEVEL_ERROR   |
    +----------------------+

pMessage (in)
    The log message.
*/
typedef void (* ma_log_callback_proc)(void* pUserData, ma_uint32 level, const char* pMessage);

typedef struct
{
    ma_log_callback_proc onLog;
    void* pUserData;
} ma_log_callback;

MA_API ma_log_callback ma_log_callback_init(ma_log_callback_proc onLog, void* pUserData);


typedef struct
{
    ma_log_callback callbacks[MA_MAX_LOG_CALLBACKS];
    ma_uint32 callbackCount;
    ma_allocation_callbacks allocationCallbacks;    /* Need to store these persistently because ma_log_postv() might need to allocate a buffer on the heap. */
#ifndef MA_NO_THREADING
    ma_mutex lock;  /* For thread safety just to make it easier and safer for the logging implementation. */
#endif
} ma_log;

MA_API ma_result ma_log_init(const ma_allocation_callbacks* pAllocationCallbacks, ma_log* pLog);
MA_API void ma_log_uninit(ma_log* pLog);
MA_API ma_result ma_log_register_callback(ma_log* pLog, ma_log_callback callback);
MA_API ma_result ma_log_unregister_callback(ma_log* pLog, ma_log_callback callback);
MA_API ma_result ma_log_post(ma_log* pLog, ma_uint32 level, const char* pMessage);
MA_API ma_result ma_log_postv(ma_log* pLog, ma_uint32 level, const char* pFormat, va_list args);
MA_API ma_result ma_log_postf(ma_log* pLog, ma_uint32 level, const char* pFormat, ...) MA_ATTRIBUTE_FORMAT(3, 4);


/**************************************************************************************************************************************************************

Biquad Filtering

**************************************************************************************************************************************************************/
typedef union
{
    float    f32;
    ma_int32 s32;
} ma_biquad_coefficient;

typedef struct
{
    ma_format format;
    ma_uint32 channels;
    double b0;
    double b1;
    double b2;
    double a0;
    double a1;
    double a2;
} ma_biquad_config;

MA_API ma_biquad_config ma_biquad_config_init(ma_format format, ma_uint32 channels, double b0, double b1, double b2, double a0, double a1, double a2);

typedef struct
{
    ma_format format;
    ma_uint32 channels;
    ma_biquad_coefficient b0;
    ma_biquad_coefficient b1;
    ma_biquad_coefficient b2;
    ma_biquad_coefficient a1;
    ma_biquad_coefficient a2;
    ma_biquad_coefficient* pR1;
    ma_biquad_coefficient* pR2;

    /* Memory management. */
    void* _pHeap;
    ma_bool32 _ownsHeap;
} ma_biquad;

MA_API ma_result ma_biquad_get_heap_size(const ma_biquad_config* pConfig, size_t* pHeapSizeInBytes);
MA_API ma_result ma_biquad_init_preallocated(const ma_biquad_config* pConfig, void* pHeap, ma_biquad* pBQ);
MA_API ma_result ma_biquad_init(const ma_biquad_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_biquad* pBQ);
MA_API void ma_biquad_uninit(ma_biquad* pBQ, const ma_allocation_callbacks* pAllocationCallbacks);
MA_API ma_result ma_biquad_reinit(const ma_biquad_config* pConfig, ma_biquad* pBQ);
MA_API ma_result ma_biquad_clear_cache(ma_biquad* pBQ);
MA_API ma_result ma_biquad_process_pcm_frames(ma_biquad* pBQ, void* pFramesOut, const void* pFramesIn, ma_uint64 frameCount);
MA_API ma_uint32 ma_biquad_get_latency(const ma_biquad* pBQ);


/**************************************************************************************************************************************************************

Low-Pass Filtering

**************************************************************************************************************************************************************/
typedef struct
{
    ma_format format;
    ma_uint32 channels;
    ma_uint32 sampleRate;
    double cutoffFrequency;
    double q;
} ma_lpf1_config, ma_lpf2_config;

MA_API ma_lpf1_config ma_lpf1_config_init(ma_format format, ma_uint32 channels, ma_uint32 sampleRate, double cutoffFrequency);
MA_API ma_lpf2_config ma_lpf2_config_init(ma_format format, ma_uint32 channels, ma_uint32 sampleRate, double cutoffFrequency, double q);

typedef struct
{
    ma_format format;
    ma_uint32 channels;
    ma_biquad_coefficient a;
    ma_biquad_coefficient* pR1;

    /* Memory management. */
    void* _pHeap;
    ma_bool32 _ownsHeap;
} ma_lpf1;

MA_API ma_result ma_lpf1_get_heap_size(const ma_lpf1_config* pConfig, size_t* pHeapSizeInBytes);
MA_API ma_result ma_lpf1_init_preallocated(const ma_lpf1_config* pConfig, void* pHeap, ma_lpf1* pLPF);
MA_API ma_result ma_lpf1_init(const ma_lpf1_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_lpf1* pLPF);
MA_API void ma_lpf1_uninit(ma_lpf1* pLPF, const ma_allocation_callbacks* pAllocationCallbacks);
MA_API ma_result ma_lpf1_reinit(const ma_lpf1_config* pConfig, ma_lpf1* pLPF);
MA_API ma_result ma_lpf1_clear_cache(ma_lpf1* pLPF);
MA_API ma_result ma_lpf1_process_pcm_frames(ma_lpf1* pLPF, void* pFramesOut, const void* pFramesIn, ma_uint64 frameCount);
MA_API ma_uint32 ma_lpf1_get_latency(const ma_lpf1* pLPF);

typedef struct
{
    ma_biquad bq;   /* The second order low-pass filter is implemented as a biquad filter. */
} ma_lpf2;

MA_API ma_result ma_lpf2_get_heap_size(const ma_lpf2_config* pConfig, size_t* pHeapSizeInBytes);
MA_API ma_result ma_lpf2_init_preallocated(const ma_lpf2_config* pConfig, void* pHeap, ma_lpf2* pHPF);
MA_API ma_result ma_lpf2_init(const ma_lpf2_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_lpf2* pLPF);
MA_API void ma_lpf2_uninit(ma_lpf2* pLPF, const ma_allocation_callbacks* pAllocationCallbacks);
MA_API ma_result ma_lpf2_reinit(const ma_lpf2_config* pConfig, ma_lpf2* pLPF);
MA_API ma_result ma_lpf2_clear_cache(ma_lpf2* pLPF);
MA_API ma_result ma_lpf2_process_pcm_frames(ma_lpf2* pLPF, void* pFramesOut, const void* pFramesIn, ma_uint64 frameCount);
MA_API ma_uint32 ma_lpf2_get_latency(const ma_lpf2* pLPF);


typedef struct
{
    ma_format format;
    ma_uint32 channels;
    ma_uint32 sampleRate;
    double cutoffFrequency;
    ma_uint32 order;    /* If set to 0, will be treated as a passthrough (no filtering will be applied). */
} ma_lpf_config;

MA_API ma_lpf_config ma_lpf_config_init(ma_format format, ma_uint32 channels, ma_uint32 sampleRate, double cutoffFrequency, ma_uint32 order);

typedef struct
{
    ma_format format;
    ma_uint32 channels;
    ma_uint32 sampleRate;
    ma_uint32 lpf1Count;
    ma_uint32 lpf2Count;
    ma_lpf1* pLPF1;
    ma_lpf2* pLPF2;

    /* Memory management. */
    void* _pHeap;
    ma_bool32 _ownsHeap;
} ma_lpf;

MA_API ma_result ma_lpf_get_heap_size(const ma_lpf_config* pConfig, size_t* pHeapSizeInBytes);
MA_API ma_result ma_lpf_init_preallocated(const ma_lpf_config* pConfig, void* pHeap, ma_lpf* pLPF);
MA_API ma_result ma_lpf_init(const ma_lpf_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_lpf* pLPF);
MA_API void ma_lpf_uninit(ma_lpf* pLPF, const ma_allocation_callbacks* pAllocationCallbacks);
MA_API ma_result ma_lpf_reinit(const ma_lpf_config* pConfig, ma_lpf* pLPF);
MA_API ma_result ma_lpf_clear_cache(ma_lpf* pLPF);
MA_API ma_result ma_lpf_process_pcm_frames(ma_lpf* pLPF, void* pFramesOut, const void* pFramesIn, ma_uint64 frameCount);
MA_API ma_uint32 ma_lpf_get_latency(const ma_lpf* pLPF);


/**************************************************************************************************************************************************************

High-Pass Filtering

**************************************************************************************************************************************************************/
typedef struct
{
    ma_format format;
    ma_uint32 channels;
    ma_uint32 sampleRate;
    double cutoffFrequency;
    double q;
} ma_hpf1_config, ma_hpf2_config;

MA_API ma_hpf1_config ma_hpf1_config_init(ma_format format, ma_uint32 channels, ma_uint32 sampleRate, double cutoffFrequency);
MA_API ma_hpf2_config ma_hpf2_config_init(ma_format format, ma_uint32 channels, ma_uint32 sampleRate, double cutoffFrequency, double q);

typedef struct
{
    ma_format format;
    ma_uint32 channels;
    ma_biquad_coefficient a;
    ma_biquad_coefficient* pR1;

    /* Memory management. */
    void* _pHeap;
    ma_bool32 _ownsHeap;
} ma_hpf1;

MA_API ma_result ma_hpf1_get_heap_size(const ma_hpf1_config* pConfig, size_t* pHeapSizeInBytes);
MA_API ma_result ma_hpf1_init_preallocated(const ma_hpf1_config* pConfig, void* pHeap, ma_hpf1* pLPF);
MA_API ma_result ma_hpf1_init(const ma_hpf1_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_hpf1* pHPF);
MA_API void ma_hpf1_uninit(ma_hpf1* pHPF, const ma_allocation_callbacks* pAllocationCallbacks);
MA_API ma_result ma_hpf1_reinit(const ma_hpf1_config* pConfig, ma_hpf1* pHPF);
MA_API ma_result ma_hpf1_process_pcm_frames(ma_hpf1* pHPF, void* pFramesOut, const void* pFramesIn, ma_uint64 frameCount);
MA_API ma_uint32 ma_hpf1_get_latency(const ma_hpf1* pHPF);

typedef struct
{
    ma_biquad bq;   /* The second order high-pass filter is implemented as a biquad filter. */
} ma_hpf2;

MA_API ma_result ma_hpf2_get_heap_size(const ma_hpf2_config* pConfig, size_t* pHeapSizeInBytes);
MA_API ma_result ma_hpf2_init_preallocated(const ma_hpf2_config* pConfig, void* pHeap, ma_hpf2* pHPF);
MA_API ma_result ma_hpf2_init(const ma_hpf2_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_hpf2* pHPF);
MA_API void ma_hpf2_uninit(ma_hpf2* pHPF, const ma_allocation_callbacks* pAllocationCallbacks);
MA_API ma_result ma_hpf2_reinit(const ma_hpf2_config* pConfig, ma_hpf2* pHPF);
MA_API ma_result ma_hpf2_process_pcm_frames(ma_hpf2* pHPF, void* pFramesOut, const void* pFramesIn, ma_uint64 frameCount);
MA_API ma_uint32 ma_hpf2_get_latency(const ma_hpf2* pHPF);


typedef struct
{
    ma_format format;
    ma_uint32 channels;
    ma_uint32 sampleRate;
    double cutoffFrequency;
    ma_uint32 order;    /* If set to 0, will be treated as a passthrough (no filtering will be applied). */
} ma_hpf_config;

MA_API ma_hpf_config ma_hpf_config_init(ma_format format, ma_uint32 channels, ma_uint32 sampleRate, double cutoffFrequency, ma_uint32 order);

typedef struct
{
    ma_format format;
    ma_uint32 channels;
    ma_uint32 sampleRate;
    ma_uint32 hpf1Count;
    ma_uint32 hpf2Count;
    ma_hpf1* pHPF1;
    ma_hpf2* pHPF2;

    /* Memory management. */
    void* _pHeap;
    ma_bool32 _ownsHeap;
} ma_hpf;

MA_API ma_result ma_hpf_get_heap_size(const ma_hpf_config* pConfig, size_t* pHeapSizeInBytes);
MA_API ma_result ma_hpf_init_preallocated(const ma_hpf_config* pConfig, void* pHeap, ma_hpf* pLPF);
MA_API ma_result ma_hpf_init(const ma_hpf_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_hpf* pHPF);
MA_API void ma_hpf_uninit(ma_hpf* pHPF, const ma_allocation_callbacks* pAllocationCallbacks);
MA_API ma_result ma_hpf_reinit(const ma_hpf_config* pConfig, ma_hpf* pHPF);
MA_API ma_result ma_hpf_process_pcm_frames(ma_hpf* pHPF, void* pFramesOut, const void* pFramesIn, ma_uint64 frameCount);
MA_API ma_uint32 ma_hpf_get_latency(const ma_hpf* pHPF);


/**************************************************************************************************************************************************************

Band-Pass Filtering

**************************************************************************************************************************************************************/
typedef struct
{
    ma_format format;
    ma_uint32 channels;
    ma_uint32 sampleRate;
    double cutoffFrequency;
    double q;
} ma_bpf2_config;

MA_API ma_bpf2_config ma_bpf2_config_init(ma_format format, ma_uint32 channels, ma_uint32 sampleRate, double cutoffFrequency, double q);

typedef struct
{
    ma_biquad bq;   /* The second order band-pass filter is implemented as a biquad filter. */
} ma_bpf2;

MA_API ma_result ma_bpf2_get_heap_size(const ma_bpf2_config* pConfig, size_t* pHeapSizeInBytes);
MA_API ma_result ma_bpf2_init_preallocated(const ma_bpf2_config* pConfig, void* pHeap, ma_bpf2* pBPF);
MA_API ma_result ma_bpf2_init(const ma_bpf2_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_bpf2* pBPF);
MA_API void ma_bpf2_uninit(ma_bpf2* pBPF, const ma_allocation_callbacks* pAllocationCallbacks);
MA_API ma_result ma_bpf2_reinit(const ma_bpf2_config* pConfig, ma_bpf2* pBPF);
MA_API ma_result ma_bpf2_process_pcm_frames(ma_bpf2* pBPF, void* pFramesOut, const void* pFramesIn, ma_uint64 frameCount);
MA_API ma_uint32 ma_bpf2_get_latency(const ma_bpf2* pBPF);


typedef struct
{
    ma_format format;
    ma_uint32 channels;
    ma_uint32 sampleRate;
    double cutoffFrequency;
    ma_uint32 order;    /* If set to 0, will be treated as a passthrough (no filtering will be applied). */
} ma_bpf_config;

MA_API ma_bpf_config ma_bpf_config_init(ma_format format, ma_uint32 channels, ma_uint32 sampleRate, double cutoffFrequency, ma_uint32 order);

typedef struct
{
    ma_format format;
    ma_uint32 channels;
    ma_uint32 bpf2Count;
    ma_bpf2* pBPF2;

    /* Memory management. */
    void* _pHeap;
    ma_bool32 _ownsHeap;
} ma_bpf;

MA_API ma_result ma_bpf_get_heap_size(const ma_bpf_config* pConfig, size_t* pHeapSizeInBytes);
MA_API ma_result ma_bpf_init_preallocated(const ma_bpf_config* pConfig, void* pHeap, ma_bpf* pBPF);
MA_API ma_result ma_bpf_init(const ma_bpf_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_bpf* pBPF);
MA_API void ma_bpf_uninit(ma_bpf* pBPF, const ma_allocation_callbacks* pAllocationCallbacks);
MA_API ma_result ma_bpf_reinit(const ma_bpf_config* pConfig, ma_bpf* pBPF);
MA_API ma_result ma_bpf_process_pcm_frames(ma_bpf* pBPF, void* pFramesOut, const void* pFramesIn, ma_uint64 frameCount);
MA_API ma_uint32 ma_bpf_get_latency(const ma_bpf* pBPF);


/**************************************************************************************************************************************************************

Notching Filter

**************************************************************************************************************************************************************/
typedef struct
{
    ma_format format;
    ma_uint32 channels;
    ma_uint32 sampleRate;
    double q;
    double frequency;
} ma_notch2_config, ma_notch_config;

MA_API ma_notch2_config ma_notch2_config_init(ma_format format, ma_uint32 channels, ma_uint32 sampleRate, double q, double frequency);

typedef struct
{
    ma_biquad bq;
} ma_notch2;

MA_API ma_result ma_notch2_get_heap_size(const ma_notch2_config* pConfig, size_t* pHeapSizeInBytes);
MA_API ma_result ma_notch2_init_preallocated(const ma_notch2_config* pConfig, void* pHeap, ma_notch2* pFilter);
MA_API ma_result ma_notch2_init(const ma_notch2_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_notch2* pFilter);
MA_API void ma_notch2_uninit(ma_notch2* pFilter, const ma_allocation_callbacks* pAllocationCallbacks);
MA_API ma_result ma_notch2_reinit(const ma_notch2_config* pConfig, ma_notch2* pFilter);
MA_API ma_result ma_notch2_process_pcm_frames(ma_notch2* pFilter, void* pFramesOut, const void* pFramesIn, ma_uint64 frameCount);
MA_API ma_uint32 ma_notch2_get_latency(const ma_notch2* pFilter);


/**************************************************************************************************************************************************************

Peaking EQ Filter

**************************************************************************************************************************************************************/
typedef struct
{
    ma_format format;
    ma_uint32 channels;
    ma_uint32 sampleRate;
    double gainDB;
    double q;
    double frequency;
} ma_peak2_config, ma_peak_config;

MA_API ma_peak2_config ma_peak2_config_init(ma_format format, ma_uint32 channels, ma_uint32 sampleRate, double gainDB, double q, double frequency);

typedef struct
{
    ma_biquad bq;
} ma_peak2;

MA_API ma_result ma_peak2_get_heap_size(const ma_peak2_config* pConfig, size_t* pHeapSizeInBytes);
MA_API ma_result ma_peak2_init_preallocated(const ma_peak2_config* pConfig, void* pHeap, ma_peak2* pFilter);
MA_API ma_result ma_peak2_init(const ma_peak2_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_peak2* pFilter);
MA_API void ma_peak2_uninit(ma_peak2* pFilter, const ma_allocation_callbacks* pAllocationCallbacks);
MA_API ma_result ma_peak2_reinit(const ma_peak2_config* pConfig, ma_peak2* pFilter);
MA_API ma_result ma_peak2_process_pcm_frames(ma_peak2* pFilter, void* pFramesOut, const void* pFramesIn, ma_uint64 frameCount);
MA_API ma_uint32 ma_peak2_get_latency(const ma_peak2* pFilter);


/**************************************************************************************************************************************************************

Low Shelf Filter

**************************************************************************************************************************************************************/
typedef struct
{
    ma_format format;
    ma_uint32 channels;
    ma_uint32 sampleRate;
    double gainDB;
    double shelfSlope;
    double frequency;
} ma_loshelf2_config, ma_loshelf_config;

MA_API ma_loshelf2_config ma_loshelf2_config_init(ma_format format, ma_uint32 channels, ma_uint32 sampleRate, double gainDB, double shelfSlope, double frequency);

typedef struct
{
    ma_biquad bq;
} ma_loshelf2;

MA_API ma_result ma_loshelf2_get_heap_size(const ma_loshelf2_config* pConfig, size_t* pHeapSizeInBytes);
MA_API ma_result ma_loshelf2_init_preallocated(const ma_loshelf2_config* pConfig, void* pHeap, ma_loshelf2* pFilter);
MA_API ma_result ma_loshelf2_init(const ma_loshelf2_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_loshelf2* pFilter);
MA_API void ma_loshelf2_uninit(ma_loshelf2* pFilter, const ma_allocation_callbacks* pAllocationCallbacks);
MA_API ma_result ma_loshelf2_reinit(const ma_loshelf2_config* pConfig, ma_loshelf2* pFilter);
MA_API ma_result ma_loshelf2_process_pcm_frames(ma_loshelf2* pFilter, void* pFramesOut, const void* pFramesIn, ma_uint64 frameCount);
MA_API ma_uint32 ma_loshelf2_get_latency(const ma_loshelf2* pFilter);


/**************************************************************************************************************************************************************

High Shelf Filter

**************************************************************************************************************************************************************/
typedef struct
{
    ma_format format;
    ma_uint32 channels;
    ma_uint32 sampleRate;
    double gainDB;
    double shelfSlope;
    double frequency;
} ma_hishelf2_config, ma_hishelf_config;

MA_API ma_hishelf2_config ma_hishelf2_config_init(ma_format format, ma_uint32 channels, ma_uint32 sampleRate, double gainDB, double shelfSlope, double frequency);

typedef struct
{
    ma_biquad bq;
} ma_hishelf2;

MA_API ma_result ma_hishelf2_get_heap_size(const ma_hishelf2_config* pConfig, size_t* pHeapSizeInBytes);
MA_API ma_result ma_hishelf2_init_preallocated(const ma_hishelf2_config* pConfig, void* pHeap, ma_hishelf2* pFilter);
MA_API ma_result ma_hishelf2_init(const ma_hishelf2_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_hishelf2* pFilter);
MA_API void ma_hishelf2_uninit(ma_hishelf2* pFilter, const ma_allocation_callbacks* pAllocationCallbacks);
MA_API ma_result ma_hishelf2_reinit(const ma_hishelf2_config* pConfig, ma_hishelf2* pFilter);
MA_API ma_result ma_hishelf2_process_pcm_frames(ma_hishelf2* pFilter, void* pFramesOut, const void* pFramesIn, ma_uint64 frameCount);
MA_API ma_uint32 ma_hishelf2_get_latency(const ma_hishelf2* pFilter);



/*
Delay
*/
typedef struct
{
    ma_uint32 channels;
    ma_uint32 sampleRate;
    ma_uint32 delayInFrames;
    ma_bool32 delayStart;       /* Set to true to delay the start of the output; false otherwise. */
    float wet;                  /* 0..1. Default = 1. */
    float dry;                  /* 0..1. Default = 1. */
    float decay;                /* 0..1. Default = 0 (no feedback). Feedback decay. Use this for echo. */
} ma_delay_config;

MA_API ma_delay_config ma_delay_config_init(ma_uint32 channels, ma_uint32 sampleRate, ma_uint32 delayInFrames, float decay);


typedef struct
{
    ma_delay_config config;
    ma_uint32 cursor;               /* Feedback is written to this cursor. Always equal or in front of the read cursor. */
    ma_uint32 bufferSizeInFrames;
    float* pBuffer;
} ma_delay;

MA_API ma_result ma_delay_init(const ma_delay_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_delay* pDelay);
MA_API void ma_delay_uninit(ma_delay* pDelay, const ma_allocation_callbacks* pAllocationCallbacks);
MA_API ma_result ma_delay_process_pcm_frames(ma_delay* pDelay, void* pFramesOut, const void* pFramesIn, ma_uint32 frameCount);
MA_API void ma_delay_set_wet(ma_delay* pDelay, float value);
MA_API float ma_delay_get_wet(const ma_delay* pDelay);
MA_API void ma_delay_set_dry(ma_delay* pDelay, float value);
MA_API float ma_delay_get_dry(const ma_delay* pDelay);
MA_API void ma_delay_set_decay(ma_delay* pDelay, float value);
MA_API float ma_delay_get_decay(const ma_delay* pDelay);


/* Gainer for smooth volume changes. */
typedef struct
{
    ma_uint32 channels;
    ma_uint32 smoothTimeInFrames;
} ma_gainer_config;

MA_API ma_gainer_config ma_gainer_config_init(ma_uint32 channels, ma_uint32 smoothTimeInFrames);


typedef struct
{
    ma_gainer_config config;
    ma_uint32 t;
    float masterVolume;
    float* pOldGains;
    float* pNewGains;

    /* Memory management. */
    void* _pHeap;
    ma_bool32 _ownsHeap;
} ma_gainer;

MA_API ma_result ma_gainer_get_heap_size(const ma_gainer_config* pConfig, size_t* pHeapSizeInBytes);
MA_API ma_result ma_gainer_init_preallocated(const ma_gainer_config* pConfig, void* pHeap, ma_gainer* pGainer);
MA_API ma_result ma_gainer_init(const ma_gainer_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_gainer* pGainer);
MA_API void ma_gainer_uninit(ma_gainer* pGainer, const ma_allocation_callbacks* pAllocationCallbacks);
MA_API ma_result ma_gainer_process_pcm_frames(ma_gainer* pGainer, void* pFramesOut, const void* pFramesIn, ma_uint64 frameCount);
MA_API ma_result ma_gainer_set_gain(ma_gainer* pGainer, float newGain);
MA_API ma_result ma_gainer_set_gains(ma_gainer* pGainer, float* pNewGains);
MA_API ma_result ma_gainer_set_master_volume(ma_gainer* pGainer, float volume);
MA_API ma_result ma_gainer_get_master_volume(const ma_gainer* pGainer, float* pVolume);



/* Stereo panner. */
typedef enum
{
    ma_pan_mode_balance = 0,    /* Does not blend one side with the other. Technically just a balance. Compatible with other popular audio engines and therefore the default. */
    ma_pan_mode_pan             /* A true pan. The sound from one side will "move" to the other side and blend with it. */
} ma_pan_mode;

typedef struct
{
    ma_format format;
    ma_uint32 channels;
    ma_pan_mode mode;
    float pan;
} ma_panner_config;

MA_API ma_panner_config ma_panner_config_init(ma_format format, ma_uint32 channels);


typedef struct
{
    ma_format format;
    ma_uint32 channels;
    ma_pan_mode mode;
    float pan;  /* -1..1 where 0 is no pan, -1 is left side, +1 is right side. Defaults to 0. */
} ma_panner;

MA_API ma_result ma_panner_init(const ma_panner_config* pConfig, ma_panner* pPanner);
MA_API ma_result ma_panner_process_pcm_frames(ma_panner* pPanner, void* pFramesOut, const void* pFramesIn, ma_uint64 frameCount);
MA_API void ma_panner_set_mode(ma_panner* pPanner, ma_pan_mode mode);
MA_API ma_pan_mode ma_panner_get_mode(const ma_panner* pPanner);
MA_API void ma_panner_set_pan(ma_panner* pPanner, float pan);
MA_API float ma_panner_get_pan(const ma_panner* pPanner);



/* Fader. */
typedef struct
{
    ma_format format;
    ma_uint32 channels;
    ma_uint32 sampleRate;
} ma_fader_config;

MA_API ma_fader_config ma_fader_config_init(ma_format format, ma_uint32 channels, ma_uint32 sampleRate);

typedef struct
{
    ma_fader_config config;
    float volumeBeg;            /* If volumeBeg and volumeEnd is equal to 1, no fading happens (ma_fader_process_pcm_frames() will run as a passthrough). */
    float volumeEnd;
    ma_uint64 lengthInFrames;   /* The total length of the fade. */
    ma_int64  cursorInFrames;   /* The current time in frames. Incremented by ma_fader_process_pcm_frames(). Signed because it'll be offset by startOffsetInFrames in set_fade_ex(). */
} ma_fader;

MA_API ma_result ma_fader_init(const ma_fader_config* pConfig, ma_fader* pFader);
MA_API ma_result ma_fader_process_pcm_frames(ma_fader* pFader, void* pFramesOut, const void* pFramesIn, ma_uint64 frameCount);
MA_API void ma_fader_get_data_format(const ma_fader* pFader, ma_format* pFormat, ma_uint32* pChannels, ma_uint32* pSampleRate);
MA_API void ma_fader_set_fade(ma_fader* pFader, float volumeBeg, float volumeEnd, ma_uint64 lengthInFrames);
MA_API void ma_fader_set_fade_ex(ma_fader* pFader, float volumeBeg, float volumeEnd, ma_uint64 lengthInFrames, ma_int64 startOffsetInFrames);
MA_API float ma_fader_get_current_volume(const ma_fader* pFader);



/* Spatializer. */
typedef struct
{
    float x;
    float y;
    float z;
} ma_vec3f;

typedef struct
{
    ma_vec3f v;
    ma_spinlock lock;
} ma_atomic_vec3f;

typedef enum
{
    ma_attenuation_model_none,          /* No distance attenuation and no spatialization. */
    ma_attenuation_model_inverse,       /* Equivalent to OpenAL's AL_INVERSE_DISTANCE_CLAMPED. */
    ma_attenuation_model_linear,        /* Linear attenuation. Equivalent to OpenAL's AL_LINEAR_DISTANCE_CLAMPED. */
    ma_attenuation_model_exponential    /* Exponential attenuation. Equivalent to OpenAL's AL_EXPONENT_DISTANCE_CLAMPED. */
} ma_attenuation_model;

typedef enum
{
    ma_positioning_absolute,
    ma_positioning_relative
} ma_positioning;

typedef enum
{
    ma_handedness_right,
    ma_handedness_left
} ma_handedness;


typedef struct
{
    ma_uint32 channelsOut;
    ma_channel* pChannelMapOut;
    ma_handedness handedness;   /* Defaults to right. Forward is -1 on the Z axis. In a left handed system, forward is +1 on the Z axis. */
    float coneInnerAngleInRadians;
    float coneOuterAngleInRadians;
    float coneOuterGain;
    float speedOfSound;
    ma_vec3f worldUp;
} ma_spatializer_listener_config;

MA_API ma_spatializer_listener_config ma_spatializer_listener_config_init(ma_uint32 channelsOut);


typedef struct
{
    ma_spatializer_listener_config config;
    ma_atomic_vec3f position;  /* The absolute position of the listener. */
    ma_atomic_vec3f direction; /* The direction the listener is facing. The world up vector is config.worldUp. */
    ma_atomic_vec3f velocity;
    ma_bool32 isEnabled;

    /* Memory management. */
    ma_bool32 _ownsHeap;
    void* _pHeap;
} ma_spatializer_listener;

MA_API ma_result ma_spatializer_listener_get_heap_size(const ma_spatializer_listener_config* pConfig, size_t* pHeapSizeInBytes);
MA_API ma_result ma_spatializer_listener_init_preallocated(const ma_spatializer_listener_config* pConfig, void* pHeap, ma_spatializer_listener* pListener);
MA_API ma_result ma_spatializer_listener_init(const ma_spatializer_listener_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_spatializer_listener* pListener);
MA_API void ma_spatializer_listener_uninit(ma_spatializer_listener* pListener, const ma_allocation_callbacks* pAllocationCallbacks);
MA_API ma_channel* ma_spatializer_listener_get_channel_map(ma_spatializer_listener* pListener);
MA_API void ma_spatializer_listener_set_cone(ma_spatializer_listener* pListener, float innerAngleInRadians, float outerAngleInRadians, float outerGain);
MA_API void ma_spatializer_listener_get_cone(const ma_spatializer_listener* pListener, float* pInnerAngleInRadians, float* pOuterAngleInRadians, float* pOuterGain);
MA_API void ma_spatializer_listener_set_position(ma_spatializer_listener* pListener, float x, float y, float z);
MA_API ma_vec3f ma_spatializer_listener_get_position(const ma_spatializer_listener* pListener);
MA_API void ma_spatializer_listener_set_direction(ma_spatializer_listener* pListener, float x, float y, float z);
MA_API ma_vec3f ma_spatializer_listener_get_direction(const ma_spatializer_listener* pListener);
MA_API void ma_spatializer_listener_set_velocity(ma_spatializer_listener* pListener, float x, float y, float z);
MA_API ma_vec3f ma_spatializer_listener_get_velocity(const ma_spatializer_listener* pListener);
MA_API void ma_spatializer_listener_set_speed_of_sound(ma_spatializer_listener* pListener, float speedOfSound);
MA_API float ma_spatializer_listener_get_speed_of_sound(const ma_spatializer_listener* pListener);
MA_API void ma_spatializer_listener_set_world_up(ma_spatializer_listener* pListener, float x, float y, float z);
MA_API ma_vec3f ma_spatializer_listener_get_world_up(const ma_spatializer_listener* pListener);
MA_API void ma_spatializer_listener_set_enabled(ma_spatializer_listener* pListener, ma_bool32 isEnabled);
MA_API ma_bool32 ma_spatializer_listener_is_enabled(const ma_spatializer_listener* pListener);


typedef struct
{
    ma_uint32 channelsIn;
    ma_uint32 channelsOut;
    ma_channel* pChannelMapIn;
    ma_attenuation_model attenuationModel;
    ma_positioning positioning;
    ma_handedness handedness;           /* Defaults to right. Forward is -1 on the Z axis. In a left handed system, forward is +1 on the Z axis. */
    float minGain;
    float maxGain;
    float minDistance;
    float maxDistance;
    float rolloff;
    float coneInnerAngleInRadians;
    float coneOuterAngleInRadians;
    float coneOuterGain;
    float dopplerFactor;                /* Set to 0 to disable doppler effect. */
    float directionalAttenuationFactor; /* Set to 0 to disable directional attenuation. */
    float minSpatializationChannelGain; /* The minimal scaling factor to apply to channel gains when accounting for the direction of the sound relative to the listener. Must be in the range of 0..1. Smaller values means more aggressive directional panning, larger values means more subtle directional panning. */
    ma_uint32 gainSmoothTimeInFrames;   /* When the gain of a channel changes during spatialization, the transition will be linearly interpolated over this number of frames. */
} ma_spatializer_config;

MA_API ma_spatializer_config ma_spatializer_config_init(ma_uint32 channelsIn, ma_uint32 channelsOut);


typedef struct
{
    ma_uint32 channelsIn;
    ma_uint32 channelsOut;
    ma_channel* pChannelMapIn;
    ma_attenuation_model attenuationModel;
    ma_positioning positioning;
    ma_handedness handedness;           /* Defaults to right. Forward is -1 on the Z axis. In a left handed system, forward is +1 on the Z axis. */
    float minGain;
    float maxGain;
    float minDistance;
    float maxDistance;
    float rolloff;
    float coneInnerAngleInRadians;
    float coneOuterAngleInRadians;
    float coneOuterGain;
    float dopplerFactor;                /* Set to 0 to disable doppler effect. */
    float directionalAttenuationFactor; /* Set to 0 to disable directional attenuation. */
    ma_uint32 gainSmoothTimeInFrames;   /* When the gain of a channel changes during spatialization, the transition will be linearly interpolated over this number of frames. */
    ma_atomic_vec3f position;
    ma_atomic_vec3f direction;
    ma_atomic_vec3f velocity;  /* For doppler effect. */
    float dopplerPitch; /* Will be updated by ma_spatializer_process_pcm_frames() and can be used by higher level functions to apply a pitch shift for doppler effect. */
    float minSpatializationChannelGain;
    ma_gainer gainer;   /* For smooth gain transitions. */
    float* pNewChannelGainsOut; /* An offset of _pHeap. Used by ma_spatializer_process_pcm_frames() to store new channel gains. The number of elements in this array is equal to config.channelsOut. */

    /* Memory management. */
    void* _pHeap;
    ma_bool32 _ownsHeap;
} ma_spatializer;

MA_API ma_result ma_spatializer_get_heap_size(const ma_spatializer_config* pConfig, size_t* pHeapSizeInBytes);
MA_API ma_result ma_spatializer_init_preallocated(const ma_spatializer_config* pConfig, void* pHeap, ma_spatializer* pSpatializer);
MA_API ma_result ma_spatializer_init(const ma_spatializer_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_spatializer* pSpatializer);
MA_API void ma_spatializer_uninit(ma_spatializer* pSpatializer, const ma_allocation_callbacks* pAllocationCallbacks);
MA_API ma_result ma_spatializer_process_pcm_frames(ma_spatializer* pSpatializer, ma_spatializer_listener* pListener, void* pFramesOut, const void* pFramesIn, ma_uint64 frameCount);
MA_API ma_result ma_spatializer_set_master_volume(ma_spatializer* pSpatializer, float volume);
MA_API ma_result ma_spatializer_get_master_volume(const ma_spatializer* pSpatializer, float* pVolume);
MA_API ma_uint32 ma_spatializer_get_input_channels(const ma_spatializer* pSpatializer);
MA_API ma_uint32 ma_spatializer_get_output_channels(const ma_spatializer* pSpatializer);
MA_API void ma_spatializer_set_attenuation_model(ma_spatializer* pSpatializer, ma_attenuation_model attenuationModel);
MA_API ma_attenuation_model ma_spatializer_get_attenuation_model(const ma_spatializer* pSpatializer);
MA_API void ma_spatializer_set_positioning(ma_spatializer* pSpatializer, ma_positioning positioning);
MA_API ma_positioning ma_spatializer_get_positioning(const ma_spatializer* pSpatializer);
MA_API void ma_spatializer_set_rolloff(ma_spatializer* pSpatializer, float rolloff);
MA_API float ma_spatializer_get_rolloff(const ma_spatializer* pSpatializer);
MA_API void ma_spatializer_set_min_gain(ma_spatializer* pSpatializer, float minGain);
MA_API float ma_spatializer_get_min_gain(const ma_spatializer* pSpatializer);
MA_API void ma_spatializer_set_max_gain(ma_spatializer* pSpatializer, float maxGain);
MA_API float ma_spatializer_get_max_gain(const ma_spatializer* pSpatializer);
MA_API void ma_spatializer_set_min_distance(ma_spatializer* pSpatializer, float minDistance);
MA_API float ma_spatializer_get_min_distance(const ma_spatializer* pSpatializer);
MA_API void ma_spatializer_set_max_distance(ma_spatializer* pSpatializer, float maxDistance);
MA_API float ma_spatializer_get_max_distance(const ma_spatializer* pSpatializer);
MA_API void ma_spatializer_set_cone(ma_spatializer* pSpatializer, float innerAngleInRadians, float outerAngleInRadians, float outerGain);
MA_API void ma_spatializer_get_cone(const ma_spatializer* pSpatializer, float* pInnerAngleInRadians, float* pOuterAngleInRadians, float* pOuterGain);
MA_API void ma_spatializer_set_doppler_factor(ma_spatializer* pSpatializer, float dopplerFactor);
MA_API float ma_spatializer_get_doppler_factor(const ma_spatializer* pSpatializer);
MA_API void ma_spatializer_set_directional_attenuation_factor(ma_spatializer* pSpatializer, float directionalAttenuationFactor);
MA_API float ma_spatializer_get_directional_attenuation_factor(const ma_spatializer* pSpatializer);
MA_API void ma_spatializer_set_position(ma_spatializer* pSpatializer, float x, float y, float z);
MA_API ma_vec3f ma_spatializer_get_position(const ma_spatializer* pSpatializer);
MA_API void ma_spatializer_set_direction(ma_spatializer* pSpatializer, float x, float y, float z);
MA_API ma_vec3f ma_spatializer_get_direction(const ma_spatializer* pSpatializer);
MA_API void ma_spatializer_set_velocity(ma_spatializer* pSpatializer, float x, float y, float z);
MA_API ma_vec3f ma_spatializer_get_velocity(const ma_spatializer* pSpatializer);
MA_API void ma_spatializer_get_relative_position_and_direction(const ma_spatializer* pSpatializer, const ma_spatializer_listener* pListener, ma_vec3f* pRelativePos, ma_vec3f* pRelativeDir);



/************************************************************************************************************************************************************
*************************************************************************************************************************************************************

DATA CONVERSION
===============

This section contains the APIs for data conversion. You will find everything here for channel mapping, sample format conversion, resampling, etc.

*************************************************************************************************************************************************************
************************************************************************************************************************************************************/

/**************************************************************************************************************************************************************

Resampling

**************************************************************************************************************************************************************/
typedef struct
{
    ma_format format;
    ma_uint32 channels;
    ma_uint32 sampleRateIn;
    ma_uint32 sampleRateOut;
    ma_uint32 lpfOrder;         /* The low-pass filter order. Setting this to 0 will disable low-pass filtering. */
    double    lpfNyquistFactor; /* 0..1. Defaults to 1. 1 = Half the sampling frequency (Nyquist Frequency), 0.5 = Quarter the sampling frequency (half Nyquest Frequency), etc. */
} ma_linear_resampler_config;

MA_API ma_linear_resampler_config ma_linear_resampler_config_init(ma_format format, ma_uint32 channels, ma_uint32 sampleRateIn, ma_uint32 sampleRateOut);

typedef struct
{
    ma_linear_resampler_config config;
    ma_uint32 inAdvanceInt;
    ma_uint32 inAdvanceFrac;
    ma_uint32 inTimeInt;
    ma_uint32 inTimeFrac;
    union
    {
        float* f32;
        ma_int16* s16;
    } x0; /* The previous input frame. */
    union
    {
        float* f32;
        ma_int16* s16;
    } x1; /* The next input frame. */
    ma_lpf lpf;

    /* Memory management. */
    void* _pHeap;
    ma_bool32 _ownsHeap;
} ma_linear_resampler;

MA_API ma_result ma_linear_resampler_get_heap_size(const ma_linear_resampler_config* pConfig, size_t* pHeapSizeInBytes);
MA_API ma_result ma_linear_resampler_init_preallocated(const ma_linear_resampler_config* pConfig, void* pHeap, ma_linear_resampler* pResampler);
MA_API ma_result ma_linear_resampler_init(const ma_linear_resampler_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_linear_resampler* pResampler);
MA_API void ma_linear_resampler_uninit(ma_linear_resampler* pResampler, const ma_allocation_callbacks* pAllocationCallbacks);
MA_API ma_result ma_linear_resampler_process_pcm_frames(ma_linear_resampler* pResampler, const void* pFramesIn, ma_uint64* pFrameCountIn, void* pFramesOut, ma_uint64* pFrameCountOut);
MA_API ma_result ma_linear_resampler_set_rate(ma_linear_resampler* pResampler, ma_uint32 sampleRateIn, ma_uint32 sampleRateOut);
MA_API ma_result ma_linear_resampler_set_rate_ratio(ma_linear_resampler* pResampler, float ratioInOut);
MA_API ma_uint64 ma_linear_resampler_get_input_latency(const ma_linear_resampler* pResampler);
MA_API ma_uint64 ma_linear_resampler_get_output_latency(const ma_linear_resampler* pResampler);
MA_API ma_result ma_linear_resampler_get_required_input_frame_count(const ma_linear_resampler* pResampler, ma_uint64 outputFrameCount, ma_uint64* pInputFrameCount);
MA_API ma_result ma_linear_resampler_get_expected_output_frame_count(const ma_linear_resampler* pResampler, ma_uint64 inputFrameCount, ma_uint64* pOutputFrameCount);
MA_API ma_result ma_linear_resampler_reset(ma_linear_resampler* pResampler);


typedef struct ma_resampler_config ma_resampler_config;

typedef void ma_resampling_backend;
typedef struct
{
    ma_result (* onGetHeapSize                )(void* pUserData, const ma_resampler_config* pConfig, size_t* pHeapSizeInBytes);
    ma_result (* onInit                       )(void* pUserData, const ma_resampler_config* pConfig, void* pHeap, ma_resampling_backend** ppBackend);
    void      (* onUninit                     )(void* pUserData, ma_resampling_backend* pBackend, const ma_allocation_callbacks* pAllocationCallbacks);
    ma_result (* onProcess                    )(void* pUserData, ma_resampling_backend* pBackend, const void* pFramesIn, ma_uint64* pFrameCountIn, void* pFramesOut, ma_uint64* pFrameCountOut);
    ma_result (* onSetRate                    )(void* pUserData, ma_resampling_backend* pBackend, ma_uint32 sampleRateIn, ma_uint32 sampleRateOut);                 /* Optional. Rate changes will be disabled. */
    ma_uint64 (* onGetInputLatency            )(void* pUserData, const ma_resampling_backend* pBackend);                                                            /* Optional. Latency will be reported as 0. */
    ma_uint64 (* onGetOutputLatency           )(void* pUserData, const ma_resampling_backend* pBackend);                                                            /* Optional. Latency will be reported as 0. */
    ma_result (* onGetRequiredInputFrameCount )(void* pUserData, const ma_resampling_backend* pBackend, ma_uint64 outputFrameCount, ma_uint64* pInputFrameCount);   /* Optional. Latency mitigation will be disabled. */
    ma_result (* onGetExpectedOutputFrameCount)(void* pUserData, const ma_resampling_backend* pBackend, ma_uint64 inputFrameCount, ma_uint64* pOutputFrameCount);   /* Optional. Latency mitigation will be disabled. */
    ma_result (* onReset                      )(void* pUserData, ma_resampling_backend* pBackend);
} ma_resampling_backend_vtable;

typedef enum
{
    ma_resample_algorithm_linear = 0,    /* Fastest, lowest quality. Optional low-pass filtering. Default. */
    ma_resample_algorithm_custom,
} ma_resample_algorithm;

struct ma_resampler_config
{
    ma_format format;   /* Must be either ma_format_f32 or ma_format_s16. */
    ma_uint32 channels;
    ma_uint32 sampleRateIn;
    ma_uint32 sampleRateOut;
    ma_resample_algorithm algorithm;    /* When set to ma_resample_algorithm_custom, pBackendVTable will be used. */
    ma_resampling_backend_vtable* pBackendVTable;
    void* pBackendUserData;
    struct
    {
        ma_uint32 lpfOrder;
    } linear;
};

MA_API ma_resampler_config ma_resampler_config_init(ma_format format, ma_uint32 channels, ma_uint32 sampleRateIn, ma_uint32 sampleRateOut, ma_resample_algorithm algorithm);

typedef struct
{
    ma_resampling_backend* pBackend;
    ma_resampling_backend_vtable* pBackendVTable;
    void* pBackendUserData;
    ma_format format;
    ma_uint32 channels;
    ma_uint32 sampleRateIn;
    ma_uint32 sampleRateOut;
    union
    {
        ma_linear_resampler linear;
    } state;    /* State for stock resamplers so we can avoid a malloc. For stock resamplers, pBackend will point here. */

    /* Memory management. */
    void* _pHeap;
    ma_bool32 _ownsHeap;
} ma_resampler;

MA_API ma_result ma_resampler_get_heap_size(const ma_resampler_config* pConfig, size_t* pHeapSizeInBytes);
MA_API ma_result ma_resampler_init_preallocated(const ma_resampler_config* pConfig, void* pHeap, ma_resampler* pResampler);

/*
Initializes a new resampler object from a config.
*/
MA_API ma_result ma_resampler_init(const ma_resampler_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_resampler* pResampler);

/*
Uninitializes a resampler.
*/
MA_API void ma_resampler_uninit(ma_resampler* pResampler, const ma_allocation_callbacks* pAllocationCallbacks);

/*
Converts the given input data.

Both the input and output frames must be in the format specified in the config when the resampler was initialized.

On input, [pFrameCountOut] contains the number of output frames to process. On output it contains the number of output frames that
were actually processed, which may be less than the requested amount which will happen if there's not enough input data. You can use
ma_resampler_get_expected_output_frame_count() to know how many output frames will be processed for a given number of input frames.

On input, [pFrameCountIn] contains the number of input frames contained in [pFramesIn]. On output it contains the number of whole
input frames that were actually processed. You can use ma_resampler_get_required_input_frame_count() to know how many input frames
you should provide for a given number of output frames. [pFramesIn] can be NULL, in which case zeroes will be used instead.

If [pFramesOut] is NULL, a seek is performed. In this case, if [pFrameCountOut] is not NULL it will seek by the specified number of
output frames. Otherwise, if [pFramesCountOut] is NULL and [pFrameCountIn] is not NULL, it will seek by the specified number of input
frames. When seeking, [pFramesIn] is allowed to NULL, in which case the internal timing state will be updated, but no input will be
processed. In this case, any internal filter state will be updated as if zeroes were passed in.

It is an error for [pFramesOut] to be non-NULL and [pFrameCountOut] to be NULL.

It is an error for both [pFrameCountOut] and [pFrameCountIn] to be NULL.
*/
MA_API ma_result ma_resampler_process_pcm_frames(ma_resampler* pResampler, const void* pFramesIn, ma_uint64* pFrameCountIn, void* pFramesOut, ma_uint64* pFrameCountOut);


/*
Sets the input and output sample rate.
*/
MA_API ma_result ma_resampler_set_rate(ma_resampler* pResampler, ma_uint32 sampleRateIn, ma_uint32 sampleRateOut);

/*
Sets the input and output sample rate as a ratio.

The ration is in/out.
*/
MA_API ma_result ma_resampler_set_rate_ratio(ma_resampler* pResampler, float ratio);

/*
Retrieves the latency introduced by the resampler in input frames.
*/
MA_API ma_uint64 ma_resampler_get_input_latency(const ma_resampler* pResampler);

/*
Retrieves the latency introduced by the resampler in output frames.
*/
MA_API ma_uint64 ma_resampler_get_output_latency(const ma_resampler* pResampler);

/*
Calculates the number of whole input frames that would need to be read from the client in order to output the specified
number of output frames.

The returned value does not include cached input frames. It only returns the number of extra frames that would need to be
read from the input buffer in order to output the specified number of output frames.
*/
MA_API ma_result ma_resampler_get_required_input_frame_count(const ma_resampler* pResampler, ma_uint64 outputFrameCount, ma_uint64* pInputFrameCount);

/*
Calculates the number of whole output frames that would be output after fully reading and consuming the specified number of
input frames.
*/
MA_API ma_result ma_resampler_get_expected_output_frame_count(const ma_resampler* pResampler, ma_uint64 inputFrameCount, ma_uint64* pOutputFrameCount);

/*
Resets the resampler's timer and clears its internal cache.
*/
MA_API ma_result ma_resampler_reset(ma_resampler* pResampler);


/**************************************************************************************************************************************************************

Channel Conversion

**************************************************************************************************************************************************************/
typedef enum
{
    ma_channel_conversion_path_unknown,
    ma_channel_conversion_path_passthrough,
    ma_channel_conversion_path_mono_out,    /* Converting to mono. */
    ma_channel_conversion_path_mono_in,     /* Converting from mono. */
    ma_channel_conversion_path_shuffle,     /* Simple shuffle. Will use this when all channels are present in both input and output channel maps, but just in a different order. */
    ma_channel_conversion_path_weights      /* Blended based on weights. */
} ma_channel_conversion_path;

typedef enum
{
    ma_mono_expansion_mode_duplicate = 0,   /* The default. */
    ma_mono_expansion_mode_average,         /* Average the mono channel across all channels. */
    ma_mono_expansion_mode_stereo_only,     /* Duplicate to the left and right channels only and ignore the others. */
    ma_mono_expansion_mode_default = ma_mono_expansion_mode_duplicate
} ma_mono_expansion_mode;

typedef struct
{
    ma_format format;
    ma_uint32 channelsIn;
    ma_uint32 channelsOut;
    const ma_channel* pChannelMapIn;
    const ma_channel* pChannelMapOut;
    ma_channel_mix_mode mixingMode;
    ma_bool32 calculateLFEFromSpatialChannels;  /* When an output LFE channel is present, but no input LFE, set to true to set the output LFE to the average of all spatial channels (LR, FR, etc.). Ignored when an input LFE is present. */
    float** ppWeights;  /* [in][out]. Only used when mixingMode is set to ma_channel_mix_mode_custom_weights. */
} ma_channel_converter_config;

MA_API ma_channel_converter_config ma_channel_converter_config_init(ma_format format, ma_uint32 channelsIn, const ma_channel* pChannelMapIn, ma_uint32 channelsOut, const ma_channel* pChannelMapOut, ma_channel_mix_mode mixingMode);

typedef struct
{
    ma_format format;
    ma_uint32 channelsIn;
    ma_uint32 channelsOut;
    ma_channel_mix_mode mixingMode;
    ma_channel_conversion_path conversionPath;
    ma_channel* pChannelMapIn;
    ma_channel* pChannelMapOut;
    ma_uint8* pShuffleTable;    /* Indexed by output channel index. */
    union
    {
        float**    f32;
        ma_int32** s16;
    } weights;  /* [in][out] */

    /* Memory management. */
    void* _pHeap;
    ma_bool32 _ownsHeap;
} ma_channel_converter;

MA_API ma_result ma_channel_converter_get_heap_size(const ma_channel_converter_config* pConfig, size_t* pHeapSizeInBytes);
MA_API ma_result ma_channel_converter_init_preallocated(const ma_channel_converter_config* pConfig, void* pHeap, ma_channel_converter* pConverter);
MA_API ma_result ma_channel_converter_init(const ma_channel_converter_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_channel_converter* pConverter);
MA_API void ma_channel_converter_uninit(ma_channel_converter* pConverter, const ma_allocation_callbacks* pAllocationCallbacks);
MA_API ma_result ma_channel_converter_process_pcm_frames(ma_channel_converter* pConverter, void* pFramesOut, const void* pFramesIn, ma_uint64 frameCount);
MA_API ma_result ma_channel_converter_get_input_channel_map(const ma_channel_converter* pConverter, ma_channel* pChannelMap, size_t channelMapCap);
MA_API ma_result ma_channel_converter_get_output_channel_map(const ma_channel_converter* pConverter, ma_channel* pChannelMap, size_t channelMapCap);


/**************************************************************************************************************************************************************

Data Conversion

**************************************************************************************************************************************************************/
typedef struct
{
    ma_format formatIn;
    ma_format formatOut;
    ma_uint32 channelsIn;
    ma_uint32 channelsOut;
    ma_uint32 sampleRateIn;
    ma_uint32 sampleRateOut;
    ma_channel* pChannelMapIn;
    ma_channel* pChannelMapOut;
    ma_dither_mode ditherMode;
    ma_channel_mix_mode channelMixMode;
    ma_bool32 calculateLFEFromSpatialChannels;  /* When an output LFE channel is present, but no input LFE, set to true to set the output LFE to the average of all spatial channels (LR, FR, etc.). Ignored when an input LFE is present. */
    float** ppChannelWeights;  /* [in][out]. Only used when mixingMode is set to ma_channel_mix_mode_custom_weights. */
    ma_bool32 allowDynamicSampleRate;
    ma_resampler_config resampling;
} ma_data_converter_config;

MA_API ma_data_converter_config ma_data_converter_config_init_default(void);
MA_API ma_data_converter_config ma_data_converter_config_init(ma_format formatIn, ma_format formatOut, ma_uint32 channelsIn, ma_uint32 channelsOut, ma_uint32 sampleRateIn, ma_uint32 sampleRateOut);


typedef enum
{
    ma_data_converter_execution_path_passthrough,       /* No conversion. */
    ma_data_converter_execution_path_format_only,       /* Only format conversion. */
    ma_data_converter_execution_path_channels_only,     /* Only channel conversion. */
    ma_data_converter_execution_path_resample_only,     /* Only resampling. */
    ma_data_converter_execution_path_resample_first,    /* All conversions, but resample as the first step. */
    ma_data_converter_execution_path_channels_first     /* All conversions, but channels as the first step. */
} ma_data_converter_execution_path;

typedef struct
{
    ma_format formatIn;
    ma_format formatOut;
    ma_uint32 channelsIn;
    ma_uint32 channelsOut;
    ma_uint32 sampleRateIn;
    ma_uint32 sampleRateOut;
    ma_dither_mode ditherMode;
    ma_data_converter_execution_path executionPath; /* The execution path the data converter will follow when processing. */
    ma_channel_converter channelConverter;
    ma_resampler resampler;
    ma_bool8 hasPreFormatConversion;
    ma_bool8 hasPostFormatConversion;
    ma_bool8 hasChannelConverter;
    ma_bool8 hasResampler;
    ma_bool8 isPassthrough;

    /* Memory management. */
    ma_bool8 _ownsHeap;
    void* _pHeap;
} ma_data_converter;

MA_API ma_result ma_data_converter_get_heap_size(const ma_data_converter_config* pConfig, size_t* pHeapSizeInBytes);
MA_API ma_result ma_data_converter_init_preallocated(const ma_data_converter_config* pConfig, void* pHeap, ma_data_converter* pConverter);
MA_API ma_result ma_data_converter_init(const ma_data_converter_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_data_converter* pConverter);
MA_API void ma_data_converter_uninit(ma_data_converter* pConverter, const ma_allocation_callbacks* pAllocationCallbacks);
MA_API ma_result ma_data_converter_process_pcm_frames(ma_data_converter* pConverter, const void* pFramesIn, ma_uint64* pFrameCountIn, void* pFramesOut, ma_uint64* pFrameCountOut);
MA_API ma_result ma_data_converter_set_rate(ma_data_converter* pConverter, ma_uint32 sampleRateIn, ma_uint32 sampleRateOut);
MA_API ma_result ma_data_converter_set_rate_ratio(ma_data_converter* pConverter, float ratioInOut);
MA_API ma_uint64 ma_data_converter_get_input_latency(const ma_data_converter* pConverter);
MA_API ma_uint64 ma_data_converter_get_output_latency(const ma_data_converter* pConverter);
MA_API ma_result ma_data_converter_get_required_input_frame_count(const ma_data_converter* pConverter, ma_uint64 outputFrameCount, ma_uint64* pInputFrameCount);
MA_API ma_result ma_data_converter_get_expected_output_frame_count(const ma_data_converter* pConverter, ma_uint64 inputFrameCount, ma_uint64* pOutputFrameCount);
MA_API ma_result ma_data_converter_get_input_channel_map(const ma_data_converter* pConverter, ma_channel* pChannelMap, size_t channelMapCap);
MA_API ma_result ma_data_converter_get_output_channel_map(const ma_data_converter* pConverter, ma_channel* pChannelMap, size_t channelMapCap);
MA_API ma_result ma_data_converter_reset(ma_data_converter* pConverter);


/************************************************************************************************************************************************************

Format Conversion

************************************************************************************************************************************************************/
MA_API void ma_pcm_u8_to_s16(void* pOut, const void* pIn, ma_uint64 count, ma_dither_mode ditherMode);
MA_API void ma_pcm_u8_to_s24(void* pOut, const void* pIn, ma_uint64 count, ma_dither_mode ditherMode);
MA_API void ma_pcm_u8_to_s32(void* pOut, const void* pIn, ma_uint64 count, ma_dither_mode ditherMode);
MA_API void ma_pcm_u8_to_f32(void* pOut, const void* pIn, ma_uint64 count, ma_dither_mode ditherMode);
MA_API void ma_pcm_s16_to_u8(void* pOut, const void* pIn, ma_uint64 count, ma_dither_mode ditherMode);
MA_API void ma_pcm_s16_to_s24(void* pOut, const void* pIn, ma_uint64 count, ma_dither_mode ditherMode);
MA_API void ma_pcm_s16_to_s32(void* pOut, const void* pIn, ma_uint64 count, ma_dither_mode ditherMode);
MA_API void ma_pcm_s16_to_f32(void* pOut, const void* pIn, ma_uint64 count, ma_dither_mode ditherMode);
MA_API void ma_pcm_s24_to_u8(void* pOut, const void* pIn, ma_uint64 count, ma_dither_mode ditherMode);
MA_API void ma_pcm_s24_to_s16(void* pOut, const void* pIn, ma_uint64 count, ma_dither_mode ditherMode);
MA_API void ma_pcm_s24_to_s32(void* pOut, const void* pIn, ma_uint64 count, ma_dither_mode ditherMode);
MA_API void ma_pcm_s24_to_f32(void* pOut, const void* pIn, ma_uint64 count, ma_dither_mode ditherMode);
MA_API void ma_pcm_s32_to_u8(void* pOut, const void* pIn, ma_uint64 count, ma_dither_mode ditherMode);
MA_API void ma_pcm_s32_to_s16(void* pOut, const void* pIn, ma_uint64 count, ma_dither_mode ditherMode);
MA_API void ma_pcm_s32_to_s24(void* pOut, const void* pIn, ma_uint64 count, ma_dither_mode ditherMode);
MA_API void ma_pcm_s32_to_f32(void* pOut, const void* pIn, ma_uint64 count, ma_dither_mode ditherMode);
MA_API void ma_pcm_f32_to_u8(void* pOut, const void* pIn, ma_uint64 count, ma_dither_mode ditherMode);
MA_API void ma_pcm_f32_to_s16(void* pOut, const void* pIn, ma_uint64 count, ma_dither_mode ditherMode);
MA_API void ma_pcm_f32_to_s24(void* pOut, const void* pIn, ma_uint64 count, ma_dither_mode ditherMode);
MA_API void ma_pcm_f32_to_s32(void* pOut, const void* pIn, ma_uint64 count, ma_dither_mode ditherMode);
MA_API void ma_pcm_convert(void* pOut, ma_format formatOut, const void* pIn, ma_format formatIn, ma_uint64 sampleCount, ma_dither_mode ditherMode);
MA_API void ma_convert_pcm_frames_format(void* pOut, ma_format formatOut, const void* pIn, ma_format formatIn, ma_uint64 frameCount, ma_uint32 channels, ma_dither_mode ditherMode);

/*
Deinterleaves an interleaved buffer.
*/
MA_API void ma_deinterleave_pcm_frames(ma_format format, ma_uint32 channels, ma_uint64 frameCount, const void* pInterleavedPCMFrames, void** ppDeinterleavedPCMFrames);

/*
Interleaves a group of deinterleaved buffers.
*/
MA_API void ma_interleave_pcm_frames(ma_format format, ma_uint32 channels, ma_uint64 frameCount, const void** ppDeinterleavedPCMFrames, void* pInterleavedPCMFrames);


/************************************************************************************************************************************************************

Channel Maps

************************************************************************************************************************************************************/
/*
This is used in the shuffle table to indicate that the channel index is undefined and should be ignored.
*/
#define MA_CHANNEL_INDEX_NULL   255

/*
Retrieves the channel position of the specified channel in the given channel map.

The pChannelMap parameter can be null, in which case miniaudio's default channel map will be assumed.
*/
MA_API ma_channel ma_channel_map_get_channel(const ma_channel* pChannelMap, ma_uint32 channelCount, ma_uint32 channelIndex);

/*
Initializes a blank channel map.

When a blank channel map is specified anywhere it indicates that the native channel map should be used.
*/
MA_API void ma_channel_map_init_blank(ma_channel* pChannelMap, ma_uint32 channels);

/*
Helper for retrieving a standard channel map.

The output channel map buffer must have a capacity of at least `channelMapCap`.
*/
MA_API void ma_channel_map_init_standard(ma_standard_channel_map standardChannelMap, ma_channel* pChannelMap, size_t channelMapCap, ma_uint32 channels);

/*
Copies a channel map.

Both input and output channel map buffers must have a capacity of at least `channels`.
*/
MA_API void ma_channel_map_copy(ma_channel* pOut, const ma_channel* pIn, ma_uint32 channels);

/*
Copies a channel map if one is specified, otherwise copies the default channel map.

The output buffer must have a capacity of at least `channels`. If not NULL, the input channel map must also have a capacity of at least `channels`.
*/
MA_API void ma_channel_map_copy_or_default(ma_channel* pOut, size_t channelMapCapOut, const ma_channel* pIn, ma_uint32 channels);


/*
Determines whether or not a channel map is valid.

A blank channel map is valid (all channels set to MA_CHANNEL_NONE). The way a blank channel map is handled is context specific, but
is usually treated as a passthrough.

Invalid channel maps:
  - A channel map with no channels
  - A channel map with more than one channel and a mono channel

The channel map buffer must have a capacity of at least `channels`.
*/
MA_API ma_bool32 ma_channel_map_is_valid(const ma_channel* pChannelMap, ma_uint32 channels);

/*
Helper for comparing two channel maps for equality.

This assumes the channel count is the same between the two.

Both channels map buffers must have a capacity of at least `channels`.
*/
MA_API ma_bool32 ma_channel_map_is_equal(const ma_channel* pChannelMapA, const ma_channel* pChannelMapB, ma_uint32 channels);

/*
Helper for determining if a channel map is blank (all channels set to MA_CHANNEL_NONE).

The channel map buffer must have a capacity of at least `channels`.
*/
MA_API ma_bool32 ma_channel_map_is_blank(const ma_channel* pChannelMap, ma_uint32 channels);

/*
Helper for determining whether or not a channel is present in the given channel map.

The channel map buffer must have a capacity of at least `channels`.
*/
MA_API ma_bool32 ma_channel_map_contains_channel_position(ma_uint32 channels, const ma_channel* pChannelMap, ma_channel channelPosition);

/*
Find a channel position in the given channel map. Returns MA_TRUE if the channel is found; MA_FALSE otherwise. The
index of the channel is output to `pChannelIndex`.

The channel map buffer must have a capacity of at least `channels`.
*/
MA_API ma_bool32 ma_channel_map_find_channel_position(ma_uint32 channels, const ma_channel* pChannelMap, ma_channel channelPosition, ma_uint32* pChannelIndex);

/*
Generates a string representing the given channel map.

This is for printing and debugging purposes, not serialization/deserialization.

Returns the length of the string, not including the null terminator.
*/
MA_API size_t ma_channel_map_to_string(const ma_channel* pChannelMap, ma_uint32 channels, char* pBufferOut, size_t bufferCap);

/*
Retrieves a human readable version of a channel position.
*/
MA_API const char* ma_channel_position_to_string(ma_channel channel);


/************************************************************************************************************************************************************

Conversion Helpers

************************************************************************************************************************************************************/

/*
High-level helper for doing a full format conversion in one go. Returns the number of output frames. Call this with pOut set to NULL to
determine the required size of the output buffer. frameCountOut should be set to the capacity of pOut. If pOut is NULL, frameCountOut is
ignored.

A return value of 0 indicates an error.

This function is useful for one-off bulk conversions, but if you're streaming data you should use the ma_data_converter APIs instead.
*/
MA_API ma_uint64 ma_convert_frames(void* pOut, ma_uint64 frameCountOut, ma_format formatOut, ma_uint32 channelsOut, ma_uint32 sampleRateOut, const void* pIn, ma_uint64 frameCountIn, ma_format formatIn, ma_uint32 channelsIn, ma_uint32 sampleRateIn);
MA_API ma_uint64 ma_convert_frames_ex(void* pOut, ma_uint64 frameCountOut, const void* pIn, ma_uint64 frameCountIn, const ma_data_converter_config* pConfig);


/************************************************************************************************************************************************************

Data Source

************************************************************************************************************************************************************/
typedef void ma_data_source;

#define MA_DATA_SOURCE_SELF_MANAGED_RANGE_AND_LOOP_POINT    0x00000001

typedef struct
{
    ma_result (* onRead)(ma_data_source* pDataSource, void* pFramesOut, ma_uint64 frameCount, ma_uint64* pFramesRead);
    ma_result (* onSeek)(ma_data_source* pDataSource, ma_uint64 frameIndex);
    ma_result (* onGetDataFormat)(ma_data_source* pDataSource, ma_format* pFormat, ma_uint32* pChannels, ma_uint32* pSampleRate, ma_channel* pChannelMap, size_t channelMapCap);
    ma_result (* onGetCursor)(ma_data_source* pDataSource, ma_uint64* pCursor);
    ma_result (* onGetLength)(ma_data_source* pDataSource, ma_uint64* pLength);
    ma_result (* onSetLooping)(ma_data_source* pDataSource, ma_bool32 isLooping);
    ma_uint32 flags;
} ma_data_source_vtable;

typedef ma_data_source* (* ma_data_source_get_next_proc)(ma_data_source* pDataSource);

typedef struct
{
    const ma_data_source_vtable* vtable;
} ma_data_source_config;

MA_API ma_data_source_config ma_data_source_config_init(void);


typedef struct
{
    const ma_data_source_vtable* vtable;
    ma_uint64 rangeBegInFrames;
    ma_uint64 rangeEndInFrames;             /* Set to -1 for unranged (default). */
    ma_uint64 loopBegInFrames;              /* Relative to rangeBegInFrames. */
    ma_uint64 loopEndInFrames;              /* Relative to rangeBegInFrames. Set to -1 for the end of the range. */
    ma_data_source* pCurrent;               /* When non-NULL, the data source being initialized will act as a proxy and will route all operations to pCurrent. Used in conjunction with pNext/onGetNext for seamless chaining. */
    ma_data_source* pNext;                  /* When set to NULL, onGetNext will be used. */
    ma_data_source_get_next_proc onGetNext; /* Will be used when pNext is NULL. If both are NULL, no next will be used. */
    MA_ATOMIC(4, ma_bool32) isLooping;
} ma_data_source_base;

MA_API ma_result ma_data_source_init(const ma_data_source_config* pConfig, ma_data_source* pDataSource);
MA_API void ma_data_source_uninit(ma_data_source* pDataSource);
MA_API ma_result ma_data_source_read_pcm_frames(ma_data_source* pDataSource, void* pFramesOut, ma_uint64 frameCount, ma_uint64* pFramesRead);   /* Must support pFramesOut = NULL in which case a forward seek should be performed. */
MA_API ma_result ma_data_source_seek_pcm_frames(ma_data_source* pDataSource, ma_uint64 frameCount, ma_uint64* pFramesSeeked); /* Can only seek forward. Equivalent to ma_data_source_read_pcm_frames(pDataSource, NULL, frameCount, &framesRead); */
MA_API ma_result ma_data_source_seek_to_pcm_frame(ma_data_source* pDataSource, ma_uint64 frameIndex);
MA_API ma_result ma_data_source_seek_seconds(ma_data_source* pDataSource, float secondCount, float* pSecondsSeeked); /* Can only seek forward. Abstraction to ma_data_source_seek_pcm_frames() */
MA_API ma_result ma_data_source_seek_to_second(ma_data_source* pDataSource, float seekPointInSeconds); /* Abstraction to ma_data_source_seek_to_pcm_frame() */
MA_API ma_result ma_data_source_get_data_format(ma_data_source* pDataSource, ma_format* pFormat, ma_uint32* pChannels, ma_uint32* pSampleRate, ma_channel* pChannelMap, size_t channelMapCap);
MA_API ma_result ma_data_source_get_cursor_in_pcm_frames(ma_data_source* pDataSource, ma_uint64* pCursor);
MA_API ma_result ma_data_source_get_length_in_pcm_frames(ma_data_source* pDataSource, ma_uint64* pLength);    /* Returns MA_NOT_IMPLEMENTED if the length is unknown or cannot be determined. Decoders can return this. */
MA_API ma_result ma_data_source_get_cursor_in_seconds(ma_data_source* pDataSource, float* pCursor);
MA_API ma_result ma_data_source_get_length_in_seconds(ma_data_source* pDataSource, float* pLength);
MA_API ma_result ma_data_source_set_looping(ma_data_source* pDataSource, ma_bool32 isLooping);
MA_API ma_bool32 ma_data_source_is_looping(const ma_data_source* pDataSource);
MA_API ma_result ma_data_source_set_range_in_pcm_frames(ma_data_source* pDataSource, ma_uint64 rangeBegInFrames, ma_uint64 rangeEndInFrames);
MA_API void ma_data_source_get_range_in_pcm_frames(const ma_data_source* pDataSource, ma_uint64* pRangeBegInFrames, ma_uint64* pRangeEndInFrames);
MA_API ma_result ma_data_source_set_loop_point_in_pcm_frames(ma_data_source* pDataSource, ma_uint64 loopBegInFrames, ma_uint64 loopEndInFrames);
MA_API void ma_data_source_get_loop_point_in_pcm_frames(const ma_data_source* pDataSource, ma_uint64* pLoopBegInFrames, ma_uint64* pLoopEndInFrames);
MA_API ma_result ma_data_source_set_current(ma_data_source* pDataSource, ma_data_source* pCurrentDataSource);
MA_API ma_data_source* ma_data_source_get_current(const ma_data_source* pDataSource);
MA_API ma_result ma_data_source_set_next(ma_data_source* pDataSource, ma_data_source* pNextDataSource);
MA_API ma_data_source* ma_data_source_get_next(const ma_data_source* pDataSource);
MA_API ma_result ma_data_source_set_next_callback(ma_data_source* pDataSource, ma_data_source_get_next_proc onGetNext);
MA_API ma_data_source_get_next_proc ma_data_source_get_next_callback(const ma_data_source* pDataSource);


typedef struct
{
    ma_data_source_base ds;
    ma_format format;
    ma_uint32 channels;
    ma_uint32 sampleRate;
    ma_uint64 cursor;
    ma_uint64 sizeInFrames;
    const void* pData;
} ma_audio_buffer_ref;

MA_API ma_result ma_audio_buffer_ref_init(ma_format format, ma_uint32 channels, const void* pData, ma_uint64 sizeInFrames, ma_audio_buffer_ref* pAudioBufferRef);
MA_API void ma_audio_buffer_ref_uninit(ma_audio_buffer_ref* pAudioBufferRef);
MA_API ma_result ma_audio_buffer_ref_set_data(ma_audio_buffer_ref* pAudioBufferRef, const void* pData, ma_uint64 sizeInFrames);
MA_API ma_uint64 ma_audio_buffer_ref_read_pcm_frames(ma_audio_buffer_ref* pAudioBufferRef, void* pFramesOut, ma_uint64 frameCount, ma_bool32 loop);
MA_API ma_result ma_audio_buffer_ref_seek_to_pcm_frame(ma_audio_buffer_ref* pAudioBufferRef, ma_uint64 frameIndex);
MA_API ma_result ma_audio_buffer_ref_map(ma_audio_buffer_ref* pAudioBufferRef, void** ppFramesOut, ma_uint64* pFrameCount);
MA_API ma_result ma_audio_buffer_ref_unmap(ma_audio_buffer_ref* pAudioBufferRef, ma_uint64 frameCount);    /* Returns MA_AT_END if the end has been reached. This should be considered successful. */
MA_API ma_bool32 ma_audio_buffer_ref_at_end(const ma_audio_buffer_ref* pAudioBufferRef);
MA_API ma_result ma_audio_buffer_ref_get_cursor_in_pcm_frames(const ma_audio_buffer_ref* pAudioBufferRef, ma_uint64* pCursor);
MA_API ma_result ma_audio_buffer_ref_get_length_in_pcm_frames(const ma_audio_buffer_ref* pAudioBufferRef, ma_uint64* pLength);
MA_API ma_result ma_audio_buffer_ref_get_available_frames(const ma_audio_buffer_ref* pAudioBufferRef, ma_uint64* pAvailableFrames);



typedef struct
{
    ma_format format;
    ma_uint32 channels;
    ma_uint32 sampleRate;
    ma_uint64 sizeInFrames;
    const void* pData;  /* If set to NULL, will allocate a block of memory for you. */
    ma_allocation_callbacks allocationCallbacks;
} ma_audio_buffer_config;

MA_API ma_audio_buffer_config ma_audio_buffer_config_init(ma_format format, ma_uint32 channels, ma_uint64 sizeInFrames, const void* pData, const ma_allocation_callbacks* pAllocationCallbacks);

typedef struct
{
    ma_audio_buffer_ref ref;
    ma_allocation_callbacks allocationCallbacks;
    ma_bool32 ownsData;             /* Used to control whether or not miniaudio owns the data buffer. If set to true, pData will be freed in ma_audio_buffer_uninit(). */
    ma_uint8 _pExtraData[1];        /* For allocating a buffer with the memory located directly after the other memory of the structure. */
} ma_audio_buffer;

MA_API ma_result ma_audio_buffer_init(const ma_audio_buffer_config* pConfig, ma_audio_buffer* pAudioBuffer);
MA_API ma_result ma_audio_buffer_init_copy(const ma_audio_buffer_config* pConfig, ma_audio_buffer* pAudioBuffer);
MA_API ma_result ma_audio_buffer_alloc_and_init(const ma_audio_buffer_config* pConfig, ma_audio_buffer** ppAudioBuffer);  /* Always copies the data. Doesn't make sense to use this otherwise. Use ma_audio_buffer_uninit_and_free() to uninit. */
MA_API void ma_audio_buffer_uninit(ma_audio_buffer* pAudioBuffer);
MA_API void ma_audio_buffer_uninit_and_free(ma_audio_buffer* pAudioBuffer);
MA_API ma_uint64 ma_audio_buffer_read_pcm_frames(ma_audio_buffer* pAudioBuffer, void* pFramesOut, ma_uint64 frameCount, ma_bool32 loop);
MA_API ma_result ma_audio_buffer_seek_to_pcm_frame(ma_audio_buffer* pAudioBuffer, ma_uint64 frameIndex);
MA_API ma_result ma_audio_buffer_map(ma_audio_buffer* pAudioBuffer, void** ppFramesOut, ma_uint64* pFrameCount);
MA_API ma_result ma_audio_buffer_unmap(ma_audio_buffer* pAudioBuffer, ma_uint64 frameCount);    /* Returns MA_AT_END if the end has been reached. This should be considered successful. */
MA_API ma_bool32 ma_audio_buffer_at_end(const ma_audio_buffer* pAudioBuffer);
MA_API ma_result ma_audio_buffer_get_cursor_in_pcm_frames(const ma_audio_buffer* pAudioBuffer, ma_uint64* pCursor);
MA_API ma_result ma_audio_buffer_get_length_in_pcm_frames(const ma_audio_buffer* pAudioBuffer, ma_uint64* pLength);
MA_API ma_result ma_audio_buffer_get_available_frames(const ma_audio_buffer* pAudioBuffer, ma_uint64* pAvailableFrames);


/*
Paged Audio Buffer
==================
A paged audio buffer is made up of a linked list of pages. It's expandable, but not shrinkable. It
can be used for cases where audio data is streamed in asynchronously while allowing data to be read
at the same time.

This is lock-free, but not 100% thread safe. You can append a page and read from the buffer across
simultaneously across different threads, however only one thread at a time can append, and only one
thread at a time can read and seek.
*/
typedef struct ma_paged_audio_buffer_page ma_paged_audio_buffer_page;
struct ma_paged_audio_buffer_page
{
    MA_ATOMIC(MA_SIZEOF_PTR, ma_paged_audio_buffer_page*) pNext;
    ma_uint64 sizeInFrames;
    ma_uint8 pAudioData[1];
};

typedef struct
{
    ma_format format;
    ma_uint32 channels;
    ma_paged_audio_buffer_page head;                                /* Dummy head for the lock-free algorithm. Always has a size of 0. */
    MA_ATOMIC(MA_SIZEOF_PTR, ma_paged_audio_buffer_page*) pTail;    /* Never null. Initially set to &head. */
} ma_paged_audio_buffer_data;

MA_API ma_result ma_paged_audio_buffer_data_init(ma_format format, ma_uint32 channels, ma_paged_audio_buffer_data* pData);
MA_API void ma_paged_audio_buffer_data_uninit(ma_paged_audio_buffer_data* pData, const ma_allocation_callbacks* pAllocationCallbacks);
MA_API ma_paged_audio_buffer_page* ma_paged_audio_buffer_data_get_head(ma_paged_audio_buffer_data* pData);
MA_API ma_paged_audio_buffer_page* ma_paged_audio_buffer_data_get_tail(ma_paged_audio_buffer_data* pData);
MA_API ma_result ma_paged_audio_buffer_data_get_length_in_pcm_frames(ma_paged_audio_buffer_data* pData, ma_uint64* pLength);
MA_API ma_result ma_paged_audio_buffer_data_allocate_page(ma_paged_audio_buffer_data* pData, ma_uint64 pageSizeInFrames, const void* pInitialData, const ma_allocation_callbacks* pAllocationCallbacks, ma_paged_audio_buffer_page** ppPage);
MA_API ma_result ma_paged_audio_buffer_data_free_page(ma_paged_audio_buffer_data* pData, ma_paged_audio_buffer_page* pPage, const ma_allocation_callbacks* pAllocationCallbacks);
MA_API ma_result ma_paged_audio_buffer_data_append_page(ma_paged_audio_buffer_data* pData, ma_paged_audio_buffer_page* pPage);
MA_API ma_result ma_paged_audio_buffer_data_allocate_and_append_page(ma_paged_audio_buffer_data* pData, ma_uint32 pageSizeInFrames, const void* pInitialData, const ma_allocation_callbacks* pAllocationCallbacks);


typedef struct
{
    ma_paged_audio_buffer_data* pData;  /* Must not be null. */
} ma_paged_audio_buffer_config;

MA_API ma_paged_audio_buffer_config ma_paged_audio_buffer_config_init(ma_paged_audio_buffer_data* pData);


typedef struct
{
    ma_data_source_base ds;
    ma_paged_audio_buffer_data* pData;              /* Audio data is read from here. Cannot be null. */
    ma_paged_audio_buffer_page* pCurrent;
    ma_uint64 relativeCursor;                       /* Relative to the current page. */
    ma_uint64 absoluteCursor;
} ma_paged_audio_buffer;

MA_API ma_result ma_paged_audio_buffer_init(const ma_paged_audio_buffer_config* pConfig, ma_paged_audio_buffer* pPagedAudioBuffer);
MA_API void ma_paged_audio_buffer_uninit(ma_paged_audio_buffer* pPagedAudioBuffer);
MA_API ma_result ma_paged_audio_buffer_read_pcm_frames(ma_paged_audio_buffer* pPagedAudioBuffer, void* pFramesOut, ma_uint64 frameCount, ma_uint64* pFramesRead);   /* Returns MA_AT_END if no more pages available. */
MA_API ma_result ma_paged_audio_buffer_seek_to_pcm_frame(ma_paged_audio_buffer* pPagedAudioBuffer, ma_uint64 frameIndex);
MA_API ma_result ma_paged_audio_buffer_get_cursor_in_pcm_frames(ma_paged_audio_buffer* pPagedAudioBuffer, ma_uint64* pCursor);
MA_API ma_result ma_paged_audio_buffer_get_length_in_pcm_frames(ma_paged_audio_buffer* pPagedAudioBuffer, ma_uint64* pLength);



/************************************************************************************************************************************************************

Ring Buffer

************************************************************************************************************************************************************/
typedef struct
{
    void* pBuffer;
    ma_uint32 subbufferSizeInBytes;
    ma_uint32 subbufferCount;
    ma_uint32 subbufferStrideInBytes;
    MA_ATOMIC(4, ma_uint32) encodedReadOffset;  /* Most significant bit is the loop flag. Lower 31 bits contains the actual offset in bytes. Must be used atomically. */
    MA_ATOMIC(4, ma_uint32) encodedWriteOffset; /* Most significant bit is the loop flag. Lower 31 bits contains the actual offset in bytes. Must be used atomically. */
    ma_bool8 ownsBuffer;                        /* Used to know whether or not miniaudio is responsible for free()-ing the buffer. */
    ma_bool8 clearOnWriteAcquire;               /* When set, clears the acquired write buffer before returning from ma_rb_acquire_write(). */
    ma_allocation_callbacks allocationCallbacks;
} ma_rb;

MA_API ma_result ma_rb_init_ex(size_t subbufferSizeInBytes, size_t subbufferCount, size_t subbufferStrideInBytes, void* pOptionalPreallocatedBuffer, const ma_allocation_callbacks* pAllocationCallbacks, ma_rb* pRB);
MA_API ma_result ma_rb_init(size_t bufferSizeInBytes, void* pOptionalPreallocatedBuffer, const ma_allocation_callbacks* pAllocationCallbacks, ma_rb* pRB);
MA_API void ma_rb_uninit(ma_rb* pRB);
MA_API void ma_rb_reset(ma_rb* pRB);
MA_API ma_result ma_rb_acquire_read(ma_rb* pRB, size_t* pSizeInBytes, void** ppBufferOut);
MA_API ma_result ma_rb_commit_read(ma_rb* pRB, size_t sizeInBytes);
MA_API ma_result ma_rb_acquire_write(ma_rb* pRB, size_t* pSizeInBytes, void** ppBufferOut);
MA_API ma_result ma_rb_commit_write(ma_rb* pRB, size_t sizeInBytes);
MA_API ma_result ma_rb_seek_read(ma_rb* pRB, size_t offsetInBytes);
MA_API ma_result ma_rb_seek_write(ma_rb* pRB, size_t offsetInBytes);
MA_API ma_int32 ma_rb_pointer_distance(ma_rb* pRB);    /* Returns the distance between the write pointer and the read pointer. Should never be negative for a correct program. Will return the number of bytes that can be read before the read pointer hits the write pointer. */
MA_API ma_uint32 ma_rb_available_read(ma_rb* pRB);
MA_API ma_uint32 ma_rb_available_write(ma_rb* pRB);
MA_API size_t ma_rb_get_subbuffer_size(ma_rb* pRB);
MA_API size_t ma_rb_get_subbuffer_stride(ma_rb* pRB);
MA_API size_t ma_rb_get_subbuffer_offset(ma_rb* pRB, size_t subbufferIndex);
MA_API void* ma_rb_get_subbuffer_ptr(ma_rb* pRB, size_t subbufferIndex, void* pBuffer);


typedef struct
{
    ma_data_source_base ds;
    ma_rb rb;
    ma_format format;
    ma_uint32 channels;
    ma_uint32 sampleRate; /* Not required for the ring buffer itself, but useful for associating the data with some sample rate, particularly for data sources. */
} ma_pcm_rb;

MA_API ma_result ma_pcm_rb_init_ex(ma_format format, ma_uint32 channels, ma_uint32 subbufferSizeInFrames, ma_uint32 subbufferCount, ma_uint32 subbufferStrideInFrames, void* pOptionalPreallocatedBuffer, const ma_allocation_callbacks* pAllocationCallbacks, ma_pcm_rb* pRB);
MA_API ma_result ma_pcm_rb_init(ma_format format, ma_uint32 channels, ma_uint32 bufferSizeInFrames, void* pOptionalPreallocatedBuffer, const ma_allocation_callbacks* pAllocationCallbacks, ma_pcm_rb* pRB);
MA_API void ma_pcm_rb_uninit(ma_pcm_rb* pRB);
MA_API void ma_pcm_rb_reset(ma_pcm_rb* pRB);
MA_API ma_result ma_pcm_rb_acquire_read(ma_pcm_rb* pRB, ma_uint32* pSizeInFrames, void** ppBufferOut);
MA_API ma_result ma_pcm_rb_commit_read(ma_pcm_rb* pRB, ma_uint32 sizeInFrames);
MA_API ma_result ma_pcm_rb_acquire_write(ma_pcm_rb* pRB, ma_uint32* pSizeInFrames, void** ppBufferOut);
MA_API ma_result ma_pcm_rb_commit_write(ma_pcm_rb* pRB, ma_uint32 sizeInFrames);
MA_API ma_result ma_pcm_rb_seek_read(ma_pcm_rb* pRB, ma_uint32 offsetInFrames);
MA_API ma_result ma_pcm_rb_seek_write(ma_pcm_rb* pRB, ma_uint32 offsetInFrames);
MA_API ma_int32 ma_pcm_rb_pointer_distance(ma_pcm_rb* pRB); /* Return value is in frames. */
MA_API ma_uint32 ma_pcm_rb_available_read(ma_pcm_rb* pRB);
MA_API ma_uint32 ma_pcm_rb_available_write(ma_pcm_rb* pRB);
MA_API ma_uint32 ma_pcm_rb_get_subbuffer_size(ma_pcm_rb* pRB);
MA_API ma_uint32 ma_pcm_rb_get_subbuffer_stride(ma_pcm_rb* pRB);
MA_API ma_uint32 ma_pcm_rb_get_subbuffer_offset(ma_pcm_rb* pRB, ma_uint32 subbufferIndex);
MA_API void* ma_pcm_rb_get_subbuffer_ptr(ma_pcm_rb* pRB, ma_uint32 subbufferIndex, void* pBuffer);
MA_API ma_format ma_pcm_rb_get_format(const ma_pcm_rb* pRB);
MA_API ma_uint32 ma_pcm_rb_get_channels(const ma_pcm_rb* pRB);
MA_API ma_uint32 ma_pcm_rb_get_sample_rate(const ma_pcm_rb* pRB);
MA_API void ma_pcm_rb_set_sample_rate(ma_pcm_rb* pRB, ma_uint32 sampleRate);


/*
The idea of the duplex ring buffer is to act as the intermediary buffer when running two asynchronous devices in a duplex set up. The
capture device writes to it, and then a playback device reads from it.

At the moment this is just a simple naive implementation, but in the future I want to implement some dynamic resampling to seamlessly
handle desyncs. Note that the API is work in progress and may change at any time in any version.

The size of the buffer is based on the capture side since that's what'll be written to the buffer. It is based on the capture period size
in frames. The internal sample rate of the capture device is also needed in order to calculate the size.
*/
typedef struct
{
    ma_pcm_rb rb;
} ma_duplex_rb;

MA_API ma_result ma_duplex_rb_init(ma_format captureFormat, ma_uint32 captureChannels, ma_uint32 sampleRate, ma_uint32 captureInternalSampleRate, ma_uint32 captureInternalPeriodSizeInFrames, const ma_allocation_callbacks* pAllocationCallbacks, ma_duplex_rb* pRB);
MA_API ma_result ma_duplex_rb_uninit(ma_duplex_rb* pRB);


/************************************************************************************************************************************************************

Miscellaneous Helpers

************************************************************************************************************************************************************/
/*
Retrieves a human readable description of the given result code.
*/
MA_API const char* ma_result_description(ma_result result);

/*
malloc()
*/
MA_API void* ma_malloc(size_t sz, const ma_allocation_callbacks* pAllocationCallbacks);

/*
calloc()
*/
MA_API void* ma_calloc(size_t sz, const ma_allocation_callbacks* pAllocationCallbacks);

/*
realloc()
*/
MA_API void* ma_realloc(void* p, size_t sz, const ma_allocation_callbacks* pAllocationCallbacks);

/*
free()
*/
MA_API void ma_free(void* p, const ma_allocation_callbacks* pAllocationCallbacks);

/*
Performs an aligned malloc, with the assumption that the alignment is a power of 2.
*/
MA_API void* ma_aligned_malloc(size_t sz, size_t alignment, const ma_allocation_callbacks* pAllocationCallbacks);

/*
Free's an aligned malloc'd buffer.
*/
MA_API void ma_aligned_free(void* p, const ma_allocation_callbacks* pAllocationCallbacks);

/*
Retrieves a friendly name for a format.
*/
MA_API const char* ma_get_format_name(ma_format format);

/*
Blends two frames in floating point format.
*/
MA_API void ma_blend_f32(float* pOut, float* pInA, float* pInB, float factor, ma_uint32 channels);

/*
Retrieves the size of a sample in bytes for the given format.

This API is efficient and is implemented using a lookup table.

Thread Safety: SAFE
  This API is pure.
*/
MA_API ma_uint32 ma_get_bytes_per_sample(ma_format format);
static MA_INLINE ma_uint32 ma_get_bytes_per_frame(ma_format format, ma_uint32 channels) { return ma_get_bytes_per_sample(format) * channels; }

/*
Converts a log level to a string.
*/
MA_API const char* ma_log_level_to_string(ma_uint32 logLevel);




/************************************************************************************************************************************************************

Synchronization

************************************************************************************************************************************************************/
/*
Locks a spinlock.
*/
MA_API ma_result ma_spinlock_lock(volatile ma_spinlock* pSpinlock);

/*
Locks a spinlock, but does not yield() when looping.
*/
MA_API ma_result ma_spinlock_lock_noyield(volatile ma_spinlock* pSpinlock);

/*
Unlocks a spinlock.
*/
MA_API ma_result ma_spinlock_unlock(volatile ma_spinlock* pSpinlock);


#ifndef MA_NO_THREADING

/*
Creates a mutex.

A mutex must be created from a valid context. A mutex is initially unlocked.
*/
MA_API ma_result ma_mutex_init(ma_mutex* pMutex);

/*
Deletes a mutex.
*/
MA_API void ma_mutex_uninit(ma_mutex* pMutex);

/*
Locks a mutex with an infinite timeout.
*/
MA_API void ma_mutex_lock(ma_mutex* pMutex);

/*
Unlocks a mutex.
*/
MA_API void ma_mutex_unlock(ma_mutex* pMutex);


/*
Initializes an auto-reset event.
*/
MA_API ma_result ma_event_init(ma_event* pEvent);

/*
Uninitializes an auto-reset event.
*/
MA_API void ma_event_uninit(ma_event* pEvent);

/*
Waits for the specified auto-reset event to become signalled.
*/
MA_API ma_result ma_event_wait(ma_event* pEvent);

/*
Signals the specified auto-reset event.
*/
MA_API ma_result ma_event_signal(ma_event* pEvent);


MA_API ma_result ma_semaphore_init(int initialValue, ma_semaphore* pSemaphore);
MA_API void ma_semaphore_uninit(ma_semaphore* pSemaphore);
MA_API ma_result ma_semaphore_wait(ma_semaphore* pSemaphore);
MA_API ma_result ma_semaphore_release(ma_semaphore* pSemaphore);
#endif  /* MA_NO_THREADING */


/*
Fence
=====
This locks while the counter is larger than 0. Counter can be incremented and decremented by any
thread, but care needs to be taken when waiting. It is possible for one thread to acquire the
fence just as another thread returns from ma_fence_wait().

The idea behind a fence is to allow you to wait for a group of operations to complete. When an
operation starts, the counter is incremented which locks the fence. When the operation completes,
the fence will be released which decrements the counter. ma_fence_wait() will block until the
counter hits zero.

If threading is disabled, ma_fence_wait() will spin on the counter.
*/
typedef struct
{
#ifndef MA_NO_THREADING
    ma_event e;
#endif
    ma_uint32 counter;
} ma_fence;

MA_API ma_result ma_fence_init(ma_fence* pFence);
MA_API void ma_fence_uninit(ma_fence* pFence);
MA_API ma_result ma_fence_acquire(ma_fence* pFence);    /* Increment counter. */
MA_API ma_result ma_fence_release(ma_fence* pFence);    /* Decrement counter. */
MA_API ma_result ma_fence_wait(ma_fence* pFence);       /* Wait for counter to reach 0. */



/*
Notification callback for asynchronous operations.
*/
typedef void ma_async_notification;

typedef struct
{
    void (* onSignal)(ma_async_notification* pNotification);
} ma_async_notification_callbacks;

MA_API ma_result ma_async_notification_signal(ma_async_notification* pNotification);


/*
Simple polling notification.

This just sets a variable when the notification has been signalled which is then polled with ma_async_notification_poll_is_signalled()
*/
typedef struct
{
    ma_async_notification_callbacks cb;
    ma_bool32 signalled;
} ma_async_notification_poll;

MA_API ma_result ma_async_notification_poll_init(ma_async_notification_poll* pNotificationPoll);
MA_API ma_bool32 ma_async_notification_poll_is_signalled(const ma_async_notification_poll* pNotificationPoll);


/*
Event Notification

This uses an ma_event. If threading is disabled (MA_NO_THREADING), initialization will fail.
*/
typedef struct
{
    ma_async_notification_callbacks cb;
#ifndef MA_NO_THREADING
    ma_event e;
#endif
} ma_async_notification_event;

MA_API ma_result ma_async_notification_event_init(ma_async_notification_event* pNotificationEvent);
MA_API ma_result ma_async_notification_event_uninit(ma_async_notification_event* pNotificationEvent);
MA_API ma_result ma_async_notification_event_wait(ma_async_notification_event* pNotificationEvent);
MA_API ma_result ma_async_notification_event_signal(ma_async_notification_event* pNotificationEvent);




/************************************************************************************************************************************************************

Job Queue

************************************************************************************************************************************************************/

/*
Slot Allocator
--------------
The idea of the slot allocator is for it to be used in conjunction with a fixed sized buffer. You use the slot allocator to allocate an index that can be used
as the insertion point for an object.

Slots are reference counted to help mitigate the ABA problem in the lock-free queue we use for tracking jobs.

The slot index is stored in the low 32 bits. The reference counter is stored in the high 32 bits:

    +-----------------+-----------------+
    | 32 Bits         | 32 Bits         |
    +-----------------+-----------------+
    | Reference Count | Slot Index      |
    +-----------------+-----------------+
*/
typedef struct
{
    ma_uint32 capacity;    /* The number of slots to make available. */
} ma_slot_allocator_config;

MA_API ma_slot_allocator_config ma_slot_allocator_config_init(ma_uint32 capacity);


typedef struct
{
    MA_ATOMIC(4, ma_uint32) bitfield;   /* Must be used atomically because the allocation and freeing routines need to make copies of this which must never be optimized away by the compiler. */
} ma_slot_allocator_group;

typedef struct
{
    ma_slot_allocator_group* pGroups;   /* Slots are grouped in chunks of 32. */
    ma_uint32* pSlots;                  /* 32 bits for reference counting for ABA mitigation. */
    ma_uint32 count;                    /* Allocation count. */
    ma_uint32 capacity;

    /* Memory management. */
    ma_bool32 _ownsHeap;
    void* _pHeap;
} ma_slot_allocator;

MA_API ma_result ma_slot_allocator_get_heap_size(const ma_slot_allocator_config* pConfig, size_t* pHeapSizeInBytes);
MA_API ma_result ma_slot_allocator_init_preallocated(const ma_slot_allocator_config* pConfig, void* pHeap, ma_slot_allocator* pAllocator);
MA_API ma_result ma_slot_allocator_init(const ma_slot_allocator_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_slot_allocator* pAllocator);
MA_API void ma_slot_allocator_uninit(ma_slot_allocator* pAllocator, const ma_allocation_callbacks* pAllocationCallbacks);
MA_API ma_result ma_slot_allocator_alloc(ma_slot_allocator* pAllocator, ma_uint64* pSlot);
MA_API ma_result ma_slot_allocator_free(ma_slot_allocator* pAllocator, ma_uint64 slot);


typedef struct ma_job ma_job;

/*
Callback for processing a job. Each job type will have their own processing callback which will be
called by ma_job_process().
*/
typedef ma_result (* ma_job_proc)(ma_job* pJob);

/* When a job type is added here an callback needs to be added go "g_jobVTable" in the implementation section. */
typedef enum
{
    /* Miscellaneous. */
    MA_JOB_TYPE_QUIT = 0,
    MA_JOB_TYPE_CUSTOM,

    /* Resource Manager. */
    MA_JOB_TYPE_RESOURCE_MANAGER_LOAD_DATA_BUFFER_NODE,
    MA_JOB_TYPE_RESOURCE_MANAGER_FREE_DATA_BUFFER_NODE,
    MA_JOB_TYPE_RESOURCE_MANAGER_PAGE_DATA_BUFFER_NODE,
    MA_JOB_TYPE_RESOURCE_MANAGER_LOAD_DATA_BUFFER,
    MA_JOB_TYPE_RESOURCE_MANAGER_FREE_DATA_BUFFER,
    MA_JOB_TYPE_RESOURCE_MANAGER_LOAD_DATA_STREAM,
    MA_JOB_TYPE_RESOURCE_MANAGER_FREE_DATA_STREAM,
    MA_JOB_TYPE_RESOURCE_MANAGER_PAGE_DATA_STREAM,
    MA_JOB_TYPE_RESOURCE_MANAGER_SEEK_DATA_STREAM,

    /* Device. */
    MA_JOB_TYPE_DEVICE_AAUDIO_REROUTE,

    /* Count. Must always be last. */
    MA_JOB_TYPE_COUNT
} ma_job_type;

struct ma_job
{
    union
    {
        struct
        {
            ma_uint16 code;         /* Job type. */
            ma_uint16 slot;         /* Index into a ma_slot_allocator. */
            ma_uint32 refcount;
        } breakup;
        ma_uint64 allocation;
    } toc;  /* 8 bytes. We encode the job code into the slot allocation data to save space. */
    MA_ATOMIC(8, ma_uint64) next; /* refcount + slot for the next item. Does not include the job code. */
    ma_uint32 order;    /* Execution order. Used to create a data dependency and ensure a job is executed in order. Usage is contextual depending on the job type. */

    union
    {
        /* Miscellaneous. */
        struct
        {
            ma_job_proc proc;
            ma_uintptr data0;
            ma_uintptr data1;
        } custom;

        /* Resource Manager */
        union
        {
            struct
            {
                /*ma_resource_manager**/ void* pResourceManager;
                /*ma_resource_manager_data_buffer_node**/ void* pDataBufferNode;
                char* pFilePath;
                wchar_t* pFilePathW;
                ma_uint32 flags;                                /* Resource manager data source flags that were used when initializing the data buffer. */
                ma_async_notification* pInitNotification;       /* Signalled when the data buffer has been initialized and the format/channels/rate can be retrieved. */
                ma_async_notification* pDoneNotification;       /* Signalled when the data buffer has been fully decoded. Will be passed through to MA_JOB_TYPE_RESOURCE_MANAGER_PAGE_DATA_BUFFER_NODE when decoding. */
                ma_fence* pInitFence;                           /* Released when initialization of the decoder is complete. */
                ma_fence* pDoneFence;                           /* Released if initialization of the decoder fails. Passed through to PAGE_DATA_BUFFER_NODE untouched if init is successful. */
            } loadDataBufferNode;
            struct
            {
                /*ma_resource_manager**/ void* pResourceManager;
                /*ma_resource_manager_data_buffer_node**/ void* pDataBufferNode;
                ma_async_notification* pDoneNotification;
                ma_fence* pDoneFence;
            } freeDataBufferNode;
            struct
            {
                /*ma_resource_manager**/ void* pResourceManager;
                /*ma_resource_manager_data_buffer_node**/ void* pDataBufferNode;
                /*ma_decoder**/ void* pDecoder;
                ma_async_notification* pDoneNotification;       /* Signalled when the data buffer has been fully decoded. */
                ma_fence* pDoneFence;                           /* Passed through from LOAD_DATA_BUFFER_NODE and released when the data buffer completes decoding or an error occurs. */
            } pageDataBufferNode;

            struct
            {
                /*ma_resource_manager_data_buffer**/ void* pDataBuffer;
                ma_async_notification* pInitNotification;       /* Signalled when the data buffer has been initialized and the format/channels/rate can be retrieved. */
                ma_async_notification* pDoneNotification;       /* Signalled when the data buffer has been fully decoded. */
                ma_fence* pInitFence;                           /* Released when the data buffer has been initialized and the format/channels/rate can be retrieved. */
                ma_fence* pDoneFence;                           /* Released when the data buffer has been fully decoded. */
                ma_uint64 rangeBegInPCMFrames;
                ma_uint64 rangeEndInPCMFrames;
                ma_uint64 loopPointBegInPCMFrames;
                ma_uint64 loopPointEndInPCMFrames;
                ma_uint32 isLooping;
            } loadDataBuffer;
            struct
            {
                /*ma_resource_manager_data_buffer**/ void* pDataBuffer;
                ma_async_notification* pDoneNotification;
                ma_fence* pDoneFence;
            } freeDataBuffer;

            struct
            {
                /*ma_resource_manager_data_stream**/ void* pDataStream;
                char* pFilePath;                            /* Allocated when the job is posted, freed by the job thread after loading. */
                wchar_t* pFilePathW;                        /* ^ As above ^. Only used if pFilePath is NULL. */
                ma_uint64 initialSeekPoint;
                ma_async_notification* pInitNotification;   /* Signalled after the first two pages have been decoded and frames can be read from the stream. */
                ma_fence* pInitFence;
            } loadDataStream;
            struct
            {
                /*ma_resource_manager_data_stream**/ void* pDataStream;
                ma_async_notification* pDoneNotification;
                ma_fence* pDoneFence;
            } freeDataStream;
            struct
            {
                /*ma_resource_manager_data_stream**/ void* pDataStream;
                ma_uint32 pageIndex;                    /* The index of the page to decode into. */
            } pageDataStream;
            struct
            {
                /*ma_resource_manager_data_stream**/ void* pDataStream;
                ma_uint64 frameIndex;
            } seekDataStream;
        } resourceManager;

        /* Device. */
        union
        {
            union
            {
                struct
                {
                    /*ma_device**/ void* pDevice;
                    /*ma_device_type*/ ma_uint32 deviceType;
                } reroute;
            } aaudio;
        } device;
    } data;
};

MA_API ma_job ma_job_init(ma_uint16 code);
MA_API ma_result ma_job_process(ma_job* pJob);


/*
When set, ma_job_queue_next() will not wait and no semaphore will be signaled in
ma_job_queue_post(). ma_job_queue_next() will return MA_NO_DATA_AVAILABLE if nothing is available.

This flag should always be used for platforms that do not support multithreading.
*/
typedef enum
{
    MA_JOB_QUEUE_FLAG_NON_BLOCKING = 0x00000001
} ma_job_queue_flags;

typedef struct
{
    ma_uint32 flags;
    ma_uint32 capacity; /* The maximum number of jobs that can fit in the queue at a time. */
} ma_job_queue_config;

MA_API ma_job_queue_config ma_job_queue_config_init(ma_uint32 flags, ma_uint32 capacity);


typedef struct
{
    ma_uint32 flags;                /* Flags passed in at initialization time. */
    ma_uint32 capacity;             /* The maximum number of jobs that can fit in the queue at a time. Set by the config. */
    MA_ATOMIC(8, ma_uint64) head;   /* The first item in the list. Required for removing from the top of the list. */
    MA_ATOMIC(8, ma_uint64) tail;   /* The last item in the list. Required for appending to the end of the list. */
#ifndef MA_NO_THREADING
    ma_semaphore sem;               /* Only used when MA_JOB_QUEUE_FLAG_NON_BLOCKING is unset. */
#endif
    ma_slot_allocator allocator;
    ma_job* pJobs;
#ifndef MA_USE_EXPERIMENTAL_LOCK_FREE_JOB_QUEUE
    ma_spinlock lock;
#endif

    /* Memory management. */
    void* _pHeap;
    ma_bool32 _ownsHeap;
} ma_job_queue;

MA_API ma_result ma_job_queue_get_heap_size(const ma_job_queue_config* pConfig, size_t* pHeapSizeInBytes);
MA_API ma_result ma_job_queue_init_preallocated(const ma_job_queue_config* pConfig, void* pHeap, ma_job_queue* pQueue);
MA_API ma_result ma_job_queue_init(const ma_job_queue_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_job_queue* pQueue);
MA_API void ma_job_queue_uninit(ma_job_queue* pQueue, const ma_allocation_callbacks* pAllocationCallbacks);
MA_API ma_result ma_job_queue_post(ma_job_queue* pQueue, const ma_job* pJob);
MA_API ma_result ma_job_queue_next(ma_job_queue* pQueue, ma_job* pJob); /* Returns MA_CANCELLED if the next job is a quit job. */



/************************************************************************************************************************************************************
*************************************************************************************************************************************************************

DEVICE I/O
==========

This section contains the APIs for device playback and capture. Here is where you'll find ma_device_init(), etc.

*************************************************************************************************************************************************************
************************************************************************************************************************************************************/
#ifndef MA_NO_DEVICE_IO
/* Some backends are only supported on certain platforms. */
#if defined(MA_WIN32) && !defined(MA_XBOX)
    #define MA_SUPPORT_WASAPI

    #if defined(MA_WIN32_DESKTOP)   /* DirectSound and WinMM backends are only supported on desktops. */
        #define MA_SUPPORT_DSOUND
        #define MA_SUPPORT_WINMM

        /* Don't enable JACK here if compiling with Cosmopolitan. It'll be enabled in the Linux section below. */
        #if !defined(__COSMOPOLITAN__)
            #define MA_SUPPORT_JACK    /* JACK is technically supported on Windows, but I don't know how many people use it in practice... */
        #endif
    #endif
#endif
#if defined(MA_UNIX) && !defined(MA_ORBIS) && !defined(MA_PROSPERO)
    #if defined(MA_LINUX)
        #if !defined(MA_ANDROID) && !defined(__COSMOPOLITAN__)   /* ALSA is not supported on Android. */
            #define MA_SUPPORT_ALSA
        #endif
    #endif
    #if !defined(MA_BSD) && !defined(MA_ANDROID) && !defined(MA_EMSCRIPTEN)
        #define MA_SUPPORT_PULSEAUDIO
        #define MA_SUPPORT_JACK
    #endif
    #if defined(__OpenBSD__)        /* <-- Change this to "#if defined(MA_BSD)" to enable sndio on all BSD flavors. */
        #define MA_SUPPORT_SNDIO    /* sndio is only supported on OpenBSD for now. May be expanded later if there's demand. */
    #endif
    #if defined(__NetBSD__) || defined(__OpenBSD__)
        #define MA_SUPPORT_AUDIO4   /* Only support audio(4) on platforms with known support. */
    #endif
    #if defined(__FreeBSD__) || defined(__DragonFly__)
        #define MA_SUPPORT_OSS      /* Only support OSS on specific platforms with known support. */
    #endif
#endif
#if defined(MA_ANDROID)
    #define MA_SUPPORT_AAUDIO
    #define MA_SUPPORT_OPENSL
#endif
#if defined(MA_APPLE)
    #define MA_SUPPORT_COREAUDIO
#endif
#if defined(MA_EMSCRIPTEN)
    #define MA_SUPPORT_WEBAUDIO
#endif

/* All platforms should support custom backends. */
#define MA_SUPPORT_CUSTOM

/* Explicitly disable the Null backend for Emscripten because it uses a background thread which is not properly supported right now. */
#if !defined(MA_EMSCRIPTEN)
#define MA_SUPPORT_NULL
#endif


#if defined(MA_SUPPORT_WASAPI) && !defined(MA_NO_WASAPI) && (!defined(MA_ENABLE_ONLY_SPECIFIC_BACKENDS) || defined(MA_ENABLE_WASAPI))
    #define MA_HAS_WASAPI
#endif
#if defined(MA_SUPPORT_DSOUND) && !defined(MA_NO_DSOUND) && (!defined(MA_ENABLE_ONLY_SPECIFIC_BACKENDS) || defined(MA_ENABLE_DSOUND))
    #define MA_HAS_DSOUND
#endif
#if defined(MA_SUPPORT_WINMM) && !defined(MA_NO_WINMM) && (!defined(MA_ENABLE_ONLY_SPECIFIC_BACKENDS) || defined(MA_ENABLE_WINMM))
    #define MA_HAS_WINMM
#endif
#if defined(MA_SUPPORT_ALSA) && !defined(MA_NO_ALSA) && (!defined(MA_ENABLE_ONLY_SPECIFIC_BACKENDS) || defined(MA_ENABLE_ALSA))
    #define MA_HAS_ALSA
#endif
#if defined(MA_SUPPORT_PULSEAUDIO) && !defined(MA_NO_PULSEAUDIO) && (!defined(MA_ENABLE_ONLY_SPECIFIC_BACKENDS) || defined(MA_ENABLE_PULSEAUDIO))
    #define MA_HAS_PULSEAUDIO
#endif
#if defined(MA_SUPPORT_JACK) && !defined(MA_NO_JACK) && (!defined(MA_ENABLE_ONLY_SPECIFIC_BACKENDS) || defined(MA_ENABLE_JACK))
    #define MA_HAS_JACK
#endif
#if defined(MA_SUPPORT_COREAUDIO) && !defined(MA_NO_COREAUDIO) && (!defined(MA_ENABLE_ONLY_SPECIFIC_BACKENDS) || defined(MA_ENABLE_COREAUDIO))
    #define MA_HAS_COREAUDIO
#endif
#if defined(MA_SUPPORT_SNDIO) && !defined(MA_NO_SNDIO) && (!defined(MA_ENABLE_ONLY_SPECIFIC_BACKENDS) || defined(MA_ENABLE_SNDIO))
    #define MA_HAS_SNDIO
#endif
#if defined(MA_SUPPORT_AUDIO4) && !defined(MA_NO_AUDIO4) && (!defined(MA_ENABLE_ONLY_SPECIFIC_BACKENDS) || defined(MA_ENABLE_AUDIO4))
    #define MA_HAS_AUDIO4
#endif
#if defined(MA_SUPPORT_OSS) && !defined(MA_NO_OSS) && (!defined(MA_ENABLE_ONLY_SPECIFIC_BACKENDS) || defined(MA_ENABLE_OSS))
    #define MA_HAS_OSS
#endif
#if defined(MA_SUPPORT_AAUDIO) && !defined(MA_NO_AAUDIO) && (!defined(MA_ENABLE_ONLY_SPECIFIC_BACKENDS) || defined(MA_ENABLE_AAUDIO))
    #define MA_HAS_AAUDIO
#endif
#if defined(MA_SUPPORT_OPENSL) && !defined(MA_NO_OPENSL) && (!defined(MA_ENABLE_ONLY_SPECIFIC_BACKENDS) || defined(MA_ENABLE_OPENSL))
    #define MA_HAS_OPENSL
#endif
#if defined(MA_SUPPORT_WEBAUDIO) && !defined(MA_NO_WEBAUDIO) && (!defined(MA_ENABLE_ONLY_SPECIFIC_BACKENDS) || defined(MA_ENABLE_WEBAUDIO))
    #define MA_HAS_WEBAUDIO
#endif
#if defined(MA_SUPPORT_CUSTOM) && !defined(MA_NO_CUSTOM) && (!defined(MA_ENABLE_ONLY_SPECIFIC_BACKENDS) || defined(MA_ENABLE_CUSTOM))
    #define MA_HAS_CUSTOM
#endif
#if defined(MA_SUPPORT_NULL) && !defined(MA_NO_NULL) && (!defined(MA_ENABLE_ONLY_SPECIFIC_BACKENDS) || defined(MA_ENABLE_NULL))
    #define MA_HAS_NULL
#endif

typedef enum
{
    ma_device_state_uninitialized = 0,
    ma_device_state_stopped       = 1,  /* The device's default state after initialization. */
    ma_device_state_started       = 2,  /* The device is started and is requesting and/or delivering audio data. */
    ma_device_state_starting      = 3,  /* Transitioning from a stopped state to started. */
    ma_device_state_stopping      = 4   /* Transitioning from a started state to stopped. */
} ma_device_state;

MA_ATOMIC_SAFE_TYPE_DECL(i32, 4, device_state)


#ifdef MA_SUPPORT_WASAPI
/* We need a IMMNotificationClient object for WASAPI. */
typedef struct
{
    void* lpVtbl;
    ma_uint32 counter;
    ma_device* pDevice;
} ma_IMMNotificationClient;
#endif

/* Backend enums must be in priority order. */
typedef enum
{
    ma_backend_wasapi,
    ma_backend_dsound,
    ma_backend_winmm,
    ma_backend_coreaudio,
    ma_backend_sndio,
    ma_backend_audio4,
    ma_backend_oss,
    ma_backend_pulseaudio,
    ma_backend_alsa,
    ma_backend_jack,
    ma_backend_aaudio,
    ma_backend_opensl,
    ma_backend_webaudio,
    ma_backend_custom,  /* <-- Custom backend, with callbacks defined by the context config. */
    ma_backend_null     /* <-- Must always be the last item. Lowest priority, and used as the terminator for backend enumeration. */
} ma_backend;

#define MA_BACKEND_COUNT (ma_backend_null+1)


/*
Device job thread. This is used by backends that require asynchronous processing of certain
operations. It is not used by all backends.

The device job thread is made up of a thread and a job queue. You can post a job to the thread with
ma_device_job_thread_post(). The thread will do the processing of the job.
*/
typedef struct
{
    ma_bool32 noThread; /* Set this to true if you want to process jobs yourself. */
    ma_uint32 jobQueueCapacity;
    ma_uint32 jobQueueFlags;
} ma_device_job_thread_config;

MA_API ma_device_job_thread_config ma_device_job_thread_config_init(void);

typedef struct
{
    ma_thread thread;
    ma_job_queue jobQueue;
    ma_bool32 _hasThread;
} ma_device_job_thread;

MA_API ma_result ma_device_job_thread_init(const ma_device_job_thread_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_device_job_thread* pJobThread);
MA_API void ma_device_job_thread_uninit(ma_device_job_thread* pJobThread, const ma_allocation_callbacks* pAllocationCallbacks);
MA_API ma_result ma_device_job_thread_post(ma_device_job_thread* pJobThread, const ma_job* pJob);
MA_API ma_result ma_device_job_thread_next(ma_device_job_thread* pJobThread, ma_job* pJob);



/* Device notification types. */
typedef enum
{
    ma_device_notification_type_started,
    ma_device_notification_type_stopped,
    ma_device_notification_type_rerouted,
    ma_device_notification_type_interruption_began,
    ma_device_notification_type_interruption_ended,
    ma_device_notification_type_unlocked
} ma_device_notification_type;

typedef struct
{
    ma_device* pDevice;
    ma_device_notification_type type;
    union
    {
        struct
        {
            int _unused;
        } started;
        struct
        {
            int _unused;
        } stopped;
        struct
        {
            int _unused;
        } rerouted;
        struct
        {
            int _unused;
        } interruption;
    } data;
} ma_device_notification;

/*
The notification callback for when the application should be notified of a change to the device.

This callback is used for notifying the application of changes such as when the device has started,
stopped, rerouted or an interruption has occurred. Note that not all backends will post all
notification types. For example, some backends will perform automatic stream routing without any
kind of notification to the host program which means miniaudio will never know about it and will
never be able to fire the rerouted notification. You should keep this in mind when designing your
program.

The stopped notification will *not* get fired when a device is rerouted.


Parameters
----------
pNotification (in)
    A pointer to a structure containing information about the event. Use the `pDevice` member of
    this object to retrieve the relevant device. The `type` member can be used to discriminate
    against each of the notification types.


Remarks
-------
Do not restart or uninitialize the device from the callback.

Not all notifications will be triggered by all backends, however the started and stopped events
should be reliable for all backends. Some backends do not have a good way to detect device
stoppages due to unplugging the device which may result in the stopped callback not getting
fired. This has been observed with at least one BSD variant.

The rerouted notification is fired *after* the reroute has occurred. The stopped notification will
*not* get fired when a device is rerouted. The following backends are known to do automatic stream
rerouting, but do not have a way to be notified of the change:

  * DirectSound

The interruption notifications are used on mobile platforms for detecting when audio is interrupted
due to things like an incoming phone call. Currently this is only implemented on iOS. None of the
Android backends will report this notification.
*/
typedef void (* ma_device_notification_proc)(const ma_device_notification* pNotification);


/*
The callback for processing audio data from the device.

The data callback is fired by miniaudio whenever the device needs to have more data delivered to a playback device, or when a capture device has some data
available. This is called as soon as the backend asks for more data which means it may be called with inconsistent frame counts. You cannot assume the
callback will be fired with a consistent frame count.


Parameters
----------
pDevice (in)
    A pointer to the relevant device.

pOutput (out)
    A pointer to the output buffer that will receive audio data that will later be played back through the speakers. This will be non-null for a playback or
    full-duplex device and null for a capture and loopback device.

pInput (in)
    A pointer to the buffer containing input data from a recording device. This will be non-null for a capture, full-duplex or loopback device and null for a
    playback device.

frameCount (in)
    The number of PCM frames to process. Note that this will not necessarily be equal to what you requested when you initialized the device. The
    `periodSizeInFrames` and `periodSizeInMilliseconds` members of the device config are just hints, and are not necessarily exactly what you'll get. You must
    not assume this will always be the same value each time the callback is fired.


Remarks
-------
You cannot stop and start the device from inside the callback or else you'll get a deadlock. You must also not uninitialize the device from inside the
callback. The following APIs cannot be called from inside the callback:

    ma_device_init()
    ma_device_init_ex()
    ma_device_uninit()
    ma_device_start()
    ma_device_stop()

The proper way to stop the device is to call `ma_device_stop()` from a different thread, normally the main application thread.
*/
typedef void (* ma_device_data_proc)(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount);




/*
DEPRECATED. Use ma_device_notification_proc instead.

The callback for when the device has been stopped.

This will be called when the device is stopped explicitly with `ma_device_stop()` and also called implicitly when the device is stopped through external forces
such as being unplugged or an internal error occurring.


Parameters
----------
pDevice (in)
    A pointer to the device that has just stopped.


Remarks
-------
Do not restart or uninitialize the device from the callback.
*/
typedef void (* ma_stop_proc)(ma_device* pDevice);  /* DEPRECATED. Use ma_device_notification_proc instead. */

typedef enum
{
    ma_device_type_playback = 1,
    ma_device_type_capture  = 2,
    ma_device_type_duplex   = ma_device_type_playback | ma_device_type_capture, /* 3 */
    ma_device_type_loopback = 4
} ma_device_type;

typedef enum
{
    ma_share_mode_shared = 0,
    ma_share_mode_exclusive
} ma_share_mode;

/* iOS/tvOS/watchOS session categories. */
typedef enum
{
    ma_ios_session_category_default = 0,        /* AVAudioSessionCategoryPlayAndRecord. */
    ma_ios_session_category_none,               /* Leave the session category unchanged. */
    ma_ios_session_category_ambient,            /* AVAudioSessionCategoryAmbient */
    ma_ios_session_category_solo_ambient,       /* AVAudioSessionCategorySoloAmbient */
    ma_ios_session_category_playback,           /* AVAudioSessionCategoryPlayback */
    ma_ios_session_category_record,             /* AVAudioSessionCategoryRecord */
    ma_ios_session_category_play_and_record,    /* AVAudioSessionCategoryPlayAndRecord */
    ma_ios_session_category_multi_route         /* AVAudioSessionCategoryMultiRoute */
} ma_ios_session_category;

/* iOS/tvOS/watchOS session category options */
typedef enum
{
    ma_ios_session_category_option_mix_with_others                            = 0x01,   /* AVAudioSessionCategoryOptionMixWithOthers */
    ma_ios_session_category_option_duck_others                                = 0x02,   /* AVAudioSessionCategoryOptionDuckOthers */
    ma_ios_session_category_option_allow_bluetooth                            = 0x04,   /* AVAudioSessionCategoryOptionAllowBluetooth */
    ma_ios_session_category_option_default_to_speaker                         = 0x08,   /* AVAudioSessionCategoryOptionDefaultToSpeaker */
    ma_ios_session_category_option_interrupt_spoken_audio_and_mix_with_others = 0x11,   /* AVAudioSessionCategoryOptionInterruptSpokenAudioAndMixWithOthers */
    ma_ios_session_category_option_allow_bluetooth_a2dp                       = 0x20,   /* AVAudioSessionCategoryOptionAllowBluetoothA2DP */
    ma_ios_session_category_option_allow_air_play                             = 0x40,   /* AVAudioSessionCategoryOptionAllowAirPlay */
} ma_ios_session_category_option;

/* OpenSL stream types. */
typedef enum
{
    ma_opensl_stream_type_default = 0,              /* Leaves the stream type unset. */
    ma_opensl_stream_type_voice,                    /* SL_ANDROID_STREAM_VOICE */
    ma_opensl_stream_type_system,                   /* SL_ANDROID_STREAM_SYSTEM */
    ma_opensl_stream_type_ring,                     /* SL_ANDROID_STREAM_RING */
    ma_opensl_stream_type_media,                    /* SL_ANDROID_STREAM_MEDIA */
    ma_opensl_stream_type_alarm,                    /* SL_ANDROID_STREAM_ALARM */
    ma_opensl_stream_type_notification              /* SL_ANDROID_STREAM_NOTIFICATION */
} ma_opensl_stream_type;

/* OpenSL recording presets. */
typedef enum
{
    ma_opensl_recording_preset_default = 0,         /* Leaves the input preset unset. */
    ma_opensl_recording_preset_generic,             /* SL_ANDROID_RECORDING_PRESET_GENERIC */
    ma_opensl_recording_preset_camcorder,           /* SL_ANDROID_RECORDING_PRESET_CAMCORDER */
    ma_opensl_recording_preset_voice_recognition,   /* SL_ANDROID_RECORDING_PRESET_VOICE_RECOGNITION */
    ma_opensl_recording_preset_voice_communication, /* SL_ANDROID_RECORDING_PRESET_VOICE_COMMUNICATION */
    ma_opensl_recording_preset_voice_unprocessed    /* SL_ANDROID_RECORDING_PRESET_UNPROCESSED */
} ma_opensl_recording_preset;

/* WASAPI audio thread priority characteristics. */
typedef enum
{
    ma_wasapi_usage_default = 0,
    ma_wasapi_usage_games,
    ma_wasapi_usage_pro_audio,
} ma_wasapi_usage;

/* AAudio usage types. */
typedef enum
{
    ma_aaudio_usage_default = 0,                    /* Leaves the usage type unset. */
    ma_aaudio_usage_media,                          /* AAUDIO_USAGE_MEDIA */
    ma_aaudio_usage_voice_communication,            /* AAUDIO_USAGE_VOICE_COMMUNICATION */
    ma_aaudio_usage_voice_communication_signalling, /* AAUDIO_USAGE_VOICE_COMMUNICATION_SIGNALLING */
    ma_aaudio_usage_alarm,                          /* AAUDIO_USAGE_ALARM */
    ma_aaudio_usage_notification,                   /* AAUDIO_USAGE_NOTIFICATION */
    ma_aaudio_usage_notification_ringtone,          /* AAUDIO_USAGE_NOTIFICATION_RINGTONE */
    ma_aaudio_usage_notification_event,             /* AAUDIO_USAGE_NOTIFICATION_EVENT */
    ma_aaudio_usage_assistance_accessibility,       /* AAUDIO_USAGE_ASSISTANCE_ACCESSIBILITY */
    ma_aaudio_usage_assistance_navigation_guidance, /* AAUDIO_USAGE_ASSISTANCE_NAVIGATION_GUIDANCE */
    ma_aaudio_usage_assistance_sonification,        /* AAUDIO_USAGE_ASSISTANCE_SONIFICATION */
    ma_aaudio_usage_game,                           /* AAUDIO_USAGE_GAME */
    ma_aaudio_usage_assitant,                       /* AAUDIO_USAGE_ASSISTANT */
    ma_aaudio_usage_emergency,                      /* AAUDIO_SYSTEM_USAGE_EMERGENCY */
    ma_aaudio_usage_safety,                         /* AAUDIO_SYSTEM_USAGE_SAFETY */
    ma_aaudio_usage_vehicle_status,                 /* AAUDIO_SYSTEM_USAGE_VEHICLE_STATUS */
    ma_aaudio_usage_announcement                    /* AAUDIO_SYSTEM_USAGE_ANNOUNCEMENT */
} ma_aaudio_usage;

/* AAudio content types. */
typedef enum
{
    ma_aaudio_content_type_default = 0,             /* Leaves the content type unset. */
    ma_aaudio_content_type_speech,                  /* AAUDIO_CONTENT_TYPE_SPEECH */
    ma_aaudio_content_type_music,                   /* AAUDIO_CONTENT_TYPE_MUSIC */
    ma_aaudio_content_type_movie,                   /* AAUDIO_CONTENT_TYPE_MOVIE */
    ma_aaudio_content_type_sonification             /* AAUDIO_CONTENT_TYPE_SONIFICATION */
} ma_aaudio_content_type;

/* AAudio input presets. */
typedef enum
{
    ma_aaudio_input_preset_default = 0,             /* Leaves the input preset unset. */
    ma_aaudio_input_preset_generic,                 /* AAUDIO_INPUT_PRESET_GENERIC */
    ma_aaudio_input_preset_camcorder,               /* AAUDIO_INPUT_PRESET_CAMCORDER */
    ma_aaudio_input_preset_voice_recognition,       /* AAUDIO_INPUT_PRESET_VOICE_RECOGNITION */
    ma_aaudio_input_preset_voice_communication,     /* AAUDIO_INPUT_PRESET_VOICE_COMMUNICATION */
    ma_aaudio_input_preset_unprocessed,             /* AAUDIO_INPUT_PRESET_UNPROCESSED */
    ma_aaudio_input_preset_voice_performance        /* AAUDIO_INPUT_PRESET_VOICE_PERFORMANCE */
} ma_aaudio_input_preset;

typedef enum
{
    ma_aaudio_allow_capture_default = 0,            /* Leaves the allowed capture policy unset. */
    ma_aaudio_allow_capture_by_all,                 /* AAUDIO_ALLOW_CAPTURE_BY_ALL */
    ma_aaudio_allow_capture_by_system,              /* AAUDIO_ALLOW_CAPTURE_BY_SYSTEM */
    ma_aaudio_allow_capture_by_none                 /* AAUDIO_ALLOW_CAPTURE_BY_NONE */
} ma_aaudio_allowed_capture_policy;

typedef union
{
    ma_int64 counter;
    double counterD;
} ma_timer;

typedef union
{
    ma_wchar_win32 wasapi[64];      /* WASAPI uses a wchar_t string for identification. */
    ma_uint8 dsound[16];            /* DirectSound uses a GUID for identification. */
    /*UINT_PTR*/ ma_uint32 winmm;   /* When creating a device, WinMM expects a Win32 UINT_PTR for device identification. In practice it's actually just a UINT. */
    char alsa[256];                 /* ALSA uses a name string for identification. */
    char pulse[256];                /* PulseAudio uses a name string for identification. */
    int jack;                       /* JACK always uses default devices. */
    char coreaudio[256];            /* Core Audio uses a string for identification. */
    char sndio[256];                /* "snd/0", etc. */
    char audio4[256];               /* "/dev/audio", etc. */
    char oss[64];                   /* "dev/dsp0", etc. "dev/dsp" for the default device. */
    ma_int32 aaudio;                /* AAudio uses a 32-bit integer for identification. */
    ma_uint32 opensl;               /* OpenSL|ES uses a 32-bit unsigned integer for identification. */
    char webaudio[32];              /* Web Audio always uses default devices for now, but if this changes it'll be a GUID. */
    union
    {
        int i;
        char s[256];
        void* p;
    } custom;                       /* The custom backend could be anything. Give them a few options. */
    int nullbackend;                /* The null backend uses an integer for device IDs. */
} ma_device_id;

MA_API ma_bool32 ma_device_id_equal(const ma_device_id* pA, const ma_device_id* pB);


typedef struct ma_context_config    ma_context_config;
typedef struct ma_device_config     ma_device_config;
typedef struct ma_backend_callbacks ma_backend_callbacks;

#define MA_DATA_FORMAT_FLAG_EXCLUSIVE_MODE (1U << 1)    /* If set, this is supported in exclusive mode. Otherwise not natively supported by exclusive mode. */

#ifndef MA_MAX_DEVICE_NAME_LENGTH
#define MA_MAX_DEVICE_NAME_LENGTH   255
#endif

typedef struct
{
    /* Basic info. This is the only information guaranteed to be filled in during device enumeration. */
    ma_device_id id;
    char name[MA_MAX_DEVICE_NAME_LENGTH + 1];   /* +1 for null terminator. */
    ma_bool32 isDefault;

    ma_uint32 nativeDataFormatCount;
    struct
    {
        ma_format format;       /* Sample format. If set to ma_format_unknown, all sample formats are supported. */
        ma_uint32 channels;     /* If set to 0, all channels are supported. */
        ma_uint32 sampleRate;   /* If set to 0, all sample rates are supported. */
        ma_uint32 flags;        /* A combination of MA_DATA_FORMAT_FLAG_* flags. */
    } nativeDataFormats[/*ma_format_count * ma_standard_sample_rate_count * MA_MAX_CHANNELS*/ 64];  /* Not sure how big to make this. There can be *many* permutations for virtual devices which can support anything. */
} ma_device_info;

struct ma_device_config
{
    ma_device_type deviceType;
    ma_uint32 sampleRate;
    ma_uint32 periodSizeInFrames;
    ma_uint32 periodSizeInMilliseconds;
    ma_uint32 periods;
    ma_performance_profile performanceProfile;
    ma_bool8 noPreSilencedOutputBuffer; /* When set to true, the contents of the output buffer passed into the data callback will be left undefined rather than initialized to silence. */
    ma_bool8 noClip;                    /* When set to true, the contents of the output buffer passed into the data callback will not be clipped after returning. Only applies when the playback sample format is f32. */
    ma_bool8 noDisableDenormals;        /* Do not disable denormals when firing the data callback. */
    ma_bool8 noFixedSizedCallback;      /* Disables strict fixed-sized data callbacks. Setting this to true will result in the period size being treated only as a hint to the backend. This is an optimization for those who don't need fixed sized callbacks. */
    ma_device_data_proc dataCallback;
    ma_device_notification_proc notificationCallback;
    ma_stop_proc stopCallback;
    void* pUserData;
    ma_resampler_config resampling;
    struct
    {
        const ma_device_id* pDeviceID;
        ma_format format;
        ma_uint32 channels;
        ma_channel* pChannelMap;
        ma_channel_mix_mode channelMixMode;
        ma_bool32 calculateLFEFromSpatialChannels;  /* When an output LFE channel is present, but no input LFE, set to true to set the output LFE to the average of all spatial channels (LR, FR, etc.). Ignored when an input LFE is present. */
        ma_share_mode shareMode;
    } playback;
    struct
    {
        const ma_device_id* pDeviceID;
        ma_format format;
        ma_uint32 channels;
        ma_channel* pChannelMap;
        ma_channel_mix_mode channelMixMode;
        ma_bool32 calculateLFEFromSpatialChannels;  /* When an output LFE channel is present, but no input LFE, set to true to set the output LFE to the average of all spatial channels (LR, FR, etc.). Ignored when an input LFE is present. */
        ma_share_mode shareMode;
    } capture;

    struct
    {
        ma_wasapi_usage usage;              /* When configured, uses Avrt APIs to set the thread characteristics. */
        ma_bool8 noAutoConvertSRC;          /* When set to true, disables the use of AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM. */
        ma_bool8 noDefaultQualitySRC;       /* When set to true, disables the use of AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY. */
        ma_bool8 noAutoStreamRouting;       /* Disables automatic stream routing. */
        ma_bool8 noHardwareOffloading;      /* Disables WASAPI's hardware offloading feature. */
        ma_uint32 loopbackProcessID;        /* The process ID to include or exclude for loopback mode. Set to 0 to capture audio from all processes. Ignored when an explicit device ID is specified. */
        ma_bool8 loopbackProcessExclude;    /* When set to true, excludes the process specified by loopbackProcessID. By default, the process will be included. */
    } wasapi;
    struct
    {
        ma_bool32 noMMap;           /* Disables MMap mode. */
        ma_bool32 noAutoFormat;     /* Opens the ALSA device with SND_PCM_NO_AUTO_FORMAT. */
        ma_bool32 noAutoChannels;   /* Opens the ALSA device with SND_PCM_NO_AUTO_CHANNELS. */
        ma_bool32 noAutoResample;   /* Opens the ALSA device with SND_PCM_NO_AUTO_RESAMPLE. */
    } alsa;
    struct
    {
        const char* pStreamNamePlayback;
        const char* pStreamNameCapture;
        int channelMap;
    } pulse;
    struct
    {
        ma_bool32 allowNominalSampleRateChange; /* Desktop only. When enabled, allows changing of the sample rate at the operating system level. */
    } coreaudio;
    struct
    {
        ma_opensl_stream_type streamType;
        ma_opensl_recording_preset recordingPreset;
        ma_bool32 enableCompatibilityWorkarounds;
    } opensl;
    struct
    {
        ma_aaudio_usage usage;
        ma_aaudio_content_type contentType;
        ma_aaudio_input_preset inputPreset;
        ma_aaudio_allowed_capture_policy allowedCapturePolicy;
        ma_bool32 noAutoStartAfterReroute;
        ma_bool32 enableCompatibilityWorkarounds;
        ma_bool32 allowSetBufferCapacity;
    } aaudio;
};


/*
The callback for handling device enumeration. This is fired from `ma_context_enumerate_devices()`.


Parameters
----------
pContext (in)
    A pointer to the context performing the enumeration.

deviceType (in)
    The type of the device being enumerated. This will always be either `ma_device_type_playback` or `ma_device_type_capture`.

pInfo (in)
    A pointer to a `ma_device_info` containing the ID and name of the enumerated device. Note that this will not include detailed information about the device,
    only basic information (ID and name). The reason for this is that it would otherwise require opening the backend device to probe for the information which
    is too inefficient.

pUserData (in)
    The user data pointer passed into `ma_context_enumerate_devices()`.
*/
typedef ma_bool32 (* ma_enum_devices_callback_proc)(ma_context* pContext, ma_device_type deviceType, const ma_device_info* pInfo, void* pUserData);


/*
Describes some basic details about a playback or capture device.
*/
typedef struct
{
    const ma_device_id* pDeviceID;
    ma_share_mode shareMode;
    ma_format format;
    ma_uint32 channels;
    ma_uint32 sampleRate;
    ma_channel channelMap[MA_MAX_CHANNELS];
    ma_uint32 periodSizeInFrames;
    ma_uint32 periodSizeInMilliseconds;
    ma_uint32 periodCount;
} ma_device_descriptor;

/*
These are the callbacks required to be implemented for a backend. These callbacks are grouped into two parts: context and device. There is one context
to many devices. A device is created from a context.

The general flow goes like this:

  1) A context is created with `onContextInit()`
     1a) Available devices can be enumerated with `onContextEnumerateDevices()` if required.
     1b) Detailed information about a device can be queried with `onContextGetDeviceInfo()` if required.
  2) A device is created from the context that was created in the first step using `onDeviceInit()`, and optionally a device ID that was
     selected from device enumeration via `onContextEnumerateDevices()`.
  3) A device is started or stopped with `onDeviceStart()` / `onDeviceStop()`
  4) Data is delivered to and from the device by the backend. This is always done based on the native format returned by the prior call
     to `onDeviceInit()`. Conversion between the device's native format and the format requested by the application will be handled by
     miniaudio internally.

Initialization of the context is quite simple. You need to do any necessary initialization of internal objects and then output the
callbacks defined in this structure.

Once the context has been initialized you can initialize a device. Before doing so, however, the application may want to know which
physical devices are available. This is where `onContextEnumerateDevices()` comes in. This is fairly simple. For each device, fire the
given callback with, at a minimum, the basic information filled out in `ma_device_info`. When the callback returns `MA_FALSE`, enumeration
needs to stop and the `onContextEnumerateDevices()` function returns with a success code.

Detailed device information can be retrieved from a device ID using `onContextGetDeviceInfo()`. This takes as input the device type and ID,
and on output returns detailed information about the device in `ma_device_info`. The `onContextGetDeviceInfo()` callback must handle the
case when the device ID is NULL, in which case information about the default device needs to be retrieved.

Once the context has been created and the device ID retrieved (i
