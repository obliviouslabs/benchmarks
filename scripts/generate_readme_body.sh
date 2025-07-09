set -e
files="./results-azure/results/mc_oblivious_1751554416_ebfd6fc3d8f64d817c2da74dcbac2d93606f9d00 ./results-azure/results/olabs_oram_1751232764_bbab52166942eecb604ca86c710b5ecbe592ba26 ./results-azure/results/meta_oram_1751554817_ebfd6fc3d8f64d817c2da74dcbac2d93606f9d00 ./results-azure/results/signal_icelake_1751309067_c45da8e4fe61c9de131e2aa92ee7049fb27aaddd"

python ./scripts/draw_table.py -f $files -t "NRORAM - Latency" -xl "N" -yl "Latency (us) vs ORAM number of entries (N), using 8 byte keys, 8 byte values" -c 'Key_bytes=8' 'Value_bytes=8' 'benchmark_type=NRORAM' 'Read_latency_us' -x '{N}' -y '{Read_latency_us}' -l '{implementation}' -xt '"2^{{" + str(len(bin({x})[3:])) + "}}"'


python ./scripts/draw_table.py -f $files -t "RORAM - Latency" -xl "N" -yl "Latency (us) vs ORAM number of entries (N), using 8 byte keys, 8 byte values" -c 'Key_bytes=8' 'Value_bytes=8' 'benchmark_type=RORAM' 'Read_latency_us' -x '{N}' -y '{Read_latency_us}' -l '{implementation}' -xt '"2^{{" + str(len(bin({x})[3:])) + "}}"'

python ./scripts/draw_table.py -f $files -t "OMAP - Latency" -xl "N" -yl "Latency (us) vs Map number of entries (N), using 8 byte keys, 56 byte values" -c 'Key_bytes=8' 'Value_bytes=56' 'benchmark_type=UnorderedMap' 'Get_latency_us' '!Sharded' -x '{N}' -y '{Get_latency_us}' -l '{implementation}' -xt '"2^{{" + str(len(bin({x})[3:])) + "}}"'

python ./scripts/draw_table.py -f $files -t "OMAP - Sharded Latency" -xl "N" -yl "Latency (us) vs Map number of entries (N) vs BatchSize, using 8 byte keys, 56 byte values, 15 Shards" -c 'Key_bytes=8' 'Value_bytes=56' 'benchmark_type=UnorderedMap' 'Get_latency_us' 'Shards=15' -x '{N}' -y '{Get_latency_us}' -l '{implementation}-{Batch_size}' -xt '"2^{{" + str(len(bin({x})[3:])) + "}}"'

python ./scripts/draw_table.py -f $files -t "OMAP - Sharded Throughput" -xl "N" -yl "Througput (queries per second) vs Map number of entries (N), using 8 byte keys, 56 byte values, 15 Shards" -c 'Key_bytes=8' 'Value_bytes=56' 'benchmark_type=UnorderedMap' 'Get_throughput_qps' 'Shards=15' -x '{N}' -y '{Get_throughput_qps}' -l '{implementation}-{Batch_size}' -xt '"2^{{" + str(len(bin({x})[3:])) + "}}"'