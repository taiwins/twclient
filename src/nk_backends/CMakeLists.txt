add_library(nk_egl_backend STATIC
  nk_wl_egl.c)

target_include_directories(nk_egl_backend PUBLIC
  ${TWCLIENT_PROTO_INCLUDE_DIR}
  ${TWCLIENT_INCLUDE_DIRS})

target_link_libraries(nk_egl_backend
  twclient
  ${TWCLIENT_TRD_LIB}
  twclient_proto_libraries
  )

add_library(nk_cairo_backend STATIC
  nk_wl_cairo.c)


target_include_directories(nk_cairo_backend PUBLIC
  ${TWCLIENT_PROTO_INCLUDE_DIR}
  ${TWCLIENT_INCLUDE_DIRS})

target_link_libraries(nk_cairo_backend
  twclient
  ${TWCLIENT_TRD_LIB}
  twclient_proto_libraries
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
