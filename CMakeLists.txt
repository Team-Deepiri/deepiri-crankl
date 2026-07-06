cmake_minimum_required(VERSION 3.20)
project(deepiri-crankle VERSION 0.2.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(CRANKLE_BUILD_TESTS "Build tests" ON)
option(CRANKLE_ENABLE_SIMD "Enable AVX2 dispatch" ON)

include(cmake/warnings.cmake)

set(CRANKLE_CORE_SOURCES
    src/core/trit.cpp
    src/core/crank_word.cpp
    src/core/clifford.cpp
    src/core/linalg.cpp
    src/core/sheaf.cpp
    src/core/resonance.cpp
    src/core/symplectic.cpp
    src/core/rg.cpp
    src/core/simd.cpp
    src/core/diff.cpp
    src/pack/fold.cpp
    src/pack/unfold.cpp
    src/pack/persistence.cpp
    src/io/cran_reader.cpp
    src/io/cran_writer.cpp
    src/io/cran_metadata.cpp
    src/holonomy/wilson.cpp
    src/c_api/c_api.cpp
)

add_library(crankle SHARED ${CRANKLE_CORE_SOURCES})
target_include_directories(crankle
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/third_party)
set_target_properties(crankle PROPERTIES OUTPUT_NAME crankle VERSION 0.2.0 SOVERSION 0)
crankle_set_warnings(crankle)

if(CRANKLE_ENABLE_SIMD)
    target_compile_definitions(crankle PRIVATE CRANKLE_HAVE_SIMD=1)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(crankle PRIVATE -mavx2 -mfma)
    endif()
endif()

add_executable(crankle-cli src/cli/main.cpp)
target_link_libraries(crankle-cli PRIVATE crankle)
set_target_properties(crankle-cli PROPERTIES OUTPUT_NAME crankle)
crankle_set_warnings(crankle-cli)

install(TARGETS crankle crankle-cli
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib)
install(FILES include/crankle/crankle.h include/crankle/version.h DESTINATION include/crankle)

configure_file(crankle.pc.in crankle.pc @ONLY)
install(FILES ${CMAKE_BINARY_DIR}/crankle.pc DESTINATION lib/pkgconfig)

if(CRANKLE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
