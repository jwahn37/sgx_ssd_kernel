#!/bin/bash


#########################################################################################
# ORNL OLCF-3 single host block level I/O benchmark scipt		                #
#                                                                                       #
#########################################################################################
# Originally by Sarp Oral <oralhs@ornl.gov>                                             #
# Modified by David Dillow <dillowda@ornl.gov> and Sarp Oral <oralhs@ornl.gov>          #
# Oak Ridge Leadership Computing Facility                                               #
# National Center for Computational Science                                             #
# Oak Ridge National Laboratory                                                         #
#                                                                                       #
#                                                                                       #
# Copyright (C) 2009-2011 UT-Battelle, LLC                                              #
# This source code was developed under contract DE-AC05-00OR22725                       #
# and there is a non-exclusive license for use of this work by or                       #
# on behalf of the US Government.                                                       #
#                                                                                       #
# UT-Battelle, LLC AND THE GOVERNMENT MAKE NO REPRESENTATIONS AND                       #
# DISCLAIM ALL WARRANTIES, BOTH EXPRESSED AND IMPLIED. THERE ARE NO                     #
# EXPRESS OR IMPLIED WARRANTIES OF MERCHANTABILITY OR FITNESS FOR A                     #
# PARTICULAR PURPOSE, OR THAT THE USE OF THE SOFTWARE WILL NOT                          #
# INFRINGE ANY PATENT, COPYRIGHT, TRADEMARK, OR OTHER PROPRIETARY                       #
# RIGHTS, OR THAT THE SOFTWARE WILL ACCOMPLISH THE INTENDED RESULTS                     #
# OR THAT THE SOFTWARE OR ITS USE WILL NOT RESULT IN INJURY OR DAMAGE.                  #
# The user assumes responsibility for all liabilities, penalties, fines,                #
# claims, causes of action, and costs and expenses, caused by, resulting                #
# from or arising out of, in whole or in part the use, storage or disposal              #
# of the SOFTWARE.                                                                      #
#########################################################################################

#########################################################################################
# NOTICE										#
#########################################################################################
# This script requires:									#
# 1) Passwordless PDSH root access between the head node and the test system.		#
# 2) A pre-built fair-lio binary ready to execute.					#
#											#
# Please refer to the README file distributed with this script for further details.	#
#########################################################################################


#########################################################################################
# OFFEROR, MODIFY BELOW VARIABLES TO FIT TO YOUR TEST SYSTEM                            #
#########################################################################################
#                                                                                       #
#########################################################################################
# BEGIN OF MODIFIABLE VARIABLES AND PARAMETERS                                          #
#########################################################################################
# Update and modify below to fit for your test system

# List host to be tested below
# A template is provided below as an example
#target_host="hostname-1"

#target_host="tick-oss1"
target_host="root@localhost"

# Device to be tested on the host
# A template is provided as an example
#DEV=(/dev/sda)
#DEV=(/dev/mpath/mptah0) 

#DEV=(/dev/sda)
DEV=(/dev/sdb1)

#########################################################################################
# END OF MODIFIABLE VARIABLES AND PARAMETERS                                            #
#########################################################################################


#########################################################################
#               W A R N I N G !         W A R N I N G !                 #
#########################################################################
# DO NOT MODIFY ANYTHING BELOW !                                        #
# THERE ARE NO USER MODIFIABLE VARIABLES BELOW  !                       #
# ALL MODIFIABLE VARIABLES ARE LOCATED AT THE BEGINNING OF THIS FILE !  #
#########################################################################


#########################################################################
# DO NOT MODIFY ANYTHING BELOW !                                        #
#########################################################################

bhome=`pwd`
bname="fair-lio"


RUNTIME=30
ITERS=5

qs_arr=(4 8 16)
bs_arr=(4096 8192 16384 32768 65536 131072 262144 524288 1048576 2097152 4194304 8388608)
io_mode_arr=(seq_wr seq_rd rand_wr rand_rd)

SSH=ssh
PDSH=pdsh


# delay between iters
delay=15

# Just an arbit seed for the random gen
base_seed=8023

#timestamp=$(date +"%b_%d_%y_%H_%M")
timestamp=$(date +"%d_%y_%H_%M")
#hate korean!
#fix


out_raw_dir="block_io_single_host_full_run_raw_${timestamp}"

out_summary_file="block_io_single_host_full_run_${timestamp}.summary"

out_csv_file="block_io_single_host_full_run_${timestamp}.csv"

tmp_file="temp_out_single_host_full_run_${timestamp}.tmp"

#fix
echo "${timestamp} and ${out_summary_file}"
echo -e "***********************************************************"
echo -e "*   ORNL OLCF Block-I/O Single Host Full Run Benchmark    *"
echo -e "***********************************************************"
echo -e "*                	W A R N I N G !                    *"
echo -e "*        	 This is a destructive test!               *"
echo -e "***********************************************************\n"
echo -n "Please press (Y/y) to continue:"

read answer
if test "$answer" != "Y" -a "$answer" != "y";
        then exit 0;
fi

echo -e "\n\n**************************************************"
echo -e "GATHERING SYSTEM SETTINGS!\n"

echo -e "----------------------\nMultipath settings are:\n----------------------">> ${out_summary_file}

pdsh -w ${target_host} "multipath -ll" | dshbak -c >> ${out_summary_file}

for devs in ${DEV[@]}; do
        target_dev=${devs##*/}
        echo -e "\n------------------------\nFor ${devs} device:\n------------------------\n" >> ${out_summary_file}
        for files in iostats  max_hw_sectors_kb  max_sectors_kb  nr_requests  read_ahead_kb  scheduler; do
                echo -e "${files} values:\n+++++++++++++++++++++++++++++\n"  >> ${out_summary_file}
                pdsh -w ${target_host} "cat /sys/block/${target_dev}/queue/${files}" | dshbak -c >> ${out_summary_file}
                echo -e "\n" >> ${out_summary_file}
        done
done

echo -e "\n\n**************************************************"
echo -e "GENERATING RANDOMIZED TEST LIST!\n"

mkdir -p ${out_raw_dir}

RANDOM=$base_seed

total_test_num=0
total_test_num=$(( ${#DEV[@]} * ${#io_mode_arr[@]} * ${#qs_arr[@]} * ${#bs_arr[@]} * ${ITERS} ))

for devs in ${DEV[@]}; do
	drvs="${drvs} $devs"
	num_drvs=`echo $drvs | grep -o " " | wc -l | sed s/\ //g`
	num_drvs=$(( ${num_drvs} + 1 ))
	for io in ${io_mode_arr[@]}; do
		for qs in ${qs_arr[@]}; do
			for bs in ${bs_arr[@]}; do
				for ((i = 0; i < ITERS; i++)); do
					rand=$RANDOM
					let "f_seed = $i + ${rand}"
					echo $rand $io $bs $qs $i $f_seed $num_drvs $drvs 
				done
			done 
		done		
	done
done | sort -n  > ${tmp_file}


# Do the actual runs
index=1

echo -e "\nSTARTING THE TEST EXECUTION! TOTAL NUMBER OF TESTS TO RUN: ${total_test_num}\n"

cat ${tmp_file} \
| while read rand_num io_mode block_size queue_size iter fair_seed num_devices devices; do
	#echo $rand_num $io_mode $block_size $queue_size $iter $fair_seed $num_devices $devices
	if [ "${io_mode}" == "seq_wr" ]; then
		io_flags="-w"
	elif [ "${io_mode}" == "rand_wr" ]; then
		io_flags="-w --random=${fair_seed}"
	elif [ "${io_mode}" == "seq_rd" ]; then
		io_flags=""
	elif [ "${io_mode}" == "rand_rd" ]; then
		io_flags="--random=${fair_seed}"
	fi
	run_tests="${bhome}/${bname} -q ${queue_size} -s ${block_size} -t $RUNTIME ${io_flags} ${devices}"
	echo -e "\n******************************************************************************************************************************************************************************************************" 
	echo -e "------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------"
	echo -e "Current test number: ${index}\t\tNumber of remaining tests: $(( total_test_num - index ))"
	echo -e "------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------"
	echo -e "Running iteration ${iter} for ${io_mode} I/O with \"${io_flags}\" flags for ${block_size} bytes and ${queue_size} queue depth for ${num_devices} devices ($devices) on host ${target_host}" | tee -a ${out_summary_file}
	echo -e "------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------"
	pdsh -w ${target_host} ${run_tests}  | dshbak -c | tee ${out_raw_dir}/"${io_mode}-${block_size}-${queue_size}-${num_devices}-${iter}.raw"\
						 | awk '$1 == "total:" {tot=tot+$3} END {print "Test Result: Aggregate observed bandwidth",tot,"MB/s"}'\
						 | tee -a ${out_summary_file}
	echo -e "------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------"
	echo -e "******************************************************************************************************************************************************************************************************\n" 
	sleep $delay
	((index++))
done

echo -e "\nEND OF TEST EXECUTION!\n"
echo -e "\nGENERATING AGGREGATE SUMMARY STATISTICS. PLEASE WAIT...\n"
#until know success here
# Generate summary statistics

# Header for the CSV file
echo -e "#io-mode,block-size,queue-size,num-devices,max,min,median,average,stddev" > ${out_csv_file}

# Calculate the summary stats
for io_mode in ${io_mode_arr[@]}; do
	for block_size in ${bs_arr[@]}; do
		for queue_size in ${qs_arr[@]}; do
			for ((num_devices=1; num_devices <= ${#DEV[@]}; num_devices++)); do
				for ((i = 0; i < ITERS; i++)); do
					iter_tot[i]=`cat ${out_raw_dir}/${io_mode}-${block_size}-${queue_size}-${num_devices}-${i}.raw \
						| awk '$1 == "total:" {tot=tot+$3} END {print tot}'`
				done
				echo ${iter_tot[@]} | awk -v io_mode="${io_mode}" -v block_size="${block_size}" -v queue_size="${queue_size}" -v num_drives="${num_devices}"\
						'BEGIN{RS="";OFS=",";c=0;max=0;min=0;total=0;delta_sum=0;delta_avg=0}
						{
							n=split($0,arr)
							asort(arr)
							for(i=1;i<=n;i++)
							total+=arr[i]}
						END{if (n % 2) {
							median=arr[(n + 1) / 2];
						} else {
							median=(arr[(n / 2)] + arr[(n / 2) + 1]) / 2.0;
						}
						average=total/n
						for(i=1;i<=n;i++)
							{delta[i]=((arr[i]-average)^2);delta_sum+=delta[i];}
						delta_avg=delta_sum/n
						stddev=sqrt(delta_avg)
						print io_mode, block_size, queue_size, num_drives, arr[n], arr[1], median, average, stddev}'\
						>> ${out_csv_file}
			done
		done
	done
done

echo -e "\nALL DONE!"
echo -e "AGGREGATE STATICS FOR THE TESTS IS WRITTEN TO THE ${out_csv_file} IN CSV FORMAT!"
echo -e "PLEASE TAR THE CONTENTS OF TEST PARENT DIRECTORY AND REPORT TO THE ORNL SUBCONTRACT ADMINISTRATOR!\n"
