################################################################################
# library
################################################################################

add_library(twclient-nkegl STATIC
  nk_wl_egl.c)

target_link_libraries(twclient-nkegl PUBLIC
  twclient
  twclient-icons
  twclient-protos
  Wayland::Client
  Wayland::EGL
  OpenGL::EGL
  XKBCommon::XKBCommon
  Freetype::Freetype
  Fontconfig::Fontconfig
  )
add_library(twclient::nkegl ALIAS twclient-nkegl)

add_library(twclient-nkcairo STATIC
  nk_wl_cairo.c)

target_link_libraries(twclient-nkcairo PUBLIC
  twclient
  twclient-icons
  twclient-protos
  Cairo::Cairo
  Wayland::Client
  XKBCommon::XKBCommon
  Freetype::Freetype
  Fontconfig::Fontconfig
  )
add_library(twclient::nkcairo ALIAS twclient-nkcairo)

# add_library(twclient-nkvk STATIC
#   nk_wl_vulkan.c
#   )

# target_link_libraries(twcleint-nkvk
#   twclient
#   )

################################################################################
# install
################################################################################
include(GNUInstallDirs)
install(TARGETS twclient-nkegl twclient-nkcairo
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  )
