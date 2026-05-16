Steps to build on Windows:
* winget install -e --id Ninja-build.Ninja
* install vcpkg and set the VCPKG_ROOT environment variable to the vcpkg directory
* cmake --preset=default # I think MSVS does that automatically.

Steps to build on Ubuntu:
* sudo apt update
* sudo apt install -y build-essential libglvnd-dev libgl1-mesa-dev mesa-common-dev pkgconf libglfw3 libglfw3-dev libglm-dev libkissfft-dev
* cmake . -B build -G "Ninja Multi-Config"
* cmake --build build --config Release