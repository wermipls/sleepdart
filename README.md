# sleepdart
A half-hearted attempt at writing a reasonably accurate ZX Spectrum emulator.

![sleepdart running Shock](https://user-images.githubusercontent.com/32251376/234389694-45e35ba2-b437-4d82-a1cd-40884a48b6d3.png)

See it in action:
* ["Shock" megademo](https://www.youtube.com/watch?v=ak0xUiLwCu0)
* [DIZZZRUPTOR](https://www.youtube.com/watch?v=YDia18bqKFo)
* [New View 48K](https://www.youtube.com/watch?v=IAeNbatPNpk)

## Features
* Mostly cycle-accurate Z80 emulation, including contention and fetch/write timings
* Sub-frame accuracy for ULA draws
* Tape (.tap file) support with basic hooks for fast/automated loading
* Beeper sound
* AY-3 sound (based on [ayumi](https://github.com/true-grue/ayumi) library)
* Reasonably fast (typically ~3500 FPS uncapped on an i3-4150)
* Keyboard input
* SZX state loading
* Adjustable palettes
* More to come...

## Building

Clone the repository, including submodules:
```
git clone --recursive https://github.com/wermipls/sleepdart
```

### Windows (MSYS2 MinGW 64-bit)

```sh
pacman -S git mingw-w64-x86_64-meson mingw-w64-x86_64-pkgconf mingw-w64-x86_64-binutils mingw-w64-x86_64-gcc mingw-w64-x86_64-SDL2 zlib mingw-w64-x86_64-xxhash

meson setup build # add "-Dbuildtype=debug" for a debug build
meson compile -C build
```
