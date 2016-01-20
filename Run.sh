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
cmd="make -j8 DEBUG=no DPDK=yes"
echo $cmd
lrun $cmd

if [ $? -ne 0 ]
then
    echo "make Error!"
    exit 1
fi

### For recovery test.
# use Dpdk among client and servers, tcp to/from coordinator
# with default timeout (250s)
# cmd="scripts/recovery.py -v --dry --transport=basic+dpdk"
cmd="scripts/recovery.py -v --timeout=500 --transport=basic+dpdk"
# cmd="scripts/recovery.py --transport=basic+dpdk"
# cmd="scripts/recovery.py -v --transport=fast+dpdk"
# cmd="scripts/recovery.py --transport=fast+dpdk"

# use Dpdk among client and servers, tcp to/from coordinator
# with default timeout (250s)
# cmd="mmfilter scripts/clusterperf.py -v --transport=basic+dpdk"
# cmd="scripts/clusterperf.py -v --transport=basic+dpdk basic"

# with default timeout (250s)
# cmd="scripts/clusterperf.py -v --transport=fast+dpdk basic"

# with shorter timeout 100s because cluster.py doesnot abort with Exception
# cmd="scripts/clusterperf.py -v --transport=fast+dpdk --timeout=50 basic"

# extend timout but it crashed after long wait.
# cmd="scripts/clusterperf.py -v --transport=fast+dpdk --timeout=1500 basic"

# Use TCP instead
# cmd="scripts/clusterperf.py -v --transport=tcp basic"

echo $cmd
lrun $cmd

mv 20*.log logs/latest
ls -l logs | grep latest
