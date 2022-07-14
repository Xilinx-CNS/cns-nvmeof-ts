/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2018 - 2022 Xilinx, Inc. All rights reserved. */
/*
 * NVMoF Test Suite
 * fio pattern validation testing
 */

/** @page date-fio_pattern Run fio pattern
 *
 * @objective Run fio pattern
 *
 * @type perf
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

#define TE_TEST_NAME  "perf/fio_pattern"

#include "nvmeof_test.h"

static void
get_common_verify_opts(te_string *vopts, tapi_fio_opts *opts)
{
    te_string_reset(vopts);
    CHECK_RC(te_string_append(vopts, " --verify=md5"));
    CHECK_RC(te_string_append(vopts, " --verify_interval=%d", opts->blocksize));
}

static void
get_writter_verify_opts(te_string *vopts, tapi_fio_opts *opts)
{
    get_common_verify_opts(vopts, opts);
    CHECK_RC(te_string_append(vopts, " --do_verify=0"));
    CHECK_RC(te_string_append(vopts, " --verify_state_save=1"));
    CHECK_RC(te_string_append(vopts, " --rw=randwrite"));
    opts->rwmixread = 100;
    opts->user = vopts->ptr;
}

static void
get_verification_opts(te_string *vopts, tapi_fio_opts *opts)
{
    get_common_verify_opts(vopts, opts);
    CHECK_RC(te_string_append(vopts, " --verify_state_load=1"));
    CHECK_RC(te_string_append(vopts, " --do_verify=1"));
    CHECK_RC(te_string_append(vopts, " --verify_fatal=1"));
    CHECK_RC(te_string_append(vopts, " --verify_dump=1"));
    CHECK_RC(te_string_append(vopts, " --rw=randread"));
    opts->rwmixread = 0;
    opts->user = vopts->ptr;
}

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
    tapi_nvme_target target;
    te_string verify_opts = TE_STRING_INIT;

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

    TEST_STEP("Read target options");
    tsapi_read_nvme_target_opts(&target, argc, argv);

    TEST_STEP("Setup target");
    CHECK_RC(tapi_nvme_target_init(&target, NULL));
    CHECK_RC(tapi_nvme_target_setup(&target));
    TEST_STEP("Setup initiator");
    CHECK_RC(tsapi_nvme_initiator_init(&initiator));
    CHECK_RC(tsapi_nvme_initiator_connect(&initiator, &target));
    CHECK_RC(tapi_nvme_initiator_list(&initiator.initiator));

    TEST_STEP("Read and apply interface options");
    tsapi_read_if_feature_opts(&sys_opts, argc, argv);
    tsapi_apply_if_feature_opts(&sys_opts, init->ta, init_if->if_name);
    tsapi_apply_if_feature_opts(&sys_opts, tgt->ta, tgt_if->if_name);

    TEST_STEP("Prepare FIO run");
    tsapi_read_fio_opts(&opts, argc, argv);
    opts.filename = initiator.initiator.device;
    fio = tapi_fio_create(&opts, init_rpcs);

    tsapi_set_fdlimits(init_rpcs);
    TEST_STEP("Start FIO as writter");

    get_writter_verify_opts(&verify_opts, &(fio->app.opts));
    CHECK_RC(tapi_fio_start(fio));
    CHECK_RC(tapi_fio_wait(fio, TAPI_FIO_TIMEOUT_DEFAULT));
    CHECK_RC(tapi_fio_stop(fio));

    TEST_STEP("Get FIO writter report and log it");
    CHECK_RC(tapi_fio_get_report(fio, &report));

    TEST_STEP("Start FIO for verificaion");
    get_verification_opts(&verify_opts, &(fio->app.opts));
    CHECK_RC(tapi_fio_start(fio));
    CHECK_RC(tapi_fio_wait(fio, TAPI_FIO_TIMEOUT_DEFAULT));
    CHECK_RC(tapi_fio_stop(fio));

    TEST_STEP("Get FIO report and log it");
    CHECK_RC(tapi_fio_get_report(fio, &report));

    tapi_fio_log_report(&report);

    TEST_SUCCESS;
cleanup:
    TEST_STEP("Disconnect the initiator and wipe target config.");
    te_string_free(&verify_opts);
    tapi_fio_destroy(fio);
    CLEANUP_CHECK_RC(tsapi_nvme_initiator_disconnect(&initiator));
    tapi_nvme_target_fini(&target);

    TEST_END;
}
