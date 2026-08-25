add_executable(test_trit ctest/test_trit.cpp)
add_executable(test_clifford ctest/test_clifford.cpp)
add_executable(test_cran ctest/test_cran.cpp)
add_executable(test_pack ctest/test_pack.cpp)
add_executable(test_guct ctest/test_guct.cpp)
add_executable(test_diff ctest/test_diff.cpp)
add_executable(test_simd ctest/test_simd.cpp)
add_executable(test_sheaf ctest/test_sheaf.cpp)
add_executable(test_clifford_parity ctest/test_clifford_parity.cpp)
add_executable(test_pack_roundtrip ctest/test_pack_roundtrip.cpp)
add_executable(test_integration ctest/test_integration.cpp)
add_executable(test_c_api ctest/test_c_api.cpp)
add_executable(test_safetensors ctest/test_safetensors.cpp)
add_executable(test_turn_target ctest/test_turn_target.cpp)
add_executable(test_metrics ctest/test_metrics.cpp)
add_executable(test_flow_pipeline ctest/test_flow_pipeline.cpp)
add_executable(test_pack_decrank_roundtrip ctest/test_pack_decrank_roundtrip.cpp)
add_executable(test_finetune ctest/test_finetune.cpp)
add_executable(test_holonomy_blocked ctest/test_holonomy_blocked.cpp)
add_executable(test_peel_stack ctest/test_peel_stack.cpp)
add_executable(test_cran_multi_open ctest/test_cran_multi_open.cpp)
add_executable(test_pack_anneal ctest/test_pack_anneal.cpp)
add_executable(test_ingest_multi ctest/test_ingest_multi.cpp)
add_executable(test_gguf ctest/test_gguf.cpp)
add_executable(test_fuzz_parsers ctest/test_fuzz_parsers.cpp)
add_executable(test_threading ctest/test_threading.cpp)
add_executable(test_cohomology_crossval ctest/test_cohomology_crossval.cpp)
target_include_directories(test_cohomology_crossval PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_sources(test_cohomology_crossval PRIVATE
    ${CMAKE_SOURCE_DIR}/src/topology/sheaf.cpp
    ${CMAKE_SOURCE_DIR}/src/algebra/clifford.cpp
    ${CMAKE_SOURCE_DIR}/src/algebra/crank_word.cpp
    ${CMAKE_SOURCE_DIR}/src/algebra/trit.cpp)
find_package(Threads REQUIRED)
target_link_libraries(test_threading PRIVATE Threads::Threads)

set(CRANKL_TEST_INCLUDE ${CMAKE_SOURCE_DIR}/tests/golden)

# test_simd and test_safetensors exercise implementation internals
# (crankl::simd, crankl::mat8_*, crankl::io). Those symbols are local to the
# shared library (version script), so these tests compile the needed TUs
# directly instead of linking them out of the DSO.
target_sources(test_simd PRIVATE
    ${CMAKE_SOURCE_DIR}/src/simd/simd.cpp
    ${CMAKE_SOURCE_DIR}/src/algebra/linalg.cpp)
target_include_directories(test_simd PRIVATE
    ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/third_party)
if(CRANKL_ENABLE_SIMD)
    target_compile_definitions(test_simd PRIVATE CRANKL_HAVE_SIMD=1)
endif()

target_sources(test_safetensors PRIVATE
    ${CMAKE_SOURCE_DIR}/src/ingest/safetensors.cpp)
target_include_directories(test_safetensors PRIVATE
    ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/third_party)

foreach(t test_trit test_clifford test_cran test_pack test_guct test_diff test_simd test_sheaf
        test_clifford_parity test_pack_roundtrip test_integration test_c_api test_safetensors
        test_turn_target test_metrics test_flow_pipeline test_pack_decrank_roundtrip test_finetune
        test_holonomy_blocked test_peel_stack test_cran_multi_open test_pack_anneal
        test_ingest_multi test_gguf test_fuzz_parsers test_threading test_cohomology_crossval)
    target_link_libraries(${t} PRIVATE crankl)
    target_include_directories(${t} PRIVATE ${CRANKL_TEST_INCLUDE})
    crankl_set_warnings(${t})
    add_test(NAME ${t} COMMAND ${t})
endforeach()

target_compile_definitions(test_safetensors PRIVATE GOLDEN_DIR="${CMAKE_SOURCE_DIR}/tests/golden")
target_include_directories(test_safetensors PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_definitions(test_pack_anneal PRIVATE GOLDEN_DIR="${CMAKE_SOURCE_DIR}/tests/golden")
target_include_directories(test_ingest_multi PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_include_directories(test_gguf PRIVATE ${CMAKE_SOURCE_DIR}/src)

# test_simd calls crankl::simd::mat8_mul_avx2 directly, which is only declared internally
target_include_directories(test_simd PRIVATE ${CMAKE_SOURCE_DIR}/src)

# Parity header must exist before build
add_custom_target(golden_headers ALL
    COMMAND ${CMAKE_COMMAND} -E echo "Ensure scripts/export_golden.py has been run"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

if(CRANKL_BUILD_FUZZERS)
    foreach(tgt IN ITEMS cran safetensors gguf)
        add_executable(fuzz_${tgt} fuzz/fuzz_${tgt}.cpp)
        target_compile_options(fuzz_${tgt} PRIVATE -fsanitize=fuzzer -g -O1)
        target_link_libraries(fuzz_${tgt} PRIVATE crankl -fsanitize=fuzzer)
    endforeach()
endif()
