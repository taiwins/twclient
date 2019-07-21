find_package(PkgConfig)
pkg_check_modules(pc_udev QUIET libudev)

find_path(UDEV_INCLUDE_DIR NAMES libudev.h
  HINTS
  ${pc_udev_INCLUDEDIR}
  ${pc_udev_INCLUDE_DIRS}
  )

find_library(UDEV_LIBRARY NAMES udev
  HINTS
  ${pc_udev_LIBDIR}
  ${pc_udev_LIBRARY_DIRS}
  )

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(UDEV DEFAULT_MSG
  UDEV_INCLUDE_DIR UDEV_LIBRARY)

mark_as_advanced(UDEV_INCLUDE_DIR UDEV_LIBRARY)
set(UDEV_INCLUDE_DIRS ${UDEV_INCLUDE_DIR})
set(UDEV_LIBRARIES ${UDEV_LIBRARY})
