cmake_minimum_required(VERSION 3.16)
project(pascal_tasm_compiler LANGUAGES CXX)

add_library(project_options INTERFACE)
target_compile_features(project_options INTERFACE cxx_std_17)
target_include_directories(project_options INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")
if(MSVC)
    target_compile_options(project_options INTERFACE /W4 /utf-8)
else()
    target_compile_options(project_options INTERFACE -Wall -Wextra -Wpedantic)
endif()

add_executable(lexer lexer/main.cpp)
add_executable(parser parser/main.cpp)
add_executable(pascal_codegen
    codegen/main.cpp
    codegen/code_generator.cpp
    codegen/code_generator_statements.cpp
    codegen/code_generator_expressions.cpp
)
set_target_properties(pascal_codegen PROPERTIES OUTPUT_NAME codegen)

foreach(target IN ITEMS lexer parser pascal_codegen)
    target_link_libraries(${target} PRIVATE project_options)
    set_target_properties(${target} PROPERTIES CXX_EXTENSIONS OFF)
endforeach()

include(CTest)
if(BUILD_TESTING)
    add_executable(fixture_tests tests/fixture_tests.cpp)
    add_executable(parser_declaration_tests parser/declaration_tests.cpp)
    add_executable(parser_tests parser/parser_tests.cpp)

    foreach(test_target IN ITEMS fixture_tests parser_declaration_tests parser_tests)
        target_link_libraries(${test_target} PRIVATE project_options)
        set_target_properties(${test_target} PROPERTIES CXX_EXTENSIONS OFF)
    endforeach()

    add_test(NAME fixtures COMMAND fixture_tests "${CMAKE_CURRENT_SOURCE_DIR}/tests")
    add_test(NAME parser_declarations COMMAND parser_declaration_tests)
    add_test(NAME parser_semantics COMMAND parser_tests)
    set_tests_properties(parser_declarations parser_semantics PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    )

    add_test(
        NAME parser_codegen_integration
        COMMAND "${CMAKE_COMMAND}"
            "-DPARSER_EXE=$<TARGET_FILE:parser>"
            "-DPARSER_TESTS_EXE=$<TARGET_FILE:parser_tests>"
            "-DCODEGEN_EXE=$<TARGET_FILE:pascal_codegen>"
            "-DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}"
            "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/parser_codegen_integration"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/parser_codegen_integration.cmake"
    )
endif()
