# - Try to find the mpg123 libraries
#  Once done this will define
#
#  MPG123_FOUND       - system has mpg123
#  MPG123_INCLUDE_DIR - the mpg123 include directory
#  MPG123_LIBRARIES   - the libraries needed to use mpg123
#
#  Copyright (c) 2016 Lien Chiang  <xsoameix@gmail.com>
#
#  Redistribution and use is allowed according to the terms of the New
#  BSD license.

if ("${CMAKE_C_COMPILER_ID}" STREQUAL "MSVC")
else()
  find_package(PkgConfig QUIET REQUIRED)
  pkg_search_module(MPG123 REQUIRED QUIET libmpg123)
  find_path(MPG123_INCLUDE_DIR NAMES mpg123.h HINTS ${MPG123_INCLUDE_DIRS})
  find_package(PackageHandleStandardArgs QUIET REQUIRED)
  find_package_handle_standard_args(mpg123
    REQUIRED_VARS MPG123_LIBRARIES MPG123_INCLUDE_DIR
    VERSION_VAR MPG123_VERSION)
endif()

mark_as_advanced(MPG123_INCLUDE_DIR MPG123_LIBRARIES)
