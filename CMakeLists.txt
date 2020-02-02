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
  set(CMAKE_BUILD_TYPE Release)
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

################################################################################
# include
################################################################################
add_library(nuklear INTERFACE)
target_include_directories(nuklear INTERFACE "${TWCLIENT_INCLUDE_DIR}")

add_library(stb INTERFACE)
target_include_directories(stb INTERFACE "${TWCLIENT_INCLUDE_DIR}")
target_compile_options(stb INTERFACE "-Wno-type-limits")

add_subdirectory(ctypes)
add_subdirectory(protocols)
add_subdirectory(src)
add_subdirectory(test)

################################################################################
# install
################################################################################
include(GNUInstallDirs)
foreach(PCIN "config/twclient.pc.in"
    "config/twclient-icons.pc.in"
    "config/twclient-nkcairo.pc.in"
    "config/twclient-nkegl.pc.in")
  get_filename_component(FILE ${PCIN} NAME_WE)
  # get_filename_component(PREFIX $ENV{DESTDIR}/${CMAKE_INSTALL_PREFIX} ABSOLUTE) #for debug
  set(PREFIX ${CMAKE_INSTALL_PREFIX})
  set(LIBPREFIX ${CMAKE_INSTALL_LIBDIR})
  set(VERSION ${PROJECT_VERSION})
  set(PCOUT "${CMAKE_BINARY_DIR}/${FILE}.pc")
  configure_file(${PCIN} ${PCOUT} @ONLY)
  install(FILES ${PCOUT}
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)
  unset(PREFIX)
  unset(LIBPREFIX)
  unset(VERSION)
  unset(PCOUT)
endforeach()

include(CMakePackageConfigHelpers)
write_basic_package_version_file("twclient-config-version.cmake"
  VERSION ${twclient_VERSION}
  COMPATIBILITY SameMajorVersion
  )
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/twclient-config-version.cmake"
  "config/twclient-config.cmake"
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/twclient)
