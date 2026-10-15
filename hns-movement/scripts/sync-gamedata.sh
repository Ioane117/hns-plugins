#!/bin/bash
# Pull the latest gamedata from upstream CS2KZ after a CS2 update.
# hns-movement uses the same signatures as CS2KZ, so its gamedata is a straight copy of cs2kz-core.games.txt.
#
# Usage: scripts/sync-gamedata.sh [branch-or-commit]   (default: master)
set -euo pipefail
REF="${1:-master}"
URL="https://raw.githubusercontent.com/KZGlobalTeam/cs2kz-metamod/${REF}/gamedata/cs2kz-core.games.txt"
DEST="$(dirname "$0")/../gamedata/hns-movement.games.txt"
curl -fsSL "$URL" -o "$DEST.tmp"
mv "$DEST.tmp" "$DEST"
echo "Updated $DEST from cs2kz-metamod@$REF"
git -C "$(dirname "$0")/.." diff --stat -- gamedata/ || true
