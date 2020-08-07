echo -e "n\n\n\n\nw" | fdisk /dev/sdd
mkfs.ext4 /dev/sdd
mount /dev/sdd /home/jinu/SSD
