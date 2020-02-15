################################################################################
# library
################################################################################
include(Wayland)

set(TAIWINS_PROTOS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/protocols")

WAYLAND_ADD_PROTOCOL_CLIENT(proto_twtheme_client
  "${TAIWINS_PROTOS_DIR}/taiwins-theme.xml"
  taiwins-theme
  )

#not able to utilse targets
WAYLAND_ADD_PROTOCOL_CLIENT(proto_xdg_shell_client
  "${WLPROTO_PATH}/stable/xdg-shell/xdg-shell.xml"
  xdg-shell)

add_library(twclient-protos STATIC
  ${proto_twtheme_client}
  ${proto_xdg_shell_client}
  )

target_include_directories(twclient-protos PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>)

#those are alias, but can we actually move those alias into a findpackage file?
add_library(twclient::protos ALIAS twclient-protos)
