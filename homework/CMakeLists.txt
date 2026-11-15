add_executable(homework2 main.cpp)

target_link_libraries(homework2 PRIVATE math wgpu_app)

target_compile_definitions(homework2 PRIVATE
    PROJECT_ROOT="${CMAKE_CURRENT_SOURCE_DIR}"
)
