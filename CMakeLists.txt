cmake_minimum_required(VERSION 3.17)
project(app3d-uikit
    VERSION 0.0.1
    LANGUAGES CXX C
    DESCRIPTION "App3d UIKIT Lib")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++ -fexperimental-library")
add_definitions(-D_LIBCPP_NO_VCRUNTIME)

list(APPEND SOURCE_FILES
    "src/button/button.cpp"
    "src/combobox/combobox.cpp"
    "src/image/image.cpp"
    "src/icon/icon.cpp"
    "src/selectable/selectable.cpp"
    "src/menu/menu.cpp"
    "src/tab/tab.cpp"
    "src/text/text.cpp"
    "src/modal/modal.cpp"
    "src/window@imgui.cpp"
)

if(WIN32)
    list(APPEND SOURCE_FILES
        "src/titlebar/titlebar@win32.cpp"
    )
endif()

add_library(${PROJECT_NAME} SHARED ${SOURCE_FILES})
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/include/")

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
    )
endif()

set_target_properties(${PROJECT_NAME}
    PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS YES
)