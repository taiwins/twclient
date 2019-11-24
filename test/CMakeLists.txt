# add_executable(test_nk_vulkan
#   test_nk_vulkan.c)

# target_include_directories(test_nk_vulkan PUBLIC
#   ${TWCLIENT_INCLUDE_DIRS})

# target_link_libraries(test_nk_vulkan
#   twclient
#   nk_vk_backend
#   ${TWCLIENT_TRD_LIB}
#   )

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

add_executable(test_event_queue
  test_event_queue.c)

target_link_libraries(test_event_queue
  ${TWCLIENT_LIBRARIES}
  ${TWCLIENT_TRD_LIB}
  )

add_executable(test_udev
  test_udev.c)

target_link_libraries(test_udev
  ${TWCLIENT_LIBRARIES}
  ${TWCLIENT_TRD_LIB}
  )

add_executable(test_stbimage
  test_stb_image.c)

target_include_directories(test_stbimage PUBLIC
  ${TWCLIENT_INCLUDE_DIRS})

target_link_libraries(test_stbimage
  ${TWCLIENT_LIBRARIES}
  m
  )

add_executable(test_desktop_entry
  test_dot_desktop.c)

target_include_directories(test_desktop_entry PRIVATE
  ${TWCLIENT_INCLUDE_DIRS}
  )

target_link_libraries(test_desktop_entry
  twclient
  m
  )
