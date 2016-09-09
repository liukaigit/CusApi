#!/bin/sh
for dir in `ls $1`
do
	if [ -d "$dir" ];then
		echo -e "\033[1;34m$dir\033[0m"
	elif [ -x "$dir" ];then
		echo -e "\033[1;32m$dir\033[0m"
	else
		echo "$dir"
	fi
done
