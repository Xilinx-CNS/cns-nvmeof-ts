/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2018 - 2022 Xilinx, Inc. All rights reserved. */
/*
 * NVMoF Test Suite
 * Performance testing
 */

/** @page perf-fio_simple Back to back FIO test with perf collection
 *
 * @objective Run back to back FIO with performance numbers collection.
 *
 * @type perf
 *
 * See @ref fio_simple-xml for iteration details.
 *
 * Most of the parameters are applied by common API during corresponding
 * subsystem configuration.
 *
 * @param blocksize - Block size unit
 * @param iodepth - Number of IO buffers to keep in flight
 * @param ioengine - IO engine to use
 * @param rwtype - Rand or sequential IO direction
 * @param numjobs - Duplicate job many times
 * @param rwmixread - Percentage of mixed workload that is reads
 * @param runtime_sec - Stop workload when this amount of time has passed, sec.
 *
 * @param mtu - MTU value for NIC
 * @param tco - off or on TCO feature value for NIC
 * @param lro - off or on LRO feature value for NIC
 *
 * @param nqn - NVMe Qualified Name
 *
 * @author Nikita Somenkov <Nikita.Somenkov@oktetlabs.ru>
 *
 * @par Scenario:
 */

#define TE_TEST_NAME  "perf/fio_simple"

#include "nvmeof_test.h"

int
main(int argc, char *argv[])
{
    rcf_rpc_server *init_rpcs = NULL;
    rcf_rpc_server *tgt_rpcs = NULL;
    const struct sockaddr *tgt_addr;
    const struct if_nameindex *init_if, *tgt_if;
    tapi_env_host *init, *tgt;
    tapi_fio *fio;
    tapi_fio_opts opts = TAPI_FIO_OPTS_DEFAULTS;
    if_feature_opts sys_opts = IF_FEATURE_OPTS_DEFAULTS;
    tapi_fio_report report;
    tsapi_nvme_initiator initiator = TSAPI_NVME_INITIATOR;
    tapi_nvme_connect_opts nc_opts;
    tapi_nvme_target target;
    void *specific_tgt_opts = NULL;
    struct tsapi_sysstat stats[] = {
        TSAPI_SYSSTAT_INIT(CPU),
        TSAPI_SYSSTAT_INIT(IO),
    };

    TEST_START;
    TEST_GET_HOST(init);
    TEST_GET_HOST(tgt);
    TEST_GET_PCO(init_rpcs);
    TEST_GET_PCO(tgt_rpcs);
    TEST_GET_ADDR(tgt_rpcs, tgt_addr);
    TEST_GET_IF(init_if);
    TEST_GET_IF(tgt_if);

    target = tsapi_get_target_ta(tgt->ta);
    target.rpcs = tgt_rpcs;
    target.addr = tgt_addr;
    initiator.initiator.rpcs = init_rpcs;

    TEST_STEP("Start collection system statistic");
    CHECK_RC(tsapi_sysstat_start_all(stats, TE_ARRAY_LEN(stats), init_rpcs));

    TEST_STEP("Read target options");
    tsapi_read_nvme_target_opts(&target, argc, argv);
    tsapi_read_nvme_target_specific(&target, &specific_tgt_opts, tgt_if->if_name,
                                    argc, argv);

    TEST_STEP("Setup target");
    CHECK_RC(tapi_nvme_target_init(&target, specific_tgt_opts));
    CHECK_RC(tapi_nvme_target_setup(&target));
    TEST_STEP("Setup initiator");
    tsapi_get_nvme_connect_opts(init->ta, &nc_opts);
    CHECK_RC(tsapi_nvme_initiator_init(&initiator));
    CHECK_RC(tapi_nvme_initiator_connect_opts(&initiator.initiator, &target, &nc_opts));
    CHECK_RC(tapi_nvme_initiator_list(&initiator.initiator));

    TEST_STEP("Read and apply interface options");
    tsapi_read_if_feature_opts(&sys_opts, argc, argv);
    tsapi_apply_if_feature_opts(&sys_opts, init->ta, init_if->if_name);
    tsapi_apply_if_feature_opts(&sys_opts, tgt->ta, tgt_if->if_name);

    TEST_STEP("Prepare FIO run");
    tsapi_read_fio_opts(&opts, argc, argv);
    tsapi_apply_perf_profiles_fio_opts(&opts, init_if->if_name, argc, argv);
    opts.filename = initiator.initiator.device;
    fio = tapi_fio_create(&opts, init_rpcs);

    TEST_STEP("Start FIO and wait for completion");

    /* Bug 9788:
     * When we running really huge amount of threads scenarious, we
     * constantly hit "Too many open files" error.
     * */
    tsapi_set_fdlimits(init_rpcs);
    CHECK_RC(tapi_fio_start(fio));
    CHECK_RC(tapi_fio_wait(fio, TAPI_FIO_TIMEOUT_DEFAULT));
    CHECK_RC(tapi_fio_stop(fio));

    TEST_STEP("Get FIO report and log it");
    CHECK_RC(tapi_fio_get_report(fio, &report));

    tapi_fio_log_report(&report);

    TEST_SUCCESS;
cleanup:
    TEST_STEP("Disconnect the initiator and wipe target config.");
    CLEANUP_CHECK_RC(tsapi_sysstat_stop_all(stats, TE_ARRAY_LEN(stats)));
    free(specific_tgt_opts);
    tapi_fio_destroy(fio);
    CLEANUP_CHECK_RC(tsapi_nvme_initiator_disconnect(&initiator));
    tapi_nvme_target_fini(&target);

    TEST_END;
}
