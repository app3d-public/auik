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

include(tools/theme_build.cmake)

create_version_file()
create_symbol_export_file()

set(AUIK_BASE_THEME "${CMAKE_CURRENT_SOURCE_DIR}/src/style/theme.css")

add_files(${PROJECT_NAME} RECURSE "${CMAKE_CURRENT_SOURCE_DIR}/src/core")
add_files(${PROJECT_NAME} RECURSE "${CMAKE_CURRENT_SOURCE_DIR}/src/widgets")
compile_auik_default_style_ids("${AUIK_BASE_THEME}")
list(APPEND AUIK_SRC ${AUIK_STYLE_IDS_HEADER})

set(AUIK_CSS_THEME_ARGS)

if(DEFINED AUIK_CSS_DIR)
    list(APPEND AUIK_CSS_THEME_ARGS INPUT_FOLDER "${AUIK_CSS_DIR}")
endif()

compile_auik_css_theme("${AUIK_BASE_THEME}" ${AUIK_CSS_THEME_ARGS})

add_library(${PROJECT_NAME} SHARED ${AUIK_SRC})
add_dependencies(${PROJECT_NAME} ${AUIK_STYLE_IDS_TARGET})

set_target_properties(${PROJECT_NAME} PROPERTIES
    DEFINE_SYMBOL AUIK_BUILD
    CXX_VISIBILITY_PRESET hidden
)
find_package(PkgConfig REQUIRED)
pkg_check_modules(FREETYPE2 REQUIRED freetype2)
pkg_search_module(FONTCONFIG REQUIRED fontconfig)
pkg_check_modules(HARFBUZZ REQUIRED harfbuzz)

target_include_directories(${PROJECT_NAME} PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${AUIK_STYLE_IDS_INCLUDE_DIR}"
    "${AUIK_CONFIGURE_PATH}"
    ${FREETYPE2_INCLUDE_DIRS}
    ${FONTCONFIG_INCLUDE_DIRS}
    ${HARFBUZZ_INCLUDE_DIRS}
)

if(PROJECT_IS_TOP_LEVEL)
    add_subdirectory(modules/acul)
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE acul amal umbf)

target_link_libraries(${PROJECT_NAME} PRIVATE
    ${FREETYPE2_LIBRARIES}
    ${FONTCONFIG_LIBRARIES}
    ${HARFBUZZ_LIBRARIES}
)

if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE Winmm)
    target_compile_definitions(${PROJECT_NAME} PRIVATE -DWIN32_LEAN_AND_MEAN -DNOMINMAX -DWINVER=0x0A00)

    # Manifest
    gen_manifest_lib(lib${PROJECT_NAME} ${PROJECT_VERSION})

    if(NOT DEFINED BUILD_EXTERNAL_MANIFEST)
        set(BUILD_EXTERNAL_MANIFEST OFF)
    endif()

    if(BUILD_EXTERNAL_MANIFEST)
        gen_manifest_lib("freetype-6" ${FREETYPE2_VERSION})
        gen_manifest_lib("fontconfig" ${FONTCONFIG_VERSION})
        gen_manifest_lib("harfbuzz" ${HARFBUZZ_VERSION})
    endif()
endif()

if(UNIX AND NOT APPLE)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${CMAKE_DL_LIBS})
endif()

set_target_properties(${PROJECT_NAME}
    PROPERTIES
    CXX_EXTENSIONS YES
)

get_directory_property(AUIK_HAS_PARENT PARENT_DIRECTORY)

if(AUIK_HAS_PARENT)
    set(AUIK_CSS_SRC "${AUIK_CSS_SRC}" PARENT_SCOPE)
    set(AUIK_CSS_HEADER "${AUIK_CSS_HEADER}" PARENT_SCOPE)
    set(AUIK_CSS_INCLUDE_DIR "${AUIK_CSS_INCLUDE_DIR}" PARENT_SCOPE)
    set(AUIK_CSS_TARGET "${AUIK_CSS_TARGET}" PARENT_SCOPE)
endif()
