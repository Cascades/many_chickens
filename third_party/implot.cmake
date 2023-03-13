set(IMPLOT_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/implot)

add_library(implot STATIC
	${IMPLOT_SRC_DIR}/implot.cpp
	${IMPLOT_SRC_DIR}/implot_demo.cpp
	${IMPLOT_SRC_DIR}/implot_items.cpp
)

target_include_directories(implot PUBLIC
	${IMPLOT_SRC_DIR}
)

target_link_libraries(implot PUBLIC
	imgui
)
