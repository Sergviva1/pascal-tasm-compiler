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
    target_link_libraries(fixture_tests PRIVATE project_options)
    set_target_properties(fixture_tests PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME fixtures COMMAND fixture_tests "${CMAKE_CURRENT_SOURCE_DIR}/tests")
endif()
