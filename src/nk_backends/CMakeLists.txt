################################################################################
# library
################################################################################

add_library(twclient-nkegl STATIC
  nk_wl_egl.c)

target_link_libraries(twclient-nkegl
  twclient
  )
add_library(wclient::nkegl ALIAS twclient-nkegl)

add_library(twclient-nkcairo STATIC
  nk_wl_cairo.c)

target_link_libraries(twclient-nkcairo
  twclient
  )
add_library(wclient::nkcairo ALIAS twclient-nkcairo)

# add_library(twclient-nkvk STATIC
#   nk_wl_vulkan.c
#   )

# target_link_libraries(twcleint-nkvk
#   twclient
#   )

################################################################################
# install
################################################################################

install(TARGETS twclient-nkegl twclient-nkcairo
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib)
