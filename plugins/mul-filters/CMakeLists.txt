project(mul-filters)

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/mul-filters-config.h.in"
	"${CMAKE_BINARY_DIR}/plugins/mul-filters/config/mul-filters-config.h")

set(mul-filters_config_HEADERS
	"${CMAKE_BINARY_DIR}/plugins/mul-filters/config/mul-filters-config.h")
include_directories(${LIBSPEEXDSP_INCLUDE_DIRS}
	"${CMAKE_BINARY_DIR}/plugins/mul-filters/config")

set(mul-filters_SOURCES
	mul-filters.c
	color-inverse.c
	mirror-filter.c
	radial-wave-filter.c
	gauss-filter.c
	edge-detection-filter.c
	pixelize-filter.c
	)

add_library(mul-filters MODULE
	${mul-filters_SOURCES}
	${mul-filters_config_HEADERS}
target_link_libraries(mul-filters
	libobs
	${mul-filters_PLATFORM_DEPS}

install_obs_plugin_with_data(mul-filters data)
