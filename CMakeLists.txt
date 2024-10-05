cmake_minimum_required(VERSION 3.28)
project(sdlgputest)

set(CMAKE_CXX_STANDARD 20)

# No-DLL yes static build
set(SDL_STATIC ON)
set(SDL_SHARED OFF)

# Multithreading
add_compile_options($<$<CXX_COMPILER_ID:MSVC>:/MP>)

# Adding SDL
include(FetchContent)
#set(FETCHCONTENT_QUIET FALSE)
FetchContent_Declare(
        SDL
        GIT_REPOSITORY "https://github.com/libsdl-org/SDL.git"
        GIT_TAG 1ca45c58912aaa2c02e0f143d36d7f171e5afbb5
        GIT_PROGRESS TRUE
)

FetchContent_MakeAvailable(SDL)

include_directories(${sdl_SOURCE_DIR}/test)
include_directories(${sdl_SOURCE_DIR}/src)

add_executable(sdlgputest
        ${sdl_SOURCE_DIR}/src/test/SDL_test_common.c
        ${sdl_SOURCE_DIR}/src/test/SDL_test_memory.c
        ${sdl_SOURCE_DIR}/src/test/SDL_test_font.c
        ${sdl_SOURCE_DIR}/src/test/SDL_test_crc32.c
        ${sdl_SOURCE_DIR}/src/test/SDL_test_fuzzer.c
        #${sdl_SOURCE_DIR}/test/testgpu_spinning_cube.c
        "main.c"
        )

function(PRINT_VARIABLES)
    get_cmake_property(_variableNames VARIABLES)
    list (SORT _variableNames)
    foreach (_variableName ${_variableNames})
        message(STATUS "${_variableName}=${${_variableName}}")
    endforeach()
endfunction()

#print_variables()

target_link_libraries(sdlgputest PUBLIC
        SDL3::SDL3
)

if(WIN32)
    set_target_properties(sdlgputest PROPERTIES
        WIN32_EXECUTABLE TRUE
    )
endif()
