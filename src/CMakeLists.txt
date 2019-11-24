include(Wayland)

add_library(twclient
  client.c
  appsurf.c
  ui.c
  egl.c
  buffer.c
  event_queue.c
  image_cache.c
  desktop_entry.c
  #inputs
  keyboard.c
  pointer.c)

target_link_libraries(twclient
  m
  ctype
  OpenGL::EGL
  ${XKBCOMMON_LIBRARIES}
  ${WAYLAND_CLIENT_LIBRARIES}
  ${CAIRO_LIBRARIES}
  ${WAYLAND_EGL_LIBRARIES}
  ${WAYLAND_CURSOR_LIBRARIES}
  ${PIXMAN_LIBRARIES}
  ${UDEV_LIBRARIES}
  ${WAYLAND_EGL_LIBRARIES}
  ${FONTCONFIG_LIBRARIES}
  )

target_include_directories(twclient PUBLIC
  ${CTYPES_INCLUDE_DIR}
  ${TWCLIENT_PROTO_INCLUDE_DIR}
  ${TWCLIENT_INCLUDE_DIRS}
  ${CMAKE_CURRENT_BINARY_DIR})

set(TWCLIENT_LIBRARIES
  twclient
  ctype
  twclient_proto_libraries
  CACHE STRING "twclient libraries"
  )

#library used for
set(TWCLIENT_TRD_LIB
  ${CTYPES_LIBRARIES}
  ${XKBCOMMON_LIBRARIES}
  ${WAYLAND_CLIENT_LIBRARIES}
  ${CAIRO_LIBRARIES}
  ${WAYLAND_EGL_LIBRARIES}
  ${WAYLAND_CURSOR_LIBRARIES}
  OpenGL::EGL
  OpenGL::OpenGL
  Vulkan::Vulkan
  ${PIXMAN_LIBRARIES}
  ${FONTCONFIG_LIBRARIES}
  ${FREETYPE_LIBRARIES}
  ${UDEV_LIBRARIES}
  m

  PARENT_SCOPE
  )

add_subdirectory(nk_backends)
