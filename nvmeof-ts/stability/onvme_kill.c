/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2018 - 2022 Xilinx, Inc. All rights reserved. */
/*
 * NVMoF Test Suite
 * Stability testing
 */

/** @page stability-onvme_kill FIO tests over bad link
 *
 * @objective Kill ONVMe target while fio work
 *
 * @type onvme_kill
 *
 * @ref onvme_kill-xml for interation detai
 *
 * @param blocksize - Block size unit
 * @param iodepth - Number of IO buffers to keep in flight
 * @param ioengine - IO engine to use
 * @param rwtype - Rand or sequential IO direction
 * @param numjobs - Duplicate job many times
 * @param rwmixread - Percentage of mixed workload that is reads
 * @param runtime_sec - Stop workload when this amount of time has passed, sec.
 *
 * @author Nikita Somenkov <Nikita.Somenkov@oktetlabs.ru>
 *
 * @par Scenario:
 */

#define TE_TEST_NAME  "stability/onvme_kill"

#include "nvmeof_test.h"

#define SUPPORTED_SIGNAL(_sig) \
    { #_sig, _sig }

#define SIGNAL_MAPPING_LIST     \
    SUPPORTED_SIGNAL(SIGINT),   \
    SUPPORTED_SIGNAL(SIGTERM),  \
    SUPPORTED_SIGNAL(SIGKILL)

static void
killpg_onvme(tapi_nvme_target *target, int sig)
{
    te_errno rc;
    const int max_delay = 20;
    unsigned int delay = rand() % max_delay;
    tapi_nvme_onvme_target_proc *proc =
        (tapi_nvme_onvme_target_proc*)target->impl;

    te_motivated_sleep(delay, "Delay for kill target");

    if ((rc = tapi_job_killpg(proc->onvme_job, sig)) != 0)
        TEST_FAIL("We cannot kill ONVMe process %r", rc);
}

int
main(int argc, char *argv[])
{
    rcf_rpc_server *init_rpcs = NULL;
    rcf_rpc_server *tgt_rpcs = NULL;
    const struct sockaddr *tgt_addr;
    const struct if_nameindex *init_if, *tgt_if;
    tapi_env_host *init, *tgt;
    tapi_fio *fio = NULL;
    tapi_fio_opts opts = TAPI_FIO_OPTS_DEFAULTS;
    if_feature_opts sys_opts = IF_FEATURE_OPTS_DEFAULTS;
    tsapi_nvme_initiator initiator = TSAPI_NVME_INITIATOR;
    tapi_nvme_target target = TAPI_NVME_ONVME_TARGET;
    enum rpc_signum sig = RPC_SIGTERM;
    int wating_reconnect = 15;

    TEST_START;
    TEST_GET_HOST(init);
    TEST_GET_HOST(tgt);
    TEST_GET_PCO(init_rpcs);
    TEST_GET_PCO(tgt_rpcs);
    TEST_GET_ADDR(init_rpcs, tgt_addr);
    TEST_GET_IF(init_if);
    TEST_GET_IF(tgt_if);
    TEST_ENUM_PARAM(sig, SIGNAL_MAPPING_LIST);
    TEST_INT_PARAM(wating_reconnect);

    TEST_STEP("Read target options.");
    target.rpcs = tgt_rpcs;
    target.addr = tgt_addr;
    tsapi_read_nvme_target_opts(&target, argc, argv);

    TEST_STEP("Setup target.");
    CHECK_RC(tapi_nvme_target_init(&target, NULL));
    CHECK_RC(tapi_nvme_target_setup(&target));

    TEST_STEP("Connect initiator to the target.");
    initiator.initiator.rpcs = init_rpcs;
    CHECK_RC(tsapi_nvme_initiator_init(&initiator));
    CHECK_RC(tapi_nvme_initiator_connect(&initiator.initiator, &target));

    TEST_STEP("Read and apply interface options.");
    tsapi_read_if_feature_opts(&sys_opts, argc, argv);
    tsapi_apply_if_feature_opts(&sys_opts, init->ta, init_if->if_name);
    tsapi_apply_if_feature_opts(&sys_opts, tgt->ta, tgt_if->if_name);

    TEST_STEP("Prepare FIO with the specified parameters.");
    tsapi_read_fio_opts(&opts, argc, argv);
    opts.filename = initiator.initiator.device;
    fio = tapi_fio_create(&opts, init_rpcs);

    TEST_STEP("Start FIO");
    CHECK_RC(tapi_fio_start(fio));

    TEST_STEP("Do kill target");
    te_motivated_sleep(fio->app.opts.runtime_sec, "Remove target after ");
    killpg_onvme(&target, sig);

    TEST_STEP("Setup new target");
    tapi_nvme_onvme_target_fini(&target);
    CHECK_RC(tapi_nvme_onvme_target_init(&target, NULL));
    CHECK_RC(tapi_nvme_target_setup(&target));

    CHECK_RC(tapi_fio_wait(fio, wating_reconnect));
    CHECK_RC(tapi_fio_stop(fio));

    TEST_SUCCESS;
cleanup:
    tapi_fio_destroy(fio);
    CLEANUP_CHECK_RC(tsapi_nvme_initiator_disconnect(&initiator));
    tapi_nvme_target_fini(&target);

    TEST_END;
}
