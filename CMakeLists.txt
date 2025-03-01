cmake_minimum_required(VERSION 3.10)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

if (NOT CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  add_definitions(-Wall -Wextra)
endif(NOT CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")

set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

set(CMAKE_CXX_STANDARD 20 CACHE STRING "The C++ standard to use")
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

## MODIFY THESE TO YOUR PATHS

set (AbletonLinkPath "/Users/sideboard/Code/link/")
set (HomebrewIncDir "/Users/sideboard/homebrew/include")
set (ReadlineIncDir "/Users/sideboard/homebrew/opt/readline/include")
set (PerlinIncDir "/Users/sideboard/Code/PerlinNoise")

set (READLINE_LIB "/Users/sideboard/homebrew/opt/readline/lib/libreadline.dylib")
set (HISTORY_LIB "/Users/sideboard/homebrew/opt/readline/lib/libhistory.dylib")
######################################################################

project(Sbsh VERSION 3.0
  DESCRIPTION "Audio REPL"
  LANGUAGES CXX)

file (GLOB_RECURSE Sbsh_SOURCES CONFIGURE_DEPENDS "src/*.cpp")
file (GLOB_RECURSE Sbsh_HEADERS CONFIGURE_DEPENDS "include/*.h")

set (Sbsh_INCLUDE_DIRS "")
foreach (_headerFile ${Sbsh_HEADERS})
  get_filename_component(_dir ${_headerFile} PATH)
  list (APPEND Sbsh_INCLUDE_DIRS ${_dir})
endforeach()
list (REMOVE_DUPLICATES Sbsh_INCLUDE_DIRS)

add_executable(Sbsh ${Sbsh_SOURCES})

find_library(PORTAUDIO_LIB portaudio)
find_library(PORTMIDI_LIB portmidi)
find_library(SNDFILE_LIB sndfile)

target_include_directories(Sbsh PRIVATE ${Sbsh_INCLUDE_DIRS})
target_include_directories(Sbsh PRIVATE ${HomebrewIncDir})
target_include_directories(Sbsh PRIVATE ${ReadlineIncDir})
target_include_directories(Sbsh PRIVATE ${PerlinIncDir})

include(${AbletonLinkPath}/AbletonLinkConfig.cmake)
target_link_libraries(Sbsh Ableton::Link)
target_link_libraries(Sbsh ${PORTAUDIO_LIB})
target_link_libraries(Sbsh ${PORTMIDI_LIB})
target_link_libraries(Sbsh ${READLINE_LIB})
target_link_libraries(Sbsh ${HISTORY_LIB})
target_link_libraries(Sbsh ${SNDFILE_LIB})
