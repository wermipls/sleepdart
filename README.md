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
pacman -S git mingw-w64-x86_64-meson mingw-w64-x86_64-pkgconf mingw-w64-x86_64-binutils mingw-w64-x86_64-gcc mingw-w64-x86_64-sdl3 zlib mingw-w64-x86_64-xxhash

meson setup build # add "-Dbuildtype=debug" for a debug build
meson compile -C build
```

## License

sleepdart is licensed under [0-clause BSD](LICENSE).

The program makes use of several third-party libraries, in part or in whole:
- **[ayumi](https://github.com/true-grue/ayumi)**, (MIT, see `ayumi/LICENSE`)
- **[Convenient Containers (CC)](https://github.com/JacksonAllan/CC)**, (MIT, see `external/cc.h`)
- **[PhysicsFS](https://icculus.org/physfs/)** (zlib)
- **[rouziclib](https://github.com/Photosounder/rouziclib)** (MIT, see `src/unicode.c`)
- **[SDL3](https://libsdl.org/)** (zlib)
- **[stb_image](https://github.com/nothings/stb/blob/master/stb_image.h)** (public domain or MIT, see `external/stb_image.h`)
- **[stb_image_write](https://github.com/nothings/stb/blob/master/stb_image_write.h)** (public domain or MIT, see `external/stb_image_write.h`)
- **[xxHash](https://xxhash.com/)** (2-clause BSD)

This emulator bundles the unmodified ZX Spectrum ROMs. Amstrad have kindly given their permission to redistribute the Spectrum ROMs, as long as the original copyright notices are unchanged; see [Cliff Lawson's post on Usenet](https://groups.google.com/g/comp.sys.amstrad.8bit/c/HtpBU2Bzv_U/m/HhNDSU3MksAJ).

Binaries in the `tests` directory are subject to their own distribution terms, some of them unspecified; look at text files in the respective test directories for any licensing information.
