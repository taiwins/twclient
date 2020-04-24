### packages ###################################################################
dep_xkbcommon = dependency('xkbcommon', version: '>= 0.3.0')
dep_wayland_client = dependency('wayland-client', version: '>= 1.17.0')
dep_wayland_egl = dependency('wayland-egl', version: '>= 1.17.0')
dep_wayland_cursor = dependency('wayland-cursor', version: '>= 1.17.0')

dep_threads = dependency('threads')
dep_libm = cc.find_library('m')
dep_cairo = dependency('cairo')
dep_fontconfig = dependency('fontconfig')
dep_freetype = dependency('freetype2')
dep_udev = dependency('libudev')
dep_rsvg = dependency('librsvg-2.0')

#this can go into nkegl
dep_egl = dependency('egl')
dep_gls = []
foreach name : ['gl', 'glesv2']
  d = dependency(name, required: false)
  if d.found()
    dep_gls += d
  endif
endforeach
if dep_gls.length() == 0
  error('GL not found, required by nkegl\n')
endif

pkgconfig = import('pkgconfig')

### twclient ###################################################################

src_twclient = [
  'client.c',
  'appsurf.c',
  'ui.c',
  'egl.c',
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
twclient_flags = global_args + ['-D_GNU_SOURCE']

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


### twclient-icons ##############################################################

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

### wayland-protocols ###########################################################
dep_scanner = dependency('wayland-scanner', native: true)
prog_scanner = find_program(dep_scanner.get_pkgconfig_variable('wayland_scanner'))
dep_wp = dependency('wayland-protocols', version: '>= 1.18')
dir_wp_base = dep_wp.get_pkgconfig_variable('pkgdatadir')
protocols = [['xdg-shell', 'stable']]

foreach proto: protocols
  xml_path = '@0@/@1@/@2@/@2@.xml'.format(dir_wp_base, proto[1], proto[0])
  base_file = proto[0]
  foreach type : [ 'client-header', 'private-code']
    if type == 'client-header'
      output_file = 'wayland-@0@-client-protocol.h'.format(base_file)
    else
      output_file = 'wayland-@0@-client-protocol.c'.format(base_file)
    endif
    target = custom_target(
      '@0@-@1@'.format(base_file, type),
      command : [ prog_scanner, type, '@INPUT@', '@OUTPUT@'],
      input: xml_path,
      output: output_file,
    )
    var_name = output_file.underscorify()
    set_variable(var_name, target)
  endforeach
endforeach

### twclient-nkbackends #########################################################
nkcairo_deps = [
  dep_wayland_client,
  dep_cairo,
  dep_xkbcommon,
  dep_fontconfig,
  dep_freetype,
  dep_ctypes,
  dep_libm,
  dep_twclient,
  dep_twclient_icons,
]

lib_nkcairo = both_libraries(
  'twclient-nkcairo',
  ['nk_backends/nk_wl_cairo.c',
   wayland_xdg_shell_client_protocol_h,
   wayland_xdg_shell_client_protocol_c],
  c_args : twclient_flags,
  version: meson.project_version(),
  include_directories : inc_twclient,
  dependencies : nkcairo_deps,
  install : true,
)

pkgconfig.generate(
  lib_nkcairo,
  filebase: 'twclient-nkcairo',
  name: 'twclient-nkcairo',
  version: meson.project_version(),
  description: 'nuklear GUI library with Cairo backend for twclient',
)

nkegl_deps = [
  dep_wayland_client,
  dep_wayland_egl,
  dep_xkbcommon,
  dep_fontconfig,
  dep_freetype,
  dep_egl,
  dep_ctypes,
  dep_libm,
  dep_twclient,
  dep_twclient_icons,
] + dep_gls

lib_nkegl = both_libraries(
  'twclient-nkegl',
  ['nk_backends/nk_wl_egl.c',
   wayland_xdg_shell_client_protocol_h,
   wayland_xdg_shell_client_protocol_c],
  c_args : twclient_flags,
  version: meson.project_version(),
  include_directories : inc_twclient,
  dependencies : nkegl_deps,
  install : true,
)

pkgconfig.generate(
  lib_nkegl,
  filebase: 'twclient-nkegl',
  name: 'twclient-nkegl',
  version: meson.project_version(),
  description: 'nuklear GUI library with EGL backend for twclient',
)
