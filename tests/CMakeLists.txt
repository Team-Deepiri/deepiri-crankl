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

set(CRANKLE_TEST_INCLUDE ${CMAKE_SOURCE_DIR}/tests/golden)

foreach(t test_trit test_clifford test_cran test_pack test_guct test_diff test_simd test_sheaf
        test_clifford_parity test_pack_roundtrip test_integration)
    target_link_libraries(${t} PRIVATE crankle)
    target_include_directories(${t} PRIVATE ${CRANKLE_TEST_INCLUDE})
    crankle_set_warnings(${t})
    add_test(NAME ${t} COMMAND ${t})
endforeach()

# Parity header must exist before build
add_custom_target(golden_headers ALL
    COMMAND ${CMAKE_COMMAND} -E echo "Ensure scripts/export_golden.py has been run"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
