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

set(CRANKLE_TEST_INCLUDE ${CMAKE_SOURCE_DIR}/tests/golden)

foreach(t test_trit test_clifford test_cran test_pack test_guct test_diff test_simd test_sheaf
        test_clifford_parity test_pack_roundtrip test_integration test_c_api test_safetensors
        test_turn_target test_metrics test_flow_pipeline)
    target_link_libraries(${t} PRIVATE crankle)
    target_include_directories(${t} PRIVATE ${CRANKLE_TEST_INCLUDE})
    crankle_set_warnings(${t})
    add_test(NAME ${t} COMMAND ${t})
endforeach()

target_compile_definitions(test_safetensors PRIVATE GOLDEN_DIR="${CMAKE_SOURCE_DIR}/tests/golden")
target_include_directories(test_safetensors PRIVATE ${CMAKE_SOURCE_DIR}/src)

# Parity header must exist before build
add_custom_target(golden_headers ALL
    COMMAND ${CMAKE_COMMAND} -E echo "Ensure scripts/export_golden.py has been run"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
