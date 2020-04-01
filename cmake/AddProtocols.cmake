################################################################################
# library
################################################################################
include(Wayland)

#not able to utilse targets
WAYLAND_ADD_PROTOCOL_CLIENT(proto_xdg_shell_client
  "${WLPROTO_PATH}/stable/xdg-shell/xdg-shell.xml"
  xdg-shell)

add_library(twclient-protos STATIC
  ${proto_xdg_shell_client}
  )

target_include_directories(twclient-protos PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>)

#those are alias, but can we actually move those alias into a findpackage file?
add_library(twclient::protos ALIAS twclient-protos)
