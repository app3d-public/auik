cmake_minimum_required(VERSION 3.17)

project(auik-v2-vulkan
    VERSION 0.0.1
    LANGUAGES CXX C
    DESCRIPTION "AUIK v2 Vulkan Backend")

add_files(${PROJECT_NAME} RECURSE "${CMAKE_CURRENT_SOURCE_DIR}")

add_library(${PROJECT_NAME} SHARED ${AUIK_V2_VULKAN_SRC})

include(${CMAKE_SOURCE_DIR}/modules/agrb/tools/shader_tools.cmake)

if(NOT DEFINED AUIK_SHADERS_OUTPUT_DIR)
    message(FATAL_ERROR "Shaders output dir must be set")
endif()

add_shaders(
    "${CMAKE_CURRENT_SOURCE_DIR}/env.yaml"
    "${CMAKE_CURRENT_SOURCE_DIR}/shaders.yaml"
    "${AUIK_SHADERS_OUTPUT_DIR}"
    TARGET_PREFIX UI_SHADERS
    BUILD_DIR "${CMAKE_BINARY_DIR}/src/ui_shaders"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    S2U_TARGET agrb_s2u
)
add_dependencies(${PROJECT_NAME} UI_SHADERS)

set_target_properties(${PROJECT_NAME} PROPERTIES CXX_VISIBILITY_PRESET hidden)
file(TO_CMAKE_PATH "${AUIK_SHADERS_OUTPUT_DIR}" AUIK_SHADERS_OUTPUT_DIR_CMAKE)
target_compile_definitions(${PROJECT_NAME} PRIVATE
    -DAPPLIB_BUILD
    AUIK_SHADERS_OUTPUT_DIR=\"${AUIK_SHADERS_OUTPUT_DIR_CMAKE}\"
)
target_include_directories(${PROJECT_NAME} PRIVATE
    "${CMAKE_BINARY_DIR}/src/ui_shaders"
)

target_link_libraries(${PROJECT_NAME} PRIVATE acul amal auik auik-v2 agrb)

if(WIN32)
    # Manifest
    gen_manifest_lib(lib${PROJECT_NAME} ${PROJECT_VERSION})
endif()

set_target_properties(${PROJECT_NAME}
    PROPERTIES
    CXX_EXTENSIONS YES
)
