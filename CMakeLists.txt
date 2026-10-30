cmake_minimum_required(VERSION 3.16)

project(ProyectoPr3 VERSION 0.1.0 LANGUAGES CXX)

include(FetchContent)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(SFML_BUILD_AUDIO OFF CACHE BOOL "" FORCE)
set(SFML_BUILD_NETWORK OFF CACHE BOOL "" FORCE)
set(SFML_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SFML_BUILD_TEST_SUITE OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    SFML
    GIT_REPOSITORY https://github.com/SFML/SFML.git
    GIT_TAG 2.6.1
    GIT_SHALLOW TRUE
)

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.89.9
    GIT_SHALLOW TRUE
)

FetchContent_Declare(
    imgui-sfml
    GIT_REPOSITORY https://github.com/SFML/imgui-sfml.git
    GIT_TAG v2.6.1
    GIT_SHALLOW TRUE
)

FetchContent_Declare(
    Designar
    GIT_REPOSITORY https://github.com/R3mmurd/DeSiGNAR.git
    GIT_TAG v4.0.1
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(SFML imgui)
set(IMGUI_DIR "${imgui_SOURCE_DIR}" CACHE PATH "Dear ImGui source directory" FORCE)
set(IMGUI_SFML_FIND_SFML OFF CACHE BOOL "" FORCE)
set(IMGUI_SFML_IMGUI_DEMO ON CACHE BOOL "" FORCE)
set(IMGUI_SFML_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(IMGUI_SFML_BUILD_TESTING OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(imgui-sfml)

FetchContent_GetProperties(Designar)
if(NOT designar_POPULATED)
    FetchContent_Populate(Designar)
endif()

add_library(Designar INTERFACE)
target_include_directories(Designar INTERFACE "${designar_SOURCE_DIR}/include")

add_executable(ProyectoPr3
    src/main.cpp
    src/Game.cpp
    src/AssetManager.cpp
    src/Enemy.cpp
)

target_include_directories(ProyectoPr3 PRIVATE include)
target_link_libraries(ProyectoPr3 PRIVATE ImGui-SFML::ImGui-SFML Designar)

if(MSVC)
    target_compile_options(ProyectoPr3 PRIVATE /W4 /permissive-)
else()
    target_compile_options(ProyectoPr3 PRIVATE -Wall -Wextra -Wpedantic)
endif()
