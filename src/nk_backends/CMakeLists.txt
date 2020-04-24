################################################################################
# nkegl library
################################################################################

add_library(twclient-nkegl OBJECT
  nk_wl_egl.c
  )
set_target_properties(twclient-nkegl
  PROPERTIES POSITION_INDEPENDENT_CODE 1
  )
target_link_libraries(twclient-nkegl PUBLIC
  twclient::twclient
  twclient::icons
  twclient::protos
  Wayland::Client
  Wayland::EGL
  OpenGL::EGL
  XKBCommon::XKBCommon
  Freetype::Freetype
  Fontconfig::Fontconfig
  )

add_library(twclient-nkegl-static STATIC $<TARGET_OBJECTS:twclient-nkegl>)
add_library(twclient-nkegl-shared SHARED $<TARGET_OBJECTS:twclient-nkegl>)
add_library(twclient::nkegl ALIAS twclient-nkegl-static)

set_target_properties(twclient-nkegl-static
  PROPERTIES OUTPUT_NAME "twclient-nkegl"
  )
target_link_libraries(twclient-nkegl-static PUBLIC
  $<TARGET_PROPERTY:twclient-nkegl,INTERFACE_LINK_LIBRARIES>
  )

set_target_properties(twclient-nkegl-shared
  PROPERTIES VERSION ${PROJECT_VERSION} OUTPUT_NAME "twclient-nkegl"
  )
target_link_libraries(twclient-nkegl-shared PUBLIC
  $<TARGET_PROPERTY:twclient-nkegl,INTERFACE_LINK_LIBRARIES>
  )

################################################################################
# nkcairo library
################################################################################

add_library(twclient-nkcairo OBJECT
  nk_wl_cairo.c
  )
set_target_properties(twclient-nkcairo
  PROPERTIES POSITION_INDEPENDENT_CODE 1
  )
target_link_libraries(twclient-nkcairo PUBLIC
  twclient::twclient
  twclient::icons
  twclient::protos
  Cairo::Cairo
  Wayland::Client
  XKBCommon::XKBCommon
  Freetype::Freetype
  Fontconfig::Fontconfig
 )

add_library(twclient-nkcairo-static STATIC $<TARGET_OBJECTS:twclient-nkcairo>)
add_library(twclient-nkcairo-shared SHARED $<TARGET_OBJECTS:twclient-nkcairo>)
add_library(twclient::nkcairo ALIAS twclient-nkcairo-static)

target_link_libraries(twclient-nkcairo-static PUBLIC
  $<TARGET_PROPERTY:twclient-nkcairo,INTERFACE_LINK_LIBRARIES>
  )
set_target_properties(twclient-nkcairo-static
  PROPERTIES OUTPUT_NAME "twclient-nkcairo"
  )

target_link_libraries(twclient-nkcairo-shared PUBLIC
  $<TARGET_PROPERTY:twclient-nkcairo,INTERFACE_LINK_LIBRARIES>
  )
set_target_properties(twclient-nkcairo-shared
  PROPERTIES VERSION ${PROJECT_VERSION} OUTPUT_NAME "twclient-nkcairo"
  )

################################################################################
# install
################################################################################
include(GNUInstallDirs)
install(TARGETS
  twclient-nkegl-static twclient-nkegl-shared
  twclient-nkcairo-static twclient-nkcairo-shared
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  )
