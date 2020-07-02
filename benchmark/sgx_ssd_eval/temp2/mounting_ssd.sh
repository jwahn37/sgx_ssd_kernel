echo -e "g\ng\nn\n\n\n\nw\n" | fdisk /dev/sdf
mkfs.ext4 /dev/sdf1
mount /dev/sdf1 /home/jinu/SSD
./eval_final 20 888888880
beep
sleep 0.5
beep
sleep 0.5
beep
sleep 0.5
beep
