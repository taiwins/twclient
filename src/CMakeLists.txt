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
