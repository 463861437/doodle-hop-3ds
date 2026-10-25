#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
mkdir -p build-host
# Optional argument: a complete older project checkout. Both renderers receive
# the same fixtures, compiler, optimization and physical backbuffer alternation.
if [ "$#" -gt 1 ]; then
    printf '%s\n' 'Usage: tests/benchmark-render.sh [baseline-project-directory]' >&2
    exit 2
fi
if [ "$#" -eq 1 ]; then
    baseline=$(CDPATH= cd -- "$1" && pwd)
    "${HOST_CXX:-c++}" -std=c++17 -O2 -Wall -Wextra -Wpedantic -Werror \
        '-DHOP_BENCHMARK_LABEL="baseline"' -I"$baseline/include" \
        tests/benchmark-render.cpp "$baseline/source/render.cpp" -o build-host/benchmark-render-baseline
    ./build-host/benchmark-render-baseline
fi
"${HOST_CXX:-c++}" -std=c++17 -O2 -Wall -Wextra -Wpedantic -Werror \
    -Iinclude tests/benchmark-render.cpp source/render.cpp -o build-host/benchmark-render
./build-host/benchmark-render
