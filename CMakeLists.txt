cmake_minimum_required(VERSION 3.1.0)
project(twclient
  VERSION 0.1
  LANGUAGES C CXX)

add_definitions(-D_GNU_SOURCE)
set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH}" "${PROJECT_SOURCE_DIR}/cmake")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_BINARY_DIR "${CMAKE_SOURCE_DIR}/build")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

################### compiler flags ########################

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()

if(CMAKE_BUILD_TYPE MATCHES Debug)
  add_definitions(-D__DEBUG)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ggdb3 -Wall -fsanitize=address -fno-omit-frame-pointer")
endif()

if(CMAKE_BUILD_TYPE MATCHES Release)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC -O2")
endif()


###################### packages ###########################
find_package(Pixman REQUIRED)
find_package(PkgConfig)
find_package(XKBCommon REQUIRED)
find_package(Wayland REQUIRED)
find_package(WaylandProtocols REQUIRED)
find_package(Cairo REQUIRED)
find_package(OpenGL REQUIRED COMPONENTS EGL OpenGL)
find_package(NVEGL)
find_package(fontconfig REQUIRED)
find_package(Freetype REQUIRED)
find_package(UDEV REQUIRED)
find_package(Vulkan REQUIRED)
find_package(LibRSVG REQUIRED)

###########################################################

set(TWCLIENT_INCLUDE_DIRS
  ${CTYPES_INCLUDE_DIR}
  ${XKBCOMMON_INCLUDE_DIRS}
  ${WAYLAND_CLIENT_INCLUDE_DIR}
  ${CAIRO_INCLUDE_DIRS}
  ${WAYLAND_CURSOR_INCLUDE_DIR}
  ${WAYLAND_EGL_INCLUDE_DIR}
  ${PIXMAN_INCLUDE_DIRS}
  ${FONTCONFIG_INCLUDE_DIR}
  ${FREETYPE_INCLUDE_DIRS}
  ${UDEV_INCLUDE_DIRS}
  ${LibRSVG_INCLUDE_DIRS}
  "${CMAKE_CURRENT_LIST_DIR}/include"
  )

list(REMOVE_DUPLICATES TWCLIENT_INCLUDE_DIRS)


add_subdirectory(ctypes)
add_subdirectory(protocols)
add_subdirectory(src)
add_subdirectory(test)
