#!/bin/bash

MODE=$1
NUM=$2
	
echo "$NUM"

i=1
#for i in {1..$NUM}	
while [ $i -le $NUM ]
do


	if [ $MODE = "CREATE" ]
	then
		echo "cr 9 9\n"
	fi	

	if [ $MODE = "CHANGE" ]
	then
		echo "change 1 1\n"
		echo "$i"
	fi	

	if [ $MODE = "DELETE" ]
	then
		echo "delete\n"
		echo "0"
	fi	

	i=$(($i+1))

done
