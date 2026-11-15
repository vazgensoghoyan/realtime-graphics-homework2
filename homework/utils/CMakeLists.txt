add_library(utils STATIC
    utils.cpp
    utils.hpp
)

target_include_directories(utils PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(utils PUBLIC wgpu_app)
