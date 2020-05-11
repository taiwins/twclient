cc = meson.get_compiler('c')
if cc.has_argument('-Wno-type-limits')
  cflags_maybe = ['-Wno-type-limits']
else
  cflags_maybe = []
endif

dep_nuklear = declare_dependency(
  include_directories : include_directories('.'),
)

dep_stb = declare_dependency(
  include_directories : include_directories('.'),
  compile_args : cflags_maybe,
)

#TODO, change it to twclient
install_headers(
  ['twclient/client.h',
   'twclient/desktop_entry.h',
   'twclient/egl.h',
   'twclient/event_queue.h',
   'twclient/image_cache.h',
   'twclient/nk_backends.h',
   'twclient/theme.h',
   'twclient/twclient.h',
   'twclient/ui_event.h',
   'twclient/ui.h'
  ],
  subdir: 'twclient'
)

install_headers(
  'twclient/nuklear/nuklear.h',
  subdir: 'twclient/nuklear'
)
