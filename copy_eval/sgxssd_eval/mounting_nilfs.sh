#echo -e "n\n\n\n\nw" | fdisk /dev/sdd
mkfs -t nilfs2 /dev/sdc
mount -t nilfs2 /dev/sdc /home/jinu/SSD
