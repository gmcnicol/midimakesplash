#!/usr/bin/env bash
set -euo pipefail

PLUGIN="Midi Make Splash.vst3"
SRC="$PWD/build/MidiSpash_artefacts/VST3/$PLUGIN"
DST="$HOME/Library/Audio/Plug-Ins/VST3/$PLUGIN"
REAPER_DIR="$HOME/Library/Application Support/REAPER"

echo "Source: $SRC"
echo "Dest:   $DST"

if [ ! -d "$SRC" ]; then
  echo "ERROR: Source plugin bundle not found: $SRC"
  exit 1
fi

echo
echo "Removing REAPER cache entries for $PLUGIN..."
find "$REAPER_DIR" -type f \( -iname "*vst*.ini" -o -iname "*vst*.txt" \) -print0 |
while IFS= read -r -d '' f; do
  if grep -q "$PLUGIN" "$f"; then
    echo "  editing: $f"
    cp "$f" "$f.bak"
    grep -v "$PLUGIN" "$f" > "$f.tmp"
    mv "$f.tmp" "$f"
  fi
done

echo
echo "Installing real plugin bundle, not symlink..."
mkdir -p "$HOME/Library/Audio/Plug-Ins/VST3"
rm -rf "$DST"
cp -R "$SRC" "$DST"

echo
echo "Removing macOS quarantine..."
xattr -dr com.apple.quarantine "$DST" 2>/dev/null || true

echo
echo "Architecture check:"
find "$DST" -type f -perm +111 -print -exec lipo -info {} \; || true

echo
echo "Done. Now open REAPER and run: Preferences > Plug-ins > VST > Re-scan failed plugins"
