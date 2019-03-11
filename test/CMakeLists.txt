add_executable(test_nk_vulkan
  test_nk_vulkan.c)

target_include_directories(test_nk_vulkan PUBLIC
  ${TWCLIENT_INCLUDE_DIRS})

target_link_libraries(test_nk_vulkan
  twclient
  nk_vk_backend
  ${TWCLIENT_TRD_LIB}
  )

add_executable(test_nk_cairo
  test_nk_cairo.c)

target_include_directories(test_nk_cairo PUBLIC
  ${TWCLIENT_INCLUDE_DIRS})


target_link_libraries(test_nk_cairo
  twclient
  nk_cairo_backend
  ${TWCLIENT_TRD_LIB}
  )

add_executable(test_nk_egl
  test_nk_egl.c
  )

target_include_directories(test_nk_egl PUBLIC
  ${TWCLIENT_INCLUDE_DIRS})

target_link_libraries(test_nk_egl
  twclient
  nk_egl_backend
  ${TWCLIENT_TRD_LIB}
  )


add_executable(test_multi_lua
  test_multi_lua.c
  )

target_include_directories(test_multi_lua PUBLIC
  ${LUA_INCLUDE_DIR}
  )

target_link_libraries(test_multi_lua
  ${LUA_LIBRARIES}
  )
