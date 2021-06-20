#!/bin/bash
set -e

sudo apt-get update

sudo apt-get -y install cmake gcc-8 g++-8

sudo apt-get -y --no-install-recommends install \
     autoconf \
     automake \
     build-essential \
     curl \
     doxygen \
     freerdp2-dev \
     git \
     libcairo2-dev \
     libcolord-dev \
     libdbus-1-dev \
     libegl1-mesa-dev \
     libevdev-dev \
     libexpat1-dev \
     libffi-dev \
     libgbm-dev \
     libgdk-pixbuf2.0-dev \
     libgles2-mesa-dev \
     libglu1-mesa-dev \
     libgstreamer1.0-dev \
     libgstreamer-plugins-base1.0-dev \
     libinput-dev \
     libjpeg-dev \
     liblcms2-dev \
     libmtdev-dev \
     libpam0g-dev \
     libpango1.0-dev \
     libpixman-1-dev \
     libpng-dev \
     libsystemd-dev \
     libtool \
     libudev-dev \
     libva-dev \
     libvpx-dev \
     libwayland-dev \
     libwebp-dev \
     libxkbcommon-dev \
     libasound2-dev \
     libxml2-dev \
     mesa-common-dev \
     ninja-build \
     pkg-config \
     libglvnd-dev \
     libfontconfig1-dev \
     libfreetype6-dev \
     librsvg2-dev \
     liblua5.3-dev \
     libpam0g-dev

# for meson
sudo apt-get -y install \
     python3-pip \
     python3-setuptools

#register currdir
export CURDIR=$(pwd)

#install meson
export PATH=~/.local/bin:$PATH
pip3 install --user git+https://github.com/mesonbuild/meson.git@0.54

# #install upstream wayland
# git clone --depth=1 -b 1.19.0 https://gitlab.freedesktop.org/wayland/wayland /tmp/wayland
# export MAKEFLAGS="-j4"
# cd /tmp/wayland
# mkdir build
# cd build
# ../autogen.sh --disable-documentation
# sudo make install

#install wayland-protocols
# git clone --depth=1 -b 1.20 https://gitlab.freedesktop.org/wayland/wayland-protocols /tmp/wayland-protocols
# cd /tmp/wayland-protocols
# mkdir build
# cd build
# ../autogen.sh
# sudo make install

cd $(CURDIR)

#adding user to the input group for testing
sudo usermod -a -G input $(echo ${USER})
