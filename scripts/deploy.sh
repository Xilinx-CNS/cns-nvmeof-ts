#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# (c) Copyright 2018 - 2022 Xilinx, Inc. All rights reserved.

. $TE_BASE/scripts/guess.sh

echo ${CONFDIR} ${CONFDIR}/env.${USER}
if [ -e ${CONFDIR}/env.${USER} ] ; then
    . ${CONFDIR}/env.${USER}
fi

do_build=false
do_deploy=true
do_install=false
do_all=false
do_fio=false
do_nvmf=false
do_cli=false

local_prefix=/tmp/nvme_deploy

fail () {
    echo $*
    exit 1
}

try () {
    $* || fail "ERROR: $*"
}

help() {
    echo "Build NVMe tools"
    echo ""
    echo "Set something like:"
    echo "  SF_NVME_DIR : directory with NVMeoF clone"
    echo "  SF_FIO_DIR : directory with FIO clone"
    echo "  SF_NVME_CLI_DIR : directory with nvme-cli clone"
    echo ""
    echo "Yours are:"
    echo "  SF_NVME_DIR=${SF_NVME_DIR}"
    echo "  SF_FIO_DIR=${SF_FIO_DIR}"
    echo "  SF_NVME_CLI_DIR=${SF_NVME_CLI_DIR}"
    echo ""
    echo "  -b|--build          Build things"
    echo "  -d|--deploy         Install things"
    echo ""
    echo "  -f|--fio            FIO is one of the things; will be taken from SF_FIO_DIR"
    echo "  -n|--nvmf           NVMe code is one of the things; will be taken from SF_NVME_DIR"
    echo "  -c|--cli            CLI tools"
    echo "  -a|--all            All the above"
    echo ""
    echo "  -l|--location       Where to install things"
    echo "  -h|--help"
}

while [ -n "$1" ] ; do
    case "$1" in
        -b|--build)
            do_build=true
            ;;
        -d|--deploy)
            do_deploy=true
            ;;
        -f|--fio)
            do_fio=true
            ;;
        -n|--nvmf)
            do_nvmf=true
            ;;
        -c|--cli)
            do_cli=true
            ;;
        -a|--all)
            do_all=true
            do_cli=true
            do_nvmf=true
            do_fio=true
            ;;
        -h|--help)
            help
            exit 0
            ;;
        -l|--location)
            install_dest=$2 ; shift ;
            ;;
        *)
            echo "Pardon? What is this '$1' you're asking me to do?"
            help
            exit 1
            ;;
    esac
    shift
done


if $do_build ; then
    mkdir -p $local_prefix

    if $do_fio ; then
        try pushd $SF_FIO_DIR
        try make clean
        try ./configure --prefix=${local_prefix}
        try make -j
        cp fio ${local_prefix}/
        popd
    fi

    if $do_nvmf ; then
        try pushd $SF_NVME_DIR
        if [ -e /etc/redhat-release ] ; then
            try make -C drivers/rhel7.5_nvme KDIR=/usr/src/kernels/3.10.0-862.el7.x86_64/ ONLY_SFC_DEVICES=0 clean
            try make -C drivers/rhel7.5_nvme KDIR=/usr/src/kernels/3.10.0-862.el7.x86_64/ ONLY_SFC_DEVICES=0 build -j
        else
            try make -C drivers/rhel7.5_nvme KDIR=/lib/modules/*/build clean
            try make -C drivers/rhel7.5_nvme KDIR=/lib/modules/*/build build -j
        fi
        try cp drivers/rhel7.5_nvme/target/*.ko ${local_prefix}/
        try cp drivers/rhel7.5_nvme/host/*.ko ${local_prefix}/
        popd
    fi

    if $do_cli ; then
        try pushd $SF_NVME_CLI_DIR
        try make clean
        try make -j
        try cp nvme ${local_prefix}/
        popd
    fi
fi

if $do_deploy ; then
    if $do_fio ; then
        try pushd $SF_FIO_DIR
        cp fio ${local_prefix}/
        popd
    fi

    if $do_nvmf ; then
        try pushd $SF_NVME_DIR
        try cp drivers/rhel7.5_nvme/target/*.ko ${local_prefix}/
        try cp drivers/rhel7.5_nvme/host/*.ko ${local_prefix}/
        popd
    fi

    if $do_cli ; then
        try pushd $SF_NVME_CLI_DIR
        try cp nvme ${local_prefix}/
        popd
    fi
fi
