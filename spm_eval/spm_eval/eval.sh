#!/bin/bash

NUM=$1

#MODE=$2
#./input.sh $MODE $NUM | ./app

sh drop_cache.sh
./input.sh CREATE $NUM | ./app
sh drop_cache.sh
./input.sh CHANGE $NUM | ./app
sh drop_cache.sh
./input.sh DELETE $NUM | ./app
#sh drop_cache
