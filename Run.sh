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

# use Dpdk among client and servers, tcp to/from coordinator
# with shorter default timeout (250s)
cmd="scripts/clusterperf.py -v --transport=fast+dpdk --timeout=100 basic"
# extend timout but it crashed after long wait.
# cmd="scripts/clusterperf.py -v --transport=fast+dpdk --timeout=2000 basic"

# Use TCP instead
# cmd="scripts/clusterperf.py -v --transport=tcp basic"

echo $cmd
lrun $cmd

mv 20*.log logs/latest
ls -l logs | grep latest