#!/usr/bin/env bash
# Builds the programs a release carries, oui_code, oui_run and oui_calc, for another machine: as small as they get,
# needing nothing installed beside them, and stripped. Each kind is built inside a container, with podman or docker,
# the repository mounted read-only, so this machine and GitHub's build them the same way.
#
#   tools/build_portable.sh                   Linux, terminal only: static against musl in Alpine, so they run on
#                                             any Linux of their processor whatever its C and C++ runtime.
#                                             oui_code and oui_calc are smallest with clang, libc++, full link-time
#                                             optimisation, -Oz and identical code folded; oui_run with GCC.
#   tools/build_portable.sh --gui             Linux, also opening in a desktop window, the same three programs.
#                                             They carry SDL3 and the C++ runtime, and take the C library and the
#                                             display libraries from the system, so they are built on Ubuntu 22.04
#                                             to run on that and anything newer.
#   tools/build_portable.sh --windows         Windows, terminal only: oui_code.exe and the rest, cross-built with
#                                             llvm-mingw and linked statically.
#   tools/build_portable.sh --windows --gui   Windows, also opening in a window, the same three programs.
#
# On a Mac, with or without --gui, it builds macOS's programs there, as Apple's tools run in no container: for macOS
# 11 and newer, the first Apple's own processors run, linked against the system's libraries, which every Mac has,
# unused code dropped (-dead_strip). The machine's processor says which: arm64 on Apple's, x86_64 on Intel's.
#
# The Linux ones are for the processor the container runs on: x86_64 here, aarch64 on an ARM machine; the Windows
# ones are for x86_64. Output: dist/<system>-<processor>-<kind>/, as dist/linux-x86_64-terminal/, with the programs,
# named the same in either kind: a build here names the gui ones apart, oui_code_gui, to keep them beside the
# terminal ones in bin/, but a system has one or the other. tools/package_release.sh then makes a release's files of
# them.
set -euo pipefail

gui=0
windows=0
for argument in "$@"; do
    case "$argument" in
        --gui) gui=1 ;;
        --windows) windows=1 ;;
        *) echo "Unknown option: $argument" >&2; exit 2 ;;
    esac
done

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

if [[ "$(uname -s)" == Darwin ]]; then
    if [[ $windows -eq 1 ]]; then
        echo "Windows' programs are built on Linux" >&2
        exit 2
    fi
    kind=terminal
    window=OFF
    suffix=""
    if [[ $gui -eq 1 ]]; then
        kind=gui
        window=ON
        suffix=_gui
    fi
    output="$root/dist/macos-$(uname -m)-$kind"
    rm -rf "$output"
    mkdir -p "$output"
    work="$(mktemp -d)"
    trap 'rm -rf "$work"' EXIT
    cmake -S "$root" -B "$work/build" -G Ninja -DCMAKE_BUILD_TYPE=MinSizeRel -DOLAGARRO_BUILD_TESTS=OFF \
        -DOLAGARRO_OUI_WINDOW="$window" -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 \
        -DCMAKE_EXE_LINKER_FLAGS=-Wl,-dead_strip -DOLAGARRO_RUNTIME_OUTPUT_DIRECTORY="$work/bin" >/dev/null
    cmake --build "$work/build" --target olagarro_oui_code olagarro_oui_run olagarro_oui_calc >/dev/null
    for program in oui_code oui_run oui_calc; do
        strip -o "$output/$program" "$work/bin/$program$suffix"
        echo "$output/$program: $(stat -f%z "$output/$program") bytes"
    done
    exit 0
fi

engine="$(command -v podman || command -v docker || true)"
if [[ -z "$engine" ]]; then
    echo "podman or docker is required" >&2
    exit 1
fi

kind=terminal
suffix=""
if [[ $gui -eq 1 ]]; then
    kind=gui
    suffix=_gui
fi
if [[ $windows -eq 1 ]]; then
    output="$root/dist/windows-x86_64-$kind"
else
    output="$root/dist/linux-$(uname -m)-$kind"
fi
rm -rf "$output"
mkdir -p "$output"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

# With docker the container writes as root: each opens what it wrote to everyone as it ends, for the trap to clean.
# What every build of a release shares: optimised for size, its programs linked statically, no tests.
common="-G Ninja -DCMAKE_BUILD_TYPE=MinSizeRel -DOLAGARRO_BUILD_TESTS=OFF -DOLAGARRO_STATIC_LINK=ON"
targets="olagarro_oui_code olagarro_oui_run olagarro_oui_calc"

if [[ $windows -eq 1 ]]; then
    # llvm-mingw, a version that does not change under the release, checked against its sum.
    "$engine" run --rm -v "$root:/src:ro,Z" -v "$work:/work:Z" -e "COMMON=$common" -e "TARGETS=$targets" \
        -e "GUI=$gui" -e "SUFFIX=$suffix" docker.io/library/ubuntu:22.04 bash -euc '
        export DEBIAN_FRONTEND=noninteractive
        apt-get update -qq
        apt-get install -qq -y --no-install-recommends cmake ninja-build curl ca-certificates xz-utils >/dev/null
        version=20260922
        name=llvm-mingw-$version-ucrt-ubuntu-22.04-x86_64
        curl -fsSL -o /work/llvm-mingw.tar.xz \
            https://github.com/mstorsjo/llvm-mingw/releases/download/$version/$name.tar.xz
        echo "bb7bb7654b33d5aa8712acb837c963b2e0c56352560c76105270a3268c665c21  /work/llvm-mingw.tar.xz" | sha256sum -c -
        tar -xJf /work/llvm-mingw.tar.xz -C /work
        toolchain=/work/$name/bin
        cat > /work/windows.cmake <<EOF
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
set(CMAKE_C_COMPILER $toolchain/x86_64-w64-mingw32-clang)
set(CMAKE_CXX_COMPILER $toolchain/x86_64-w64-mingw32-clang++)
set(CMAKE_RC_COMPILER $toolchain/x86_64-w64-mingw32-windres)
set(CMAKE_FIND_ROOT_PATH /work/$name/x86_64-w64-mingw32)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
EOF
        window=OFF
        [ "$GUI" = 1 ] && window=ON
        cmake -S /src -B /work/build $COMMON -DCMAKE_TOOLCHAIN_FILE=/work/windows.cmake -DOLAGARRO_OUI_WINDOW=$window \
            -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON -DOLAGARRO_RUNTIME_OUTPUT_DIRECTORY=/work/bin >/dev/null
        cmake --build /work/build --target $TARGETS >/dev/null
        for program in oui_code oui_run oui_calc; do
            $toolchain/x86_64-w64-mingw32-strip -o /work/$program$SUFFIX.exe /work/bin/$program$SUFFIX.exe
        done
        chmod -R a+rwX /work
    '
    programs="oui_code$suffix.exe oui_run$suffix.exe oui_calc$suffix.exe"
elif [[ $gui -eq 1 ]]; then
    "$engine" run --rm -v "$root:/src:ro,Z" -v "$work:/work:Z" -e "COMMON=$common" -e "TARGETS=$targets" \
        docker.io/library/ubuntu:22.04 sh -euc '
        export DEBIAN_FRONTEND=noninteractive
        apt-get update -qq
        apt-get install -qq -y --no-install-recommends g++ cmake ninja-build pkg-config ca-certificates \
            libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxi-dev libxfixes-dev libxss-dev libxtst-dev \
            libxkbcommon-dev libwayland-dev libwayland-bin wayland-protocols libegl-dev libgl-dev libdrm-dev \
            libgbm-dev >/dev/null
        # Ubuntu 22.04 has GCC 11, which warns of uninitialised values where there are none.
        cmake -S /src -B /work/build $COMMON -DOLAGARRO_OUI_WINDOW=ON -DOLAGARRO_RUNTIME_OUTPUT_DIRECTORY=/work/bin \
            -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON -DCMAKE_CXX_FLAGS=-Wno-error=maybe-uninitialized >/dev/null
        cmake --build /work/build --target $TARGETS >/dev/null
        for program in oui_code_gui oui_run_gui oui_calc_gui; do
            strip -o /work/$program /work/bin/$program
        done
        chmod -R a+rwX /work
    '
    programs="oui_code_gui oui_run_gui oui_calc_gui"
else
    "$engine" run --rm -v "$root:/src:ro,Z" -v "$work:/work:Z" -e "COMMON=$common" docker.io/library/alpine:3.20 sh -euc '
        apk add --quiet --no-cache g++ cmake ninja linux-headers clang llvm lld libc++-dev libc++-static \
            llvm-libunwind-static compiler-rt >/dev/null
        CC=clang CXX=clang++ cmake -S /src -B /work/clang $COMMON -DOLAGARRO_RUNTIME_OUTPUT_DIRECTORY=/work/clang/bin \
            -DCMAKE_AR=/usr/bin/llvm-ar -DCMAKE_RANLIB=/usr/bin/llvm-ranlib \
            -DCMAKE_C_FLAGS_MINSIZEREL="-Oz -DNDEBUG" -DCMAKE_CXX_FLAGS_MINSIZEREL="-Oz -DNDEBUG" \
            -DCMAKE_C_FLAGS="-flto=full" -DCMAKE_CXX_FLAGS="-flto=full -stdlib=libc++" \
            -DCMAKE_CXX_STANDARD_LIBRARIES="-lc++abi" \
            -DCMAKE_EXE_LINKER_FLAGS="-flto=full -stdlib=libc++ -rtlib=compiler-rt -unwindlib=libunwind -fuse-ld=lld -Wl,--icf=all" \
            >/dev/null
        cmake --build /work/clang --target olagarro_oui_code olagarro_oui_calc >/dev/null
        cmake -S /src -B /work/gcc $COMMON -DOLAGARRO_RUNTIME_OUTPUT_DIRECTORY=/work/gcc/bin \
            -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON -DCMAKE_C_FLAGS_MINSIZEREL="-Oz -DNDEBUG" \
            -DCMAKE_CXX_FLAGS_MINSIZEREL="-Oz -DNDEBUG" >/dev/null
        cmake --build /work/gcc --target olagarro_oui_run >/dev/null
        strip -o /work/oui_code /work/clang/bin/oui_code
        strip -o /work/oui_calc /work/clang/bin/oui_calc
        strip -o /work/oui_run /work/gcc/bin/oui_run
        chmod -R a+rwX /work
    '
    programs="oui_code oui_run oui_calc"
fi

for program in $programs; do
    cp "$work/$program" "$output/${program/_gui/}"
    echo "$output/${program/_gui/}: $(stat -c%s "$output/${program/_gui/}") bytes"
done
