cmake_minimum_required(VERSION 3.17)
project(app3d-uikit
    VERSION 0.0.1
    LANGUAGES CXX C
    DESCRIPTION "App3d UIKIT Lib")

add_library(imgui SHARED
    modules/3rd-party/imgui/imgui.cpp
    modules/3rd-party/imgui/imgui_draw.cpp
    modules/3rd-party/imgui/imgui_widgets.cpp
    modules/3rd-party/imgui/imgui_demo.cpp
    modules/3rd-party/imgui/imgui_tables.cpp
    modules/3rd-party/imgui/backends/imgui_impl_vulkan.cpp
    modules/3rd-party/imgui/misc/freetype/imgui_freetype.cpp
)

target_compile_definitions(imgui PUBLIC
    IMGUI_IMPL_VULKAN_NO_PROTOTYPES
    IMGUI_ENABLE_FREETYPE
)

if(USE_ASAN)
    target_link_options(imgui PUBLIC -fsanitize=address -fno-omit-frame-pointer)
endif(USE_ASAN)

find_package(PkgConfig REQUIRED)
pkg_check_modules(FREETYPE2 REQUIRED freetype2)
include_directories(modules/3rd-party/imgui ${FREETYPE2_INCLUDE_DIRS})
target_include_directories(imgui PUBLIC modules/3rd-party/imgui ${FREETYPE2_INCLUDE_DIRS})
target_link_libraries(imgui PUBLIC ${FREETYPE2_LIBRARIES})

add_files(${PROJECT_NAME} RECURSE "${CMAKE_CURRENT_SOURCE_DIR}/src")

add_library(${PROJECT_NAME} SHARED ${SOURCE_FILES})
set_target_properties(${PROJECT_NAME} PROPERTIES CXX_VISIBILITY_PRESET hidden)
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/include/")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/modules/3rd-party/")

target_include_directories(${PROJECT_NAME} PRIVATE
    $<TARGET_PROPERTY:imgui,INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:app3d-window,INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:app3d-core,INCLUDE_DIRECTORIES>
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    app3d-core
    app3d-window
    imgui
)

if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        Winmm
        dwmapi
    )
endif()

set_target_properties(${PROJECT_NAME}
    PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS YES
)