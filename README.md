Clone via:
* git clone --recurse-submodules https://github.com/logixoul/shade-awesome

Steps to build on Windows:
* winget install -e --id Ninja-build.Ninja
* install vcpkg and set the VCPKG_ROOT environment variable to the vcpkg directory
* cmake --preset=default # I think MSVS does that automatically.

Steps to build on Ubuntu:
* sudo apt update
* sudo apt install curl zip unzip tar pkg-config build-essential ninja-build git # for vcpkg
* sudo apt install libgl1-mesa-dev xorg-dev libglu1-mesa-dev xorg-dev libxinerama-dev libxcursor-dev # for opengl and glfw
* cmake --preset=linux
* cmake --build --preset=linux-release