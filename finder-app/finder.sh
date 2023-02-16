#!/bin/bash
filesdir=$1
searchstr=$2
if [ $filesdir ] && [ $searchstr ]
then
	if [ -d $filesdir ]
	then
		echo "The number of files are $(ls $filesdir | wc -l) and the number of matching lines are $(grep -r $2 $1* | wc -l)"
	else
		echo "the dir does not exists"
		exit 1
	fi
else
	echo "failed: Invalid Arguments"
	echo "usage: finder.sh 'files dir' 'string to search'"
	exit 1
fi
