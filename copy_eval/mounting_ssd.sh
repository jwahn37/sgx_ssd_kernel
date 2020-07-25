echo -e "n\n\n\n\nw" | fdisk /dev/sdc
mkfs.ext4 /dev/sdc
mount /dev/sdc /home/jinu/SSD
