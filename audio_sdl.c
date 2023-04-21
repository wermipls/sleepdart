#include "audio_sdl.h"
#include <SDL2/SDL.h>

static SDL_AudioDeviceID device;
static SDL_AudioSpec device_spec;

void audio_sdl_init(int sample_rate)
{
    SDL_Init(SDL_INIT_AUDIO);

    SDL_AudioSpec desired = { 0 };
    desired.channels = 2;
    desired.format = AUDIO_F32SYS;
    desired.freq = sample_rate;
    desired.samples = 2048;
    desired.callback = NULL;

    device = SDL_OpenAudioDevice(NULL, 0, &desired, &device_spec, 0);
    SDL_PauseAudioDevice(device, 0);
}

void audio_sdl_queue(float *buf, size_t bytes)
{
    uint32_t queue = SDL_GetQueuedAudioSize(device);
    // data too old (uncapped fps? a/v desync?), drop it
    if (queue > device_spec.size*2) {
        SDL_ClearQueuedAudio(device);
    }

    SDL_QueueAudio(device, buf, bytes);
}
