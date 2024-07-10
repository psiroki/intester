cmake_minimum_required(VERSION 3.8)
project(intester)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Option to use SDL2
option(USE_SDL2 "Use SDL2 instead of SDL1" OFF)

if(USE_SDL2)
    find_package(SDL2 REQUIRED)
    find_package(SDL2_ttf REQUIRED)
    include_directories(${SDL2_INCLUDE_DIRS} ${SDL2_TTF_INCLUDE_DIRS})
    add_definitions(-DUSE_SDL2)
else()
    find_package(SDL REQUIRED)
    find_package(SDL_ttf REQUIRED)
    include_directories(${SDL_INCLUDE_DIR} ${SDL_TTF_INCLUDE_DIR})
endif()

# Additional options
option(FLIP "Flip the display content 180 degrees" OFF)
option(PORTRAIT "Portrait orientation mode (SDL2 only)" OFF)

if(FLIP)
    add_definitions(-DFLIP)
endif()

if(PORTRAIT)
    add_definitions(-DPORTRAIT)
endif()

# Collect all source files in the src directory
file(GLOB_RECURSE SOURCES "src/*.cc")

add_executable(intester ${SOURCES})

if(USE_SDL2)
    target_link_libraries(intester ${SDL2_LIBRARIES} ${SDL2_TTF_LIBRARIES})
else()
    target_link_libraries(intester ${SDL_LIBRARY} ${SDL_TTF_LIBRARY})
endif()
