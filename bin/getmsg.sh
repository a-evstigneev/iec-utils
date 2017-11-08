#!/bin/sh

chdir ./drop

for i in 1 2 3 4 5 6 7 8 9 10 11 12; do
	printf "$1 $i 1 $((($i+$2)%2))\n" >> $1
done

ls -ai1 $1 | while read inode fname; do
		mv $1 $inode
done

