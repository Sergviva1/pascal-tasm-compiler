foreach(variable IN ITEMS PARSER_EXE PARSER_TESTS_EXE CODEGEN_EXE SOURCE_DIR TEST_DIR)
    if(NOT DEFINED ${variable} OR "${${variable}}" STREQUAL "")
        message(FATAL_ERROR "Missing required variable: ${variable}")
    endif()
endforeach()

set(actual_dir "${TEST_DIR}/actual")
set(fixture_dir "${TEST_DIR}/fixture")
file(REMOVE_RECURSE "${TEST_DIR}")
file(MAKE_DIRECTORY "${actual_dir}" "${fixture_dir}")

execute_process(
    COMMAND "${PARSER_EXE}" "${SOURCE_DIR}/tests/tokens.lst"
    WORKING_DIRECTORY "${actual_dir}"
    RESULT_VARIABLE parser_result
    OUTPUT_VARIABLE parser_stdout
    ERROR_VARIABLE parser_stderr
)
if(NOT parser_result EQUAL 0)
    message(FATAL_ERROR
        "Parser failed with exit code ${parser_result}\n"
        "stdout: ${parser_stdout}\n"
        "stderr: ${parser_stderr}"
    )
endif()

execute_process(
    COMMAND "${PARSER_TESTS_EXE}" "${actual_dir}"
    WORKING_DIRECTORY "${SOURCE_DIR}"
    RESULT_VARIABLE parser_check_result
    OUTPUT_VARIABLE parser_check_stdout
    ERROR_VARIABLE parser_check_stderr
)
if(NOT parser_check_result EQUAL 0)
    message(FATAL_ERROR
        "Parser output does not match the reference fixtures\n"
        "stdout: ${parser_check_stdout}\n"
        "stderr: ${parser_check_stderr}"
    )
endif()

execute_process(
    COMMAND "${CODEGEN_EXE}" tree.json symbols.tab
    WORKING_DIRECTORY "${actual_dir}"
    RESULT_VARIABLE actual_codegen_result
    OUTPUT_VARIABLE actual_codegen_stdout
    ERROR_VARIABLE actual_codegen_stderr
)
if(NOT actual_codegen_result EQUAL 0)
    message(FATAL_ERROR
        "Code generator rejected parser output with exit code ${actual_codegen_result}\n"
        "stdout: ${actual_codegen_stdout}\n"
        "stderr: ${actual_codegen_stderr}"
    )
endif()

execute_process(
    COMMAND "${CODEGEN_EXE}"
        "${SOURCE_DIR}/tests/tree.json"
        "${SOURCE_DIR}/tests/symbols.tab"
    WORKING_DIRECTORY "${fixture_dir}"
    RESULT_VARIABLE fixture_codegen_result
    OUTPUT_VARIABLE fixture_codegen_stdout
    ERROR_VARIABLE fixture_codegen_stderr
)
if(NOT fixture_codegen_result EQUAL 0)
    message(FATAL_ERROR
        "Code generator rejected reference fixtures with exit code ${fixture_codegen_result}\n"
        "stdout: ${fixture_codegen_stdout}\n"
        "stderr: ${fixture_codegen_stderr}"
    )
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E compare_files
        "${actual_dir}/program.asm"
        "${fixture_dir}/program.asm"
    RESULT_VARIABLE compare_result
)
if(NOT compare_result EQUAL 0)
    message(FATAL_ERROR "ASM generated from parser output differs from fixture-based ASM")
endif()

message(STATUS "Parser output is structurally correct and accepted by codegen")
