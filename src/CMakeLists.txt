################################################################################
# twclient
################################################################################

add_library(twclient OBJECT
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
set_target_properties(twclient
  PROPERTIES POSITION_INDEPENDENT_CODE 1
  )
target_include_directories(twclient
  PUBLIC "${TWCLIENT_INCLUDE_DIR}"
  )
target_link_libraries(twclient PUBLIC
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

add_library(twclient-static STATIC $<TARGET_OBJECTS:twclient>)
add_library(twclient-shared SHARED $<TARGET_OBJECTS:twclient>)
add_library(twclient::twclient ALIAS twclient)

target_link_libraries(twclient-static PUBLIC
  $<TARGET_PROPERTY:twclient,INTERFACE_LINK_LIBRARIES>
  )
set_target_properties(twclient-static
  PROPERTIES OUTPUT_NAME "twclient"
  )

target_link_libraries(twclient-shared PUBLIC
  $<TARGET_PROPERTY:twclient,INTERFACE_LINK_LIBRARIES>
  )
set_target_properties(twclient-shared
  PROPERTIES
  VERSION ${PROJECT_VERSION}
  OUTPUT_NAME "twclient"
  )

################################################################################
# twclient-icons
################################################################################

add_library(twclient-icons OBJECT
  image_cache.c
  icon_search.c
  desktop_entry.c
  )
set_target_properties(twclient-icons
  PROPERTIES POSITION_INDEPENDENT_CODE 1
  )
target_include_directories(twclient-icons PUBLIC
  "${TWCLIENT_INCLUDE_DIR}"
  )
target_link_libraries(twclient-icons
  Wayland::Client
  Cairo::Cairo
  LibRSVG::LibRSVG
  stb
  ctypes
  m
  )

add_library(twclient-icons-static STATIC $<TARGET_OBJECTS:twclient-icons>)
add_library(twclient-icons-shared SHARED $<TARGET_OBJECTS:twclient-icons>)
add_library(twclient::icons ALIAS twclient-icons-static)

target_link_libraries(twclient-icons-static PUBLIC
  $<TARGET_PROPERTY:twclient-icons,INTERFACE_LINK_LIBRARIES>
  )
set_target_properties(twclient-icons-static
  PROPERTIES OUTPUT_NAME "twclient-icons"
  )

target_link_libraries(twclient-icons-shared PUBLIC
  $<TARGET_PROPERTY:twclient-icons,INTERFACE_LINK_LIBRARIES>
  )
set_target_properties(twclient-icons-shared
  PROPERTIES VERSION ${PROJECT_VERSION} OUTPUT_NAME "twclient-icons"
  )

add_subdirectory(nk_backends)

################################################################################
# install
################################################################################
include(GNUInstallDirs)

install(
  TARGETS
  twclient-static twclient-shared
  twclient-icons-static twclient-icons-shared
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
