#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "${BASH_SOURCE[0]}")/.."
deps=${DEPS_DIR:-.deps}
mkdir -p "$deps"
temp=$(mktemp -d)
trap 'rm -rf "$temp"' EXIT

fetch() {
    local url=$1 hash=$2 file=$3
    curl --fail --silent --show-error --location --retry 3 "$url" -o "$file"
    printf '%s  %s\n' "$hash" "$file" | sha256sum --check
}

archive() {
    local url=$1 hash=$2 name=$3
    if [[ ! -d "$deps/$name" ]]; then
        fetch "$url" "$hash" "$temp/$name.tar.gz"
        mkdir "$temp/$name"
        tar -xzf "$temp/$name.tar.gz" --strip-components=1 -C "$temp/$name"
        mv "$temp/$name" "$deps/$name"
    fi
}

archive https://codeload.github.com/hathach/tinyusb/tar.gz/refs/tags/0.18.0 \
    e7fa1bd723213749a0362c79eaccc99e84c8adea8f0a63588c4e4812608b7aa9 tinyusb
archive https://codeload.github.com/NordicSemiconductor/nrfx/tar.gz/7c47cc0a56ce44658e6da2458e86cd8783ccc4a2 \
    0f31fae4297f8307448f32ac19195df6a5e3b0c62ae5c123aa3895cc95dc8816 nrfx
archive https://codeload.github.com/ARM-software/CMSIS_5/tar.gz/2b7495b8535bdcb306dac29b9ded4cfb679d7e5c \
    9f7f0080795b1635b82d9f7b084d37011866cd78365749997391ba0f9602f9fe cmsis

base=https://raw.githubusercontent.com/Saniro1/nRF52840-LSM6DVS-1000Hz-Firmware-Test/6c3f1a707a19950a8a5af6eb34e7db7001d0f096/releases/ds4-fix42
mkdir -p "$deps/fix42"
for record in \
    'normal d09a01816048f5a84cdda909d03fec308e2184dbdedcba400b1055aec664bb79' \
    'inverted a29b8a0ed1b154d6d51425d8261a2b5a632471d69dbdd705d9581bdf42d8db68' \
    'handheld 36906cdb55bee571a509c97bb1a0711694115642ea8d68debe743365311838ba'; do
    read -r profile hash <<< "$record"
    file="fix42-ds4-$profile.uf2"
    if [[ ! -f "$deps/fix42/$file" ]]; then
        fetch "$base/$file" "$hash" "$temp/$file"
        mv "$temp/$file" "$deps/fix42/$file"
    fi
done
