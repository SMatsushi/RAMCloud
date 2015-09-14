#!/bin/sh

if [ $# -ne 2 ]; then
  echo "Usage: $0 <ServerNumberFrom> <ServerNumberTo>"
  exit 1 
fi

for i in `seq $1 $2`
do 
    `printf "ssh atom%03d  hostname" $i`
done
