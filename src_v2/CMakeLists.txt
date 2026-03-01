cmake_minimum_required(VERSION 3.17)

if(NOT DEFINED ACBT_PROJECT_LOADED)
    include(cmake/project.cmake)
endif()

project(auik-v2
    VERSION 0.0.1
    LANGUAGES CXX C
    DESCRIPTION "App3d UI Kit Library v2")

if(NOT DEFINED ACBT_LOADED)
    include(cmake/utils.cmake)
endif()

add_files(${PROJECT_NAME} RECURSE "${CMAKE_CURRENT_SOURCE_DIR}/core")
add_files(${PROJECT_NAME} RECURSE "${CMAKE_CURRENT_SOURCE_DIR}/widgets")

add_library(${PROJECT_NAME} SHARED ${AUIK_V2_SRC})

set_target_properties(${PROJECT_NAME} PROPERTIES CXX_VISIBILITY_PRESET hidden)
target_compile_definitions(${PROJECT_NAME} PRIVATE -DAPPLIB_BUILD)

if(PROJECT_IS_TOP_LEVEL)
    add_subdirectory(modules/acul)
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE acul amal auik)

if(WIN32)
    # Manifest
    gen_manifest_lib(lib${PROJECT_NAME} ${PROJECT_VERSION})
endif()

set_target_properties(${PROJECT_NAME}
    PROPERTIES
    CXX_EXTENSIONS YES
)

if(NOT DEFINED AUIK_GPU_BACKEND)
    message(FATAL_ERROR "AUIK_GPU_BACKEND must be set by the parent project")
endif()

if(AUIK_GPU_BACKEND STREQUAL "VULKAN")
    add_subdirectory(backends/agrb)
else()
    message(FATAL_ERROR "Unknown AUIK_GPU_BACKEND='${AUIK_GPU_BACKEND}'")
endif()

if(AUIK_WINDOW_BACKEND STREQUAL "AWIN")
    add_subdirectory(backends/awin)
else()
    message(FATAL_ERROR "Unknown AUIK_WINDOW_BACKEND='${AUIK_WINDOW_BACKEND}'")
endif()
