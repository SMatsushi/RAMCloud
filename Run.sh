#!/bin/bash
#
# Compile RAMCloud and Run Clusterperf.py basic for debugging

CMDNAME=`basename $0`
while getopts chop: OPT
do
    case $OPT in
	"c" ) flag_c="TRUE";;
	"o" ) flag_o="TRUE";;
	"p" ) flag_p="TRUE"; prefix=$OPTARG ;;
	"h" ) echo "Usage: $CMDNAME [-c][-h][-o][-p log_prefix]" 1>&2
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
# cmd="make -j8 DEBUG=no DPDK=yes"
# cmd="make -j8 ARCH=atom DEBUG=no DPDK=yes"
cmd="make -j8 DEBUG=no"
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

### For recovery test.
# use Dpdk among client and servers, tcp to/from coordinator
# with default timeout (250s)
# cmd="scripts/recovery.py -v --dry --transport=basic+dpdk"
## cmd="mmfilter scripts/recovery.py -v --timeout=1000 --transport=basic+dpdk"
# cmd="scripts/recovery.py -v --transport=basic+dpdk"

# use Dpdk among client and servers, tcp to/from coordinator
# with default timeout (250s)
cmd="mmfilter scripts/clusterperf.py -v --disjunct --clients=6 --servers=10 --timeout=500 --transport=basic+dpdk basic"
## cmd="mmfilter scripts/clusterperf.py -v --timeout=500 --disjunct --transport=basic+dpdk basic"
# cmd="mmfilter scripts/clusterperf.py -v --disjunct --clients=6 --servers=10 --timeout=500 --transport=basic+dpdk readDistRandom writeDistRandom readThroughput transaction_oneMaster"

# echo "tests=${cperfTests[@]}"
## cmd="mmfilter scripts/clusterperf.py --disjunct --clients=6 --servers=10 --timeout=1000 --transport=basic+dpdk ${cperfTests[@]}"
## cmd="mmfilter scripts/clusterperf.py -v --disjunct --clients=6 --servers=10 --timeout=500 --transport=basic+dpdk indexBasic"
# cmd="mmfilter scripts/clusterperf.py -v --disjunct --clients=1 --servers=10 --timeout=2000 --transport=basic+dpdk indexBasic"
# cmd="mmfilter scripts/clusterperf.py -v --disjunct --clients=6 --servers=10 --timeout=500 --transport=basic+dpdk indexBasic indexMultiple"
# cmd="mmfilter scripts/clusterperf.py -v --disjunct --clients=6 --servers=10 --timeout=1000 --transport=basic+dpdk transaction_oneMaster"
# cmd="scripts/clusterperf.py -v --transport=basic+dpdk basic"

# with default timeout (250s)
# cmd="scripts/clusterperf.py -v --transport=fast+dpdk basic"

# Use TCP instead
# cmd="scripts/clusterperf.py -v --transport=tcp basic"

echo $cmd
if [ "$flag_o" = "TRUE" ]; then
    echo "Running opfile on atom002"
   # ssh atom002 opcontrol --start
fi
lrun $cmd
lrun date
if [ "$flag_o" = "TRUE" ]; then
    echo "Summarizing result"
   # ssh atom002 'opreport -c obj.new-dpdk/server > logs/oprofile.out'
fi

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

