set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

# Lute language-learning slice: database access, word extraction, the
# controller exposed to QML and the AI translation client.
set(lute_SRC
    lute_book.hpp
    lute_ai_access.cpp
    lute_ai_access.hpp
    lute_controller.cpp
    lute_controller.hpp
    lute_db_access.cpp
    lute_db_access.hpp
    lute_language.hpp
    lute_tag.hpp
    lute_paginator.cpp
    lute_paginator.hpp
    lute_schema.cpp
    lute_schema.hpp
    lute_stats.hpp
    lute_status.cpp
    lute_status.hpp
    lute_term.hpp
    lute_text_parser.cpp
    lute_translation_access.cpp
    lute_translation_access.hpp
    lute_text_parser.hpp
    lute_word_extractor.cpp
    lute_word_extractor.hpp
    lute_data.qrc
    phrase_matcher.cpp
    phrase_matcher.hpp
    word_quad.cpp
    word_quad.hpp
)

add_library(lute STATIC ${lute_SRC})

# mupdfwrap generates the wrapper headers (e.g. platform/c++/include/mupdf/
# classes.h) that lute_word_extractor.cpp includes, so the lute target must
# not compile before the mupdf target has run.
add_dependencies(lute mupdf)

target_include_directories(lute
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE
        ${PROJECT_SOURCE_DIR}/libs/mupdf/include
        ${PROJECT_SOURCE_DIR}/libs/mupdf/platform/c++/include
)

target_link_libraries(lute
    PUBLIC
        Qt6::Core
        Qt6::Gui
        Qt6::Sql
        Qt6::Network
)

if(WIN32)
    # The MuPDF C++ wrapper DLL does not export the C API, so the word
    # extractor links the static MuPDF libraries that the same build produced.
    # Link directories are INTERFACE so they reach the final executable link.
    set(MUPDF_STATIC_DIR
        "${PROJECT_SOURCE_DIR}/libs/mupdf/platform/win32-vs-upgrade/x64/$<IF:$<CONFIG:Debug>,Debug,Release>")
    target_link_directories(lute INTERFACE ${MUPDF_STATIC_DIR})
    target_link_libraries(lute INTERFACE
        libmupdf
        libthirdparty
        libresources
        libharfbuzz
        libmuthreads
        gdi32
        user32
        advapi32
        shell32
        ole32
        crypt32
        ws2_32
    )
else()
    # The word extractor calls the raw MuPDF C API (fz_*), which the C++
    # wrapper library does not re-export, so libmupdf.so must be linked
    # wherever liblute.a is linked. INTERFACE propagates this to the
    # executable and the test targets.
    set(LUTE_MUPDF_SHARED_DIR
        "${PROJECT_SOURCE_DIR}/libs/mupdf/build/$<IF:$<CONFIG:Debug>,shared-debug,shared-release>")
    target_link_libraries(lute INTERFACE
        "${LUTE_MUPDF_SHARED_DIR}/libmupdfcpp.so"
        "${LUTE_MUPDF_SHARED_DIR}/libmupdf.so"
    )
endif()

target_compile_options(lute PRIVATE "$<$<CONFIG:DEBUG>:${LIBRUM_SANITIZERS};-Wall;-O0;-g>")
target_compile_options(lute PRIVATE "$<$<CONFIG:RELEASE>:-O3>")
