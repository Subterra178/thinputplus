cmake_minimum_required(VERSION 4.3)
project(thinput+ LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(thinput+ WIN32
        main.cpp
        src/app/AppState.cpp
        src/process/Process.cpp
        src/memory/MemoryReader.cpp
        src/gui/Renderer.cpp
        src/gui/Window.cpp
)

target_include_directories(thinput+ PRIVATE
        src
)

target_link_options(thinput+ PRIVATE
        -municode
)

target_link_libraries(thinput+ PRIVATE
        user32
        gdi32
)
