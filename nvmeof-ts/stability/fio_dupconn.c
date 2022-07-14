/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2018 - 2022 Xilinx, Inc. All rights reserved. */
/*
 * NVMoF Test Suite
 * Performance testing
 */

/** @page stability-fio_dupconn FIO tests with nvme connect
 *
 * @objective FIO tests with nvme connect --duplicate_connect
 *
 * @type fio_stability
 *
 * @ref fio_stability-xml for interation details
 *
 * @param blocksize - Block size unit
 * @param iodepth - Number of IO buffers to keep in flight
 * @param ioengine - IO engine to use
 * @param rwtype - Rand or sequential IO direction
 * @param numjobs - Duplicate job many times
 * @param rwmixread - Percentage of mixed workload that is reads
 * @param runtime_sec - Stop workload when this amount of time has passed, sec.
 *
 * @param interface settings (lro/tso/mtu).
 *
 * @param if_delay_msec Delay that should be written into the netem
 * @param if_loss_proc Loss percentage
 * @param if_corruption_prob Probability to corrupt the packet.
 *
 * @author Nikita Somenkov <Nikita.Somenkov@oktetlabs.ru>
 *
 * @par Scenario:
 */

#define TE_TEST_NAME  "stability/fio_dupconn"

#include "nvmeof_test.h"

typedef struct dupconn_context {
    tapi_fio *fio;
    tsapi_nvme_initiator init;
} dupconn_context;

#define DUPCONN_CONTEXT_DEFAULTS (dupconn_context) \
{                                                  \
    .fio = NULL,                                   \
    .init = TSAPI_NVME_INITIATOR,                  \
}

int
main(int argc, char *argv[])
{
    rcf_rpc_server *init_rpcs = NULL;
    rcf_rpc_server *tgt_rpcs = NULL;
    const struct sockaddr *tgt_addr;
    const struct if_nameindex *init_if, *tgt_if;
    tapi_env_host *init, *tgt;
    tapi_fio_opts opts = TAPI_FIO_OPTS_DEFAULTS;
    tapi_nvme_target target;
    dupconn_context ctxs[] = {
        DUPCONN_CONTEXT_DEFAULTS,
        DUPCONN_CONTEXT_DEFAULTS,
    };
    unsigned int dups = TE_ARRAY_LEN(ctxs);
    unsigned int i;

    TEST_START;
    TEST_GET_HOST(init);
    TEST_GET_HOST(tgt);
    TEST_GET_PCO(init_rpcs);
    TEST_GET_PCO(tgt_rpcs);
    TEST_GET_ADDR(init_rpcs, tgt_addr);
    TEST_GET_IF(init_if);
    TEST_GET_IF(tgt_if);

    TEST_STEP("Read target options.");
    target = tsapi_get_target_ta(tgt->ta);
    target.rpcs = tgt_rpcs;
    target.addr = tgt_addr;
    tsapi_read_nvme_target_opts(&target, argc, argv);

    TEST_STEP("Setup target.");
    CHECK_RC(tapi_nvme_target_init(&target, NULL));
    CHECK_RC(tapi_nvme_target_setup(&target));

    TEST_STEP("Setup initiators and connect");
    for (i = 0; i < dups; i++)
    {
        tapi_nvme_connect_opts nvme_connect_opts =
            TAPI_NVME_CONNECT_OPTS_DEFAULTS;

        TEST_SUBSTEP("Work with %d/%d initiator", i + 1, dups);
        nvme_connect_opts.duplicate_connection = TRUE;
        ctxs[i].init.initiator.rpcs = init_rpcs;
        CHECK_RC(tsapi_nvme_initiator_init(&ctxs[i].init));
        CHECK_RC(tsapi_nvme_initiator_connect_opts(
            &ctxs[i].init, &target, &nvme_connect_opts));
    }

    TEST_STEP("Prepare FIO with the specified parameters.");
    tsapi_read_fio_opts(&opts, argc, argv);
    for (i = 0; i < dups; i++)
    {
        opts.filename = ctxs[i].init.initiator.device;
        ctxs[i].fio = tapi_fio_create(&opts, init_rpcs);
    }

    TEST_STEP("Start fio");
    tsapi_read_fio_opts(&opts, argc, argv);
    for (i = 0; i < dups; i++)
    {
        opts.filename = ctxs[i].init.initiator.device;
        ctxs[i].fio = tapi_fio_create(&opts, init_rpcs);
        CHECK_RC(tapi_fio_start(ctxs[i].fio));
    }

    te_motivated_sleep(opts.runtime_sec, "Wating for fio done");

    TEST_STEP("Wating PID for fio");
    for (i = 0; i < dups; i++)
    {
        CHECK_RC(tapi_fio_wait(ctxs[i].fio, TAPI_FIO_TIMEOUT_DEFAULT));
        CHECK_RC(tapi_fio_stop(ctxs[i].fio));
    }

    TEST_SUCCESS;
cleanup:
    for (i = 0; i < dups; i++)
    {
        tapi_fio_destroy(ctxs[i].fio);
        CLEANUP_CHECK_RC(tsapi_nvme_initiator_disconnect(&ctxs[i].init));
    }
    tapi_nvme_target_fini(&target);

    TEST_END;
}
