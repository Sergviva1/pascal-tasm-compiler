add_library(pascal_lexer Lexer.cpp)
target_include_directories(pascal_lexer PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(pascal_lexer PUBLIC pascal_contracts)
add_executable(lexer_cli main.cpp)
target_link_libraries(lexer_cli PRIVATE pascal_lexer)

if(BUILD_TESTING)
    foreach(n RANGE 1 4)
        add_test(NAME lexer.T${n} COMMAND ${CMAKE_COMMAND}
            -DCLI=$<TARGET_FILE:lexer_cli> -DN=${n} -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run.cmake)
    endforeach()
endif()
