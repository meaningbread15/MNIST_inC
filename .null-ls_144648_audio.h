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
The high level AP
