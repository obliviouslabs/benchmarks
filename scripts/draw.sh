set -e
files="./results/olabs_oram_1743959356_bbab52166942eecb604ca86c710b5ecbe592ba26 ./results/olabs_rods_1749411400_cd1304dc134ce4357710965d0c3e1cacd50ccd5a ./results/meta_oram_1743618869_ea6c18c37475f012c61dfd912ef690d4729ae831 ./results/signal_icelake_1744234307_ca00ab6b9eb5152277473a2755110b176975f4a4"

python ./scripts/draw_figure.py -f $files  -x N -y Read_latency_us -yl '$\mu s$' -l '{implementation}' -c 'Key_bytes=8' 'Value_bytes=8' 'benchmark_type=RORAM' -t 'RORAM Read Latency (8b key, 8b value)' 
python ./scripts/draw_figure.py -f $files  -x N -y Initialization_zeroed_time_us -yl '$\mu s$' -l '{implementation}' -c 'Key_bytes=8' 'Value_bytes=8' 'benchmark_type=RORAM' -t 'RORAM Initialization Zeroed Time (8b key, 8b value)' 
python ./scripts/draw_figure.py -f $files  -x N -y Read_throughput_qps -yl 'queries/s' -l '{implementation}' -c 'Key_bytes=8' 'Value_bytes=8' 'benchmark_type=RORAM' -t 'RORAM Read Throughput (8b key, 8b value)' 
python ./scripts/draw_figure.py -f $files  -x N -y Memory_kb -yl 'Memory (kb)' -l '{implementation}' -c 'Key_bytes=8' 'Value_bytes=8' 'benchmark_type=RORAM' -t 'RORAM Memory (8b key, 8b value)' 

python ./scripts/draw_figure.py -f $files  -x N -y Read_latency_us -yl '$\mu s$' -l '{implementation}' -c 'Key_bytes=8' 'Value_bytes=8' 'benchmark_type=NRORAM' -t 'NRORAM Read Latency (8b key, 8b value)' 
python ./scripts/draw_figure.py -f $files  -x N -y Initialization_zeroed_time_us -yl '$\mu s$' -l '{implementation}' -c 'Key_bytes=8' 'Value_bytes=8' 'benchmark_type=NRORAM' -t 'NRORAM Initialization Zeroed Time (8b key, 8b value)' 
python ./scripts/draw_figure.py -f $files  -x N -y Read_throughput_qps -yl 'queries/s' -l '{implementation}' -c 'Key_bytes=8' 'Value_bytes=8' 'benchmark_type=NRORAM' -t 'NRORAM Read Throughput (8b key, 8b value)' 
python ./scripts/draw_figure.py -f $files  -x N -y Memory_kb -yl 'Memory (kb)' -l '{implementation}' -c 'Key_bytes=8' 'Value_bytes=8' 'benchmark_type=NRORAM' -t 'NRORAM Memory (8b key, 8b value)' 

python ./scripts/draw_figure.py -f $files  -x N -y Get_latency_us -yl '$\mu s$' -l '{implementation}' -c 'Key_bytes=8' 'Value_bytes=56' 'benchmark_type=UnorderedMap' '!Shards' -t 'OMap Get Latency (8b key, 56b value)' 
python ./scripts/draw_figure.py -f $files  -x N -y Initialization_time_us -yl '$\mu s$' -l '{implementation}' -c 'Key_bytes=8' 'Value_bytes=56' 'benchmark_type=UnorderedMap' '!Shards' -t 'OMap Initialization Time (8b key, 56b value)' 
python ./scripts/draw_figure.py -f $files  -x N -y Get_throughput_qps -yl 'queries/s' -l '{implementation}' -c 'Key_bytes=8' 'Value_bytes=56' 'benchmark_type=UnorderedMap' '!Shards' -t 'OMap Throughput (8b key, 56b value)' 
python ./scripts/draw_figure.py -f $files  -x N -y Memory_kb -yl 'Memory (kb)' -l '{implementation}' -c 'Key_bytes=8' 'Value_bytes=56' 'benchmark_type=UnorderedMap' '!Shards' -t 'OMap Memory (8b key, 56b value)' 

python ./scripts/draw_figure.py -f $files  -x N -y Get_latency_us -yl '$\mu s$' -l '{implementation}-{Batch_size}' -c 'Key_bytes=8' 'Value_bytes=56' 'benchmark_type=UnorderedMap' 'Batch_size@100,1000,4096' -t 'OMap_Sharded Get Latency (8b key, 56b value)'   
python ./scripts/draw_figure.py -f $files  -x N -y Initialization_time_us -yl '$\mu s$' -l '{implementation}-{Batch_size}' -c 'Key_bytes=8' 'Value_bytes=56' 'benchmark_type=UnorderedMap' 'Batch_size@100,1000,4096' -t 'OMap_Sharded Initialization Time (8b key, 56b value)' 
python ./scripts/draw_figure.py -f $files  -x N -y Get_throughput_qps -yl 'queries/s' -l '{implementation}-{Batch_size}' -c 'Key_bytes=8' 'Value_bytes=56' 'benchmark_type=UnorderedMap' 'Batch_size@100,1000,4096' -t 'OMap_Sharded Throughput (8b key, 56b value)' 
python ./scripts/draw_figure.py -f $files  -x N -y Memory_kb -yl 'Memory (kb)' -l '{implementation}-{Batch_size}' -c 'Key_bytes=8' 'Value_bytes=56' 'benchmark_type=UnorderedMap' 'Batch_size@100,1000,4096' -t 'OMap_Sharded Memory (8b key, 56b value)' 
