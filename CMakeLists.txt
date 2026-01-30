cmake_minimum_required(VERSION 3.17)

if(NOT DEFINED ACBT_PROJECT_LOADED)
    include(cmake/project.cmake)
endif()

project(auik
    VERSION 0.0.1
    LANGUAGES CXX C
    DESCRIPTION "App3d UI Kit Library")

if(NOT DEFINED ACBT_LOADED)
    include(cmake/utils.cmake)
endif()

set(IMGUI_SRC_FILES
    modules/3rd-party/imgui/imgui.cpp
    modules/3rd-party/imgui/imgui_draw.cpp
    modules/3rd-party/imgui/imgui_widgets.cpp
    modules/3rd-party/imgui/imgui_tables.cpp
    modules/3rd-party/imgui/backends/imgui_impl_vulkan.cpp
    modules/3rd-party/imgui/misc/freetype/imgui_freetype.cpp
    modules/3rd-party/implot/implot.cpp
    modules/3rd-party/implot/implot_items.cpp
)

# Freetype2
find_package(PkgConfig REQUIRED)
pkg_check_modules(FREETYPE2 REQUIRED freetype2)

if(WIN32)
    if(NOT DEFINED BUILD_EXTERNAL_MANIFEST)
        set(BUILD_EXTERNAL_MANIFEST OFF)
    endif()

    if(BUILD_EXTERNAL_MANIFEST)
        gen_manifest_lib("freetype-6" ${FREETYPE2_VERSION})
    endif()
endif()

add_files(${PROJECT_NAME} RECURSE "${CMAKE_CURRENT_SOURCE_DIR}/src")

add_library(${PROJECT_NAME} SHARED ${IMGUI_SRC_FILES} ${AUIK_SRC})

set_target_properties(${PROJECT_NAME} PROPERTIES CXX_VISIBILITY_PRESET hidden)
target_compile_definitions(${PROJECT_NAME} PRIVATE -DAPPLIB_BUILD)
target_compile_definitions(${PROJECT_NAME} PUBLIC
    IMGUI_IMPL_VULKAN_NO_PROTOTYPES
    IMGUI_ENABLE_FREETYPE
    IMGUI_USER_CONFIG="auik/integration/imgui.hpp"
)
target_include_directories(${PROJECT_NAME} PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include/"
    "${CMAKE_CURRENT_SOURCE_DIR}/modules/3rd-party/"
    "${CMAKE_CURRENT_SOURCE_DIR}/modules/3rd-party/imgui/"
    ${FREETYPE2_INCLUDE_DIRS}
)

if(PROJECT_IS_TOP_LEVEL)
    add_subdirectory(modules/acul)
    add_subdirectory(modules/awin)
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE
    acul
    awin
)
target_link_libraries(${PROJECT_NAME} PUBLIC ${FREETYPE2_LIBRARIES})

if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        Winmm
        dwmapi
    )

    # Manifest
    gen_manifest_lib(lib${PROJECT_NAME} ${PROJECT_VERSION})
endif()

set_target_properties(${PROJECT_NAME}
    PROPERTIES
    CXX_EXTENSIONS YES
)