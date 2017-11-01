#!/bin/bash -xv
# QFlex consists of several software components that are governed by various
# licensing terms, in addition to software that was developed internally.
# Anyone interested in using QFlex needs to fully understand and abide by the
# licenses governing all the software components.

# ### Software developed externally (not by the QFlex group)

#     * [NS-3](https://www.gnu.org/copyleft/gpl.html)
#     * [QEMU](http://wiki.qemu.org/License)
#     * [SimFlex](http://parsa.epfl.ch/simflex/)

# ### Software developed internally (by the QFlex group)
# **QFlex License**

# QFlex
# Copyright (c) 2016, Parallel Systems Architecture Lab, EPFL
# All rights reserved.

# Redistribution and use in source and binary forms, with or without modification,
# are permitted provided that the following conditions are met:

#     * Redistributions of source code must retain the above copyright notice,
#       this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above copyright notice,
#       this list of conditions and the following disclaimer in the documentation
#       and/or other materials provided with the distribution.
#     * Neither the name of the Parallel Systems Architecture Laboratory, EPFL,
#       nor the names of its contributors may be used to endorse or promote
#       products derived from this software without specific prior written
#       permission.

# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE PARALLEL SYSTEMS ARCHITECTURE LABORATORY,
# EPFL BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
# GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
# THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

cd $HOME/build/parsa-epfl/qemu/tests/
cp ../scripts/user_example.cfg ../scripts/user.cfg

export IS_EXTSNAP=`grep enable-extsnap ../configure`
export IS_QUANTUM=`grep enable-quantum ../configure`
export IS_PTH=`grep enable-pth ../configure`

if [[ "$IS_EXTSNAP" == "" ]] && [[ "$TEST_EXTSNAP" == "yes" ]]; then
    exit 0
fi

if [[ "$IS_QUANTUM" == "" ]] && [[ "$TEST_QUANTUM" == "yes" ]]; then
    exit 0
fi

if [[ "$IS_PTH" == "" ]] && [[ "$TEST_PTH" == "yes" ]]; then
    exit 0
fi

sed -i "s/USER_NAME username/USER_NAME $USER/" ../scripts/user.cfg
sed -i "s/QEMU_CORE_NUM 4/QEMU_CORE_NUM 1/" ../scripts/user.cfg
sed -i "s/MEM 4096/MEM 512/" ../scripts/user.cfg
path_to_kernel=$(printf '%s\n' "$HOME/build/parsa-epfl/qemu/images/kernel" | sed 's/[[\.*^$/]/\\&/g')
sed -i "s/\/path\/to\/qemu\/image\/kernel/$path_to_kernel/g" ../scripts/user.cfg
path_to_qemu=$(printf '%s\n' "$HOME/build/parsa-epfl/qemu/" | sed 's/[[\.*^$/]/\\&/g')
sed -i "s/\/path\/to\/qemu/$path_to_qemu/g" ../scripts/user.cfg
path_to_image=$(printf '%s\n' "$HOME/build/parsa-epfl/qemu/images/ubuntu-16.04-blanc/ubuntu-stripped-comp3.qcow2" | sed 's/[[\.*^$/]/\\&/g')
sed -i "s/\/path\/to\/server\/image\/.qcow2\/or\/.img/$path_to_image/g" ../scripts/user.cfg
sed -i "s/\/path\/to\/client\/image\/.qcow2\/or\/.img/$path_to_image/g" ../scripts/user.cfg

# Font Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m'
# Getting the Absolute Path to the script
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

if [ -d $DIR/results ]; then
    rm -r $DIR/results
fi

mkdir $DIR/results

LOG=$DIR/results/summary

# An attempt to obtain sudo permissions at the start of the test
# Some helper functions
check_status() {
    if [ ! -z "$1" ]; then
        printf "[ ${GREEN} PASSED ${NC} ] $2\n" >> $LOG
        printf "[ ${GREEN} PASSED ${NC} ] $2\n"
    else
        # print logs on error
        cat $3
        printf "[ ${RED} FAILED ${NC} ] $2\n" >> $LOG
        printf "[ ${RED} FAILED ${NC} ] $2\n"
        exit 1
    fi
}

if [ "$TEST_EXTSNAP" == "yes" ]; then
    # Test run_system.sh in default mode
    bash $DIR/scripts/run_system.sh --kill -exp=../results/single_save -ow -sn=test_snap
    pushd $DIR/results/single_save >> /dev/null

    # Check to see if commands are executing
    TEST_DEFAULT=`grep cmd_0_success logs`
    check_status "$TEST_DEFAULT" "Booting Default" "$DIR/results/single_save/Qemu_0/logs"

    # Check to see if snapshot can be saved
    TEST_SNAPSHOT=`grep SNAP_SUCCESS logs`
    check_status "$TEST_SNAPSHOT" "Saving Snapshot" "$DIR/results/single_save/Qemu_0/logs"
    popd >> /dev/null
    # Test run_system.sh in load mode
    bash $DIR/scripts/run_system.sh --kill -exp=../results/single_load -ow -lo=test_snap -rs -sn=test_snap
    pushd $DIR/results/single_load >> /dev/null

    # Test to see if commands are executing
    TEST_RELOAD=`grep cmd_0_success logs`
    check_status "$TEST_RELOAD" "Loading Snapshot" "$DIR/results/single_load/Qemu_0/logs"

    # Check to see if snapshot can be removed
    TEST_REMOVE=`grep SNAP_REMOVED logs`
    check_status "$TEST_REMOVE" "Deleting Snapshot" "$DIR/results/single_load/Qemu_0/logs"
    popd >> /dev/null

fi

# PTH test
if [ "$TEST_PTH" == "yes" ]; then
    bash $DIR/scripts/run_system.sh --kill -exp=../results/pth -ow -pth -mult --no_ns3
    pushd $DIR/results/pth >> /dev/null
    TEST_PTH_DIFF=`grep PTH_SUCCESS logs`
    check_status "$TEST_PTH_DIFF" "PTH works" "$DIR/results/pth/diffout"
    popd >> /dev/null
fi

if [ "$TEST_QUANTUM" == "yes" ]; then
    bash $DIR/scripts/run_system.sh --kill -exp=../results/quantum -ow -qn -mult --no_ns3
    pushd $DIR/results/quantum >> /dev/null
    TEST_QUANTUM_BOOT=`grep QUANTUM_SUCCESS logs`
    check_status "$TEST_QUANTUM_BOOT" "QUANTUM works" "$DIR/results/quantum"
    popd >> /dev/null
fi

exit 0

#################################################################################

# ustiugov FIXME: enable multiple nodes testing in travis (soft-lockup bug)
#################################################################################

# Test run_system.sh in multiple instance mode
bash $DIR/scripts/run_system.sh --kill -exp=../results/multiple_ping -mult -ow --no_ns3
pushd $DIR/results/multiple_ping >> /dev/null

# Test to see if instances are configured
TEST_CONFIG_0=`grep "inet 192.168.2.100/24" logs`
TEST_CONFIG_1=`grep "inet 192.168.2.101/24" logs`
check_status "$TEST_CONFIG_0" "Configuration Multiple Instance 0" "$DIR/results/multiple_ping/Qemu_0/logs"
check_status "$TEST_CONFIG_1" "Configuration Multiple Instance 1" "$DIR/results/multiple_ping/Qemu_1/logs"

# Test to see if commands are executing
TEST_MULTIPLE_0=`grep cmd_0_success logs`
TEST_MULTIPLE_1=`grep cmd_1_success logs`
check_status "$TEST_MULTIPLE_0" "Booting Multiple Instance 0" "$DIR/results/multiple_ping/Qemu_0/logs"
check_status "$TEST_MULTIPLE_1" "Booting Multiple Instance 1" "$DIR/results/multiple_ping/Qemu_1/logs"

# Test to see if network ping works
TEST_MULTIPLE_PING=`grep "192.168.2.100: icmp_seq=1" logs`
check_status "$TEST_MULTIPLE_PING" "PING through NS3" "$DIR/results/multiple_ping/logs"
popd >> /dev/null

printf "\nSummary:\n"

cat $DIR/results/summary
