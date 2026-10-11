#!/usr/bin/env bash
# Makes a release's files from what tools/build_portable.sh left in dist/<system>-<processor>-<kind>/, in
# dist/release/:
#
#   olagarro-<system>-<processor>-<kind>.tar.gz, or .zip for Windows: the programs of that kind, with LICENSE and
#       THIRD_PARTY_NOTICES.txt, the licences of what they carry;
#   <program>-<system>-<processor>-<kind>, with .exe for Windows: each program alone, as oui_code-linux-x86_64-gui;
#   THIRD_PARTY_NOTICES.txt: every licence any of them carries, for whoever downloads one program alone;
#   SHA256SUMS: the sum of each of those files.
#
# The names carry no version, so a release's files are always at the same address: GitHub's
# releases/latest/download/<name> is the latest one's, which install.sh and install.ps1 download from.
#
# usage: tools/package_release.sh
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
release="$root/dist/release"
rm -rf "$release"
mkdir -p "$release"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

# One licence, under a heading saying whose it is and why it is here.
notice() {
    printf '%s\n%s\n\n' "$1" "$(printf '%s' "$1" | sed 's/./=/g')"
    cat "$2"
    printf '\n\n'
}

# The licences of what programs of a system and kind carry.
notices() {
    local system="$1" kind="$2"
    printf 'The programs carry code from these projects, under their licences.\n\n'
    notice "QuickJS-ng, the JavaScript engine" "$root/third_party/quickjs-ng/LICENSE"
    notice "stb_truetype, which reads the interface font" "$root/licenses/stb_truetype.txt"
    notice "Spleen, the cell font" "$root/oui/fonts/spleen_license.txt"
    notice "Jersey 10, the interface font" "$root/oui/fonts/jersey10_ofl.txt"
    if [[ "$kind" == gui || "$kind" == all ]]; then
        notice "SDL3, the desktop window (the gui programs)" "$root/licenses/SDL3.txt"
    fi
    if [[ "$system" == linux || "$system" == all ]]; then
        notice "musl, the C library (the Linux terminal programs)" "$root/licenses/musl.txt"
    fi
    if [[ "$system" == windows || "$system" == all ]]; then
        notice "The mingw-w64 runtime (the Windows programs)" "$root/licenses/mingw-w64-runtime.txt"
        notice "winpthreads (the Windows programs)" "$root/licenses/winpthreads.txt"
    fi
}

found=0
for built in "$root"/dist/*-*-terminal "$root"/dist/*-*-gui; do
    [[ -d "$built" ]] || continue
    found=1
    name="$(basename "$built")"
    system="${name%%-*}"
    kind="${name##*-}"
    # The Linux gui programs take their C library from the system: musl's licence is the terminal ones' only.
    notice_system="$system"
    [[ "$system" == linux && "$kind" == gui ]] && notice_system=none
    package="$work/olagarro-$name"
    mkdir -p "$package"
    cp "$built"/* "$package/"
    # Programs that went through GitHub's artifacts come back without the bit that lets them run.
    chmod +x "$package"/oui_*
    cp "$root/LICENSE" "$package/LICENSE"
    notices "$notice_system" "$kind" > "$package/THIRD_PARTY_NOTICES.txt"
    if [[ "$system" == windows ]]; then
        (cd "$work" && zip -q -r -X "$release/olagarro-$name.zip" "olagarro-$name")
    else
        tar -C "$work" --owner=0 --group=0 --numeric-owner -czf "$release/olagarro-$name.tar.gz" "olagarro-$name"
    fi
    for program in "$built"/*; do
        file="$(basename "$program")"
        extension=""
        [[ "$file" == *.exe ]] && extension=.exe
        base="${file%.exe}"
        base="${base%_gui}"
        cp "$program" "$release/$base-$name$extension"
        chmod +x "$release/$base-$name$extension"
    done
done
if [[ $found -eq 0 ]]; then
    echo "Nothing built in dist/: run tools/build_portable.sh first." >&2
    exit 1
fi

notices all all > "$release/THIRD_PARTY_NOTICES.txt"
(cd "$release" && sha256sum -- * > SHA256SUMS)
ls -l "$release"
