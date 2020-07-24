#!/bin/bash

rm ~/SSD/*.ppt
#make 50*20MB files
cd piggyback_eval
./eval_each.sh 1 1
sh drop_cache.sh
./mapping_app
