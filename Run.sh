#!/bin/bash
#
# Compile RAMCloud and Run Clusterperf.py basic for debugging

CMDNAME=`basename $0`
while getopts chp: OPT
do
    case $OPT in
	"c" ) flag_c="TRUE";;
	"p" ) flag_p="TRUE"; prefix=$OPTARG ;;
	"h" ) echo "Usage: $CMDNAME [-c][-h][-p log_prefix]" 1>&2
            exit  ;;
    esac
done

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
if [ "$flag_c" = "TRUE" ]; then
    $cmd
    exit 0
else
    lrun $cmd
fi

if [ $? -ne 0 ]
then
    echo "make Error!"
    exit 1
fi

cperfTests=(         "basic" \
                     "multiRead_oneMaster" \
                     "multiRead_oneObjectPerMaster" \
                     "multiReadThroughput" \
                     "multiWrite_oneMaster" \
                     "readDistRandom" \
                     "writeDistRandom" \
                     "readThroughput" \
                     "readVaryingKeyLength" \
                     "writeVaryingKeyLength" \
#                     "indexBasic" \
#                     "indexMultiple" \
                     "transaction_oneMaster" )

# echo "tests=${cperfTests[@]}"

### For recovery test.
# use Dpdk among client and servers, tcp to/from coordinator
# with default timeout (250s)
# cmd="scripts/recovery.py -v --dry --transport=basic+dpdk"
# cmd="scripts/recovery.py -v --timeout=700 --transport=basic+dpdk"
# cmd="scripts/recovery.py -v --transport=basic+dpdk"
# cmd="scripts/recovery.py -v --transport=fast+dpdk"
# cmd="scripts/recovery.py --transport=fast+dpdk"

# use Dpdk among client and servers, tcp to/from coordinator
# with default timeout (250s)
## cmd="mmfilter scripts/clusterperf.py -v --disjunct --clients=6 --servers=10 --timeout=500 --transport=basic+dpdk basic"
# cmd="mmfilter scripts/clusterperf.py -v --disjunct --clients=6 --servers=10 --timeout=500 --transport=basic+dpdk readDistRandom writeDistRandom readThroughput transaction_oneMaster"
# cmd="mmfilter scripts/clusterperf.py --disjunct --clients=6 --servers=10 --timeout=1000 --transport=basic+dpdk ${cperfTests[@]}"
# cmd="mmfilter scripts/clusterperf.py -v --disjunct --clients=6 --servers=10 --timeout=1000 --transport=basic+dpdk indexBasic"
cmd="mmfilter scripts/clusterperf.py -v --disjunct --clients=1 --servers=10 --timeout=2000 --transport=basic+dpdk indexBasic"
# cmd="mmfilter scripts/clusterperf.py -v --disjunct --clients=6 --servers=10 --timeout=500 --transport=basic+dpdk indexBasic indexMultiple"
# cmd="mmfilter scripts/clusterperf.py -v --disjunct --clients=6 --servers=10 --timeout=1000 --transport=basic+dpdk transaction_oneMaster"
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
lrun date

# mv multiple log dirs to the latest. For clusterperf...
mv 20*.log logs/latest
ls -l logs | grep latest
fdir=`ls -l logs | grep latest | awk '{ print $NF }'`
mv logs/201* logs/$fdir

# rename logdir to newdir name
if [ "$flag_p" = "TRUE" ]; then
    newdir="${prefix}-${fdir}"
    mv logs/$fdir logs/$newdir

else
    newdir=$fdir
fi
echo logs/$newdir is created.

