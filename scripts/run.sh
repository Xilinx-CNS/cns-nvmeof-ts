#! /bin/bash
# SPDX-License-Identifier: Apache-2.0
# (c) Copyright 2018 - 2022 Xilinx, Inc. All rights reserved.
#
# Helper script to run Test Environment for the Test Suite
#
# Author: Konstantin Ushakov <Konstantin.Ushakov@oktetlabs.ru>
#

RUNDIR="$(pwd -P)"
if test -z "${TE_SSH_KEY_DIR}" ; then
    export TE_SSH_KEY_DIR=${RUNDIR}/conf
fi

if test -z "${TS_TOPDIR}" ; then
    if [ -d .hg ] ; then
        export TS_TOPDIR="$(cd "$(dirname "$(which "$0")")" ; pwd -P)"
    elif [ -d "../.hg" ]; then
        export TS_TOPDIR="$(cd "$(dirname "$(which "$0")")"/.. ; pwd -P)"
    else
        echo "We can't guess TS_TOPDIR. If you're using sync-ed copy w/o .hg go to TS and do"
        echo "$ mkdir .hg"
        exit 1
    fi
fi

do_parallel=true

if test -z "${TE_BASE}" ; then
    if test -e dispatcher.sh ; then
        export TE_BASE="${RUNDIR}"
    elif test -e "${TS_TOPDIR}/dispatcher.sh" ; then
        export TE_BASE="${TS_TOPDIR}"
    elif test -e "${TS_TOPDIR}/../te/dispatcher.sh" ; then
        export TE_BASE="${TS_TOPDIR}/../te"
    else
        echo "Failed to find TE, export TE_BASE" >&2
        exit 1
    fi
fi

usage() {
cat <<EOF
USAGE: run.sh [run.sh options] [dispatcher.sh options]
Options:
  --cfg=<CFG>                               Configuration to be used
  [--with-aggr=<bond|team>]                 Aggregation interfaces: bonding or teaming
  [--sf-copy]                               The necessary tools will be copied
  [--sf-tools=<fio[,nvme-cli,etc]>]         Specifies tool for copy to Agent
  [--sf-load]                               Load necessary NVMeOF modules
  [--test-scenario=<fast|nightly|...>]      Specifies the matrix of arguments
  [--include-high-memory-pressue-tests]     Includes tests that require a lot of memory resources
  [--with-perf]                             Create perf report after run
  [--tester-count]                          Do fake run for print count of tests
  [--target=<onvme,kernel>]                 Default target type [default: kernel]
  [--hdr-digest]                            Enable transport protocol header for nvme connect
  [--data-digest]                           Enable transport protocol data for nvme connect
  [--use-null]                              Use nullblk instead real SSD

EOF
"${TE_BASE}"/dispatcher.sh --help
exit 1
}

export TE_DEFAULT_NVME_TGT_TYPE=kernel
export TE_DEFAULT_NVME_CONN_HDR_DIGEST=0
export TE_DEFAULT_NVME_CONN_DATA_DIGEST=0
TURN_OFF_UNREAL_TESTS_OPTS="--tester-req=\"!(SMALL_BLOCK&NUMJOBS_GT_100)\" "
for opt ; do
    case "${opt}" in
        --help) usage ;;
        --cfg=*)
            cfg=${opt#--cfg=}
            RUN_OPTS="${RUN_OPTS} --opts=run.conf.\"${cfg}\""
            ;;
        --sf-copy)
            export SF_TOOLS_COPY=true
            ;;
        --sf-tools=*)
            export SF_TOOLS=${opt#--sf-tools=}
            ;;
        --sf-load)
            export SF_TOOLS_COPY=true
            export SF_MODULES_LOAD=true
            ;;
        --build-noparallel|--build-sequential) do_parallel=false ;;
        --with-aggr=*)
            export TE_CS_BOND=${opt#--with-aggr=}
            TRC_TAGS="$TRC_TAGS $TE_CS_BOND"
            if test -z "${TE_CS_BOND_IF}" ; then
                export TE_CS_BOND_IF=bond0
            fi
            RUN_OPTS="${RUN_OPTS} --opts=opts.aggregation"
            ;;
        --test-scenario=*)
            matrix=${opt#--test-scenario=}
            TRC_TAGS+="$matrix "
            RUN_OPTS="${RUN_OPTS} --opts=opts.matrix.${matrix}"
            ;;
        --include-high-memory-pressue-tests)
            TURN_OFF_UNREAL_TESTS_OPTS=""
            ;;
        --with-perf)
            export WITH_PERF=true
            ;;
        --tester-count)
            RUN_OPTS+=" --tester-fake=nvmeof-ts"
            POST_PROC+=" | uniq -c | grep -e 'Starting test' -e 'Starting package' | awk '{print \$3, \$4, \$1}' | column -t"
            TESTER_COUNT=true
            ;;
        --target=*)
            export TE_DEFAULT_NVME_TGT_TYPE=${opt#--target=}
            TRC_TAGS+="$TE_DEFAULT_NVME_TGT_TYPE "
            ;;
        --hdr-digest)
            TRC_TAGS+="hdr-digest "
            export TE_DEFAULT_NVME_CONN_HDR_DIGEST=1
            ;;
        --data-digest)
            TRC_TAGS+="data-digest "
            export TE_DEFAULT_NVME_CONN_DATA_DIGEST=1
            ;;
        --use-null)
            TRC_TAGS+="nullblock "
            RUN_OPTS="$RUN_OPTS --script=env.nullblock"
            ;;
        *)  RUN_OPTS="${RUN_OPTS} \"${opt}\"" ;;
    esac
    shift 1
done

REQS_OPTS+="$TURN_OFF_UNREAL_TESTS_OPTS "
if [[ $TE_DEFAULT_NVME_TGT_TYPE == "kernel" ]]; then
    REQS_OPTS+="--tester-req=\!ONVME_TARGET "
fi

if test -z "${TE_BUILD}" ; then
    if test "${TS_TOPDIR}" = "${RUNDIR}" ; then
        TE_BUILD="${TS_TOPDIR}/build"
        mkdir -p build
    else
        TE_BUILD="${RUNDIR}"
    fi
    export TE_BUILD
fi

if test -z "${SF_TS_CONFDIR}" ; then
    SF_TS_CONFDIR="${TS_TOPDIR}"/../ts_conf
    if test -d "${SF_TS_CONFDIR}" ; then
        echo "Guessed SF_TS_CONFDIR=${SF_TS_CONFDIR}"
    else
        echo "Cannot import SF shared TS confdir '$SF_TS_CONFDIR'" >&2
        exit 1
    fi
fi

MY_OPTS=
MY_OPTS="${MY_OPTS} --conf-dirs=\"${TS_TOPDIR}/conf:${TS_TOPDIR}/conf/matrices:${SF_TS_CONFDIR}\""

test -e "${TS_TOPDIR}/conf/trc.xml" &&
    MY_OPTS="${MY_OPTS} --trc-db=\"${TS_TOPDIR}\"/conf/trc.xml"
MY_OPTS="${MY_OPTS} --trc-comparison=normalised"
MY_OPTS="${MY_OPTS} --trc-html=trc-full.html"
MY_OPTS="${MY_OPTS} --trc-html-logs=html"
MY_OPTS="${MY_OPTS} --trc-no-unspec"
MY_OPTS="${MY_OPTS} --trc-no-stats-not-run"
MY_OPTS="${MY_OPTS} --trc-key2html=${SF_TS_CONFDIR}/trc.key2html"

MY_OPTS="${MY_OPTS} --log-html=html"

for tag in $TRC_TAGS; do
    MY_OPTS="${MY_OPTS} --trc-tag=$tag"
done

MY_OPTS="${MY_OPTS} ${REQS_OPTS}"

$do_parallel && RUN_OPTS="${RUN_OPTS} --build-parallel"

# Add to RUN_OPTS since it specified in user environment and should
# override everything else
test "$TE_NOBUILD" = "yes" &&
    RUN_OPTS="${RUN_OPTS} --no-builder --tester-nobuild"
test -z "${TE_BUILDER_CONF}" ||
    RUN_OPTS="${RUN_OPTS} --conf-builder=\"${TE_BUILDER_CONF}\""
test -z "${TE_TESTER_CONF}" ||
    RUN_OPTS="${RUN_OPTS} --conf-tester=\"${TE_TESTER_CONF}\""

test -z "${TE_TS_SRC}" -a -d "${TS_TOPDIR}/nvmeof-ts" && export TE_TS_SRC="${TS_TOPDIR}/nvmeof-ts"
test -e "${TE_TS_SRC}/auxdir" || ln -s ${TE_BASE}/auxdir ${TE_TS_SRC}/auxdir


# Make sure that old TRC and performance reports is not kept
rm -f trc-brief.html perf.txt

eval "${TE_BASE}/dispatcher.sh ${MY_OPTS} ${RUN_OPTS} ${POST_PROC}"
RESULT=$?

if test ${RESULT} -ne 0 ; then
    echo FAIL
    echo ""
fi

if test -n "${WITH_PERF}" ; then
    echo "Generating performance report..."
    ${TS_TOPDIR}/scripts/perf.py --te_log=${RUNDIR}/log.txt > perf.txt
fi
RESULT=$?

if test ${RESULT} -ne 0 ; then
    echo FAIL
    echo ""
fi

echo -ne "\a"
exit ${RESULT}
