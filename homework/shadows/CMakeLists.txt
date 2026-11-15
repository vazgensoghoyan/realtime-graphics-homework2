add_library(shadows STATIC
    directional_shadow.cpp
    directional_shadow.hpp
)

target_include_directories(shadows PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(shadows PUBLIC math wgpu_app)
