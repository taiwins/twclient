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
  ['client.h',
   'desktop_entry.h',
   'egl.h',
   'event_queue.h',
   'image_cache.h',
   'nk_backends.h',
   'theme.h',
   'twclient.h',
   'ui_event.h',
   'ui.h'
  ],
  subdir: 'taiwins'
)

install_headers(
  'nuklear/nuklear.h',
  subdir: 'taiwins/nuklear'
)
