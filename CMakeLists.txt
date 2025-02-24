cmake_minimum_required(VERSION 3.17)
project(app3d-uikit
    VERSION 0.0.1
    LANGUAGES CXX C
    DESCRIPTION "App3d UIKIT Lib")

add_library(imgui SHARED
    modules/3rd-party/imgui/imgui.cpp
    modules/3rd-party/imgui/imgui_draw.cpp
    modules/3rd-party/imgui/imgui_widgets.cpp
    modules/3rd-party/imgui/imgui_tables.cpp
    modules/3rd-party/imgui/backends/imgui_impl_vulkan.cpp
    modules/3rd-party/imgui/misc/freetype/imgui_freetype.cpp
    modules/3rd-party/imgui/misc/cpp/imgui_stdlib.cpp
    modules/3rd-party/implot/implot.cpp
    modules/3rd-party/implot/implot_items.cpp
)

if(WIN32)
    set(imgui_VERSION 1.91.9 PARENT_SCOPE)
    gen_manifest_lib("imgui" 1.91.9)
endif()

target_compile_definitions(imgui PUBLIC
    IMGUI_IMPL_VULKAN_NO_PROTOTYPES
    IMGUI_ENABLE_FREETYPE
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

target_include_directories(imgui PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/modules/3rd-party/imgui
    ${FREETYPE2_INCLUDE_DIRS})
target_link_libraries(imgui PUBLIC ${FREETYPE2_LIBRARIES})

if(USE_ASAN)
    target_link_options(imgui PUBLIC -fsanitize=address -fno-omit-frame-pointer)
endif(USE_ASAN)

add_files(${PROJECT_NAME} RECURSE "${CMAKE_CURRENT_SOURCE_DIR}/src")

add_library(${PROJECT_NAME} SHARED ${SOURCE_FILES})
set_target_properties(${PROJECT_NAME} PROPERTIES CXX_VISIBILITY_PRESET hidden)
target_compile_definitions(${PROJECT_NAME} PRIVATE -DAPPLIB_BUILD)
target_include_directories(${PROJECT_NAME} PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include/"
    "${CMAKE_CURRENT_SOURCE_DIR}/modules/3rd-party/")

target_link_libraries(${PROJECT_NAME} PRIVATE
    app3d-core
    app3d-window
)
target_link_libraries(${PROJECT_NAME} PUBLIC imgui)

if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        Winmm
        dwmapi
    )

    # Manifest
    gen_manifest_lib(${PROJECT_NAME} ${PROJECT_VERSION})
endif()

set_target_properties(${PROJECT_NAME}
    PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS YES
)