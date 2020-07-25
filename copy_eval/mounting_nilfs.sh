#echo -e "n\n\n\n\nw" | fdisk /dev/sdd
mkfs -t nilfs2 /dev/sdd
mount -t nilfs2 /dev/sdd /home/jinu/SSD2
