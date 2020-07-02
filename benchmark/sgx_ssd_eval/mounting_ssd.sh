echo -e "g\ng\nn\n\n\n\nw\n" | fdisk /dev/sde
mkfs.ext4 /dev/sde1
mount /dev/sde1 /home/jinu/SSD
./eval2 40 0
