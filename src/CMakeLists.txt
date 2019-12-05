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
  pointer.c)

target_include_directories(twclient
  PUBLIC "${TWCLIENT_INCLUDE_DIR}"
  )
add_library(twclient::twclient ALIAS twclient)

target_link_libraries(twclient
  m
  ctype
  Pixman::Pixman
  XKBCommon::XKBCommon
  OpenGL::EGL #for egl_env
  Cairo::Cairo
  Fontconfig::Fontconfig
  LibRSVG::LibRSVG
  UDEV::UDEV
  Freetype::Freetype
  #wayland
  Wayland::EGL
  Wayland::Client
  Wayland::Cursor
  #protocols
  twclient-protos
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
  ctype
  m
  )
add_library(twclient::icons ALIAS twclient-icons)

add_subdirectory(nk_backends)

################################################################################
# install
################################################################################

install(TARGETS twclient twclient-icons
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
  )

install(DIRECTORY ${TWCLIENT_INCLUDE_DIR}/
  DESTINATION include/taiwins
  PATTERN "stb/*" EXCLUDE
  PATTERN "nuklear/*" EXCLUDE)

install(DIRECTORY ${CTYPES_INCLUDE_DIR}/
  DESTINATION include/taiwins/ctypes
  FILES_MATCHING PATTERN "*.h")
