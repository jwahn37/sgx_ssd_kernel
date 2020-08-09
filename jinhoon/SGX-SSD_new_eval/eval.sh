IFS=:
THREAD_NUM="1:2:4:8:16:32:64"
BASELINE=$1


for thread in $THREAD_NUM
do 
    rm ~/SSD/*.txt
    echo 3 > /proc/sys/vm/drop_caches
    ./eval_performance_final $thread $BASELINE
done
