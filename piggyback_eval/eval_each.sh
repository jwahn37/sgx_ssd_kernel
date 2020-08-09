
thread=$1
baseline=$2

#rm ~/SSD/*.txt
#echo 3 > /proc/sys/vm/drop_caches
./eval_performance_final $thread $baseline
