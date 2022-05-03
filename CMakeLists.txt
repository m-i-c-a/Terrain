cmake_minimum_required(VERSION 3.0)
set(CMAKE_BUILD_TYPE Debug)
project(app)

find_package(glfw3 REQUIRED FATAL_ERROR)

# set(TINYGLTF_HEADER_ONLY ON CACHE INTERNAL "" FORCE)
# set(TINYGLTF_INSTALL OFF CACHE INTERNAL "" FORCE)
# add_subdirectory(${CMAKE_HOME_DIRECTORY}/external/tinygltf)

set(IMGUI_INCLUDE_DIRS external/imgui external/imgui/backends)
include_directories(${IMGUI_INCLUDE_DIRS})

set(IMGUI_CORE_FILES
    ${CMAKE_HOME_DIRECTORY}/external/imgui/imgui_demo.cpp
    ${CMAKE_HOME_DIRECTORY}/external/imgui/imgui_draw.cpp
    ${CMAKE_HOME_DIRECTORY}/external/imgui/imgui_tables.cpp
    ${CMAKE_HOME_DIRECTORY}/external/imgui/imgui_widgets.cpp
    ${CMAKE_HOME_DIRECTORY}/external/imgui/imgui.cpp
)
set(IMGUI_BACKEND_FILES
    ${CMAKE_HOME_DIRECTORY}/external/imgui/backends/imgui_impl_glfw.cpp
    ${CMAKE_HOME_DIRECTORY}/external/imgui/backends/imgui_impl_opengl3.cpp)

set(IMGUI_SOURCES "")
set(IMGUI_SOURCES ${IMGUI_CORE_FILES} ${IMGUI_BACKEND_FILES})

add_executable(${PROJECT_NAME} src/main.cpp src/glad.c
    ${IMGUI_SOURCES}
    src/Helpers.cpp src/Helpers.hpp
    src/Camera.cpp src/Camera.hpp)

target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_20)
target_link_libraries(${PROJECT_NAME} glfw GL dl)
target_include_directories(${PROJECT_NAME} PUBLIC
    ${CMAKE_HOME_DIRECTORY}/external/glad/include
    ${CMAKE_HOME_DIRECTORY}/external)
