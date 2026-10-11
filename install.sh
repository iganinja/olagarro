#!/bin/sh
# Installs OUI Code, oui_run and oui_calc on Linux from the latest release: each one file, with nothing to install
# beside it and no root needed. Or downloads a whole package, the programs with their licences.
#
#   curl -fsSL https://raw.githubusercontent.com/iganinja/olagarro/master/install.sh | sh
#
# With no options it shows a menu of what there is and asks. Options choose without asking, after "sh -s --":
#
#   oui_code, oui_run or oui_calc   that program alone (oui_code unless one is given with the others)
#   --all                           the three
#   --gui                           those that also open in a window of their own, oui_code_gui and the rest,
#                                   rather than those for the terminal only
#   --package                       the whole package, the three with LICENSE and THIRD_PARTY_NOTICES.txt, unpacked
#                                   into the folder it is run from rather than installed
#   --apt                           the three from the project's APT repository, on Debian, Ubuntu and the like, so
#                                   apt upgrade keeps them up to date: it adds the repository and its key, with sudo,
#                                   and installs the package oui-pack, or oui-pack-gui with --gui, in /usr/bin
#
#   curl -fsSL https://raw.githubusercontent.com/iganinja/olagarro/master/install.sh | sh -s -- --all --gui
#
# Every download is checked against the release's SHA256SUMS before it is used. OUI_INSTALL_DIR says where programs
# go, ~/.local/bin unless set; OUI_DOWNLOAD_URL where they come from, the latest release unless set; OUI_APT_URL the
# APT repository, https://iganinja.github.io/olagarro unless set.
set -eu

source=${OUI_DOWNLOAD_URL:-https://github.com/iganinja/olagarro/releases/latest/download}
target=${OUI_INSTALL_DIR:-$HOME/.local/bin}
apt_source=${OUI_APT_URL:-https://iganinja.github.io/olagarro}

fail() {
    echo "$*" >&2
    exit 1
}

if [ "$(uname -s)" != Linux ]; then
    fail "This installer is for Linux. On Windows, install.ps1 does the same: see the README."
fi
case "$(uname -m)" in
    x86_64 | amd64) processor=x86_64 ;;
    *) fail "The releases have no programs for $(uname -m) yet, only for x86_64." ;;
esac

# What to get: the programs, the kind, terminal or gui, and whether the whole package.
programs=""
kind=terminal
package=0
apt=0
asked=0
for argument in "$@"; do
    case "$argument" in
        oui_code | oui_run | oui_calc) programs="$programs $argument" ;;
        --all) programs="oui_code oui_run oui_calc" ;;
        --gui) kind=gui ;;
        --package) package=1 ;;
        --apt) apt=1 ;;
        *) fail "Unknown option: $argument. It takes oui_code, oui_run, oui_calc, --all, --gui, --package and --apt." ;;
    esac
    asked=1
done

if [ $asked -eq 0 ]; then
    # Through a pipe the script is what comes in: the answer comes from the terminal itself.
    # In a subshell, as a failed redirection of ":" would end the whole script.
    if ! (true < /dev/tty) 2>/dev/null; then
        fail "There is no terminal to ask in: say what to install, as \"sh -s -- --all\" or \"sh -s -- oui_code --gui\"."
    fi
    cat > /dev/tty <<'EOF'
Olagarro: what would you like?

  Installed in ~/.local/bin, one file each:
   1) OUI Code, the text editor                    terminal
   2) OUI Code, the text editor                    terminal and GUI
   3) oui_run, which runs JavaScript programs      terminal
   4) oui_run, which runs JavaScript programs      terminal and GUI
   5) oui_calc, a calculator                       terminal
   6) oui_calc, a calculator                       terminal and GUI
   7) All three                                    terminal
   8) All three                                    terminal and GUI

  The whole package, with its licences, unpacked into this folder:
   9) All three                                    terminal
  10) All three                                    terminal and GUI

  Through apt, which keeps them up to date (Debian, Ubuntu and the like; asks for sudo):
  11) All three                                    terminal
  12) All three                                    terminal and GUI

   0) Nothing

EOF
    while :; do
        printf 'Your choice [1]: ' > /dev/tty
        read -r choice < /dev/tty || choice=0
        case "${choice:-1}" in
            1) programs=oui_code ;;
            2) programs=oui_code; kind=gui ;;
            3) programs=oui_run ;;
            4) programs=oui_run; kind=gui ;;
            5) programs=oui_calc ;;
            6) programs=oui_calc; kind=gui ;;
            7) programs="oui_code oui_run oui_calc" ;;
            8) programs="oui_code oui_run oui_calc"; kind=gui ;;
            9) package=1 ;;
            10) package=1; kind=gui ;;
            11) apt=1 ;;
            12) apt=1; kind=gui ;;
            0) echo "Nothing installed."; exit 0 ;;
            *) echo "That is no choice: a number from 0 to 12." > /dev/tty; continue ;;
        esac
        break
    done
fi
[ -n "$programs" ] || programs=oui_code

# The window programs take the C library and the display libraries from the system: glibc 2.35 or newer.
if [ "$kind" = gui ]; then
    glibc=$(getconf GNU_LIBC_VERSION 2>/dev/null | awk '{print $2}') || glibc=""
    if [ -z "$glibc" ]; then
        fail "The GUI programs need glibc, which this system has not: take those for the terminal."
    fi
    major=${glibc%%.*}
    minor=${glibc#*.}
    minor=${minor%%.*}
    if [ "$major" -lt 2 ] || { [ "$major" -eq 2 ] && [ "$minor" -lt 35 ]; }; then
        fail "The GUI programs need glibc 2.35 or newer, and this system has $glibc: take those for the terminal."
    fi
fi

if command -v curl >/dev/null 2>&1; then
    fetch() { curl -fsSL "$1" -o "$2"; }
elif command -v wget >/dev/null 2>&1; then
    fetch() { wget -q "$1" -O "$2"; }
else
    fail "Neither curl nor wget is here to download with."
fi
if command -v sha256sum >/dev/null 2>&1; then
    sum() { sha256sum "$1" | awk '{print $1}'; }
elif command -v shasum >/dev/null 2>&1; then
    sum() { shasum -a 256 "$1" | awk '{print $1}'; }
else
    fail "Neither sha256sum nor shasum is here to check the download with."
fi

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

# The repository's key and its line among apt's sources, once; apt installs the package, and upgrades it from then on.
if [ $apt -eq 1 ]; then
    command -v apt-get >/dev/null 2>&1 || fail "There is no apt here: install the programs themselves, without --apt."
    if [ "$(id -u)" -eq 0 ]; then
        as_root() { "$@"; }
    elif command -v sudo >/dev/null 2>&1; then
        as_root() { sudo "$@"; }
    else
        fail "Adding the APT repository takes root, and there is no sudo here."
    fi
    deb_package=oui-pack
    [ "$kind" = gui ] && deb_package=oui-pack-gui
    fetch "$apt_source/olagarro.gpg" "$work/olagarro.gpg" || fail "The repository's key could not be downloaded from $apt_source."
    echo "Adding the APT repository $apt_source, with sudo..."
    as_root mkdir -p /etc/apt/keyrings
    as_root install -m 0644 "$work/olagarro.gpg" /etc/apt/keyrings/olagarro.gpg
    echo "deb [signed-by=/etc/apt/keyrings/olagarro.gpg] $apt_source stable main" > "$work/olagarro.list"
    as_root install -m 0644 "$work/olagarro.list" /etc/apt/sources.list.d/olagarro.list
    # Through a pipe the script is what comes in: apt must not read the rest of it as answers.
    as_root apt-get update < /dev/null
    as_root apt-get install -y "$deb_package" < /dev/null
    echo "Installed $deb_package: apt upgrade keeps it up to date."
    exit 0
fi

fetch "$source/SHA256SUMS" "$work/SHA256SUMS" || fail "The release's SHA256SUMS could not be downloaded from $source."

# A file of the release, downloaded and checked against its sum.
download() {
    echo "Downloading $1..."
    fetch "$source/$1" "$work/$1" || fail "$1 could not be downloaded from $source."
    expected=$(awk -v name="$1" '$2 == name {print $1}' "$work/SHA256SUMS")
    [ -n "$expected" ] || fail "The release's SHA256SUMS does not list $1: nothing done."
    [ "$(sum "$work/$1")" = "$expected" ] || fail "$1 is not what the release says it is: nothing done."
}

if [ $package -eq 1 ]; then
    name="olagarro-linux-$processor-$kind"
    [ -e "$name" ] && fail "$name is here already: nothing done."
    download "$name.tar.gz"
    tar -xzf "$work/$name.tar.gz"
    echo "Unpacked into $(pwd)/$name: the programs, LICENSE and THIRD_PARTY_NOTICES.txt."
    exit 0
fi

# Every program downloaded and checked before any is installed: a failure leaves nothing half done.
for program in $programs; do
    download "$program-linux-$processor-$kind"
done

mkdir -p "$target"
installed=""
for program in $programs; do
    name=$program
    [ "$kind" = gui ] && name=${program}_gui
    chmod +x "$work/$program-linux-$processor-$kind"
    mv "$work/$program-linux-$processor-$kind" "$target/$name"
    echo "Installed $target/$name"
    installed="$installed $name"
done
case ":$PATH:" in
    *":$target:"*) echo "Run it:$installed" ;;
    *) echo "$target is not in your PATH: add it, or run $target/$(echo $installed | cut -d' ' -f1)." ;;
esac
if [ "$kind" = gui ]; then
    echo "They open in the terminal; with --window, in a window of their own."
fi
