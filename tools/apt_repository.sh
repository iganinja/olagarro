#!/usr/bin/env bash
# Makes the APT repository of a release's Debian packages, signed, as a folder of files to serve as they are: what
# the release workflow publishes on GitHub Pages, at https://iganinja.github.io/olagarro.
#
#   pool/main/oui-pack_<version>_<architecture>.deb and the same of oui-pack-gui;
#   dists/stable/main/binary-<architecture>/Packages and Packages.gz: what the packages are;
#   dists/stable/Release, its signature Release.gpg, and InRelease, the two in one: what apt checks first;
#   olagarro.gpg: the public key that signs, which a system takes once to trust the repository, as install.sh does.
#
# It holds the packages of this release only: apt upgrade goes to the newest, and older ones stay in their releases.
# It signs with the secret key in gpg's keyring, the only one there should be; OLAGARRO_APT_KEY chooses among several.
#
# usage: tools/apt_repository.sh <folder with the .deb files> <folder to make>
set -euo pipefail

if [[ $# -ne 2 ]]; then
    echo "usage: $0 <folder with the .deb files> <folder to make>" >&2
    exit 1
fi
packages="$1"
site="$2"

key="${OLAGARRO_APT_KEY:-$(gpg --batch --list-secret-keys --with-colons | awk -F: '$1 == "fpr" {print $10; exit}')}"
if [[ -z "$key" ]]; then
    echo "There is no secret key in gpg's keyring to sign with." >&2
    exit 1
fi

shopt -s nullglob
debs=("$packages"/*.deb)
if [[ ${#debs[@]} -eq 0 ]]; then
    echo "There is no .deb in $packages." >&2
    exit 1
fi

rm -rf "$site"
mkdir -p "$site/pool/main"
architectures=()
for deb in "${debs[@]}"; do
    package="$(dpkg-deb --field "$deb" Package)"
    version="$(dpkg-deb --field "$deb" Version)"
    architecture="$(dpkg-deb --field "$deb" Architecture)"
    cp "$deb" "$site/pool/main/${package}_${version}_${architecture}.deb"
    [[ " ${architectures[*]} " == *" $architecture "* ]] || architectures+=("$architecture")
done

cd "$site"
for architecture in "${architectures[@]}"; do
    folder="dists/stable/main/binary-$architecture"
    mkdir -p "$folder"
    apt-ftparchive --arch "$architecture" packages pool > "$folder/Packages"
    gzip -9 -n -k "$folder/Packages"
done
apt-ftparchive \
    -o APT::FTPArchive::Release::Origin=olagarro \
    -o APT::FTPArchive::Release::Label=olagarro \
    -o APT::FTPArchive::Release::Suite=stable \
    -o APT::FTPArchive::Release::Codename=stable \
    -o APT::FTPArchive::Release::Components=main \
    -o "APT::FTPArchive::Release::Architectures=${architectures[*]}" \
    -o APT::FTPArchive::Release::Description="OUI Code, oui_run and oui_calc" \
    release dists/stable > dists/stable/Release.new
mv dists/stable/Release.new dists/stable/Release
gpg --batch --yes --local-user "$key" --armor --detach-sign --output dists/stable/Release.gpg dists/stable/Release
gpg --batch --yes --local-user "$key" --clearsign --output dists/stable/InRelease dists/stable/Release
gpg --batch --export "$key" > olagarro.gpg
echo "The APT repository is in $site, signed with $key."
