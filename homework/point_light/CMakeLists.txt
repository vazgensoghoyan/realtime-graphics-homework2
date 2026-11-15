add_library(point_light STATIC
    point_light.cpp
    point_light.hpp
)

target_include_directories(point_light PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(point_light PUBLIC math wgpu_app utils)
