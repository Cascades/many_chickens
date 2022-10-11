# CMakeList.txt : CMake project for Vulkan0, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

# Add source to this project's executable.
add_executable (app "main.cpp" "VulkanObject.cpp" "GLFWObject.cpp" "Model.cpp")

target_include_directories(app PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

find_package(Vulkan REQUIRED)

target_compile_definitions(app PRIVATE
	VK_USE_PLATFORM_WIN32_KHR
	NOMINMAX
)

target_link_libraries(app
	glfw
	Vulkan::Vulkan
	tinyobjloader
	glm::glm
	stb::image
	imgui
	meshoptimizer
)

file(MAKE_DIRECTORY ${CMAKE_INSTALL_PREFIX}/shaders/vulkan3)
file(TOUCH ${CMAKE_INSTALL_PREFIX}/shaders/vulkan3/geometry_pass_vert.spv)
file(TOUCH ${CMAKE_INSTALL_PREFIX}/shaders/vulkan3/geometry_pass_frag.spv)
file(TOUCH ${CMAKE_INSTALL_PREFIX}/shaders/vulkan3/lighting_pass_vert.spv)
file(TOUCH ${CMAKE_INSTALL_PREFIX}/shaders/vulkan3/lighting_pass_frag.spv)
file(TOUCH ${CMAKE_INSTALL_PREFIX}/shaders/vulkan3/lod_indirect.spv)
file(TOUCH ${CMAKE_INSTALL_PREFIX}/shaders/vulkan3/shadow_pass_vert.spv)
file(TOUCH ${CMAKE_INSTALL_PREFIX}/shaders/vulkan3/shadow_pass_frag.spv)

add_custom_command(OUTPUT
		${CMAKE_INSTALL_PREFIX}/shaders/vulkan3/geometry_pass_vert.spv
		${CMAKE_INSTALL_PREFIX}/shaders/vulkan3/geometry_pass_frag.spv
		${CMAKE_INSTALL_PREFIX}/shaders/vulkan3/lighting_pass_vert.spv
		${CMAKE_INSTALL_PREFIX}/shaders/vulkan3/lighting_pass_frag.spv
		${CMAKE_INSTALL_PREFIX}/shaders/vulkan3/lod_indirect.spv
		${CMAKE_INSTALL_PREFIX}/shaders/vulkan3/shadow_pass_vert.spv
		${CMAKE_INSTALL_PREFIX}/shaders/vulkan3/shadow_pass_frag.spv
	COMMENT "Recompiling shaders"
	COMMAND $ENV{VULKAN_SDK}/Bin/glslc.exe ${CMAKE_CURRENT_SOURCE_DIR}/shaders/geometry_pass.vert -o ${CMAKE_INSTALL_PREFIX}/shaders/vulkan3/geometry_pass_vert.spv
	COMMAND $ENV{VULKAN_SDK}/Bin/glslc.exe ${CMAKE_CURRENT_SOURCE_DIR}/shaders/geometry_pass.frag -o ${CMAKE_INSTALL_PREFIX}/shaders/vulkan3/geometry_pass_frag.spv
	COMMAND $ENV{VULKAN_SDK}/Bin/glslc.exe ${CMAKE_CURRENT_SOURCE_DIR}/shaders/lighting_pass.vert -o ${CMAKE_INSTALL_PREFIX}/shaders/vulkan3/lighting_pass_vert.spv
	COMMAND $ENV{VULKAN_SDK}/Bin/glslc.exe ${CMAKE_CURRENT_SOURCE_DIR}/shaders/lighting_pass.frag -o ${CMAKE_INSTALL_PREFIX}/shaders/vulkan3/lighting_pass_frag.spv
	COMMAND $ENV{VULKAN_SDK}/Bin/glslc.exe -fshader-stage=compute ${CMAKE_CURRENT_SOURCE_DIR}/shaders/lod_indirect.glsl -o ${CMAKE_INSTALL_PREFIX}/shaders/vulkan3/lod_indirect.spv
	COMMAND $ENV{VULKAN_SDK}/Bin/glslc.exe ${CMAKE_CURRENT_SOURCE_DIR}/shaders/shadow_pass.vert -o ${CMAKE_INSTALL_PREFIX}/shaders/vulkan3/shadow_pass_vert.spv
	COMMAND $ENV{VULKAN_SDK}/Bin/glslc.exe ${CMAKE_CURRENT_SOURCE_DIR}/shaders/shadow_pass.frag -o ${CMAKE_INSTALL_PREFIX}/shaders/vulkan3/shadow_pass_frag.spv
	DEPENDS
		${CMAKE_CURRENT_SOURCE_DIR}/shaders/geometry_pass.frag
		${CMAKE_CURRENT_SOURCE_DIR}/shaders/geometry_pass.vert
		${CMAKE_CURRENT_SOURCE_DIR}/shaders/lighting_pass.frag
		${CMAKE_CURRENT_SOURCE_DIR}/shaders/lighting_pass.vert
		${CMAKE_CURRENT_SOURCE_DIR}/shaders/lod_indirect.glsl
		${CMAKE_CURRENT_SOURCE_DIR}/shaders/shadow_pass.frag
		${CMAKE_CURRENT_SOURCE_DIR}/shaders/shadow_pass.vert
)

add_custom_target(shaders3 ALL
    DEPENDS
		${CMAKE_INSTALL_PREFIX}/shaders/vulkan3/geometry_pass_frag.spv
		${CMAKE_INSTALL_PREFIX}/shaders/vulkan3/geometry_pass_vert.spv
		${CMAKE_INSTALL_PREFIX}/shaders/vulkan3/lighting_pass_frag.spv
		${CMAKE_INSTALL_PREFIX}/shaders/vulkan3/lighting_pass_vert.spv
		${CMAKE_INSTALL_PREFIX}/shaders/vulkan3/lod_indirect.spv
		${CMAKE_INSTALL_PREFIX}/shaders/vulkan3/shadow_pass_frag.spv
		${CMAKE_INSTALL_PREFIX}/shaders/vulkan3/shadow_pass_vert.spv
)

install(TARGETS app)

# TODO: Add tests and install targets if needed.
