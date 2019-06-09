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
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O1" -ggdb3 -fsanitize=address -fno-omit-frame-pointer")
endif()

if(CMAKE_BUILD_TYPE MATCHES Release)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC -O2")
endif()


###################### packages ###########################
find_package(Pixman REQUIRED)
find_package(PkgConfig)
find_package(XKBCommon REQUIRED)
find_package(Wayland REQUIRED)
find_package(Cairo REQUIRED)
find_package(OpenGL REQUIRED COMPONENTS EGL OpenGL)
find_package(LuaJIT REQUIRED)
find_package(NVEGL)
find_package(fontconfig REQUIRED)
find_package(Freetype REQUIRED)
find_package(Vulkan REQUIRED)

###########################################################
#we have to slove this
if(CTYPES_INCLUDE_DIR AND CTYPES_LIBRARIES)
  #check whether we include
  if(CTYPES_INCLUDE_DIR MATCHES "${CMAKE_CURRENT_LIST_DIR}/ctypes/src")
    message("using self contained ctypes library")
    add_subdirectory(ctypes)
  else()
    message("using external ctypes library")
  endif()
else()
  message("using self contained ctypes library")
  add_subdirectory(ctypes)
endif()

set(TWCLIENT_INCLUDE_DIRS
  ${CTYPES_INCLUDE_DIR}
  ${XKBCOMMON_INCLUDE_DIRS}
  ${WAYLAND_CLIENT_INCLUDE_DIR}
  ${CAIRO_INCLUDE_DIRS}
  ${WAYLAND_CURSOR_INCLUDE_DIR}
  ${WAYLAND_EGL_INCLUDE_DIR}
  ${LUA_INCLUDE_DIR}
  ${PIXMAN_INCLUDE_DIRS}
  ${FONTCONFIG_INCLUDE_DIR}
  ${FREETYPE_INCLUDE_DIRS}
  ${CMAKE_CURRENT_LIST_DIR}
  CACHE PATH "twclient include dir"
  )

list(REMOVE_DUPLICATES TWCLIENT_INCLUDE_DIRS)

set(TWCLIENT_TRD_LIB
  ${CTYPES_LIBRARIES}
  ${XKBCOMMON_LIBRARIES}
  ${WAYLAND_CLIENT_LIBRARIES}
  ${CAIRO_LIBRARIES}
  ${WAYLAND_EGL_LIBRARIES}
  ${WAYLAND_CURSOR_LIBRARIES}
  ${LUA_LIBRARIES}
  OpenGL::EGL
  OpenGL::OpenGL
  Vulkan::Vulkan
  ${PIXMAN_LIBRARIES}
  ${FONTCONFIG_LIBRARIES}
  ${FREETYPE_LIBRARIES}
  CACHE STRING "external requirements"
  )

add_library(twclient
  client.c
  appsurf.c
  ui.c
  egl.c
  buffer.c
  )

target_include_directories(twclient PUBLIC
  ${TWCLIENT_INCLUDE_DIRS}
  )

add_subdirectory(nk_backends)

if(NK_EGL_BACKEND)
  set(NK_BACKEND_LIB nk_egl_backend)
elseif(NK_CAIRO_BACKEND)
  set(NK_BACKEND_LIB nk_cairo_backend)
elseif(NK_VK_BACKEND)
  set(NK_BACKEND_LIB nk_vk_backend)
endif()

set(TWCLIENT_LIBRARIES
  twclient
  ${NK_BACKEND_LIB}
  ${TWCLIENT_TRD_LIB}
  CACHE STRING "twclient libraries"
  )


add_subdirectory(test)
