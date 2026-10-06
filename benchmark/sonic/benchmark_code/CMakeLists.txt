add_executable(sonic_nroram_bench nroram_bench.cpp)
target_link_libraries(sonic_nroram_bench PRIVATE sonic::oram.native)

add_executable(sonic_pmchain_bench pmchain_bench.cpp)
target_link_libraries(sonic_pmchain_bench PRIVATE sonic::omap.native)

add_executable(sonic_pmchain_ramp pmchain_ramp.cpp)
target_link_libraries(sonic_pmchain_ramp PRIVATE sonic::omap.native)
