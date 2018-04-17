#!/bin/bash

if [ $# -ne 2 ]
then
  echo 'usage: ./run_virtmem.sh <rand|fifo|custom> <sort|scan|focus>'
  exit
fi

echo ./virtmem 100 3 $1 $2
./virtmem 100 3 $1 $2
echo ''

for i in {10..100..10}
do
  echo ./virtmem 100 $i $1 $2
  ./virtmem 100 $i $1 $2
  echo ''
done
