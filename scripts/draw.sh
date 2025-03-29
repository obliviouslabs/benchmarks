set -e
files="./results/olabs_oram_1742692087_bbab52166942eecb604ca86c710b5ecbe592ba26 ./results/olabs_rods_1742690948_4213ad1dce057d9249ad09f9220a90f7271e5b2b ./results/signal_icelake_1742700453_ca00ab6b9eb5152277473a2755110b176975f4a4 ./results/meta_oram_1743207872_b0bffaeca5a3b7f2522df1e68ad87e15a9a2d3e9"

python ./scripts/draw_figure.py -f $files  -x N -y Read_latency_us -yl '$\mu s$' -l '{implementation}' -c 'Key_bytes=8' 'Value_bytes=8' 'benchmark_type=RORAM' -t 'RORAM Read Latency' 
python ./scripts/draw_figure.py -f $files  -x N -y Initialization_zeroed_time_us -yl '$\mu s$' -l '{implementation}' -c 'Key_bytes=8' 'Value_bytes=8' 'benchmark_type=RORAM' -t 'RORAM Initialization Zeroed Time' 
python ./scripts/draw_figure.py -f $files  -x N -y Read_throughput_qps -yl 'queries/s' -l '{implementation}' -c 'Key_bytes=8' 'Value_bytes=8' 'benchmark_type=RORAM' -t 'RORAM Read Throughput' 
python ./scripts/draw_figure.py -f $files  -x N -y Memory_kb -yl 'Memory (kb)' -l '{implementation}' -c 'Key_bytes=8' 'Value_bytes=8' 'benchmark_type=RORAM' -t 'RORAM Memory' 

python ./scripts/draw_figure.py -f $files  -x N -y Read_latency_us -yl '$\mu s$' -l '{implementation}' -c 'Key_bytes=8' 'Value_bytes=8' 'benchmark_type=NRORAM' -t 'NRORAM Read Latency' 
python ./scripts/draw_figure.py -f $files  -x N -y Initialization_zeroed_time_us -yl '$\mu s$' -l '{implementation}' -c 'Key_bytes=8' 'Value_bytes=8' 'benchmark_type=NRORAM' -t 'NRORAM Initialization Zeroed Time' 
python ./scripts/draw_figure.py -f $files  -x N -y Read_throughput_qps -yl 'queries/s' -l '{implementation}' -c 'Key_bytes=8' 'Value_bytes=8' 'benchmark_type=NRORAM' -t 'NRORAM Read Throughput' 
python ./scripts/draw_figure.py -f $files  -x N -y Memory_kb -yl 'Memory (kb)' -l '{implementation}' -c 'Key_bytes=8' 'Value_bytes=8' 'benchmark_type=NRORAM' -t 'NRORAM Memory' 

python ./scripts/draw_figure.py -f $files  -x N -y Get_latency_us -yl '$\mu s$' -l '{implementation}' -c 'Key_bytes=8' 'Value_bytes=56' 'benchmark_type=UnorderedMap' '!Shards' -t 'OMap Get Latency' 
python ./scripts/draw_figure.py -f $files  -x N -y Initialization_time_us -yl '$\mu s$' -l '{implementation}' -c 'Key_bytes=8' 'Value_bytes=56' 'benchmark_type=UnorderedMap' '!Shards' -t 'OMap Initialization Time' 
python ./scripts/draw_figure.py -f $files  -x N -y Get_throughput_qps -yl 'queries/s' -l '{implementation}' -c 'Key_bytes=8' 'Value_bytes=56' 'benchmark_type=UnorderedMap' '!Shards' -t 'OMap Throughput' 
python ./scripts/draw_figure.py -f $files  -x N -y Memory_kb -yl 'Memory (kb)' -l '{implementation}' -c 'Key_bytes=8' 'Value_bytes=56' 'benchmark_type=UnorderedMap' '!Shards' -t 'OMap Memory' 

python ./scripts/draw_figure.py -f $files  -x N -y Get_latency_us -yl '$\mu s$' -l '{implementation}-{Batch_size}' -c 'Key_bytes=8' 'Value_bytes=56' 'benchmark_type=UnorderedMap' 'Batch_size@100,1000,4096' -t 'OMap_Sharded Get Latency'   
python ./scripts/draw_figure.py -f $files  -x N -y Initialization_time_us -yl '$\mu s$' -l '{implementation}-{Batch_size}' -c 'Key_bytes=8' 'Value_bytes=56' 'benchmark_type=UnorderedMap' 'Batch_size@100,1000,4096' -t 'OMap_Sharded Initialization Time' 
python ./scripts/draw_figure.py -f $files  -x N -y Get_throughput_qps -yl 'queries/s' -l '{implementation}-{Batch_size}' -c 'Key_bytes=8' 'Value_bytes=56' 'benchmark_type=UnorderedMap' 'Batch_size@100,1000,4096' -t 'OMap_Sharded Throughput' 
python ./scripts/draw_figure.py -f $files  -x N -y Memory_kb -yl 'Memory (kb)' -l '{implementation}-{Batch_size}' -c 'Key_bytes=8' 'Value_bytes=56' 'benchmark_type=UnorderedMap' 'Batch_size@100,1000,4096' -t 'OMap_Sharded Memory' 
