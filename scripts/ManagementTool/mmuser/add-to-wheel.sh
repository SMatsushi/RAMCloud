#!/usr/bin/env bash
# exit on error
set -o errexit

# add users to the wheel group on atom* hosts

opts=${opts:-"-v"}
inventory=${inventory:-"/usr/local/mmutils/mmuser/inventory"}
[ -r $inventory ] || { echo "can't read $inventory"; exit 1; }

for user in $*; do
    args="name=$user append=yes groups=wheel"
    ansible $opts -i $inventory -s -m user -a "$args" ramcloud
done
exit 0

