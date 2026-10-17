# Build tests
add_executable(lute_unit_tests
                    main.cpp
                    lute_text_parser_tests.cpp
                    lute_db_tests.cpp
                    lute_phrase_tests.cpp
)

target_include_directories(lute_unit_tests
                PRIVATE
                    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(lute_unit_tests
                PRIVATE
                    gtest
                    lute
                    Qt6::Core
                    Qt6::Gui
                    Qt6::Sql
)

target_compile_options(lute_unit_tests PRIVATE "${LIBRUM_SANITIZERS};-O0;-g")
target_link_options(lute_unit_tests PRIVATE "${LIBRUM_SANITIZERS}")

add_test(NAME LuteUnitTests
         COMMAND ${PROJECT_BINARY_DIR}/lute_unit_tests)
