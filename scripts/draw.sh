set -e
files="./results/olabs_oram_1742692087_bbab52166942eecb604ca86c710b5ecbe592ba26 ./results/olabs_rods_1749473903_39d38b74e02ce55060d69b478ac95dd01e96528a ./results/meta_oram_1743207872_b0bffaeca5a3b7f2522df1e68ad87e15a9a2d3e9 ./results/signal_icelake_1742700453_ca00ab6b9eb5152277473a2755110b176975f4a4"

# NRORAM
python ./scripts/draw_figure.py -f $files  -x '{N}' -y '{Read_latency_us}' -yl '$\mu s$' -l '{implementation}' -c 'Key_bytes=8' 'Value_bytes=8' 'benchmark_type=NRORAM' -t 'NRORAM Read Latency (8b key, 8b value)' 
python ./scripts/draw_figure.py -f $files  -x '{N}' -y '{Initialization_zeroed_time_us}' -yl '$\mu s$' -l '{implementation}' -c 'Key_bytes=8' 'Value_bytes=8' 'benchmark_type=NRORAM' -t 'NRORAM Initialization Zeroed Time (8b key, 8b value)' 
python ./scripts/draw_figure.py -f $files  -x '{N}' -y '{Read_throughput_qps}' -yl 'queries/s' -l '{implementation}' -c 'Key_bytes=8' 'Value_bytes=8' 'benchmark_type=NRORAM' -t 'NRORAM Read Throughput (8b key, 8b value)' 
python ./scripts/draw_figure.py -f $files  -x '{N}' -y '{Memory_kb}' -yl 'Memory (kb)' -l '{implementation}' -c 'Key_bytes=8' 'Value_bytes=8' 'benchmark_type=NRORAM' -t 'NRORAM Memory (8b key, 8b value)' 

# RORAM
python ./scripts/draw_figure.py -f $files  -x '{N}' -y '{Read_latency_us}' -yl '$\mu s$' -l '{implementation}' -c 'Key_bytes=8' 'Value_bytes=8' 'benchmark_type=RORAM' -t 'RORAM Read Latency (8b key, 8b value)' 
python ./scripts/draw_figure.py -f $files  -x '{N}' -y '{Initialization_zeroed_time_us}' -yl '$\mu s$' -l '{implementation}' -c 'Key_bytes=8' 'Value_bytes=8' 'benchmark_type=RORAM' -t 'RORAM Initialization Zeroed Time (8b key, 8b value)' 
python ./scripts/draw_figure.py -f $files  -x '{N}' -y '{Read_throughput_qps}' -yl 'queries/s' -l '{implementation}' -c 'Key_bytes=8' 'Value_bytes=8' 'benchmark_type=RORAM' -t 'RORAM Read Throughput (8b key, 8b value)' 
python ./scripts/draw_figure.py -f $files  -x '{N}' -y '{Memory_kb}' -yl 'Memory (kb)' -l '{implementation}' -c 'Key_bytes=8' 'Value_bytes=8' 'benchmark_type=RORAM' -t 'RORAM Memory (8b key, 8b value)' 

# UMAP (Non sharded)
## Signal usecase
python ./scripts/draw_figure.py -f $files  -x '{N}' -y '{Get_latency_us}' -yl '$\mu s$' -l '{implementation}' -c 'Key_bytes=8' 'Value_bytes=56' 'benchmark_type=UnorderedMap' '!Shards' -t 'OMap Get Latency (8b key, 56b value)' 
python ./scripts/draw_figure.py -f $files  -x '{N}' -y '{Initialization_time_us}' -yl '$\mu s$' -l '{implementation}' -c 'Key_bytes=8' 'Value_bytes=56' 'benchmark_type=UnorderedMap' '!Shards' -t 'OMap Initialization Time (8b key, 56b value)' 
python ./scripts/draw_figure.py -f $files  -x '{N}' -y '{Get_throughput_qps}' -yl 'queries/s' -l '{implementation}' -c 'Key_bytes=8' 'Value_bytes=56' 'benchmark_type=UnorderedMap' '!Shards' -t 'OMap Throughput (8b key, 56b value)' 
python ./scripts/draw_figure.py -f $files  -x '{N}' -y '{Memory_kb}' -yl 'Memory (kb)' -l '{implementation}' -c 'Key_bytes=8' 'Value_bytes=56' 'benchmark_type=UnorderedMap' '!Shards' -t 'OMap Memory (8b key, 56b value)' 

## Generic memory usecase
python ./scripts/draw_figure.py -f $files  -x '{N}' -y '{Get_latency_us}' -yl '$\mu s$' -l '{implementation}' -c 'Key_bytes=8' 'Value_bytes=8' 'benchmark_type=UnorderedMap' '!Shards' -t 'OMap Get Latency (8b key, 8b value)' 
python ./scripts/draw_figure.py -f $files  -x '{N}' -y '{Initialization_time_us}' -yl '$\mu s$' -l '{implementation}' -c 'Key_bytes=8' 'Value_bytes=8' 'benchmark_type=UnorderedMap' '!Shards' -t 'OMap Initialization Time (8b key, 8b value)' 
python ./scripts/draw_figure.py -f $files  -x '{N}' -y '{Get_throughput_qps}' -yl 'queries/s' -l '{implementation}' -c 'Key_bytes=8' 'Value_bytes=8' 'benchmark_type=UnorderedMap' '!Shards' -t 'OMap Throughput (8b key, 8b value)' 
python ./scripts/draw_figure.py -f $files  -x '{N}' -y '{Memory_kb}' -yl 'Memory (kb)' -l '{implementation}' -c 'Key_bytes=8' 'Value_bytes=8' 'benchmark_type=UnorderedMap' '!Shards' -t 'OMap Memory (8b key, 8b value)' 

## EVM usecase
python ./scripts/draw_figure.py -f $files  -x '{N}' -y '{Get_latency_us}' -yl '$\mu s$' -l '{implementation}' -c 'Key_bytes=32' 'Value_bytes=32' 'benchmark_type=UnorderedMap' '!Shards' -t 'OMap Get Latency (32b key, 32b value)' 
python ./scripts/draw_figure.py -f $files  -x '{N}' -y '{Initialization_time_us}' -yl '$\mu s$' -l '{implementation}' -c 'Key_bytes=32' 'Value_bytes=32' 'benchmark_type=UnorderedMap' '!Shards' -t 'OMap Initialization Time (32b key, 32b value)' 
python ./scripts/draw_figure.py -f $files  -x '{N}' -y '{Get_throughput_qps}' -yl 'queries/s' -l '{implementation}' -c 'Key_bytes=32' 'Value_bytes=32' 'benchmark_type=UnorderedMap' '!Shards' -t 'OMap Throughput (32b key, 32b value)' 
python ./scripts/draw_figure.py -f $files  -x '{N}' -y '{Memory_kb}' -yl 'Memory (kb)' -l '{implementation}' -c 'Key_bytes=32' 'Value_bytes=32' 'benchmark_type=UnorderedMap' '!Shards' -t 'OMap Memory (32b key, 32b value)' 


# UMAP (Sharded 15 partitions)
# Signal usecase
python ./scripts/draw_figure.py -f $files  -x '{N}' -y '{Get_latency_us}' -yl '$\mu s$' -l '{implementation}-{Batch_size}' -c 'Key_bytes=8' 'Value_bytes=56' 'benchmark_type=UnorderedMap' 'Batch_size@100,1000,4096' -t 'OMap_Sharded Get Latency (8b key, 56b value)'   
python ./scripts/draw_figure.py -f $files  -x '{N}' -y '{Initialization_time_us}' -yl '$\mu s$' -l '{implementation}-{Batch_size}' -c 'Key_bytes=8' 'Value_bytes=56' 'benchmark_type=UnorderedMap' 'Batch_size@100,1000,4096' -t 'OMap_Sharded Initialization Time (8b key, 56b value)' 
python ./scripts/draw_figure.py -f $files  -x '{N}' -y '{Get_throughput_qps}' -yl 'queries/s' -l '{implementation}-{Batch_size}' -c 'Key_bytes=8' 'Value_bytes=56' 'benchmark_type=UnorderedMap' 'Batch_size@100,1000,4096' -t 'OMap_Sharded Throughput (8b key, 56b value)' 
python ./scripts/draw_figure.py -f $files  -x '{N}' -y '{Memory_kb}' -yl 'Memory (kb)' -l '{implementation}-{Batch_size}' -c 'Key_bytes=8' 'Value_bytes=56' 'benchmark_type=UnorderedMap' 'Batch_size@100,1000,4096' -t 'OMap_Sharded Memory (8b key, 56b value)' 

