project('sleepdart', 'c', default_options: [
  'buildtype=release',
  'default_library=static',
  'c_std=gnu11',
])

clang = meson.get_compiler('c').get_id() == 'clang'
gcc = meson.get_compiler('c').get_id() == 'gcc'
msvc = meson.get_compiler('c').get_id() == 'msvc'

extra_src = []
extra_obj = []

release_args = []

if clang
  release_args += '-flto=thin'
endif

if gcc
  release_args += '-flto'
  release_args += '-fwhole-program'
endif

if get_option('buildtype') == 'release'
  add_global_arguments(release_args, language: ['c', 'cpp'])
  add_global_link_arguments(release_args, language: ['c', 'cpp'])
endif

if target_machine.system() == 'windows'
  winmod = import('windows')
  extra_src = [
    'src/win32/gui_windows.c', 
    winmod.compile_resources('src/win32/resource.rc')
  ]
  add_project_arguments('-DPLATFORM_WIN32', language: 'c')

  if clang or gcc
    add_project_link_arguments(['-static', '-static-libgcc', '-static-libstdc++'], language: ['c', 'cpp'])
    add_global_arguments('-gcodeview', language: ['c', 'cpp'])
    add_global_link_arguments('-Wl,--pdb=', language: ['c', 'cpp'])
  endif
endif

git = find_program('git')
sleepdart_info = vcs_tag(
  command: [git, 'describe', '--tags', '--dirty', '--always'],
  input: 'src/sleepdart_info.h.in',
  output: 'sleepdart_info.h')

# pack assets for use with physfs and embed the archive in the binary.
# using a python script for this is probably the easiest way to do it
# that is guaranteed to be working in every environment, as meson itself
# is already dependent on python. additionally, we can easily output 
# the data as a C array, avoiding reliance on C23 embed directive.
pack_assets = find_program('pack_assets.py')
asset_files = [
  'rom/48.rom',
  'palettes/default.raw',
  'palettes/loni.raw',
  'palettes/sleepdart.raw',
  'palettes/spectaculator.raw',
  'palettes/washed.raw',
]
sleepdart_assets = custom_target(
  input: asset_files,
  output: 'sleepdart_assets.h',
  command: [pack_assets, '@OUTPUT@', meson.project_source_root(), '@INPUT@']
)

cc = meson.get_compiler('c')
deps = [
  dependency('zlib', static: true),
  dependency('sdl3', static: true),
  dependency('physfs', static: true),
  cc.find_library('m', required: false),
]

exe = executable(
  'sleepdart',
  'src/main.c',
  'src/argparser.c',
  'src/audio_sdl.c',
  'src/ay.c',
  'src/beeper.c',
  'src/config.c',
  'src/config_parser.c',
  'src/dsp.c',
  'src/file.c',
  'src/hotkeys.c',
  'src/input_sdl.c',
  'src/io.c',
  'src/keyboard.c',
  'src/keyboard_macro.c',
  'src/log.c',
  'src/machine.c',
  'src/machine_hooks.c',
  'src/machine_test.c',
  'src/memory.c',
  'src/palette.c',
  'src/parser_helpers.c',
  'src/sna.c',
  'src/szx_file.c',
  'src/szx_state.c',
  'src/tape.c',
  'src/ula.c',
  'src/unicode.c',
  'src/video_sdl.c',
  'src/z80.c',
  'ayumi/ayumi.c',
  extra_src,
  sleepdart_info,
  sleepdart_assets,
  dependencies: deps,
  objects: extra_obj,
  win_subsystem: 'windows',
)

tests = [
  '48k_nec_contention',
  'butler_timingtests_48k',
  'floatspy',
  'halt2int',
  'timingtest',
  'ula48simple',
  'z80ccf',
  'z80doc',
  'z80full',
  'z80memptr',
]

foreach t : tests
  test(
    t,
    exe,
    workdir: meson.current_source_dir(),
    args: ['--headless', '--test', 'tests/' + t],
    env: ['SDL_VIDEODRIVER=dummy', 'SLEEPDART_ERRSILENT=1']
  )
endforeach
