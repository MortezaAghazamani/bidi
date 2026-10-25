cmake_minimum_required(VERSION 3.16)
project(bidi VERSION 1.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# ---------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------
option(BIDI_BUILD_TESTS    "Build bidi tests"                     ON)
option(BIDI_BUILD_EXAMPLES "Build bidi examples"                  ON)
option(BIDI_BUILD_DOCS     "Build API documentation with Doxygen" OFF)
option(BIDI_BUILD_IMGUI    "Build bidi::imbidi::InputText"        OFF)

# ---------------------------------------------------------------------
# Library sources
# ---------------------------------------------------------------------
set(BIDI_SOURCES
    src/bidi.cpp
    src/text_utils.cpp
    src/detail/utf.cpp
    src/detail/bidi_class_table.cpp
    src/detail/bidi_brackets_table.cpp
    src/detail/bidi_mirroring_table.cpp
    src/detail/bracket_pairer.cpp
    src/detail/explicit_levels.cpp
    src/detail/weak_types.cpp
    src/detail/neutral_types.cpp
    src/detail/implicit_levels.cpp
    src/detail/reorder.cpp
    src/detail/bidi_algorithm.cpp
    # Shaping is now part of the core library.
    src/shaped_text.cpp
    src/input_model.cpp
    src/detail/arabic_persian_shaping_table.cpp
)

if(BIDI_BUILD_IMGUI)
    find_package(imgui CONFIG REQUIRED)
    list(APPEND BIDI_SOURCES
        src/imbidi/input_text.cpp
    )
endif()

# ---------------------------------------------------------------------
# Library
# ---------------------------------------------------------------------
add_library(bidi STATIC ${BIDI_SOURCES})
add_library(bidi::bidi ALIAS bidi)

target_include_directories(bidi PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_include_directories(bidi PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/detail
)

# Shaping is always on now.
target_compile_definitions(bidi PUBLIC BIDI_HAS_SHAPING=1)

if(BIDI_BUILD_IMGUI)
    target_compile_definitions(bidi PUBLIC BIDI_HAS_IMGUI=1)
    target_link_libraries(bidi PUBLIC imgui::imgui)
endif()

if(MSVC)
    target_compile_options(bidi PRIVATE /W4 /permissive-)
else()
    target_compile_options(bidi PRIVATE -Wall -Wextra -Wpedantic)
endif()

# ---------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------
if(BIDI_BUILD_TESTS)
    enable_testing()

    set(BIDI_INTERNAL_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/detail
    )

    set(BIDI_TEST_TARGETS
        test_bracket_pairer
        test_explicit_levels
        test_end_to_end
        test_bidi_class
        test_bidi_object
        test_bidi_object_unit
        test_shaped_text
        test_input_model
    )

    foreach(t ${BIDI_TEST_TARGETS})
        add_executable(${t} tests/${t}.cpp)
        target_link_libraries(${t} PRIVATE bidi)
        target_include_directories(${t} PRIVATE ${BIDI_INTERNAL_INCLUDES})
        add_test(NAME ${t} COMMAND ${t})
    endforeach()

    add_executable(test_unicode_conformance tests/test_unicode_conformance.cpp)
    target_link_libraries(test_unicode_conformance PRIVATE bidi)
    target_include_directories(test_unicode_conformance PRIVATE ${BIDI_INTERNAL_INCLUDES})

    set(BIDI_CONFORMANCE_FILE
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/BidiCharacterTest.txt")
    if(EXISTS "${BIDI_CONFORMANCE_FILE}")
        add_test(NAME test_unicode_conformance
                 COMMAND test_unicode_conformance "${BIDI_CONFORMANCE_FILE}")
    else()
        message(STATUS
            "BidiCharacterTest.txt not found; conformance test disabled.")
    endif()
endif()

# ---------------------------------------------------------------------
# Examples
# ---------------------------------------------------------------------
if(BIDI_BUILD_EXAMPLES)
    foreach(ex basic editor)
        add_executable(example_${ex} examples/${ex}.cpp)
        target_link_libraries(example_${ex} PRIVATE bidi)
    endforeach()

    if(BIDI_BUILD_IMGUI)
        find_package(SDL3 CONFIG QUIET)
        if(TARGET SDL3::SDL3)
            add_executable(example_imgui_demo examples/imgui_demo.cpp)
            target_link_libraries(example_imgui_demo PRIVATE
                bidi
                imgui::imgui
                SDL3::SDL3
                opengl32
            )
        else()
            message(STATUS "SDL3 not found; imgui_demo example disabled.")
        endif()
    endif()
endif()

# ---------------------------------------------------------------------
# Documentation (Doxygen)
# ---------------------------------------------------------------------
if(BIDI_BUILD_DOCS)
    find_package(Doxygen REQUIRED)
    set(DOXYGEN_IN  "${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in")
    set(DOXYGEN_OUT "${CMAKE_CURRENT_BINARY_DIR}/Doxyfile")
    configure_file("${DOXYGEN_IN}" "${DOXYGEN_OUT}" @ONLY)
    add_custom_target(docs
        COMMAND "${DOXYGEN_EXECUTABLE}" "${DOXYGEN_OUT}"
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
        COMMENT "Generating API documentation with Doxygen"
        VERBATIM
    )
endif()

# ---------------------------------------------------------------------
# Install
# ---------------------------------------------------------------------
include(GNUInstallDirs)

install(TARGETS bidi
    EXPORT bidiTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY include/bidi
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(EXPORT bidiTargets
    FILE bidiTargets.cmake
    NAMESPACE bidi::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/bidi
)

include(CMakePackageConfigHelpers)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/bidiConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/bidiConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/bidiConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/bidi
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/bidiConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/bidiConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/bidi
)
