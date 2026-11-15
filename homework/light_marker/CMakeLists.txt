add_library(light_marker STATIC
    light_marker.cpp
    light_marker.hpp
)

target_include_directories(light_marker PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(light_marker PUBLIC math wgpu_app)
