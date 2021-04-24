cc = meson.get_compiler('c')

stb_cflags=cc.get_supported_arguments(
  ['-Wno-type-limits',
   '-Wno-sign-compare',
   '-Wno-unused-but-set-variable']
)

dep_stb = declare_dependency(
  include_directories : include_directories('.'),
  compile_args : stb_cflags,
)

#TODO, change it to twclient
install_headers(
  ['twclient/client.h',
   'twclient/desktop_entry.h',
   'twclient/egl.h',
   'twclient/event_queue.h',
   'twclient/image_cache.h',
   'twclient/theme.h',
   'twclient/twclient.h',
   'twclient/ui_event.h',
   'twclient/ui.h'
  ],
  subdir: 'twclient'
)
