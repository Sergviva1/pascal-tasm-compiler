add_library(pascal_parser parser.cpp)
target_include_directories(pascal_parser PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(pascal_parser PUBLIC pascal_contracts)

add_executable(parser_cli main.cpp)
target_link_libraries(parser_cli PRIVATE pascal_parser pascal_lexer)

if(BUILD_TESTING)
    add_executable(parser_lab1_tests tests/lab1/tests.cpp)
    target_link_libraries(parser_lab1_tests PRIVATE pascal_parser)
    foreach(n RANGE 1 4)
        add_test(NAME parser.T${n} COMMAND ${CMAKE_COMMAND}
            -DRUNNER=$<TARGET_FILE:parser_lab1_tests> -DCASE=T${n}
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/lab1/run.cmake")
        add_test(NAME parser.cli.T${n} COMMAND ${CMAKE_COMMAND}
            -DCLI=$<TARGET_FILE:parser_cli> -DCASE=T${n}
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/lab1/run.cmake")
    endforeach()
    add_test(NAME parser.boundaries COMMAND parser_lab1_tests --boundaries)
endif()
