# Build tests
add_executable(lute_unit_tests
                    main.cpp
                    lute_text_parser_tests.cpp
                    lute_db_tests.cpp
                    lute_phrase_tests.cpp
                    lute_paginator_tests.cpp
                    lute_word_extractor_tests.cpp
                    lute_status_tests.cpp
                    lute_translation_tests.cpp
                    lute_controller_tests.cpp
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

if(WIN32)
    target_link_directories(lute_unit_tests PRIVATE
        "${PROJECT_SOURCE_DIR}/libs/mupdf/platform/win32-vs-upgrade/x64/$<IF:$<CONFIG:Debug>,Debug,Release>")
    target_link_libraries(lute_unit_tests PRIVATE mupdfcpp64)
else()
    set(LUTE_MUPDF_LIB_DIR
        "${PROJECT_SOURCE_DIR}/libs/mupdf/build/$<IF:$<CONFIG:Debug>,shared-debug,shared-release>")
    target_link_libraries(lute_unit_tests PRIVATE "${LUTE_MUPDF_LIB_DIR}/libmupdfcpp.so")
    add_dependencies(lute_unit_tests mupdf)
    set_target_properties(lute_unit_tests PROPERTIES BUILD_RPATH "${LUTE_MUPDF_LIB_DIR}")
endif()

target_compile_definitions(lute_unit_tests PRIVATE
    LUTE_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data")

target_compile_options(lute_unit_tests PRIVATE "${LIBRUM_SANITIZERS};-O0;-g")
target_link_options(lute_unit_tests PRIVATE "${LIBRUM_SANITIZERS}")

add_test(NAME LuteUnitTests
         COMMAND ${PROJECT_BINARY_DIR}/lute_unit_tests)
