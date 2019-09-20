include(Wayland)

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

target_link_libraries(twclient
  ctype)

target_include_directories(twclient PUBLIC
  ${CTYPES_INCLUDE_DIR}
  ${TWCLIENT_PROTO_INCLUDE_DIR}
  ${TWCLIENT_INCLUDE_DIRS}
  ${CMAKE_CURRENT_BINARY_DIR})

add_subdirectory(nk_backends)

set(TWCLIENT_LIBRARIES
  twclient
  twclient_proto_libraries
  ${NK_BACKEND_LIB}
  ${TWCLIENT_TRD_LIB}
  CACHE STRING "twclient libraries"
  )
