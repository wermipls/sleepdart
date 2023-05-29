project('sleepdart', 'c', default_options: ['buildtype=release'])

release_args = ['-ffast-math', '-flto', '-fwhole-program']
if get_option('buildtype') == 'release'
  add_project_arguments(release_args, language: 'c')
  add_project_link_arguments(release_args, language: 'c')
endif

includes = include_directories('microui/src')

extra_src = []
extra_obj = []
static_libs = false

if target_machine.system() == 'windows'
  windres = find_program('windres')
  run_command(windres, 'src/win32/resource.rc', 'src/win32/resource.o')
  extra_src = [
    'src/win32/gui_windows.c',
  ]
  extra_obj = [
    'src/win32/resource.o',
  ]
  add_project_arguments('-DPLATFORM_WIN32', language: 'c')
  static_libs = true
endif

git = find_program('git', required: false)
if git.found()
  gitargs = ['describe', '--tags', '--dirty', '--always']
  describe = run_command(git, gitargs, check: false).stdout().strip()
  if describe != ''
    add_project_arguments('-DGIT_DESCRIBE="' + describe + '"', language: 'c')
  endif
endif

deps = [
  dependency('zlib', static: static_libs),
  dependency('sdl2', static: static_libs),
  dependency('libxxhash', static: static_libs),
  dependency('freetype2', static: static_libs),
]

add_project_link_arguments('-lstdc++', language: 'c')

exe = executable(
  'sleepdart',
  'src/main.c',
  'src/argparser.c',
  'src/audio_sdl.c',
  'src/ay.c',
  'src/beeper.c',
  'src/config.c',
  'src/config_parser.c',
  'src/debugger.c',
  'src/disasm.c',
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
  'src/microui_render.c',
  'src/palette.c',
  'src/parser_helpers.c',
  'src/szx_file.c',
  'src/szx_state.c',
  'src/tape.c',
  'src/ula.c',
  'src/video_sdl.c',
  'src/z80.c',
  'microui/src/microui.c',
  'ayumi/ayumi.c',
  extra_src,
  include_directories: includes,
  dependencies: deps,
  objects: extra_obj,
  win_subsystem: 'windows',
  link_args: '-lm',
)

# a lil hacky...
dest = meson.project_source_root()
custom_target(
  'finalize',
  depends: exe,
  input: exe,
  output: 'fake',
  command: ['cp', '@INPUT@', dest],
  build_by_default : true)
