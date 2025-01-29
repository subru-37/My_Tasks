#----------------------------------------------------------------
# Generated CMake target import file.
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "libfilter::c" for configuration ""
set_property(TARGET libfilter::c APPEND PROPERTY IMPORTED_CONFIGURATIONS NOCONFIG)
set_target_properties(libfilter::c PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_NOCONFIG "C"
  IMPORTED_LOCATION_NOCONFIG "${_IMPORT_PREFIX}/lib/libfilter.a"
  )

list(APPEND _cmake_import_check_targets libfilter::c )
list(APPEND _cmake_import_check_files_for_libfilter::c "${_IMPORT_PREFIX}/lib/libfilter.a" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
