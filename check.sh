#!/bin/sh
set -eu
cd "$(dirname "$0")"
./tests/check-game.sh
./tests/check-platform.sh
./tests/check-audio.sh
./tests/check-main.sh
./tests/check-render.sh
