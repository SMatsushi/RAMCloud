#!/bin/sh
num=$1
echo $num
if [ -e recov-fail* ]; then
    mv -f recov-fail* Old
fi
Res=`basename 20*`
New="recov-failMetric${num}-${Res}"
echo moving $Res to $New ....

mv $Res $New
cd $New
macConv.pl -g client*.log
# echo $New/client*.txt is created.
