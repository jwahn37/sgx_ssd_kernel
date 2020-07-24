#!/bin/bash


#create policy and reset the polcy to the files
cd spm_eval
./input.sh CREATE 1 | ./app
cd ../piggyback_eval
./eval_each.sh 1 0 

