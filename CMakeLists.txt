cmake_minimum_required(VERSION 3.20)
project(deepiri-crankl VERSION 0.4.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(CRANKL_BUILD_TESTS "Build tests" ON)
option(CRANKL_ENABLE_SIMD "Enable AVX2 dispatch" ON)

include(cmake/warnings.cmake)

set(CRANKL_C_API_SOURCES
    src/c_api/errors.cpp
    src/c_api/trit.cpp
    src/c_api/crank.cpp
    src/c_api/clifford.cpp
    src/c_api/sheaf.cpp
    src/c_api/symplectic.cpp
    src/c_api/pack.cpp
    src/c_api/cran.cpp
    src/c_api/holonomy.cpp
    src/c_api/diff.cpp
    src/c_api/simd.cpp
    src/c_api/metrics.cpp
)

set(CRANKL_CORE_SOURCES
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
    src/core/metrics.cpp
    src/pack/fold.cpp
    src/pack/unfold.cpp
    src/pack/persistence.cpp
    src/pack/tiling.cpp
    src/io/cran_reader.cpp
    src/io/cran_writer.cpp
    src/io/cran_metadata.cpp
    src/io/safetensors.cpp
    src/holonomy/wilson.cpp
    ${CRANKL_C_API_SOURCES}
)

add_library(crankl SHARED ${CRANKL_CORE_SOURCES})
target_include_directories(crankl
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/third_party)
set_target_properties(crankl PROPERTIES OUTPUT_NAME crankl VERSION 0.4.0 SOVERSION 0)
crankl_set_warnings(crankl)

if(CRANKL_ENABLE_SIMD)
    target_compile_definitions(crankl PRIVATE CRANKL_HAVE_SIMD=1)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(crankl PRIVATE -mavx2 -mfma)
    endif()
endif()

add_executable(crankl-cli src/cli/main.cpp)
target_link_libraries(crankl-cli PRIVATE crankl)
target_include_directories(crankl-cli PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
set_target_properties(crankl-cli PROPERTIES OUTPUT_NAME crankl)
crankl_set_warnings(crankl-cli)

install(TARGETS crankl crankl-cli
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib)
install(FILES
    include/crankl/crankl.h
    include/crankl/version.h
    include/crankl/types.h
    include/crankl/errors.h
    include/crankl/version_api.h
    include/crankl/trit.h
    include/crankl/crank.h
    include/crankl/clifford.h
    include/crankl/sheaf.h
    include/crankl/symplectic.h
    include/crankl/pack.h
    include/crankl/cran.h
    include/crankl/holonomy.h
    include/crankl/diff.h
    include/crankl/simd.h
    include/crankl/metrics.h
    DESTINATION include/crankl)

configure_file(crankl.pc.in crankl.pc @ONLY)
install(FILES ${CMAKE_BINARY_DIR}/crankl.pc DESTINATION lib/pkgconfig)

if(CRANKL_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
