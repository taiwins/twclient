# This module defines
# Rsvg_FOUND
# Rsvg_INCLUDE_DIR
# Rsvg_LIBRARY_DIR
# Rsvg_LIBS
find_package(PkgConfig)
pkg_check_modules(LIBRSVG REQUIRED QUIET librsvg-2.0)
find_library(LIBRSVG_LOCATION NAMES rsvg-2 HINTS ${LIBRSVG_})
find_package_handle_standard_args(LibRSVG DEFAULT_MSG LIBRSVG_INCLUDE_DIRS LIBRSVG_LIBRARIES)
mark_as_advanced(LIBRSVG_INCLUDE_DIRS LIBRSVG_LIBRARIES)

if (LIBRSVG_FOUND AND NOT TARGET LibRSVG::LibRSVG)
  add_library(LibRSVG::LibRSVG UNKNOWN IMPORTED)
  set_target_properties(LibRSVG::LibRSVG PROPERTIES
    IMPORTED_LINK_INTERFACE_LANGUAGES "C"
    IMPORTED_LOCATION "${LIBRSVG_LOCATION}"
    INTERFACE_LINK_DIRECTORIES "${LIBRSVG_LIBRARY_DIRS}"
    INTERFACE_LINK_LIBRARIES "${LIBRSVG_LIBRARIES}"
    INTERFACE_INCLUDE_DIRECTORIES "${LIBRSVG_INCLUDE_DIRS}"
    )
endif()
