#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
source_svg="$repo_root/icons/icon_agenc.svg"

if ! command -v magick >/dev/null 2>&1; then
  echo "ImageMagick 'magick' is required to generate Ledger icon assets." >&2
  exit 1
fi

if [[ ! -f "$source_svg" ]]; then
  echo "Missing AgenC logo SVG: $source_svg" >&2
  exit 1
fi

make_icon() {
  local size="$1"
  local inner="$2"
  local out="$3"
  local colors="$4"

  magick -background none "$source_svg" -alpha set -fill '#191919' -opaque '#f8f7ff' \
    -resize "${inner}x${inner}" -background white -gravity center -extent "${size}x${size}" \
    -colorspace Gray -colors "$colors" "$out"
}

make_png8_icon() {
  local size="$1"
  local inner="$2"
  local out="$3"
  local colors="$4"

  magick -background none "$source_svg" -alpha set -fill '#191919' -opaque '#f8f7ff' \
    -resize "${inner}x${inner}" -background white -gravity center -extent "${size}x${size}" \
    -colorspace Gray -colors "$colors" -depth 8 "PNG8:$out"
}

make_white_icon() {
  local size="$1"
  local inner="$2"
  local out="$3"

  magick -background none "$source_svg" -alpha set -fill white -opaque '#f8f7ff' \
    -resize "${inner}x${inner}" -background black -gravity center -extent "${size}x${size}" \
    -colorspace Gray -colors 2 "$out"
}

make_icon 14 12 "$repo_root/icons/icon_agenc_14px.gif" 2
make_icon 32 27 "$repo_root/icons/icon_agenc_32px.gif" 16
make_icon 40 34 "$repo_root/icons/icon_agenc_40px.gif" 16
make_png8_icon 32 27 "$repo_root/icons/icon_agenc_32px_apex.png" 16

make_icon 14 12 "$repo_root/glyphs/home_agenc_14px.gif" 2
make_white_icon 14 12 "$repo_root/glyphs/home_agenc_white_14px.gif"
make_png8_icon 48 40 "$repo_root/glyphs/home_agenc_48px.png" 16
make_icon 64 54 "$repo_root/glyphs/home_agenc_64px.gif" 16

echo "Generated Ledger icon assets from: $source_svg"
