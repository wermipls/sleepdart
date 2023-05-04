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

### Windows

Use MSYS2 (MinGW 64-bit) with following packages installed:
* `mingw-w64-x86_64-SDL2`
* `zlib`

```sh
# -d parameter produces a build optimized for debugging (-Og, gdb symbols)
# -p <platform> includes platform-specific features (win32 ui in this case)

./build.sh -p win32
```

### Fedora 38

```sh
sudo dnf install gcc SDL2-devel zlib-devel
./build.sh
```
