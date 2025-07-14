set -e
files="./results/mc_oblivious_1752453921_48e6ff4906680dcf474a65c64187f7479a911a32 ./results/meta_oram_1752454724_48e6ff4906680dcf474a65c64187f7479a911a32 ./results/olabs_oram_1752076171_bbab52166942eecb604ca86c710b5ecbe592ba26 ./results/olabs_rostl_1752472919_48e6ff4906680dcf474a65c64187f7479a911a32 ./results/signal_icelake_1752350167_51218ad60ca43af639b8cd0fc57814624b898d63"



python ./scripts/draw_table.py -f $files -t "NRORAM - Latency" -xl "N" -yl "Latency (us) vs ORAM number of entries (N), using 8 byte keys, 8 byte values" -c 'Key_bytes=8' 'Value_bytes=8' 'benchmark_type=NRORAM' 'Read_latency_us' -x '{N}' -y '{Read_latency_us}' -l '{implementation}' -xt '"2^{{" + str(len(bin({x})[3:])) + "}}"'


python ./scripts/draw_table.py -f $files -t "RORAM - Latency" -xl "N" -yl "Latency (us) vs ORAM number of entries (N), using 8 byte keys, 8 byte values" -c 'Key_bytes=8' 'Value_bytes=8' 'benchmark_type=RORAM' 'Read_latency_us' -x '{N}' -y '{Read_latency_us}' -l '{implementation}' -xt '"2^{{" + str(len(bin({x})[3:])) + "}}"'

python ./scripts/draw_table.py -f $files -t "OMAP - Latency" -xl "N" -yl "Latency (us) vs Map number of entries (N), using 8 byte keys, 56 byte values" -c 'Key_bytes=8' 'Value_bytes=56' 'benchmark_type=UnorderedMap' 'Get_latency_us' '!Shards' -x '{N}' -y '{Get_latency_us}' -l '{implementation}' -xt '"2^{{" + str(len(bin({x})[3:])) + "}}"'


 

python ./scripts/draw_table.py -f $files -t "OMAP - Sharded Latency" -xl "N" -yl "Latency (us) vs Map number of entries (N) vs BatchSize, using 8 byte keys, 56 byte values, 15 Shards" -c 'Key_bytes=8' 'Value_bytes=56' 'benchmark_type=UnorderedMap' 'Get_latency_us' 'Shards=15' -x '{N}' -y '{Get_latency_us}' -l '{implementation}-{Batch_size}' -xt '"2^{{" + str(len(bin({x})[3:])) + "}}"' -fo -fn 'olabs_umap_sharded-4096' 'olabs_rostl_sharded-4096' 'Signal_Sharded-4096'

python ./scripts/draw_table.py -f $files -t "OMAP - Sharded Throughput" -xl "N" -yl "Througput (queries per second) vs Map number of entries (N), using 8 byte keys, 56 byte values, 15 Shards" -c 'Key_bytes=8' 'Value_bytes=56' 'benchmark_type=UnorderedMap' 'Get_throughput_qps' 'Shards=15' -x '{N}' -y '{Get_throughput_qps}' -l '{implementation}-{Batch_size}' -xt '"2^{{" + str(len(bin({x})[3:])) + "}}"' -fo -fn 'olabs_umap_sharded-4096' 'olabs_rostl_sharded-4096' 'Signal_Sharded-4096'