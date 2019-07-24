include(Wayland)

WAYLAND_ADD_PROTOCOL_CLIENT(proto_xdg_shell_client
  "${WLPROTO_PATH}/stable/xdg-shell/xdg-shell.xml"
  xdg-shell
  )

add_library(xdg_shell_client
  ${proto_xdg_shell_client}
  )

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
  )

target_include_directories(twclient PUBLIC
  ${TWCLIENT_INCLUDE_DIRS}
  #this will work but we should extract the path from xdg_shell_client
  "${CMAKE_BINARY_DIR}/src"
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
