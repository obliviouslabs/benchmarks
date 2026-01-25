cmake_minimum_required(VERSION 3.18)


# Add the executable
add_executable(umap benchmark/umap.cpp)
add_executable(umap_sharded benchmark/umap_sharded.cpp)
add_executable(oram_bench benchmark/oram_bench.cpp)
add_executable(load_balancing benchmark/load_balancing.cpp)

# Link against the C++ library
target_link_libraries(umap PRIVATE oraminterface common)
target_link_libraries(umap_sharded PRIVATE oraminterface common)
target_link_libraries(oram_bench PRIVATE oraminterface common)
target_link_libraries(load_balancing PRIVATE oraminterface common)


# For signal benchmarks:
find_package(Protobuf REQUIRED)
INCLUDE(FindProtobuf)
FIND_PACKAGE(Protobuf REQUIRED)
INCLUDE_DIRECTORIES(${PROTOBUF_INCLUDE_DIR})
PROTOBUF_GENERATE_CPP(PROTO_SRC PROTO_HEADER benchmark/cds.proto)
ADD_LIBRARY(proto ${PROTO_HEADER} ${PROTO_SRC})

INCLUDE_DIRECTORIES(${CMAKE_CURRENT_BINARY_DIR})
INCLUDE_DIRECTORIES(${Protobuf_INCLUDE_DIRS})

add_executable(rw_signal benchmark/rw_signal.cpp)
target_link_libraries(rw_signal PRIVATE oraminterface common proto ${Protobuf_LIBRARIES})
