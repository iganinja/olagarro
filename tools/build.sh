#!/usr/bin/env bash
# Configures, builds, and tests Olagarro on Linux and other POSIX systems.
#
# usage: tools/build.sh [--release] [--gui] [--static] [--no-tests] [--clean]
#   --release   Build type Release in build-release/ (default: Debug in build/).
#   --gui       Build the programs able to open in a desktop window, as oui_term_gui and the rest, in build-gui/
#               (or build-gui-release/). SDL3 is linked statically: an installed one if CMake finds it,
#               otherwise its source is fetched once.
#   --static    Link the programs fully statically, so they run on older Linux systems too; with --gui, only the
#               C++ runtime goes inside, as a window needs the system's display libraries and its C library.
#   --no-tests  Skip ctest.
#   --clean     Delete the build directory first, for a cold build.
# Runnable examples such as oui_term land in bin/.
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_type=Debug
build_dir="$root/build"
run_tests=1
clean=0
static_link=OFF
window=OFF

for argument in "$@"; do
    case "$argument" in
        --release) build_type=Release; build_dir="$root/build-release" ;;
        --gui) window=ON ;;
        --static) static_link=ON ;;
        --no-tests) run_tests=0 ;;
        --clean) clean=1 ;;
        *) echo "Unknown option: $argument" >&2; exit 2 ;;
    esac
done

if [[ $window == ON ]]; then
    build_dir="$root/build-gui"
    [[ $build_type == Release ]] && build_dir="$root/build-gui-release"
fi

if [[ $clean -eq 1 ]]; then
    rm -rf "$build_dir"
fi

cmake -S "$root" -B "$build_dir" -G Ninja -DCMAKE_BUILD_TYPE="$build_type" -DOLAGARRO_STATIC_LINK="$static_link" \
    -DOLAGARRO_OUI_WINDOW="$window"
cmake --build "$build_dir"
if [[ $run_tests -eq 1 ]]; then
    ctest --test-dir "$build_dir" --output-on-failure -j "$(nproc)"
fi
