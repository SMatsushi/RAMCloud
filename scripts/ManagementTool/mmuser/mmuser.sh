#!/usr/bin/env bash
# exit on error
set -o errexit

# create a user on mgmt/atom hosts.
# initial passwd is same as username
# environment: hosts, inventory, args, playbook
# Usage:
#   - create a user on mgmt/atom hosts only
#     $ $0 user
#   - create a user on atom hosts only
#     $ args='-l ramcloud' $0 user
#   - remove a user on mgmt/atom hosts
#     $ playbook=remove-accounts.yml $0 user

script_path=$(dirname $(readlink -f $0))
script_name=$(basename $0)

hosts=${hosts:-"/etc/hosts"}
passwd=${passwd:-"/etc/passwd"}

args=${args:-''}
playbook=${playbook:-"create-accounts.yml"}
rm_playbook=${rm_playbook:-"remove-accounts.yml"}
inventory=${inventory:-""}
userdata=${userdata:-""}

# ramcloud: all hosts named with 'atomXXX'
create_inventory () {
    echo "[ramcloud]"
    grep -E '\batom[[:digit:]]{3}\b' $* | cut -f 2 | sed 's/ *$//'
}
check_inventory () {
    local inventory=$1
    [ -r $inventory ] || {
        echo "$script_name: can't read $inventory"; exit 1;
    }
    grep -q '^atom' $inventory || {
        echo "$script_name: no valid host found"; exit 1;
    }
}

create_userdata () {
    ruby create-userdata.rb $*
}
check_userdata () {
    local userdata=$1
    [ -r $userdata ] || {
        echo "$script_name: can't read $userdata"; exit 1;
    }
    grep -q 'name: ' $userdata || {
        echo "$script_name: no valid user to create"; exit 1;
    }
}

create_users () {
    ansible-playbook -i $1 $args $playbook --extra-vars "data=$2"
}

# remove_user inventory user
remove_users () {
    local inventory="$1"
    shift
    for user in $*; do
        ansible-playbook -i $inventory $args $rm_playbook --extra-vars "user=$user"
    done
}

opts="Dv"
usage () {
    echo "Usage: $script_name user [user ...]"
}

while getopts $opts opt; do
    case $opt in
        D) delete="true" ;;
        v) verbose="true"; args="-v --diff" ;;
        h) usage; exit 0 ;;
        *) usage; exit 1 ;;
    esac
done
shift $((OPTIND - 1))

[ "$#" -eq 0 ] && { usage; exit 1; }

cd $script_path

[ -z "$inventory" ] && {
    inventory="inventory"
    create_inventory $hosts | sudo tee $inventory > /dev/null
}
check_inventory $inventory

[ -n "$delete" ] && {
    if [ -n "$verbose" ]; then
        remove_users $inventory $*
    else
        remove_users $inventory $* > /dev/null
    fi
    exit 0
}

[ -z "$userdata" ] && {
    userdata="userdata.yml"
    create_userdata $* < $passwd | sudo tee $userdata > /dev/null
}
check_userdata $userdata

if [ -n "$verbose" ]; then
    create_users $inventory $userdata
else
    create_users $inventory $userdata > /dev/null
fi
