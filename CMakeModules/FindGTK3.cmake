# - Try to find the GTK3 libraries
#  Once done this will define
#
#  GTK3_FOUND       - system has GTK3
#  GTK3_INCLUDE_DIR - the GTK3 include directory
#  GTK3_LIBRARIES   - the libraries needed to use GTK3
#
#  Copyright (c) 2016 Lien Chiang  <xsoameix@gmail.com>
#
#  Redistribution and use is allowed according to the terms of the New
#  BSD license.

if ("${CMAKE_C_COMPILER_ID}" STREQUAL "MSVC")
else()
  find_package(PkgConfig QUIET REQUIRED)
  pkg_search_module(GTK3 REQUIRED QUIET gtk+-3.0)
  find_path(GTK3_INCLUDE_DIR NAMES gtk/gtk.h HINTS ${GTK3_INCLUDE_DIRS})
  find_package(PackageHandleStandardArgs QUIET REQUIRED)
  find_package_handle_standard_args(GTK3
    REQUIRED_VARS GTK3_LIBRARIES GTK3_INCLUDE_DIR
    VERSION_VAR GTK3_VERSION)
endif()

mark_as_advanced(GTK3_INCLUDE_DIR GTK3_LIBRARIES)
