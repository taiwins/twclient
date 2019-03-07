add_library(nk_egl_backend STATIC
  nk_wl_egl.c)

target_include_directories(nk_egl_backend PUBLIC
  ${TWCLIENT_INCLUDE_DIRS})

add_library(nk_cairo_backend STATIC
  nk_wl_cairo.c)

target_include_directories(nk_cairo_backend PUBLIC
  ${TWCLIENT_INCLUDE_DIRS})

add_library(nk_vk_backend STATIC
  nk_wl_vulkan.c
  )

target_include_directories(nk_vk_backend PUBLIC
  ${TWCLIENT_INCLUDE_DIRS})
