#!/usr/bin/env bash
# Makes a release's files from what tools/build_portable.sh left in dist/<system>-<processor>-<kind>/, in
# dist/release/:
#
#   olagarro-<system>-<processor>-<kind>.tar.gz, or .zip for Windows: the programs of that kind, with LICENSE and
#       THIRD_PARTY_NOTICES.txt, the licences of what they carry;
#   <program>-<system>-<processor>-<kind>, with .exe for Windows: each program alone, as oui_code-linux-x86_64-gui;
#   oui-pack-linux-<processor>-<kind>.deb: the Linux programs of a kind as a Debian package, installed in /usr/bin
#       with sudo dpkg -i, and upgraded by installing a newer one the same way; the gui one also puts OUI Code and
#       OUI Calc in the desktop's menu, with their icons, from tools/linux_desktop;
#   THIRD_PARTY_NOTICES.txt: every licence any of them carries, for whoever downloads one program alone;
#   SHA256SUMS: the sum of each of those files.
#
# The names carry no version, so a release's files are always at the same address: GitHub's
# releases/latest/download/<name> is the latest one's, which install.sh and install.ps1 download from.
#
# The packages' version is OLAGARRO_VERSION, as GitHub gives it from the tag, v0.1.0; unless set, git describe's, as
# 0.1.0+3.ged0fda3 three commits after it, which Debian orders after 0.1.0.
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
    notice "NanoSVG, which draws SVG pictures" "$root/third_party/nanosvg/LICENSE.txt"
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
        cp "$program" "$release/$base-$name$extension"
        chmod +x "$release/$base-$name$extension"
    done
done
if [[ $found -eq 0 ]]; then
    echo "Nothing built in dist/: run tools/build_portable.sh first." >&2
    exit 1
fi

# The Linux programs of each kind as a Debian package: the programs in /usr/bin, the licences as Debian keeps them.
version="${OLAGARRO_VERSION:-$(git -C "$root" describe --tags 2>/dev/null || echo 0.0.0)}"
version="${version#v}"
version="$(printf '%s' "$version" | sed -E 's/-([0-9]+)-(g[0-9a-f]+)$/+\1.\2/')"
for built in "$root"/dist/linux-*-terminal "$root"/dist/linux-*-gui; do
    [[ -d "$built" ]] || continue
    name="$(basename "$built")"
    kind="${name##*-}"
    processor="${name#linux-}"
    processor="${processor%-*}"
    case "$processor" in
        x86_64) architecture=amd64 ;;
        aarch64) architecture=arm64 ;;
        *) echo "No Debian architecture for $processor." >&2; exit 1 ;;
    esac
    package=oui-pack
    summary="OUI Code, oui_run and oui_calc, for the terminal"
    depends=""
    conflicts="Conflicts: oui-pack-gui
Replaces: oui-pack-gui
"
    notice_system=linux
    if [[ "$kind" == gui ]]; then
        package=oui-pack-gui
        summary="OUI Code, oui_run and oui_calc, for the terminal and GUI"
        # Built on Ubuntu 22.04: its C library or a newer one; the display libraries SDL3 opens as it starts.
        depends="Depends: libc6 (>= 2.35)
"
        # The same programs under the same names: one package or the other, the one installed taking the other's place.
        conflicts="Conflicts: oui-pack
Replaces: oui-pack
"
        notice_system=none
    fi
    tree="$work/deb-$name"
    mkdir -p "$tree/DEBIAN" "$tree/usr/bin" "$tree/usr/share/doc/$package"
    for program in "$built"/*; do
        install -m 0755 "$program" "$tree/usr/bin/$(basename "$program")"
    done
    { cat "$root/LICENSE"; printf '\n\n'; notices "$notice_system" "$kind"; } > "$tree/usr/share/doc/$package/copyright"
    # Started from the desktop's menu, the gui programs open in their window.
    if [[ "$kind" == gui ]]; then
        mkdir -p "$tree/usr/share/applications" "$tree/usr/share/icons/hicolor/128x128/apps"
        for program in oui_code oui_calc; do
            install -m 0644 "$root/tools/linux_desktop/$program.desktop" "$tree/usr/share/applications/$program.desktop"
            install -m 0644 "$root/tools/linux_desktop/$program.png" \
                "$tree/usr/share/icons/hicolor/128x128/apps/$program.png"
        done
    fi
    # The changelog Debian asks every package for: which version, from where, and when.
    {
        printf '%s (%s) unstable; urgency=medium\n\n' "$package" "$version"
        printf '  * Built from https://github.com/iganinja/olagarro, version %s.\n\n' "$version"
        printf ' -- iganinja <iganinja@users.noreply.github.com>  %s\n' "$(LC_ALL=C date -u -R)"
    } | gzip -9 -n > "$tree/usr/share/doc/$package/changelog.gz"
    # What it takes on disk once installed, in KiB, which apt tells before installing it.
    size="$(du -sk --apparent-size "$tree/usr" | cut -f1)"
    {
        printf 'Package: %s\nVersion: %s\nArchitecture: %s\n' "$package" "$version" "$architecture"
        printf 'Maintainer: iganinja <iganinja@users.noreply.github.com>\n'
        printf '%s' "$depends"
        printf '%s' "$conflicts"
        printf 'Installed-Size: %s\n' "$size"
        printf 'Section: editors\nPriority: optional\nHomepage: https://github.com/iganinja/olagarro\n'
        printf 'Description: %s\n' "$summary"
        printf ' OUI Code is a text editor and file explorer, oui_run runs JavaScript programs\n'
        printf " with OUI's widgets, and oui_calc is a calculator, all made with OUI, a user\n"
        printf ' interface toolkit for the terminal and the desktop.\n'
    } > "$tree/DEBIAN/control"
    # Writable by root alone, whatever the umask of the machine that builds them.
    chmod -R u=rwX,go=rX "$tree"
    dpkg-deb --build --root-owner-group -Zxz "$tree" "$release/oui-pack-$name.deb" >/dev/null
done

notices all all > "$release/THIRD_PARTY_NOTICES.txt"
(cd "$release" && sha256sum -- * > SHA256SUMS)
ls -l "$release"
