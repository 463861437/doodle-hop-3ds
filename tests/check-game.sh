#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
mkdir -p build-host
"${HOST_CXX:-c++}" -std=c++17 -O1 -g -Wall -Wextra -Wpedantic -Werror \
    -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude \
    tests/test_game.cpp source/game.cpp -o build-host/test-game
./build-host/test-game
