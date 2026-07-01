include_guard(GLOBAL)

find_package(Python3 REQUIRED COMPONENTS Interpreter)

set(AUIK_THEME_COMPILER "${CMAKE_CURRENT_LIST_DIR}/theme_compile.py")
set(AUIK_UMBF_SIGN_REQUEST_PY "${CMAKE_CURRENT_LIST_DIR}/../../umbf/scripts/sign_request.py")

function(_auik_theme_tool_paths OUT_PYTHON OUT_COMPILER OUT_SIGN_REQUEST)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    set(${OUT_PYTHON} "${Python3_EXECUTABLE}" PARENT_SCOPE)
    set(${OUT_COMPILER} "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/theme_compile.py" PARENT_SCOPE)
    set(${OUT_SIGN_REQUEST} "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../../umbf/scripts/sign_request.py" PARENT_SCOPE)
endfunction()

function(_auik_collect_css_files OUT_VAR INPUT_BASE INPUT_FOLDERS)
    set(css_files "${INPUT_BASE}")

    foreach(input_folder ${INPUT_FOLDERS})
        if(input_folder)
            file(GLOB_RECURSE folder_css "${input_folder}/*.css")
            list(APPEND css_files ${folder_css})
        endif()
    endforeach()

    list(REMOVE_DUPLICATES css_files)
    set(${OUT_VAR} ${css_files} PARENT_SCOPE)
endfunction()

function(auik_register_css_style_processed_ids IDS_CSV IDS_HEADER)
    set_property(GLOBAL APPEND PROPERTY AUIK_CSS_STYLE_PROCESSED_IDS "${IDS_CSV}" "${IDS_HEADER}")
endfunction()

function(_auik_parse_processed_style_ids OUT_ARGS OUT_DEPS)
    get_property(processed_ids GLOBAL PROPERTY AUIK_CSS_STYLE_PROCESSED_IDS)

    set(processed_args)
    set(processed_deps)
    list(LENGTH processed_ids processed_ids_count)
    math(EXPR processed_ids_last "${processed_ids_count} - 1")

    if(processed_ids_count GREATER 0)
        foreach(index RANGE 0 ${processed_ids_last} 2)
            math(EXPR header_index "${index} + 1")
            list(GET processed_ids ${index} ids_csv)
            list(GET processed_ids ${header_index} ids_header)
            list(APPEND processed_args --processed-ids "${ids_csv}" "${ids_header}")
            list(APPEND processed_deps "${ids_csv}" "${ids_header}")
        endforeach()
    endif()

    set(${OUT_ARGS} ${processed_args} PARENT_SCOPE)
    set(${OUT_DEPS} ${processed_deps} PARENT_SCOPE)
endfunction()

function(compile_auik_default_style_ids INPUT_BASE)
    _auik_theme_tool_paths(python_executable theme_compiler sign_request_py)

    set(output_dir "${CMAKE_CURRENT_BINARY_DIR}/include")
    set(output_header "${output_dir}/auik/widget_tags.hpp")
    get_filename_component(input_base_dir "${INPUT_BASE}" DIRECTORY)
    set(output_csv "${input_base_dir}/default_style_tags_id.csv")
    set(output_target "auik_default_style_ids")

    _auik_collect_css_files(css_deps "${INPUT_BASE}" "")

    add_custom_command(
        OUTPUT "${output_csv}" "${output_header}"
        COMMAND "${python_executable}" "${theme_compiler}"
            --input-base "${INPUT_BASE}"
            --ids-only
            --ids-output-csv "${output_csv}"
            --ids-header "${output_header}"
            --sign-request "${sign_request_py}"
        DEPENDS "${theme_compiler}" ${css_deps}
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
    auik_register_css_style_processed_ids("${output_csv}" "${output_header}")
endfunction()

function(compile_auik_style_ids INPUT_BASE)
    _auik_theme_tool_paths(python_executable theme_compiler sign_request_py)

    set(options)
    set(one_value_args IDS_CSV IDS_HEADER OUTPUT_DIR TARGET_NAME)
    set(multi_value_args)
    cmake_parse_arguments(AUIK_STYLE_IDS "${options}" "${one_value_args}" "${multi_value_args}" ${ARGN})

    get_filename_component(input_base_dir "${INPUT_BASE}" DIRECTORY)

    if(AUIK_STYLE_IDS_OUTPUT_DIR)
        set(output_dir "${AUIK_STYLE_IDS_OUTPUT_DIR}")
    else()
        set(output_dir "${CMAKE_CURRENT_BINARY_DIR}/include")
    endif()

    if(AUIK_STYLE_IDS_IDS_CSV)
        set(output_csv "${AUIK_STYLE_IDS_IDS_CSV}")
    else()
        set(output_csv "${input_base_dir}/style_tags_id.csv")
    endif()

    if(AUIK_STYLE_IDS_IDS_HEADER)
        set(output_header "${AUIK_STYLE_IDS_IDS_HEADER}")
    else()
        set(output_header "${output_dir}/auik/style_tags.hpp")
    endif()

    if(AUIK_STYLE_IDS_TARGET_NAME)
        set(output_target "${AUIK_STYLE_IDS_TARGET_NAME}")
    else()
        set(output_target "${PROJECT_NAME}_style_ids")
    endif()

    _auik_collect_css_files(css_deps "${INPUT_BASE}" "")

    add_custom_command(
        OUTPUT "${output_csv}" "${output_header}"
        COMMAND "${python_executable}" "${theme_compiler}"
            --input-base "${INPUT_BASE}"
            --ids-only
            --ids-output-csv "${output_csv}"
            --ids-header "${output_header}"
            --sign-request "${sign_request_py}"
        DEPENDS "${theme_compiler}" ${css_deps}
        MAIN_DEPENDENCY "${INPUT_BASE}"
        COMMENT "Generate ${PROJECT_NAME} style ids"
        VERBATIM
    )

    add_custom_target(${output_target} DEPENDS "${output_csv}" "${output_header}")
    set_source_files_properties("${output_header}" PROPERTIES GENERATED TRUE)

    set(AUIK_STYLE_IDS_CSV "${output_csv}" PARENT_SCOPE)
    set(AUIK_STYLE_IDS_HEADER "${output_header}" PARENT_SCOPE)
    set(AUIK_STYLE_IDS_INCLUDE_DIR "${output_dir}" PARENT_SCOPE)
    set(AUIK_STYLE_IDS_TARGET "${output_target}" PARENT_SCOPE)
    auik_register_css_style_processed_ids("${output_csv}" "${output_header}")
endfunction()

function(compile_auik_css_theme INPUT_BASE)
    _auik_theme_tool_paths(python_executable theme_compiler sign_request_py)

    set(options)
    set(one_value_args INPUT_FOLDER)
    set(multi_value_args INPUT_FOLDERS IDS_CSV)
    cmake_parse_arguments(AUIK_THEME "${options}" "${one_value_args}" "${multi_value_args}" ${ARGN})

    set(output_dir "${CMAKE_CURRENT_BINARY_DIR}")
    set(output_header "${output_dir}/theme_style_sheet.hpp")
    set(output_source "${output_dir}/theme_style_sheet.cpp")
    set(output_target "auik_css_theme")

    if(AUIK_THEME_INPUT_FOLDERS)
        set(input_folders ${AUIK_THEME_INPUT_FOLDERS})
    elseif(AUIK_THEME_INPUT_FOLDER)
        set(input_folders "${AUIK_THEME_INPUT_FOLDER}")
    else()
        get_filename_component(input_folder "${INPUT_BASE}" DIRECTORY)
        set(input_folders "${input_folder}")
    endif()

    _auik_collect_css_files(css_deps "${INPUT_BASE}" "${input_folders}")

    set(input_folder_args)
    foreach(input_folder ${input_folders})
        list(APPEND input_folder_args --input-folder "${input_folder}")
    endforeach()

    set(ids_csv_args)
    foreach(ids_csv ${AUIK_THEME_IDS_CSV})
        list(APPEND ids_csv_args --ids-csv "${ids_csv}")
    endforeach()
    _auik_parse_processed_style_ids(processed_ids_args processed_ids_deps)

    add_custom_command(
        OUTPUT "${output_header}" "${output_source}"
        COMMAND "${python_executable}" "${theme_compiler}"
            --input-base "${INPUT_BASE}"
            ${input_folder_args}
            ${processed_ids_args}
            ${ids_csv_args}
            --output-folder "${output_dir}"
            --sign-request "${sign_request_py}"
        DEPENDS "${theme_compiler}" ${css_deps} ${processed_ids_deps} ${AUIK_THEME_IDS_CSV}
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
