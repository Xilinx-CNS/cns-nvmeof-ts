/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2018 - 2022 Xilinx, Inc. All rights reserved. */
/*
 * NVMoF Test Suite
 * Command test
 */

/** @page conf-list Check that list command works
 *
 * @objective Check that list command works in parallel with traffic
 *
 * See @ref list-xml for iteration details.
 *
 * @type perf
 *
 *
 * @author Nikita Somenkov <Nikita.Somenkov@oktetlabs.ru>
 *
 * @par Scenario:
 */

#define TE_TEST_NAME  "conf/list"

#include "nvmeof_test.h"

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
    tapi_fio_report report;
    tsapi_nvme_initiator initiator = TSAPI_NVME_INITIATOR;
    tapi_nvme_target target;
    unsigned i, repeat;

    TEST_START;
    TEST_GET_HOST(init);
    TEST_GET_HOST(tgt);
    TEST_GET_PCO(init_rpcs);
    TEST_GET_PCO(tgt_rpcs);
    TEST_GET_ADDR(init_rpcs, tgt_addr);
    TEST_GET_IF(init_if);
    TEST_GET_IF(tgt_if);
    TEST_GET_INT_PARAM(repeat);

    target = tsapi_get_target_ta(tgt->ta);
    target.rpcs = tgt_rpcs;
    target.addr = tgt_addr;
    initiator.initiator.rpcs = init_rpcs;

    tsapi_read_nvme_target_opts(&target, argc, argv);
    CHECK_RC(tapi_nvme_target_init(&target, NULL));
    CHECK_RC(tapi_nvme_target_setup(&target));
    CHECK_RC(tsapi_nvme_initiator_init(&initiator));
    CHECK_RC(tsapi_nvme_initiator_connect(&initiator, &target));

    tsapi_read_if_feature_opts(&sys_opts, argc, argv);
    tsapi_apply_if_feature_opts(&sys_opts, init->ta, init_if->if_name);
    tsapi_apply_if_feature_opts(&sys_opts, tgt->ta, tgt_if->if_name);

    tsapi_read_fio_opts(&opts, argc, argv);
    opts.filename = initiator.initiator.device;
    fio = tapi_fio_create(&opts, init_rpcs);

    CHECK_RC(tapi_fio_start(fio));
    for (i = 0; i < repeat; i++)
       CHECK_RC(tapi_nvme_initiator_list(&initiator.initiator));

    CHECK_RC(tapi_fio_wait(fio, TAPI_FIO_TIMEOUT_DEFAULT));
    CHECK_RC(tapi_fio_stop(fio));

    CHECK_RC(tapi_fio_get_report(fio, &report));

    tapi_fio_log_report(&report);

    TEST_SUCCESS;
cleanup:
    tapi_fio_destroy(fio);
    CLEANUP_CHECK_RC(tsapi_nvme_initiator_disconnect(&initiator));
    tapi_nvme_target_fini(&target);
    TEST_END;
}
