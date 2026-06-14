cmake_minimum_required(VERSION 3.17)

project(auik-awin
    VERSION 0.0.1
    LANGUAGES CXX C
    DESCRIPTION "AUIK awin Window Backend")

add_files(${PROJECT_NAME} RECURSE "${CMAKE_CURRENT_SOURCE_DIR}")

add_library(${PROJECT_NAME} SHARED ${AUIK_AWIN_SRC})

set_target_properties(${PROJECT_NAME} PROPERTIES CXX_VISIBILITY_PRESET hidden)
set_target_properties(${PROJECT_NAME} PROPERTIES DEFINE_SYMBOL AUIK_BUILD)

target_link_libraries(${PROJECT_NAME} PRIVATE acul awin auik)

if(WIN32)
    gen_manifest_lib(lib${PROJECT_NAME} ${PROJECT_VERSION})
endif()

set_target_properties(${PROJECT_NAME}
    PROPERTIES
    CXX_EXTENSIONS YES
)
