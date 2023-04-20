#include <SDL2/SDL.h>

static SDL_AudioDeviceID device;

void sound_sdl_init(int sample_rate)
{
    SDL_Init(SDL_INIT_AUDIO);

    SDL_AudioSpec desired = { 0 };
    desired.channels = 2;
    desired.format = AUDIO_F32SYS;
    desired.freq = sample_rate;
    desired.samples = 2048;
    desired.callback = NULL;

    SDL_AudioSpec obtained;
    device = SDL_OpenAudioDevice(NULL, 0, &desired, &obtained, 0);
    SDL_PauseAudioDevice(device, 0);
}

void sound_sdl_queue(float *buf, size_t bytes)
{
    SDL_QueueAudio(device, buf, bytes);
}
