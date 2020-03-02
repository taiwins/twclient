#!/bin/bash

pacman -Syu --needed --noconfirm weston libxkbcommon pixman mesa wayland libglvnd vulkan-headers vulkan-icd-loader wayland-protocols fontconfig freetype2 librsvg
pacman -Syu --needed --noconfirm cmake make gcc clang musl git meson ninja rsync pkgconfig
# build step
mkdir -p build && cd build && cmake .. && make
