#!/bin/bash

sh mounting_ssd.sh
sh mounting_ssd_sdd.sh
cp -r ~/Desktop/eval ~/SSD2/eval
mkdir ~/SSD/eval
./mapping_app
