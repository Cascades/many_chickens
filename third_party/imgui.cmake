set(IMGUI_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/imgui)

add_library(imgui STATIC
	${IMGUI_SRC_DIR}/imgui.cpp
	${IMGUI_SRC_DIR}/imgui_demo.cpp
	${IMGUI_SRC_DIR}/imgui_draw.cpp
	${IMGUI_SRC_DIR}/backends/imgui_impl_glfw.cpp
	${IMGUI_SRC_DIR}/backends/imgui_impl_vulkan.cpp
	${IMGUI_SRC_DIR}/imgui_tables.cpp
	${IMGUI_SRC_DIR}/imgui_widgets.cpp
)

target_include_directories(imgui PUBLIC
	${IMGUI_SRC_DIR}
	${IMGUI_SRC_DIR}/backends
)

target_link_libraries(imgui
	glfw
	Vulkan::Vulkan
)
