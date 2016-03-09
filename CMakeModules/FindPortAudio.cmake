# - Try to find the PortAudio libraries
#  Once done this will define
#
#  PORTAUDIO_FOUND       - system has PortAudio
#  PORTAUDIO_INCLUDE_DIR - the PortAudio include directory
#  PORTAUDIO_LIBRARIES   - the libraries needed to use PortAudio
#
#  Copyright (c) 2016 Lien Chiang  <xsoameix@gmail.com>
#
#  Redistribution and use is allowed according to the terms of the New
#  BSD license.

if ("${CMAKE_C_COMPILER_ID}" STREQUAL "MSVC")
else()
  find_package(PkgConfig QUIET REQUIRED)
  pkg_search_module(PORTAUDIO REQUIRED QUIET portaudio-2.0)
  find_path(PORTAUDIO_INCLUDE_DIR NAMES portaudio.h HINTS ${PORTAUDIO_INCLUDE_DIRS})
  find_package(PackageHandleStandardArgs QUIET REQUIRED)
  find_package_handle_standard_args(PortAudio
    REQUIRED_VARS PORTAUDIO_LIBRARIES PORTAUDIO_INCLUDE_DIR
    VERSION_VAR PORTAUDIO_VERSION)
endif()

mark_as_advanced(PORTAUDIO_INCLUDE_DIR PORTAUDIO_LIBRARIES)
