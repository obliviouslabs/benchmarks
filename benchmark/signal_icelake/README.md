Signal-icelake has the following implementations that we want to benchmark:

+ c/path_oram -> non-recursive path oram

+ c/position_map -> recursive position map

+ c/ohtable -> non sharded hashtable

+ c/sharded_ohtable -> sharded hashtable


The goal of this project is to be able to easily benchmark both implementation on various settings, at any given commit fromt ContactDiscoveryService-Icelake.

We want to benchmark these things:

+ Initialization time - how fast can a server be initialized

+ Query time - What is the latency of a point query

+ Batched query time - What is the latency of a batched query (only relevant for sharded)

+ Insertion time - What is the latency of a point insertion

+ Batched insertion time - What is the latency of a batched insertion

