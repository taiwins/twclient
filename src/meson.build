### packages ##################################################################
dep_xkbcommon = dependency('xkbcommon', version: '>= 0.3.0')
dep_wayland_client = dependency('wayland-client', version: '>= 1.17.0')
dep_wayland_egl = dependency('wayland-egl', version: '>= 1.17.0')
dep_wayland_cursor = dependency('wayland-cursor', version: '>= 1.17.0')

dep_libm = cc.find_library('m')
dep_cairo = dependency('cairo')
dep_udev = dependency('libudev')
dep_rsvg = dependency('librsvg-2.0')

#this can go into nkegl
dep_egl = dependency('egl')
dep_gl = dependency('gl', required: false )
dep_gles = dependency('glesv2', required: false)
dep_gls = []
twclient_flags = ['-D_GNU_SOURCE'] + global_args

if dep_gl.found()
  dep_gls += dep_gl
  twclient_flags += '-D_TW_USE_GL'
elif dep_gles.found()
  dep_gls += dep_gles
  twclient_flags += '-D_TW_USE_GLES'
else
  error('no GL runtime found, required by twclient\n')
endif

pkgconfig = import('pkgconfig')

### twclient ##################################################################

src_twclient = [
  'client.c',
  'output.c',
  'appsurf.c',
  'egl.c',
  'glhelper.c',
  'buffer.c',
  'event_queue.c',
  #inputs
  'keyboard.c',
  'pointer.c',
  #theme
  'theme.c',
]

twclient_deps = [
  dep_xkbcommon,
  dep_wayland_client,
  dep_wayland_egl,
  dep_wayland_cursor,
  dep_udev,
  dep_egl,
  dep_ctypes,
] + dep_gls

# deal with version later
lib_twclient = both_libraries(
  'twclient',
  src_twclient,
  c_args : twclient_flags,
  include_directories : inc_twclient,
  version: meson.project_version(),
  dependencies : twclient_deps,
  install : true,
)


dep_twclient = declare_dependency(
  link_with : lib_twclient,
  compile_args : twclient_flags,
  include_directories : inc_twclient,
  dependencies : twclient_deps,
)

pkgconfig.generate(
  lib_twclient,
  filebase: 'twclient',
  name: 'twclient',
  version: meson.project_version(),
  description: 'Taiwins client utility library',
)

### twclient-theme ############################################################
lib_twclient_theme = static_library(
  'twclient-theme',
  'theme.c',
  include_directories : inc_twclient,
  c_args : twclient_flags,
  dependencies : [dep_ctypes],
  pic: true,
)

dep_twclient_theme = declare_dependency(
  link_with : lib_twclient_theme,
  include_directories : inc_twclient,
  compile_args : twclient_flags,
  dependencies : [dep_ctypes],
)

### twclient-icons ############################################################

src_twclient_icons = [
  'image_cache.c',
  'icon_search.c',
  'desktop_entry.c',
]

twclient_icons_deps = [
  dep_wayland_client,
  dep_cairo,
  dep_rsvg,
  dep_ctypes,
  dep_libm,
  dep_stb,
]

lib_twclient_icons = both_libraries(
  'twclient-icons',
  src_twclient_icons,
  c_args : twclient_flags,
  version: meson.project_version(),
  include_directories : inc_twclient,
  dependencies : twclient_icons_deps,
  install : true,
)

dep_twclient_icons = declare_dependency(
  link_with : lib_twclient_icons,
  compile_args : twclient_flags,
  include_directories : inc_twclient,
  dependencies : twclient_icons_deps,
)

pkgconfig.generate(
  lib_twclient_icons,
  filebase: 'twclient-icons',
  name: 'twclient-icons',
  version: meson.project_version(),
  description: 'Icon search and atlas generation library for twclient',
  requires: [dep_wayland_client],
  requires_private: [ dep_wayland_client, dep_cairo, dep_rsvg, ],
)
