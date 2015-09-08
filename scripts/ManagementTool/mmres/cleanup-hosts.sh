#!/bin/bash -p

files="
/var/run/dpdk/.rte_config
/var/run/dpdk/.rte_hugepage_info
/var/ramcloud/backup/backup1.db
/var/ramcloud/backup/backup2.db
"
# assume root@localhost can login as root@atom* with no password
user="root"

cleanup () {
    local host=$1
    ssh $user@$host /bin/rm -f $files
}

for host in $*; do
    case $host in
        atom[0-9][0-9][0-9]|mmatom)
            [ -n "$VERBOSE" ] && echo $host
            cleanup $host ;;
        *)
            [ -n "$VERBOSE" ] && echo "invalid host $host. ignored" ;;
    esac
done
exit 0
