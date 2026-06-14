include_guard(GLOBAL)

find_package(Python3 REQUIRED COMPONENTS Interpreter)

set(AUIK_THEME_COMPILER "${CMAKE_CURRENT_LIST_DIR}/theme_compile.py")

function(_auik_collect_css_files OUT_VAR INPUT_BASE INPUT_FOLDER)
    set(css_files "${INPUT_BASE}")

    if(INPUT_FOLDER)
        file(GLOB folder_css "${INPUT_FOLDER}/*.css")
        list(APPEND css_files ${folder_css})
    endif()

    list(REMOVE_DUPLICATES css_files)
    set(${OUT_VAR} ${css_files} PARENT_SCOPE)
endfunction()

function(compile_auik_default_style_ids INPUT_BASE)
    set(output_dir "${CMAKE_CURRENT_BINARY_DIR}/include")
    set(output_header "${output_dir}/auik/widget_tags.hpp")
    get_filename_component(input_base_dir "${INPUT_BASE}" DIRECTORY)
    set(output_csv "${input_base_dir}/default_style_tags_id.csv")
    set(output_target "auik_default_style_ids")

    _auik_collect_css_files(css_deps "${INPUT_BASE}" "")

    add_custom_command(
        OUTPUT "${output_csv}" "${output_header}"
        COMMAND "${Python3_EXECUTABLE}" "${AUIK_THEME_COMPILER}"
            --input-base "${INPUT_BASE}"
            --ids-only
            --ids-header "${output_header}"
        DEPENDS "${AUIK_THEME_COMPILER}" ${css_deps}
        MAIN_DEPENDENCY "${INPUT_BASE}"
        COMMENT "Generate auik default style ids"
        VERBATIM
    )

    add_custom_target(${output_target} DEPENDS "${output_csv}" "${output_header}")
    set_source_files_properties("${output_header}" PROPERTIES GENERATED TRUE)

    set(AUIK_STYLE_IDS_CSV "${output_csv}" PARENT_SCOPE)
    set(AUIK_STYLE_IDS_HEADER "${output_header}" PARENT_SCOPE)
    set(AUIK_STYLE_IDS_INCLUDE_DIR "${output_dir}" PARENT_SCOPE)
    set(AUIK_STYLE_IDS_TARGET "${output_target}" PARENT_SCOPE)
endfunction()

function(compile_auik_css_theme INPUT_BASE)
    set(options)
    set(one_value_args INPUT_FOLDER)
    set(multi_value_args)
    cmake_parse_arguments(AUIK_THEME "${options}" "${one_value_args}" "${multi_value_args}" ${ARGN})

    set(output_dir "${CMAKE_CURRENT_BINARY_DIR}/theme")
    set(output_header "${output_dir}/theme_style_sheet.hpp")
    set(output_source "${output_dir}/theme_style_sheet.cpp")
    set(output_target "auik_css_theme")

    if(AUIK_THEME_INPUT_FOLDER)
        set(input_folder "${AUIK_THEME_INPUT_FOLDER}")
    else()
        get_filename_component(input_folder "${INPUT_BASE}" DIRECTORY)
    endif()

    _auik_collect_css_files(css_deps "${INPUT_BASE}" "${input_folder}")

    add_custom_command(
        OUTPUT "${output_header}" "${output_source}"
        COMMAND "${Python3_EXECUTABLE}" "${AUIK_THEME_COMPILER}"
            --input-base "${INPUT_BASE}"
            --input-folder "${input_folder}"
            --output-folder "${output_dir}"
        DEPENDS "${AUIK_THEME_COMPILER}" ${css_deps} ${AUIK_STYLE_IDS_CSV}
        MAIN_DEPENDENCY "${INPUT_BASE}"
        COMMENT "Generate auik CSS theme"
        VERBATIM
    )

    add_custom_target(${output_target} DEPENDS "${output_header}" "${output_source}")
    if(AUIK_STYLE_IDS_TARGET)
        add_dependencies(${output_target} ${AUIK_STYLE_IDS_TARGET})
    endif()
    set_source_files_properties("${output_header}" "${output_source}" PROPERTIES GENERATED TRUE)

    set(AUIK_CSS_HEADER "${output_header}" PARENT_SCOPE)
    set(AUIK_CSS_SRC "${output_source}" PARENT_SCOPE)
    set(AUIK_CSS_INCLUDE_DIR "${output_dir}" PARENT_SCOPE)
    set(AUIK_CSS_TARGET "${output_target}" PARENT_SCOPE)
endfunction()
