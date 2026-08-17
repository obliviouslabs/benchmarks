add_executable(h2o2_oram_ramp benchmarks/h2o2_oram_ramp.cc)
target_link_libraries(h2o2_oram_ramp ORAMLib Threads::Threads)
