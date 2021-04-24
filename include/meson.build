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

install_subdir('twclient', install_dir: get_option('includedir'))
