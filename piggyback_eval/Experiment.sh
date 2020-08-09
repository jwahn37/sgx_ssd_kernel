IFS=:
EXP_FILE="experiment.txt"
DISK_PATH="/dev/sdc"
FAIR_LIO="./fair-lio"
MODE=":--random:-w:-w  --random"
QNUM="1:2:4:8:16"

for mode in $MODE
do
	for qnum in $QNUM
	do
		echo $qnum
		#$FAIR_LIO -q $qnum $mode $DISK_PATH >> $EXP_FILE
		#./fair-lio -q 1 /dev/sdb >> performance1.txt
	done
done
