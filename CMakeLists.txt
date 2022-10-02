# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#
cmake_minimum_required (VERSION 3.8)
set (CMAKE_CXX_STANDARD 20)
project ("foundry_mentoring_app")

find_package(Vulkan REQUIRED)

# Include sub-projects.
add_subdirectory ("app")
add_subdirectory ("third_party")

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/assets/ DESTINATION assets)

file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/assets/duck)
file(COPY 
	${CMAKE_CURRENT_SOURCE_DIR}/assets/duck/mat.mtl
	${CMAKE_CURRENT_SOURCE_DIR}/assets/duck/object.obj
	${CMAKE_CURRENT_SOURCE_DIR}/assets/duck/texture.jpg
	DESTINATION ${CMAKE_BINARY_DIR}/assets/duck
)