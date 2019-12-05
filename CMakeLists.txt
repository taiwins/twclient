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

################################################################################
# compiler flags
################################################################################

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

################################################################################
# find packages
################################################################################

find_package(PkgConfig REQUIRED)
find_package(Pixman REQUIRED)
find_package(XKBCommon REQUIRED)
find_package(Wayland REQUIRED)
find_package(WaylandProtocols REQUIRED)
find_package(Cairo REQUIRED)
find_package(OpenGL REQUIRED COMPONENTS EGL OpenGL)
find_package(NVEGL)
find_package(Fontconfig REQUIRED)
find_package(Freetype REQUIRED)
find_package(UDEV REQUIRED)
find_package(Vulkan REQUIRED)
find_package(LibRSVG REQUIRED)

################################################################################
# subdirs
################################################################################
set(TWCLIENT_INCLUDE_DIR "${CMAKE_CURRENT_LIST_DIR}/include")

add_subdirectory(ctypes)
add_subdirectory(protocols)
add_subdirectory(src)
add_subdirectory(test)

################################################################################
# install
################################################################################
# include(CMakePackageConfigHelpers)
# write_basic_package_version_file("twclientConfigVersion.cmake"
#   VERSION ${twclient_VERSION}
#   COMPATIBILITY SameMajorVersion
#   )
# install(FILES "${CMAKE_CURRENT_BINARY_DIR}/twclientConfigVersion.cmake"
#   DESTINATION lib/cmake/taiwins)
