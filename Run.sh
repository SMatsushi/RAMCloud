#!/bin/bash
#
# Compile RAMCloud and Run Clusterperf.py basic for debugging

if [ -d logs/20[0-9]* ]
then
    basen=`basename logs/20[0-9]*`
    newbase=c${basen}
    cmd="mv logs/$basen logs/Old/$newbase"
    echo $cmd
    $cmd
fi
cmd="make -j8 DEBUG=no"
echo $cmd
$cmd

if [ $? -ne 0 ]
then
    echo "make Error!"
    exit 1
fi

cmd="scripts/clusterperf.py -v --transport=fast+dpdk basic"
echo $cmd
lrun $cmd

mv 20*.log logs/latest
ls -l logs | grep latest