include(Wayland)

WAYLAND_ADD_PROTOCOL_CLIENT(proto_twtheme_client
  taiwins_theme.xml
  taiwins-theme
  )

WAYLAND_ADD_PROTOCOL_CLIENT(proto_xdg_shell_client
  "${WLPROTO_PATH}/stable/xdg-shell/xdg-shell.xml"
  xdg-shell)

add_library(twclient_proto_libraries STATIC
  ${proto_twtheme_client}
  ${proto_xdg_shell_client}
  )

set(TWCLIENT_PROTO_INCLUDE_DIR ${CMAKE_CURRENT_BINARY_DIR} PARENT_SCOPE)
