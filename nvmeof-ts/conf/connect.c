/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2018 - 2022 Xilinx, Inc. All rights reserved. */
/*
 * NVMoF Test Suite
 * Command test
 */

/** @page conf-connect Check that nvme connect/disconnect command works
 *
 * @objective Check that nvme connect/disconnect command works
 *
 * See @ref list-xml for iteration details.
 *
 * @type perf
 *
 * @author Nikita Somenkov <Nikita.Somenkov@oktetlabs.ru>
 *
 * @par Scenario:
 */

#define TE_TEST_NAME  "conf/connect"

#include "nvmeof_test.h"

#define DELAY_AFTER_NVME_CONNECT_MAX    (10)

enum nvme_connect_type {
    NVME_CONNECT,
    NVME_CONNECT_ALL,
};

#define NVME_CONNECT_TYPE_MAPPING_LIST      \
    {"nvme-connect", NVME_CONNECT},         \
    {"nvme-connect-all", NVME_CONNECT_ALL}


typedef te_errno (*nvme_connect_function)(tapi_nvme_host_ctrl *host_ctrl,
                                          const tapi_nvme_target *target,
                                          const tapi_nvme_connect_opts *opts);
typedef te_errno (*nvme_disconnect_function)(tapi_nvme_host_ctrl *host_ctrl);

static te_errno
nvme_disconnect_all(tapi_nvme_host_ctrl *ctrl)
{
    te_errno rc;

    /* First, make sure that tapi_nvme_host_ctrl
       disconnect from connected target */
    if ((rc = tapi_nvme_initiator_disconnect(ctrl)) == 0)
        return tapi_nvme_initiator_disconnect_all(ctrl->rpcs);

    return rc;
}

static nvme_connect_function
nvme_connect_type2func(enum nvme_connect_type type)
{
    switch (type)
    {
        case NVME_CONNECT:
            return tapi_nvme_initiator_connect_opts;
        case NVME_CONNECT_ALL:
            return tapi_nvme_initiator_connect_all_opts;
    }

    return tapi_nvme_initiator_connect_all_opts;
}

static nvme_disconnect_function
nvme_disconnect_type2func(enum nvme_connect_type type)
{
    switch (type)
    {
        case NVME_CONNECT:
            return tapi_nvme_initiator_disconnect;
        case NVME_CONNECT_ALL:
            return nvme_disconnect_all;
    }

    return tapi_nvme_initiator_disconnect;
}

int
main(int argc, char *argv[])
{
    rcf_rpc_server *init_rpcs = NULL;
    rcf_rpc_server *tgt_rpcs = NULL;
    const struct sockaddr *tgt_addr;
    const struct if_nameindex *init_if, *tgt_if;
    tapi_env_host *init, *tgt;
    tapi_nvme_host_ctrl initiator = TAPI_NVME_HOST_CTRL_DEFAULTS;
    tapi_nvme_target target;
    tapi_nvme_connect_opts nc_opts;
    int repeat = 1, i;
    enum nvme_connect_type connect_type;
    nvme_connect_function nvme_connect = NULL;
    nvme_disconnect_function nvme_disconnect = NULL;
    const char *use_tgt_addr;
    struct sockaddr_in copy_tgt_addr;
    void *target_opts = NULL;

    TEST_START;
    TEST_GET_HOST(init);
    TEST_GET_HOST(tgt);
    TEST_GET_PCO(init_rpcs);
    TEST_GET_PCO(tgt_rpcs);
    TEST_GET_ADDR(init_rpcs, tgt_addr);
    TEST_GET_IF(init_if);
    TEST_GET_IF(tgt_if);
    TEST_GET_INT_PARAM(repeat);
    TEST_GET_ENUM_PARAM(connect_type, NVME_CONNECT_TYPE_MAPPING_LIST);
    TEST_GET_STRING_PARAM(use_tgt_addr);

    nvme_connect = nvme_connect_type2func(connect_type);
    nvme_disconnect = nvme_disconnect_type2func(connect_type);

    target = tsapi_get_target_ta(tgt->ta);
    copy_tgt_addr = *SIN(tgt_addr);
    if (strcmp("wildcard", use_tgt_addr) == 0)
        te_sockaddr_set_wildcard(SA(&copy_tgt_addr));
    target.addr = SA(&copy_tgt_addr);
    target.rpcs = tgt_rpcs;
    memcpy(&copy_tgt_addr, tgt_addr, sizeof(copy_tgt_addr));
    initiator.rpcs = init_rpcs;

    TEST_STEP("Read target options");
    tsapi_read_nvme_target_opts(&target, argc, argv);
    tsapi_read_nvme_target_specific(&target, &target_opts, tgt_if->if_name,
                                    argc, argv);

    TEST_STEP("Setup target");
    CHECK_RC(tapi_nvme_target_init(&target, target_opts));
    CHECK_RC(tapi_nvme_target_setup(&target));

    target.addr = tgt_addr;
    TEST_STEP("Read nvme connect opts");
    tsapi_get_nvme_connect_opts(init->ta, &nc_opts);

    for (i = 0; i < repeat; i++)
    {
        int delay = rand() % DELAY_AFTER_NVME_CONNECT_MAX;
        TEST_SUBSTEP("[%d/%d] Connect to target", i + 1, repeat);
        CHECK_RC(nvme_connect(&initiator, &target, &nc_opts));
        te_motivated_sleep(delay, "Waiting for disconnect");
        TEST_SUBSTEP("[%d/%d] Disconnect to target", i + 1, repeat);
        CHECK_RC(nvme_disconnect(&initiator));
    }

    TEST_SUCCESS;

cleanup:
    TEST_STEP("Disconnect the initiator and wipe target config.");
    CLEANUP_CHECK_RC(nvme_disconnect(&initiator));
    tapi_nvme_target_fini(&target);
    free(target_opts);
    TEST_END;
}
