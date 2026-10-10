cmake_minimum_required(VERSION 3.17)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests)

set(TEST_LIBRARIES auik)
set(TEST_SOURCES "")

add_test_files(auik dock_tree dock_tree.cpp)
add_test_files(auik textbox_clip textbox_clip.cpp)
add_test_files(auik tree_refresh tree_refresh.cpp)
add_test_files(auik model_field_batch model_field_batch.cpp)
add_test_files(auik text_rerecord text_rerecord.cpp)
add_test_files(auik bounds_animation bounds_animation.cpp)
add_test(NAME auik_asd COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/test_asd.py")

if(ENABLE_COVERAGE)
    add_test_coverage(auik)
endif()
