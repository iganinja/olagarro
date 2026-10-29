cmake_minimum_required(VERSION 3.20)

project(Olagarro VERSION 0.3.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
# Editors (VS Code, clangd) read build/compile_commands.json for include paths and flags.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(MSVC)
    add_compile_options(/W4 /WX)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

find_package(Threads REQUIRED)
include(cmake/OlagarroEmbedFiles.cmake)

option(OLAGARRO_BUILD_TESTS "Build Olagarro's test suite" ON)
option(OLAGARRO_BUILD_EXAMPLES "Build Olagarro examples" ON)
option(OLAGARRO_BUILD_MODULES "Build experimental C++20 module adapters" OFF)
option(OLAGARRO_OUI_WINDOW
    "Build OUI's desktop window backend, on SDL3 linked statically, and let the examples open in a window" OFF)
option(OLAGARRO_OUI_DRM
    "Let the examples take the whole screen through Linux's mode setting (--drm), with no display server" OFF)
option(OLAGARRO_STATIC_LINK
    "Link the example programs fully statically, so they run on Linux systems with an older C and C++ runtime" OFF)
if(OLAGARRO_STATIC_LINK AND NOT MSVC)
    # A static binary carries the whole runtime; drop every function and object nobody references.
    add_compile_options(-ffunction-sections -fdata-sections)
    add_link_options(-Wl,--gc-sections)
    # Not position-independent either, on Linux: a program loaded at a fixed address needs no table of addresses to
    # fix up. A Windows program is not ELF, and the flags mean nothing to it.
    if(NOT WIN32)
        add_compile_options(-fno-pie)
        add_link_options(-no-pie)
    endif()
endif()

function(olagarro_configure_target target)
    target_compile_features(${target} PUBLIC cxx_std_17)
    set_target_properties(${target} PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO)
    target_include_directories(${target}
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
            $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/olagarro>)
endfunction()

# QuickJS-ng, vendored in third_party/quickjs-ng: the JavaScript engine OUI runs scripts with. Its code is
# built as upstream builds it, C11 with unsigned char, and without Olagarro's warnings as errors.
add_library(olagarro_quickjs STATIC
    third_party/quickjs-ng/dtoa.c
    third_party/quickjs-ng/libregexp.c
    third_party/quickjs-ng/libunicode.c
    third_party/quickjs-ng/quickjs.c)
add_library(olagarro::QuickJS ALIAS olagarro_quickjs)
set_target_properties(olagarro_quickjs PROPERTIES
    COMPILE_OPTIONS ""
    C_STANDARD 11
    C_STANDARD_REQUIRED YES
    EXPORT_NAME QuickJS)
if(MSVC)
    target_compile_options(olagarro_quickjs PRIVATE /W0 /J /experimental:c11atomics)
else()
    # Optimised, it carries no unwind tables: it is C, and no C++ exception crosses it, as the engine's bridge
    # catches them all before they reach it. They are a tenth of its size.
    target_compile_options(olagarro_quickjs PRIVATE -w -funsigned-char
        $<$<CONFIG:MinSizeRel,Release>:-fno-asynchronous-unwind-tables -fno-unwind-tables>)
endif()
target_compile_definitions(olagarro_quickjs PRIVATE _GNU_SOURCE)
if(WIN32)
    target_compile_definitions(olagarro_quickjs PRIVATE WIN32_LEAN_AND_MEAN _WIN32_WINNT=0x0601)
else()
    target_link_libraries(olagarro_quickjs PUBLIC m)
endif()
target_link_libraries(olagarro_quickjs PUBLIC Threads::Threads)
target_include_directories(olagarro_quickjs SYSTEM PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/third_party/quickjs-ng>)

add_library(olagarro_bytestream bytestream/bytestream.cpp)
add_library(olagarro::ByteStream ALIAS olagarro_bytestream)
set_target_properties(olagarro_bytestream PROPERTIES EXPORT_NAME ByteStream)
olagarro_configure_target(olagarro_bytestream)

add_library(olagarro_signalslot INTERFACE)
add_library(olagarro::SignalSlot ALIAS olagarro_signalslot)
set_target_properties(olagarro_signalslot PROPERTIES EXPORT_NAME SignalSlot)
target_compile_features(olagarro_signalslot INTERFACE cxx_std_17)
target_include_directories(olagarro_signalslot INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/olagarro>)

add_library(olagarro_tasks
    tasks/src/earlyexitparallel.cpp
    tasks/src/neverfinish.cpp
    tasks/src/parallel.cpp
    tasks/src/priorityselector.cpp
    tasks/src/repeat.cpp
    tasks/src/select.cpp
    tasks/src/serial.cpp
    tasks/src/task.cpp
    tasks/src/taskexecutor.cpp
    tasks/src/trigger.cpp
    tasks/src/wait.cpp)
add_library(olagarro::Tasks ALIAS olagarro_tasks)
set_target_properties(olagarro_tasks PROPERTIES EXPORT_NAME Tasks)
olagarro_configure_target(olagarro_tasks)
target_include_directories(olagarro_tasks PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/tasks/include>)

add_library(olagarro_cttasks cttasks/src/wait.cpp)
add_library(olagarro::CTTasks ALIAS olagarro_cttasks)
set_target_properties(olagarro_cttasks PROPERTIES EXPORT_NAME CTTasks)
olagarro_configure_target(olagarro_cttasks)
target_include_directories(olagarro_cttasks PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/cttasks/include>)

add_library(olagarro_concurrency INTERFACE)
add_library(olagarro::Concurrency ALIAS olagarro_concurrency)
set_target_properties(olagarro_concurrency PROPERTIES EXPORT_NAME Concurrency)
target_compile_features(olagarro_concurrency INTERFACE cxx_std_17)
target_include_directories(olagarro_concurrency INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}> $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/olagarro>)
target_link_libraries(olagarro_concurrency INTERFACE Threads::Threads)

add_library(olagarro_effects effects/image.cpp)
add_library(olagarro::Effects ALIAS olagarro_effects)
set_target_properties(olagarro_effects PROPERTIES EXPORT_NAME Effects)
olagarro_configure_target(olagarro_effects)
target_link_libraries(olagarro_effects PUBLIC olagarro::Concurrency)

set(olagarro_oui_platform_sources
    oui/src/clipboard_win32.cpp
    oui/src/crash_reporter_win32.cpp
    oui/src/file_system_win32.cpp
    oui/src/language_win32.cpp
    oui/src/process_win32.cpp
    oui/src/pty_win32.cpp
    oui/src/settings_win32.cpp
    oui/src/terminal_backend_win32.cpp)
if(NOT WIN32)
    set(olagarro_oui_platform_sources
        oui/src/clipboard_posix.cpp
        oui/src/crash_reporter_posix.cpp
        oui/src/file_system_posix.cpp
        oui/src/language_posix.cpp
        oui/src/process_posix.cpp
        oui/src/pty_posix.cpp
        oui/src/settings_posix.cpp
        oui/src/terminal_backend_posix.cpp)
endif()

add_library(olagarro_oui
    oui/src/button.cpp
    oui/src/check_button.cpp
    oui/src/check_list.cpp
    oui/src/backend.cpp
    oui/src/application.cpp
    oui/src/command.cpp
    oui/src/command_line.cpp
    oui/src/command_palette.cpp
    oui/src/completion.cpp
    oui/src/container.cpp
    oui/src/cell_buffer.cpp
    oui/src/file_dialog.cpp
    oui/src/color_dialog.cpp
    oui/src/combo_box.cpp
    oui/src/dialogs.cpp
    oui/src/dock_layout.cpp
    oui/src/status_bar.cpp
    oui/src/scripted_backend.cpp
    oui/src/crash_cleanup.cpp
    oui/src/grid.cpp
    oui/src/dynamic_grid.cpp
    oui/src/folder_view.cpp
    oui/src/declarative.cpp
    oui/src/ui.cpp
    oui/src/inflate.cpp
    oui/src/layout.cpp
    oui/src/label.cpp
    oui/src/progress_bar.cpp
    oui/src/line_edit.cpp
    oui/src/list.cpp
    oui/src/markdown_view.cpp
    oui/src/menu.cpp
    oui/src/menu_bar.cpp
    oui/src/number_field.cpp
    oui/src/painter.cpp
    oui/src/panel.cpp
    oui/src/raster_backend.cpp
    oui/src/scroll_view.cpp
    oui/src/spacer.cpp
    oui/src/settings.cpp
    oui/src/signature_tip.cpp
    oui/src/slider.cpp
    oui/src/tab_view.cpp
    oui/src/tab_switcher.cpp
    oui/src/text_layout.cpp
    oui/src/text_metrics.cpp
    oui/src/text_edit.cpp
    oui/src/text_edit_input.cpp
    oui/src/text_edit_lines.cpp
    oui/src/text_edit_search.cpp
    oui/src/text_edit_view.cpp
    oui/src/text_diff.cpp
    oui/src/diff_view.cpp
    oui/src/expression.cpp
    oui/src/calculator_view.cpp
    oui/src/terminal_themes.cpp
    oui/src/regex.cpp
    oui/src/text_search.cpp
    oui/src/split_view.cpp
    oui/src/suggester.cpp
    oui/src/syntax_highlighter.cpp
    oui/src/scrollbar.cpp
    oui/src/table.cpp
    oui/src/terminal_input_decoder.cpp
    oui/src/terminal_screen.cpp
    oui/src/terminal_view.cpp
    oui/src/popup_list.cpp
    oui/src/text_file.cpp
    oui/src/fuzzy.cpp
    oui/src/quick_pick.cpp
    oui/src/trace.cpp
    oui/src/translation.cpp
    oui/src/type_ahead.cpp
    oui/src/tree_view.cpp
    oui/src/widget.cpp
    oui/src/window.cpp
    oui/src/audio_output.cpp
    oui/src/folder_watcher.cpp
    ${olagarro_oui_platform_sources})
add_library(olagarro::OUI ALIAS olagarro_oui)
# OUI's own desktop and the programs on it, as far as cells go: shared memory and local sockets, Linux's.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_sources(olagarro_oui PRIVATE oui/src/desktop_client.cpp oui/src/desktop_protocol.cpp
        oui/src/desktop_server.cpp)
endif()
set_target_properties(olagarro_oui PROPERTIES EXPORT_NAME OUI)
olagarro_configure_target(olagarro_oui)
target_include_directories(olagarro_oui PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/oui/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/olagarro>)
# Regular expressions, in searches and in the editor, are QuickJS's: the same engine scripts use, and far smaller
# than std::regex, which brings in the C++ library's locales and streams.
target_link_libraries(olagarro_oui PUBLIC olagarro::SignalSlot Threads::Threads PRIVATE olagarro::QuickJS)
olagarro_embed_files(olagarro_oui NAMESPACE olagarro::oui FUNCTION embedded_oui_catalogs
    FILES oui/translations/es.json)
# The calculator's help, in English and then in Spanish.
olagarro_embed_files(olagarro_oui NAMESPACE olagarro::oui FUNCTION embedded_oui_calculator_help
    FILES oui/help/calculator.md oui/help/calculator.es.md)
if(WIN32)
    target_link_libraries(olagarro_oui PRIVATE user32 dbghelp shell32 ole32 uuid)
endif()

# OUI's graphical side: a canvas of pixels, the fonts built into it and a painter that draws OUI's elements in a
# graphical style. It needs nothing outside this repository; a backend that shows the pixels, such as a window, builds
# on it.
add_library(olagarro_oui_graphics
    oui/src/graphics/canvas.cpp
    oui/src/graphics/font.cpp
    oui/src/graphics/icon.cpp
    oui/src/graphics/graphical_painter.cpp
    oui/src/graphics/nanosvg_api.cpp
    oui/src/graphics/pixel_backend.cpp
    oui/src/graphics/stb_truetype_api.cpp
    oui/src/graphics/svg.cpp)
add_library(olagarro::OUIGraphics ALIAS olagarro_oui_graphics)
# The whole screen through the kernel's mode setting needs nothing but Linux's own headers.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_sources(olagarro_oui_graphics PRIVATE oui/src/graphics/drm_backend.cpp oui/src/graphics/keyboard_layout.cpp
        oui/src/graphics/desktop_pixels.cpp)
endif()
set_target_properties(olagarro_oui_graphics PROPERTIES EXPORT_NAME OUIGraphics)
olagarro_configure_target(olagarro_oui_graphics)
target_link_libraries(olagarro_oui_graphics PUBLIC olagarro::OUI)
target_include_directories(olagarro_oui_graphics PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/third_party)
# stb_truetype is used as it comes, so OUI's warnings stay off it.
set_source_files_properties(oui/src/graphics/stb_truetype_api.cpp PROPERTIES
    COMPILE_OPTIONS "$<IF:$<CXX_COMPILER_ID:MSVC>,/W0,-w>")
# NanoSVG too.
set_source_files_properties(oui/src/graphics/nanosvg_api.cpp PROPERTIES
    COMPILE_OPTIONS "$<IF:$<CXX_COMPILER_ID:MSVC>,/W0,-w>")
olagarro_embed_files(olagarro_oui_graphics NAMESPACE olagarro::oui FUNCTION embedded_oui_fonts
    FILES oui/fonts/spleen_12x24.psfu oui/fonts/jersey10_regular.ttf oui/fonts/spleen_8x16.psfu
        oui/fonts/spleen_16x32.psfu oui/fonts/noto_sans_regular.ttf)

# The desktop window: SDL3 opens it and brings its input, OUI draws everything in it, and plays sound. SDL3 is linked
# statically, so the program needs nothing installed beside it; an installed SDL3 is used when CMake finds one, and
# otherwise its source is fetched and built with only what a window and sound need.
if(OLAGARRO_OUI_WINDOW)
    find_package(SDL3 CONFIG COMPONENTS SDL3-static QUIET)
    if(NOT TARGET SDL3::SDL3-static)
        include(FetchContent)
        set(SDL_SHARED OFF CACHE BOOL "" FORCE)
        set(SDL_STATIC ON CACHE BOOL "" FORCE)
        set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
        set(SDL_TESTS OFF CACHE BOOL "" FORCE)
        set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(SDL_DISABLE_INSTALL ON CACHE BOOL "" FORCE)
        foreach(subsystem CAMERA JOYSTICK HAPTIC SENSOR HIDAPI GPU POWER VULKAN X11_XSCRNSAVER X11_XTEST)
            set(SDL_${subsystem} OFF CACHE BOOL "" FORCE)
        endforeach()
        # On even in a build made before sound was, which had it off.
        set(SDL_AUDIO ON CACHE BOOL "" FORCE)
        FetchContent_Declare(SDL3
            URL https://github.com/libsdl-org/SDL/releases/download/release-3.4.16/SDL3-3.4.16.tar.gz
            URL_HASH SHA256=7322236cd12090c3eb40b9728be4d49c76f66ad17d04369584d4ecad5cf77c68)
        # SDL3 builds with its own warnings, not Olagarro's warnings as errors.
        get_directory_property(olagarro_compile_options COMPILE_OPTIONS)
        set_directory_properties(PROPERTIES COMPILE_OPTIONS "")
        FetchContent_MakeAvailable(SDL3)
        set_directory_properties(PROPERTIES COMPILE_OPTIONS "${olagarro_compile_options}")
    endif()
    add_library(olagarro_oui_window oui/src/graphics/window_backend_sdl.cpp oui/src/graphics/system_pointer.cpp
        oui/src/graphics/system_audio_output_sdl.cpp)
    add_library(olagarro::OUIWindow ALIAS olagarro_oui_window)
    set_target_properties(olagarro_oui_window PROPERTIES EXPORT_NAME OUIWindow)
    olagarro_configure_target(olagarro_oui_window)
    target_link_libraries(olagarro_oui_window PUBLIC olagarro::OUIGraphics PRIVATE SDL3::SDL3-static)
endif()

# OUI's scripting: JavaScript run by QuickJS, and what scripts reach of OUI.
add_library(olagarro_oui_script
    oui/src/script/rule_highlighter.cpp
    oui/src/script/script_highlighter.cpp
    oui/src/script/script_engine.cpp
    oui/src/script/system_api.cpp)
add_library(olagarro::OUIScript ALIAS olagarro_oui_script)
set_target_properties(olagarro_oui_script PROPERTIES EXPORT_NAME OUIScript)
olagarro_configure_target(olagarro_oui_script)
target_link_libraries(olagarro_oui_script PUBLIC olagarro::OUI PRIVATE olagarro::QuickJS)

if(OLAGARRO_BUILD_MODULES)
    if(CMAKE_VERSION VERSION_LESS 3.28)
        message(FATAL_ERROR "Experimental module adapters require CMake 3.28 or newer.")
    endif()

    function(olagarro_add_module target alias dependency module_file)
        add_library(${target})
        add_library(${alias} ALIAS ${target})
        target_link_libraries(${target} PUBLIC ${dependency})
        set_target_properties(${target} PROPERTIES
            CXX_STANDARD 20
            CXX_STANDARD_REQUIRED YES
            CXX_EXTENSIONS NO)
        target_sources(${target}
            PUBLIC
                FILE_SET cxx_modules TYPE CXX_MODULES
                FILES ${module_file})
    endfunction()

    olagarro_add_module(olagarro_module_bytestream olagarro::ModuleByteStream olagarro::ByteStream
        modules/olagarro.bytestream.cppm)
    olagarro_add_module(olagarro_module_signalslot olagarro::ModuleSignalSlot olagarro::SignalSlot
        modules/olagarro.signalslot.cppm)
    olagarro_add_module(olagarro_module_tasks olagarro::ModuleTasks olagarro::Tasks modules/olagarro.tasks.cppm)
    # The tasks module carries WaitSignal, whose header needs the signals.
    target_link_libraries(olagarro_module_tasks PUBLIC olagarro::SignalSlot)
    olagarro_add_module(olagarro_module_cttasks olagarro::ModuleCTTasks olagarro::CTTasks modules/olagarro.cttasks.cppm)
    olagarro_add_module(olagarro_module_concurrency olagarro::ModuleConcurrency olagarro::Concurrency
        modules/olagarro.concurrency.cppm)
    olagarro_add_module(olagarro_module_oui olagarro::ModuleOUI olagarro::OUI modules/olagarro.oui.cppm)
    # One call for each: set_target_properties takes a list of targets, then one list of properties for all of them.
    set_target_properties(olagarro_module_bytestream PROPERTIES EXPORT_NAME ModuleByteStream)
    set_target_properties(olagarro_module_signalslot PROPERTIES EXPORT_NAME ModuleSignalSlot)
    set_target_properties(olagarro_module_tasks PROPERTIES EXPORT_NAME ModuleTasks)
    set_target_properties(olagarro_module_cttasks PROPERTIES EXPORT_NAME ModuleCTTasks)
    set_target_properties(olagarro_module_concurrency PROPERTIES EXPORT_NAME ModuleConcurrency)
    set_target_properties(olagarro_module_oui PROPERTIES EXPORT_NAME ModuleOUI)

    add_library(olagarro_module)
    add_library(olagarro::Module ALIAS olagarro_module)
    target_link_libraries(olagarro_module PUBLIC
        olagarro::ModuleByteStream
        olagarro::ModuleSignalSlot
        olagarro::ModuleTasks
        olagarro::ModuleCTTasks
        olagarro::ModuleConcurrency
        olagarro::ModuleOUI)
    set_target_properties(olagarro_module PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
        EXPORT_NAME Module)
    target_sources(olagarro_module
        PUBLIC
            FILE_SET cxx_modules TYPE CXX_MODULES
            FILES modules/olagarro.cppm)

    add_executable(olagarro_module_smoke modules/olagarro_smoke.cpp)
    target_link_libraries(olagarro_module_smoke PRIVATE olagarro::Module)
    set_target_properties(olagarro_module_smoke PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
        CXX_SCAN_FOR_MODULES ON)
endif()

set(olagarro_targets
    olagarro_bytestream
    olagarro_signalslot
    olagarro_tasks
    olagarro_cttasks
    olagarro_concurrency
    olagarro_effects
    olagarro_oui
    olagarro_quickjs
    olagarro_oui_script
    olagarro_oui_graphics)

install(TARGETS ${olagarro_targets}
    EXPORT OlagarroTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

if(OLAGARRO_BUILD_MODULES)
    install(TARGETS
        olagarro_module_bytestream
        olagarro_module_signalslot
        olagarro_module_tasks
        olagarro_module_cttasks
        olagarro_module_concurrency
        olagarro_module_oui
        olagarro_module
        EXPORT OlagarroTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        FILE_SET cxx_modules DESTINATION ${CMAKE_INSTALL_LIBDIR}/olagarro)
endif()
install(DIRECTORY
    bytestream
    effects
    signalslot
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/olagarro
    FILES_MATCHING PATTERN "*.h")
install(FILES
    concurrency/concurrency.h
    concurrency/pool.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/olagarro/concurrency)
install(DIRECTORY cttasks/include/ tasks/include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/olagarro
    FILES_MATCHING PATTERN "*.h")
install(DIRECTORY oui/include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/olagarro
    FILES_MATCHING PATTERN "*.h")
install(EXPORT OlagarroTargets
    FILE OlagarroTargets.cmake
    NAMESPACE olagarro::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Olagarro)

configure_package_config_file(
    cmake/OlagarroConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/OlagarroConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Olagarro)
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/OlagarroConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion)
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/OlagarroConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/OlagarroConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Olagarro)

if(OLAGARRO_BUILD_EXAMPLES)
    add_executable(olagarro_oui_terminal_demo examples/oui/terminal_demo.cpp)
    target_link_libraries(olagarro_oui_terminal_demo PRIVATE olagarro::OUI)
    add_test(NAME Olagarro.OuiTerminalSmoke COMMAND olagarro_oui_terminal_demo --smoke-test)
    set(OLAGARRO_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin" CACHE PATH
        "Directory where runnable examples are placed")
    # oui_term: a terminal, in the terminal, a desktop window or the whole screen.
    add_executable(olagarro_oui_term examples/oui_term/main.cpp)
    target_link_libraries(olagarro_oui_term PRIVATE olagarro::OUI)
    if(OLAGARRO_OUI_WINDOW)
        target_link_libraries(olagarro_oui_term PRIVATE olagarro::OUIWindow)
        target_compile_definitions(olagarro_oui_term PRIVATE OLAGARRO_OUI_TERM_WINDOW)
        set(olagarro_oui_term_name oui_term_gui)
    else()
        set(olagarro_oui_term_name oui_term)
    endif()
    if(OLAGARRO_OUI_DRM)
        target_link_libraries(olagarro_oui_term PRIVATE olagarro::OUIGraphics)
        target_compile_definitions(olagarro_oui_term PRIVATE OLAGARRO_OUI_TERM_DRM)
    endif()
    set_target_properties(olagarro_oui_term PROPERTIES
        OUTPUT_NAME ${olagarro_oui_term_name}
        RUNTIME_OUTPUT_DIRECTORY ${OLAGARRO_RUNTIME_OUTPUT_DIRECTORY})
    # oui_desktop: OUI's own desktop, in the terminal (--terminal, over SSH too) and, built with the screen or the
    # window, on the whole screen (--drm) or in a window (--window). The programs show on it with --desktop: in
    # characters always, and in pixels too when they are built with the graphics, as the desktop is.
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        add_executable(olagarro_oui_desktop examples/oui_desktop/main.cpp examples/oui_desktop/desktop_area.cpp
            examples/oui_desktop/launcher.cpp examples/oui_desktop/task_bar.cpp)
        target_link_libraries(olagarro_oui_desktop PRIVATE olagarro::OUI)
        if(OLAGARRO_OUI_DRM OR OLAGARRO_OUI_WINDOW)
            target_link_libraries(olagarro_oui_desktop PRIVATE olagarro::OUIGraphics)
            target_compile_definitions(olagarro_oui_desktop PRIVATE OLAGARRO_OUI_DESKTOP_PIXELS)
            target_compile_definitions(olagarro_oui_term PRIVATE OLAGARRO_OUI_TERM_DESKTOP_PIXELS)
            target_link_libraries(olagarro_oui_term PRIVATE olagarro::OUIGraphics)
        endif()
        if(OLAGARRO_OUI_DRM)
            target_compile_definitions(olagarro_oui_desktop PRIVATE OLAGARRO_OUI_DESKTOP_DRM)
        endif()
        if(OLAGARRO_OUI_WINDOW)
            target_link_libraries(olagarro_oui_desktop PRIVATE olagarro::OUIWindow)
            target_compile_definitions(olagarro_oui_desktop PRIVATE OLAGARRO_OUI_DESKTOP_WINDOW)
        endif()
        # Named apart with the window module, as oui_term is, so the two builds sit side by side.
        if(OLAGARRO_OUI_WINDOW)
            set(olagarro_oui_desktop_name oui_desktop_gui)
        else()
            set(olagarro_oui_desktop_name oui_desktop)
        endif()
        set_target_properties(olagarro_oui_desktop PROPERTIES
            OUTPUT_NAME ${olagarro_oui_desktop_name}
            RUNTIME_OUTPUT_DIRECTORY ${OLAGARRO_RUNTIME_OUTPUT_DIRECTORY})
        # The task bar starts the programs beside it, by their names in a build of this kind: oui_term, built here, and
        # OUI Code and oui_calc, which https://github.com/iganinja/oui_code builds, when they are copied beside it.
        if(OLAGARRO_OUI_WINDOW)
            set(olagarro_oui_suffix _gui)
        else()
            set(olagarro_oui_suffix "")
        endif()
        target_compile_definitions(olagarro_oui_desktop PRIVATE OLAGARRO_OUI_TERM_NAME="${olagarro_oui_term_name}"
            OLAGARRO_OUI_CALC_NAME="oui_calc${olagarro_oui_suffix}"
            OLAGARRO_OUI_CODE_NAME="oui_code${olagarro_oui_suffix}")
        target_compile_definitions(olagarro_oui_term PRIVATE OLAGARRO_OUI_TERM_DESKTOP)
    endif()
    # The options as they are typed: "--help", never "-- help", which a formatter once made of them.
    if(TARGET olagarro_oui_desktop)
        add_test(NAME Olagarro.OuiDesktopHelp COMMAND olagarro_oui_desktop --help)
        set_tests_properties(Olagarro.OuiDesktopHelp PROPERTIES
            PASS_REGULAR_EXPRESSION "  --help " FAIL_REGULAR_EXPRESSION "-- ")
    endif()
    if(WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_link_options(olagarro_oui_terminal_demo PRIVATE -static -static-libgcc -static-libstdc++)
        target_link_options(olagarro_oui_term PRIVATE -static -static-libgcc -static-libstdc++)
    elseif(OLAGARRO_STATIC_LINK)
        # A static libstdc++ alone is not enough: it still asks for the build machine's glibc version.
        target_link_options(olagarro_oui_terminal_demo PRIVATE -static)
        if(OLAGARRO_OUI_WINDOW)
            # A window needs the system's display libraries, which SDL3 loads when it starts: that takes the
            # system's C library, so only the C++ runtime goes inside. Build it on an old system to run on many.
            target_link_options(olagarro_oui_term PRIVATE -static-libgcc -static-libstdc++)
        else()
            target_link_options(olagarro_oui_term PRIVATE -static)
            if(TARGET olagarro_oui_desktop)
                target_link_options(olagarro_oui_desktop PRIVATE -static)
            endif()
        endif()
    endif()
endif()

if(OLAGARRO_BUILD_TESTS)
    include(CTest)
    if(BUILD_TESTING)
        find_package(GTest CONFIG REQUIRED)
        include(GoogleTest)
        # GoogleTest's warnings are not Olagarro's to fix. Its targets say its headers are the system's, but CMake drops
        # a folder the compiler searches anyway, as Homebrew's /usr/local/include on an Intel Mac, where the compiler
        # then warns in them as in Olagarro's own. /usr/include stays as it is: it is the system's already, and marked
        # again it comes before the C++ library's own headers.
        get_target_property(olagarro_gtest_includes GTest::gtest INTERFACE_INCLUDE_DIRECTORIES)
        foreach(olagarro_gtest_include IN LISTS olagarro_gtest_includes)
            if(NOT olagarro_gtest_include STREQUAL "/usr/include")
                set_property(TARGET GTest::gtest APPEND PROPERTY INTERFACE_COMPILE_OPTIONS
                    "SHELL:-isystem ${olagarro_gtest_include}")
            endif()
        endforeach()

        add_executable(olagarro_tasks_tests
            tests/tasks/taskexecutor_test.cpp
            tests/tasks/containeractions_test.cpp
            tests/tasks/execute_test.cpp
            tests/tasks/interpolate_test.cpp
            tests/tasks/priorityselector_test.cpp
            tests/tasks/trigger_test.cpp
            tests/tasks/wait_test.cpp
            tests/tasks/waitsignal_test.cpp)
        target_link_libraries(olagarro_tasks_tests PRIVATE
            olagarro::Tasks
            olagarro::SignalSlot
            GTest::gmock_main)
        gtest_discover_tests(olagarro_tasks_tests)

        add_executable(olagarro_cttasks_tests
            tests/cttasks/actionexcutor_test.cpp
            tests/cttasks/containertasks_test.cpp)
        target_link_libraries(olagarro_cttasks_tests PRIVATE
            olagarro::CTTasks
            GTest::gmock_main)
        gtest_discover_tests(olagarro_cttasks_tests)

        add_executable(olagarro_bytestream_tests tests/bytestream/main.cpp)
        target_link_libraries(olagarro_bytestream_tests PRIVATE olagarro::ByteStream)
        target_include_directories(olagarro_bytestream_tests PRIVATE tests/bytestream)
        add_test(NAME Olagarro.ByteStream COMMAND olagarro_bytestream_tests)

        # The installed package, used as another project uses it. Not where its programs cannot run here, nor with a
        # generator that builds several configurations, whose programs land in a folder each.
        if(NOT CMAKE_CROSSCOMPILING AND NOT CMAKE_CONFIGURATION_TYPES)
            add_test(NAME Olagarro.PackageConsumer
                COMMAND ${CMAKE_COMMAND} -DBUILD_DIR=${CMAKE_BINARY_DIR} -DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
                    -DWORK_DIR=${CMAKE_BINARY_DIR}/package_consumer "-DGENERATOR=${CMAKE_GENERATOR}"
                    -DCXX_COMPILER=${CMAKE_CXX_COMPILER} -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/TestPackageConsumer.cmake)
            set_tests_properties(Olagarro.PackageConsumer PROPERTIES TIMEOUT 300)
        endif()

        add_executable(olagarro_bytestream_unit_tests tests/bytestream/bytestream_test.cpp)
        target_link_libraries(olagarro_bytestream_unit_tests PRIVATE olagarro::ByteStream GTest::gtest_main)
        gtest_discover_tests(olagarro_bytestream_unit_tests)

        add_executable(olagarro_signalslot_tests tests/signalslot/main.cpp)
        target_link_libraries(olagarro_signalslot_tests PRIVATE olagarro::SignalSlot GTest::gtest_main)
        gtest_discover_tests(olagarro_signalslot_tests)
        # The same tests with AddressSanitizer and UndefinedBehaviorSanitizer, as one test: signals keep their
        # callbacks by hand, created with new and destroyed when the last holder lets go, and this sees that each is
        # destroyed once, never used after, and never leaked. Not where the sanitizers cannot run: Windows, or a
        # build linked statically.
        if(NOT WIN32 AND NOT OLAGARRO_STATIC_LINK AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            # macOS has no leak checker; and its GoogleTest, Homebrew's, is built without the marks that checking a
            # container's spare room needs, so that check finds overflows in GoogleTest's own vectors that are not
            # there.
            if(APPLE)
                set(olagarro_asan_leaks 0)
                set(olagarro_asan_more ":detect_container_overflow=0")
            else()
                set(olagarro_asan_leaks 1)
                set(olagarro_asan_more "")
            endif()
            add_executable(olagarro_signalslot_sanitized_tests tests/signalslot/main.cpp)
            target_link_libraries(olagarro_signalslot_sanitized_tests PRIVATE olagarro::SignalSlot GTest::gtest_main)
            target_compile_options(olagarro_signalslot_sanitized_tests PRIVATE
                -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer)
            target_link_options(olagarro_signalslot_sanitized_tests PRIVATE -fsanitize=address,undefined)
            add_test(NAME Olagarro.SignalSlotSanitized COMMAND olagarro_signalslot_sanitized_tests)
            set_tests_properties(Olagarro.SignalSlotSanitized PROPERTIES
                ENVIRONMENT "ASAN_OPTIONS=detect_leaks=${olagarro_asan_leaks}:abort_on_error=0${olagarro_asan_more}")
            # OUI itself under the sanitizers, for what goes wrong as its parts go: its sources built again so.
            get_target_property(olagarro_oui_sources olagarro_oui SOURCES)
            add_executable(olagarro_oui_sanitized_tests tests/oui/teardown_test.cpp ${olagarro_oui_sources})
            target_include_directories(olagarro_oui_sanitized_tests PRIVATE oui/include ${CMAKE_CURRENT_SOURCE_DIR})
            target_link_libraries(olagarro_oui_sanitized_tests PRIVATE olagarro::SignalSlot olagarro::QuickJS
                Threads::Threads GTest::gtest_main)
            target_compile_options(olagarro_oui_sanitized_tests PRIVATE
                -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer)
            target_link_options(olagarro_oui_sanitized_tests PRIVATE -fsanitize=address,undefined)
            add_test(NAME Olagarro.OuiSanitized COMMAND olagarro_oui_sanitized_tests)
            set_tests_properties(Olagarro.OuiSanitized PROPERTIES
                ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0${olagarro_asan_more}")
        endif()


        add_executable(olagarro_concurrency_unit_tests tests/concurrency/concurrency_test.cpp
            tests/concurrency/pool_test.cpp)
        target_link_libraries(olagarro_concurrency_unit_tests PRIVATE
            olagarro::Concurrency
            GTest::gtest_main)
        gtest_discover_tests(olagarro_concurrency_unit_tests)

        add_executable(olagarro_effects_unit_tests tests/effects/image_test.cpp)
        target_link_libraries(olagarro_effects_unit_tests PRIVATE olagarro::Effects GTest::gtest_main)
        gtest_discover_tests(olagarro_effects_unit_tests)

       add_executable(olagarro_oui_tests
             tests/oui/layout_test.cpp
             tests/oui/raw_pointer_test.cpp
             tests/oui/drop_test.cpp
             tests/oui/audio_output_test.cpp
             tests/oui/folder_watcher_test.cpp
             tests/oui/slider_test.cpp
             tests/oui/number_field_test.cpp
             tests/oui/clipboard_data_test.cpp
             tests/oui/script_float_array_test.cpp
             tests/oui/settings_test.cpp
             tests/oui/contrast_test.cpp
             tests/oui/text_attributes_test.cpp
             tests/oui/file_picker_test.cpp
             tests/oui/file_picker_dialog_test.cpp
             tests/oui/command_palette_test.cpp
             tests/oui/completion_test.cpp
             tests/oui/suggester_test.cpp
             tests/oui/text_file_test.cpp
             tests/oui/dialogs_test.cpp
             tests/oui/color_dialog_test.cpp
             tests/oui/file_picker_integration_test.cpp
             tests/oui/list_header_test.cpp
             tests/oui/list_metadata_test.cpp
             tests/oui/list_scroll_test.cpp
             tests/oui/scrollbar_test.cpp
             tests/oui/enabled_test.cpp
             tests/oui/check_list_test.cpp
             tests/oui/tree_view_test.cpp
             tests/oui/list_sort_test.cpp
             tests/oui/table_test.cpp
             tests/oui/terminal_input_decoder_test.cpp
             tests/oui/unicode_composition_test.cpp
             tests/oui/markdown_view_test.cpp
             tests/oui/text_search_test.cpp
             tests/oui/process_test.cpp
             tests/oui/script_test.cpp
            tests/oui/trace_test.cpp
            tests/oui/title_test.cpp
            tests/oui/translation_test.cpp
            tests/oui/turbo_c_theme_test.cpp
            tests/oui/terminal_themes_test.cpp
            tests/oui/text_diff_test.cpp
            tests/oui/diff_view_test.cpp
            tests/oui/expression_test.cpp
            tests/oui/ui_description_test.cpp
            tests/oui/inflate_test.cpp
            tests/oui/graphics_test.cpp
            tests/oui/tab_view_test.cpp
            tests/oui/text_edit_gutter_test.cpp
            tests/oui/menu_scroll_test.cpp
            tests/oui/terminal_screen_test.cpp
            tests/oui/terminal_view_test.cpp
            tests/oui/popup_list_test.cpp
            tests/oui/lean_sources_test.cpp)
        if(WIN32)
            target_sources(olagarro_oui_tests PRIVATE tests/oui/pty_win32_test.cpp)
        endif()
        if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
            target_sources(olagarro_oui_tests PRIVATE tests/oui/keyboard_layout_test.cpp tests/oui/pty_test.cpp
                tests/oui/terminal_view_pty_test.cpp tests/oui/trash_test.cpp
                tests/oui/desktop_test.cpp)
        endif()
        target_link_libraries(olagarro_oui_tests PRIVATE olagarro::OUI olagarro::OUIScript olagarro::OUIGraphics
            GTest::gtest_main)
        # The catalog checks read the sources and the catalogs in the repository.
        target_compile_definitions(olagarro_oui_tests PRIVATE OLAGARRO_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
        gtest_discover_tests(olagarro_oui_tests)
        # What needs SDL3, in a program of its own, as only a build with the window has it.
        if(OLAGARRO_OUI_WINDOW)
            add_executable(olagarro_oui_window_tests tests/oui/system_audio_output_test.cpp)
            target_link_libraries(olagarro_oui_window_tests PRIVATE olagarro::OUIWindow GTest::gtest_main)
            gtest_discover_tests(olagarro_oui_window_tests)
        endif()
       # A text and a font, compressed as every embedded file is, to see them come back whole.
       olagarro_embed_files(olagarro_oui_tests FUNCTION embedded_inflate_test_files
           FILES doc/oui_layout.md oui/fonts/spleen_16x32.psfu)
        add_test(NAME Olagarro.OuiLifecycleStress
            COMMAND olagarro_oui_tests --gtest_filter=OuiApplicationTests.StressesWidgetLifetimeAndModalIsolation)
        set_tests_properties(Olagarro.OuiLifecycleStress PROPERTIES TIMEOUT 30)

    endif()
endif()
