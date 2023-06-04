project('sleepdart', 'c', default_options: ['buildtype=release'])

release_args = ['-ffast-math', '-flto', '-fwhole-program']
if get_option('buildtype') == 'release'
  add_project_arguments(release_args, language: 'c')
  add_project_link_arguments(release_args, language: 'c')
endif

extra_src = []
extra_obj = []
static_libs = false

if target_machine.system() == 'windows'
  winmod = import('windows')
  extra_src = [
    'src/win32/gui_windows.c', 
    winmod.compile_resources('src/win32/resource.rc')
  ]
  add_project_arguments('-DPLATFORM_WIN32', language: 'c')
  static_libs = true
endif

# workaround for ancient version of meson in ubuntu-latest repos
# after updating to 0.63, can be replaced with:
#   git = find_program('git')
git = 'git'
sleepdart_info = vcs_tag(
  command: [git, 'describe', '--tags', '--dirty', '--always'],
  input: 'src/sleepdart_info.h.in',
  output: 'sleepdart_info.h')

cc = meson.get_compiler('c')
deps = [
  dependency('zlib', static: static_libs),
  dependency('sdl2', static: static_libs),
  dependency('libxxhash', static: static_libs),
  cc.find_library('m', required: false),
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
  dependencies: deps,
  objects: extra_obj,
  win_subsystem: 'windows',
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
