cmake_minimum_required(VERSION 4.3)

project(thinput_overlay LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT MINGW)
    message(FATAL_ERROR
            "このオーバーレイのびるどはMinGWの32bit専用ですっ!"
            "i686-w64-mingw32を使ってくださいな☆"
    )
endif()

add_library(thinput_overlay SHARED
        src/Overlay.cpp
)

target_include_directories(thinput_overlay PRIVATE
        ../src
)

target_compile_options(thinput_overlay PRIVATE
        -m32
        -Wall
        -Wextra
)

target_link_options(thinput_overlay PRIVATE
        -m32
        -static
        -static-libgcc
        -static-libstdc++
)

target_link_libraries(thinput_overlay PRIVATE
        d3d9
        user32
)

set_target_properties(thinput_overlay PROPERTIES
        PREFIX ""
        OUTPUT_NAME "thinput_overlay"
)


add_executable(thinput_injector
        src/Injector.cpp
)

target_include_directories(thinput_injector PRIVATE
        ../src
)

target_compile_options(thinput_injector PRIVATE
        -m32
        -Wall
        -Wextra
)

target_link_options(thinput_injector PRIVATE
        -m32
        -municode
        -static
        -static-libgcc
        -static-libstdc++
)

target_link_libraries(thinput_injector PRIVATE
        user32
)
