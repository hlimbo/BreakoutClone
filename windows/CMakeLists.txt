# The minimum version number ps vita samples use
cmake_minimum_required(VERSION 2.8)

project(breakout_clone_windows)

# Windows
set(SDL2_INCLUDE_DIR "C:/msys64/usr/include/SDL2.0.16")
set(SDL2_LIBRARY "C:/msys64/usr/lib/SDL2.0.16/x64/SDL2.lib")
set(SDL2_TTF_INCLUDE_DIR "C:/msys64/usr/include/SDL2_TTF-2.0.15")
set(SDL2_TTF_LIBRARY "C:/msys64/usr/lib/SDL2_ttf-2.0.15/lib/x64/SDL2_ttf.lib")


# Need the below symbol defined here when compiling on Visual Studio IDE
# otherwise a Linker Error occurs of SDL_MAIN not being handled
# Comment out below block if compiling using a terminal such as MinGW
# Define symbols
#if (WIN32)
#  add_definitions(-DSDL_MAIN_HANDLED)
#endif (WIN32)

# option flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")
# The -lmingw32 option ~~ links mingw32 library required for Windows only
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall -lmingw32 -lSDL2main -lSDL2")


set(SOURCE_FILES main.cpp)

include_directories(${SDL2_INCLUDE_DIR} ${SDL2_TTF_INCLUDE_DIR})
add_executable(breakout_clone_windows ${SOURCE_FILES})
target_link_libraries(breakout_clone_windows ${SDL2_LIBRARY} ${SDL2_TTF_LIBRARY})
