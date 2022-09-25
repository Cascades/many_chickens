set(STB_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/stb)

add_library(stb_image INTERFACE)

target_include_directories(stb_image INTERFACE ${STB_SRC_DIR})

add_library(stb::image ALIAS stb_image)