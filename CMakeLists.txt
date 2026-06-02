cmake_minimum_required(VERSION 3.14 FATAL_ERROR)

project(sleepdart LANGUAGES C VERSION "0.1.0")

if(NOT PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
    file(GENERATE OUTPUT .gitignore CONTENT "*")
endif()

get_property(multiconfig GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(NOT CMAKE_BUILD_TYPE AND NOT multiconfig)
    message(STATUS "No build type selected, using Release.")
    set(CMAKE_BUILD_TYPE "Release")
endif()

# make FetchContent not scream at me.
cmake_policy(SET CMP0135 NEW)

set(CMAKE_C_STANDARD 11)
set(BUILD_SHARED_LIBS OFF)

option(SLEEPDART_LTO "Enable LTO for release builds" ON)

if(CMAKE_C_COMPILER_ID MATCHES "Clang" AND NOT CMAKE_C_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    # enable LTO for release builds.
    if (CMAKE_BUILD_TYPE MATCHES "Rel" AND SLEEPDART_LTO)
        add_compile_options(-flto=thin)
        add_link_options(-flto=thin)
    endif()
    # those are more-or-less common with GCC.
    add_compile_options(-fdiagnostics-color=always)
    add_link_options(-fdiagnostics-color=always)
    if(WIN32)
        add_compile_options(-gcodeview)
        add_link_options(-Wl,--pdb=)
        add_link_options(-static -static-libgcc)
    endif()
endif()
if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
    # enable LTO for release builds.
    if (CMAKE_BUILD_TYPE MATCHES "Rel" AND SLEEPDART_LTO)
        add_compile_options(-flto)
        add_link_options(-flto)
    endif()
    # those are more-or-less common with Clang. a little copy-paste hasn't hurt anybody yet.
    add_compile_options(-fdiagnostics-color=always)
    add_link_options(-fdiagnostics-color=always)
    if(WIN32)
        add_compile_options(-gcodeview)
        add_link_options(-Wl,--pdb=)
        add_link_options(-static -static-libgcc)
    endif()
endif()

add_executable(sleepdart WIN32
    src/main.c
    src/argparser.c
    src/audio_sdl.c
    src/ay.c
    src/beeper.c
    src/config.c
    src/config_parser.c
    src/dsp.c
    src/file.c
    src/hotkeys.c
    src/input_sdl.c
    src/io.c
    src/keyboard.c
    src/keyboard_macro.c
    src/log.c
    src/machine.c
    src/machine_hooks.c
    src/machine_test.c
    src/memory.c
    src/palette.c
    src/parser_helpers.c
    src/sna.c
    src/szx_file.c
    src/szx_state.c
    src/tape.c
    src/ula.c
    src/unicode.c
    src/video_sdl.c
    src/z80.c
    ayumi/ayumi.c
    sleepdart_info.h
    sleepdart_assets.h
)

# we don't want to litter output with warnings from other libs, so this is done here.
if(CMAKE_C_COMPILER_ID STREQUAL "MSVC" OR CMAKE_C_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    target_compile_options(sleepdart PRIVATE /W3)
elseif(CMAKE_C_COMPILER_ID STREQUAL "GNU" OR CMAKE_C_COMPILER_ID MATCHES "Clang")
    target_compile_options(sleepdart PRIVATE -Wall -Wextra)
endif()

# make sure generated files are accessible.
target_include_directories(sleepdart PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")

# sleepdart_info.h
find_package(Git)
if(Git_FOUND)
  execute_process(
    COMMAND ${GIT_EXECUTABLE} -C "${CMAKE_CURRENT_SOURCE_DIR}" describe --tags --dirty --always
    OUTPUT_VARIABLE output
    RESULT_VARIABLE result
    ERROR_QUIET
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  if(result EQUAL 0)
    set(SLEEPDART_VERSION "${output}")
  endif()
endif()
if(NOT SLEEPDART_VERSION)
    set(SLEEPDART_VERSION "${CMAKE_PROJECT_VERSION}")
    message(WARNING "failed to get git tag, falling back to ${SLEEPDART_VERSION}")
endif()
configure_file("src/sleepdart_info.h.in" "sleepdart_info.h" @ONLY)

# sleepdart_assets.h
find_package(Python3)
if(Python3_FOUND)
    set(asset_files
        ${CMAKE_CURRENT_SOURCE_DIR}/rom/48.rom
        ${CMAKE_CURRENT_SOURCE_DIR}/palettes/default.raw
        ${CMAKE_CURRENT_SOURCE_DIR}/palettes/loni.raw
        ${CMAKE_CURRENT_SOURCE_DIR}/palettes/sleepdart.raw
        ${CMAKE_CURRENT_SOURCE_DIR}/palettes/spectaculator.raw
        ${CMAKE_CURRENT_SOURCE_DIR}/palettes/washed.raw
    )
    add_custom_command(
        OUTPUT sleepdart_assets.h
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/pack_assets.py sleepdart_assets.h ${CMAKE_CURRENT_SOURCE_DIR} ${asset_files}
        DEPENDS ${asset_files}
        VERBATIM
    )
else()
    message(FATAL_ERROR "Python not found, bailing out.")
endif()

# platform-specific code.
if(WIN32)
    target_compile_definitions(sleepdart PRIVATE PLATFORM_WIN32)
    target_sources(sleepdart PRIVATE
        src/win32/gui_windows.c
        src/win32/resource.rc
    )
endif()

# tests.
include(CTest)
function(sleepdart_test test_name)
    add_test(
        NAME "sleepdart_${test_name}"
        COMMAND sleepdart --headless --test "${CMAKE_CURRENT_SOURCE_DIR}/tests/${test_name}"
    )
    set_property(
        TEST "sleepdart_${test_name}"
        PROPERTY ENVIRONMENT "SDL_VIDEODRIVER=dummy;SLEEPDART_ERRSILENT=1"
    )
endfunction()

sleepdart_test(48k_nec_contention)
sleepdart_test(butler_timingtests_48k)
sleepdart_test(floatspy)
sleepdart_test(halt2int)
sleepdart_test(timingtest)
sleepdart_test(ula48simple)
sleepdart_test(z80ccf)
sleepdart_test(z80doc)
sleepdart_test(z80full)
sleepdart_test(z80memptr)

# dependencies.
# FIXME: let user disable vendoring of libraries.
include(FetchContent)

# SDL3
FetchContent_Declare(
    SDL3
    URL      https://github.com/libsdl-org/SDL/releases/download/release-3.4.10/SDL3-3.4.10.zip
    URL_HASH SHA256=be8dafef3ecc956e736cfd1c242c10737f3ad589a866f25ba437b7f2f6d5ced2
)
option(SDL_SHARED "" OFF)
option(SDL_STATIC "" ON)
FetchContent_MakeAvailable(SDL3)
target_link_libraries(sleepdart SDL3::SDL3)

# PhysFS
FetchContent_Declare(
    PhysFS
    GIT_REPOSITORY https://github.com/icculus/physfs/
    GIT_TAG        5f73e95b20bf3269b4b93d455923d33dd66ed039 # tip of main as of 2026-06-01
)
option(PHYSFS_ARCHIVE_ZIP "" ON)
option(PHYSFS_ARCHIVE_7Z "" OFF)
option(PHYSFS_ARCHIVE_GRP "" OFF)
option(PHYSFS_ARCHIVE_WAD "" OFF)
option(PHYSFS_ARCHIVE_CSM "" OFF)
option(PHYSFS_ARCHIVE_HOG "" OFF)
option(PHYSFS_ARCHIVE_ISO9660 "" OFF)
option(PHYSFS_ARCHIVE_LECARCHIVES "" OFF)
option(PHYSFS_ARCHIVE_MVL "" OFF)
option(PHYSFS_ARCHIVE_POD "" OFF)
option(PHYSFS_ARCHIVE_QPAK "" OFF)
option(PHYSFS_ARCHIVE_ROFS "" OFF)
option(PHYSFS_ARCHIVE_SLB "" OFF)
option(PHYSFS_ARCHIVE_VDF "" OFF)
option(PHYSFS_BUILD_STATIC "" ON)
option(PHYSFS_BUILD_SHARED "" OFF)
option(PHYSFS_BUILD_TEST "" OFF)
option(PHYSFS_BUILD_DOCS "" OFF)
option(PHYSFS_INSTALL "" OFF)
set(PHYSFS_TARGETNAME_UNINSTALL "physfs_uninstall" CACHE STRING "")
FetchContent_MakeAvailable(PhysFS)
target_link_libraries(sleepdart PhysFS::PhysFS-static)

# zlib
FetchContent_Declare(
    zlib
    URL      https://www.zlib.net/zlib132.zip
    URL_HASH SHA256=e8bf55f3017aa181690990cb58a994e77885da140609fc8f94abe9b65d2cae28
)
option(ZLIB_BUILD_SHARED "" OFF)
option(ZLIB_BUILD_TESTING "" OFF)
option(ZLIB_INSTALL "" OFF)
FetchContent_MakeAvailable(zlib)
target_link_libraries(sleepdart ZLIB::ZLIBSTATIC)

# rest of the single header stuff.
target_include_directories(sleepdart PRIVATE external)
