################################################################################
# library
################################################################################

add_library(twclient
  client.c
  appsurf.c
  ui.c
  egl.c
  buffer.c
  event_queue.c
  #inputs
  keyboard.c
  pointer.c
  #theme
  theme.c
  )

target_include_directories(twclient
  PUBLIC "${TWCLIENT_INCLUDE_DIR}"
  )
add_library(twclient::twclient ALIAS twclient)

target_link_libraries(twclient
  m
  ctypes
  XKBCommon::XKBCommon
  OpenGL::EGL #for egl_env
  UDEV::UDEV
  #wayland
  Wayland::EGL
  Wayland::Client
  Wayland::Cursor
  #protocols
  )

add_library(twclient-icons
  image_cache.c
  icon_search.c
  desktop_entry.c
  )

target_include_directories(twclient-icons PUBLIC
  "${TWCLIENT_INCLUDE_DIR}")

target_link_libraries(twclient-icons
  Cairo::Cairo
  LibRSVG::LibRSVG
  stb
  ctypes
  m
  )
add_library(twclient::icons ALIAS twclient-icons)

add_subdirectory(nk_backends)

################################################################################
# install
################################################################################
include(GNUInstallDirs)
install(TARGETS twclient twclient-icons
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  )

install(DIRECTORY ${TWCLIENT_INCLUDE_DIR}/
  DESTINATION include/taiwins
  FILES_MATCHING PATTERN "*.h"
  PATTERN "nuklear" EXCLUDE
  PATTERN "stb" EXCLUDE)

install(FILES ${TWCLIENT_INCLUDE_DIR}/nuklear/nuklear.h
  DESTINATION include/taiwins/nuklear)
