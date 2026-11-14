#!/usr/bin/env bash
# Fetch pinned third-party sources (vendored, built static; see docs/DECISIONS.md D-005).
set -euo pipefail
cd "$(dirname "$0")/.."
SDL_TAG=release-3.4.8
if [ ! -f third_party/SDL/CMakeLists.txt ]; then
  mkdir -p third_party
  git clone --depth 1 --branch "$SDL_TAG" https://github.com/libsdl-org/SDL.git third_party/SDL
  rm -rf third_party/SDL/.git
  echo "SDL $SDL_TAG fetched."
else
  echo "SDL already present."
fi
