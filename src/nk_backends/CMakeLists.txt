add_library(nk_egl_backend STATIC
  nk_wl_egl.c)

target_include_directories(nk_egl_backend PUBLIC
  ${TWCLIENT_INCLUDE_DIRS})

target_link_libraries(nk_egl_backend
  twclient
  ${TWCLIENT_TRD_LIB}
  )

add_library(nk_cairo_backend STATIC
  nk_wl_cairo.c)


target_include_directories(nk_cairo_backend PUBLIC
  ${TWCLIENT_INCLUDE_DIRS})

target_link_libraries(nk_cairo_backend
  twclient
  ${TWCLIENT_TRD_LIB}
  )

# add_library(nk_vk_backend STATIC
#   nk_wl_vulkan.c
#   )

# target_include_directories(nk_vk_backend PUBLIC
#   ${TWCLIENT_INCLUDE_DIRS})

# target_link_libraries(nk_vk_backend
#   twclient
#   ${TWCLIENT_TRD_LIB}
#   )
