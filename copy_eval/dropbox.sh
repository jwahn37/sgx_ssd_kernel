#/bin/bash

cp -r ~/SSD/eval ~/Dropbox
# dropbox status "up to date"

#dropbox status >> a.txt

while true;
do
	output=$(dropbox status)
	echo $output
	if [ "$output" = "Up to date" ]; then
		break
	fi
done
