project('twclient', 'c',
	version : '0.1',
	default_options: [
	  'warning_level=3',
	  'c_std=gnu11',
	  'werror=true'
	],
	meson_version: '>= 0.47',
	license: 'LGPLv2.1+')

#### compile flags
cc = meson.get_compiler('c')

global_args_maybe = [
	'-Wno-unused-parameter',
	'-Wno-missing-field-initializers',
	'-Wno-pedantic',

]
add_project_arguments(
  cc.get_supported_arguments(global_args_maybe),
  language : 'c'
)

inc_twclient = include_directories('include')

ctypes = subproject('ctypes')
dep_ctypes = ctypes.get_variable('dep_ctypes')
dir_ctypes = join_paths(meson.current_source_dir(), 'subprojects/ctypes')

subdir('include')
subdir('src')
