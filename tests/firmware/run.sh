#!/usr/bin/env bash
# Compila wokwi/final/sketch.ino en el PC con sustitutos de Arduino y ejecuta las comprobaciones.
# Requiere g++ (C++11). No sustituye la compilación real en Wokwi/Arduino IDE.
set -euo pipefail
cd "$(dirname "$0")"
g++ -std=gnu++11 -Wall -Wextra -Wno-unused-parameter -Istubs harness.cpp -o harness_bin
./harness_bin
