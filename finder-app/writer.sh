#!/bin/bash
writefile=$1
writestr=$2
if [ $writefile ] && [ $writestr ]
then
    mkdir -p -- "$(dirname -- "$writefile")" &&
        touch -- "$writefile"
    if [ -e $writefile ]
    then
        echo $writestr > $writefile
    else
        echo "the file cannot be created"
        exit 1
    fi
else
    echo "failed: Invalid Arguments"
	echo "usage: writer.sh 'file being written (w/ full dir)' 'string to write'"
	exit 1
fi
