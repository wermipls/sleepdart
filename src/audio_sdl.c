#include "audio_sdl.h"
#include "src/log.h"
#include <SDL3/SDL.h>

static SDL_AudioStream *stream = NULL;

void audio_sdl_init(int sample_rate)
{
    SDL_Init(SDL_INIT_AUDIO);

    SDL_AudioSpec spec = { 0 };
    spec.channels = 2;
    spec.format = SDL_AUDIO_F32;
    spec.freq = sample_rate;

    stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, NULL, NULL);
    if (!stream) {
        dlog(LOG_ERR, "Failed to open audio device: %s", SDL_GetError());
        return;
    }
    SDL_ResumeAudioDevice(SDL_GetAudioStreamDevice(stream));
}

void audio_sdl_queue(float *buf, size_t bytes)
{
    if (!stream) return;

    SDL_AudioSpec spec;
    int frames;
    SDL_GetAudioDeviceFormat(SDL_GetAudioStreamDevice(stream), &spec, &frames);
    uint32_t queued = SDL_GetAudioStreamQueued(stream);
    // data too old (uncapped fps? a/v desync?), drop it
    if (queued < frames * (spec.channels * 4) + bytes) {
        SDL_PutAudioStreamData(stream, buf, bytes);
    }
}
