cmake_minimum_required(VERSION 3.17)
project(app3d-uikit
    VERSION 0.0.1
    LANGUAGES CXX C
    DESCRIPTION "App3d UIKIT Lib")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++ -fexperimental-library")
add_definitions(-D_LIBCPP_NO_VCRUNTIME)

list(APPEND SOURCE_FILES
    "src/main.cpp"
)

add_library(${PROJECT_NAME} SHARED ${SOURCE_FILES})
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/include/")

set_target_properties(${PROJECT_NAME}
    PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS YES
)