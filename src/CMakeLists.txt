# thread
find_package(Threads REQUIRED)
set(LIBS ${LIBS} ${CMAKE_THREAD_LIBS_INIT})

# GTK 3
find_package(GTK3 REQUIRED)
include_directories(${GTK3_INCLUDE_DIRS})
set(LIBS ${LIBS} ${GTK3_LIBRARIES})

# PortAudio
find_package(PortAudio REQUIRED)
include_directories(${PORTAUDIO_INCLUDE_DIRS})
set(LIBS ${LIBS} ${PORTAUDIO_LIBRARIES})

# mpg123
find_package(MPG123 REQUIRED)
include_directories(${MPG123_INCLUDE_DIRS})
set(LIBS ${LIBS} ${MPG123_LIBRARIES})

# wz
set(LIBS ${LIBS} /usr/local/lib/libwz.so)

# main - main program
add_executable(main
  var_grid.c
  file_tree_store.c
  main.c
)
target_link_libraries(main ${LIBS})
if ("${CMAKE_C_COMPILER_ID}" STREQUAL "GNU" OR
    "${CMAKE_C_COMPILER_ID}" MATCHES  "Clang")
  set(CMAKE_C_FLAGS "-std=c99 -pthread -g")
elseif ("${CMAKE_C_COMPILER_ID}" STREQUAL "MSVC")
endif()

## Qt 5
#find_package(Qt5Widgets REQUIRED)
#set(LIBS ${LIBS} Qt5::Widgets)
#set(CMAKE_INCLUDE_CURRENT_DIR ON)
#set(CMAKE_AUTOMOC ON)
#
#qt5_wrap_ui(mainwindow_ui mainwindow.ui)
#
## wz - main program
#add_executable(main
#  main.cpp
#  mainwindow.cpp ${mainwindow_uik
#  mainwindow.cpp ${mainwindow_ui})
#target_link_libraries(main ${LIBS} /usr/local/lib/libwz.so)
