#!/bin/bash

cmds=("" "--random" "-w" "-w --random")
q_sizes=(1 2 4 8)

#echo "a"
for test in $(seq 3);
do
    
    for q_size in ${q_sizes[@]};
    do
        ./fair-lio -q $q_size /dev/sdb1 >> performance_key.txt
        sleep 31
        echo "seq read " $q_size "test ends"
    done

    for cmd in ${cmds[@]};
    do
#        for q_size in ${q_sizes[@]};
#        do
#            ./fair-lio -q $q_size /dev/sdb1 >> performance.txt
#            sleep 31
#            echo "seq read " $q_size "test ends"
#        done

        for q_size in ${q_sizes[@]};
        do
            ./fair-lio -q $q_size $cmd /dev/sdb1 >> performance_key.txt
            sleep 31
            echo $cmd $q_size "test ends"
        done
    done
done

bash ./performance1.sh
