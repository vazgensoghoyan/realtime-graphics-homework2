add_subdirectory(camera)
add_subdirectory(light_marker)
add_subdirectory(shadows)

add_executable(homework2 main.cpp)

target_link_libraries(homework2 PRIVATE
    math
    wgpu_app
    camera
    light_marker
    shadows
)

target_compile_definitions(homework2 PRIVATE
    PROJECT_ROOT="${CMAKE_CURRENT_SOURCE_DIR}"
)
