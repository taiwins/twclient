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

if (UDEV_FOUND AND NOT TARGET UDEV::UDEV)
  add_library(UDEV::UDEV UNKNOWN IMPORTED)
  set_target_properties(UDEV::UDEV PROPERTIES
    IMPORTED_LINK_INTERFACE_LANGUAGES "C"
    IMPORTED_LOCATION "${UDEV_LIBRARY}"
    INTERFACE_LINK_DIRECTORIES "${pc_udev_LIBRARY_DIRS}"
    INTERFACE_LINK_LIBRARIES "${pc_udev_LIBRARIES}"
    INTERFACE_INCLUDE_DIRECTORIES "${UDEV_INCLUDE_DIRS}"
    )
endif()
