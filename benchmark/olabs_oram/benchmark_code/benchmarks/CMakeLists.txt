cmake_minimum_required(VERSION 3.18)

# Add the executable
add_executable(umap benchmark/umap.cpp)
add_executable(umap_sharded benchmark/umap_sharded.cpp)

# Link against the C++ library
target_link_libraries(umap PRIVATE oraminterface common)
target_link_libraries(umap_sharded PRIVATE oraminterface common)
