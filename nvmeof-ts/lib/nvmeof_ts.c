/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2018 - 2022 Xilinx, Inc. All rights reserved. */
/** @file
 * @brief NVMeoF Test Suite
 *
 * Implementation of common functions.
 *
 * @author Konstantin Ushakov <Konstantin.Ushakov@oktetlabs.ru>
 */

#define TE_LGR_USER     "Library"

#include "nvmeof_test.h"
#include "tapi_cfg_base.h"
#include "tapi_cfg_eth.h"
#include "tapi_test.h"

#define FDLIMIT_CUR (4096)
#define FDLIMIT_MAX (8192)

char * tsapi_agent_get_device(const char *agent, unsigned number)
{
    unsigned n_dev = 0;
    cfg_handle *devs = NULL;
    cfg_handle dev_inst;
    cfg_oid *oid;

    CHECK_RC(cfg_find_pattern_fmt(&n_dev, &devs,
                                  "/agent:%s/block:*", agent));

    if (number + 1> n_dev)
        TEST_FAIL("Got %d devices on agent=%s, but you're asking for "
                  "instance number %d",
                  n_dev, agent, number + 1);

    dev_inst = devs[number];
    CHECK_RC(cfg_get_oid(dev_inst, &oid));
    return te_string_fmt("/dev/%s", CFG_OID_GET_INST_NAME(oid, 2));
}

char * tsapi_agent_get_nqn(const char *agent, const char *dev)
{
    te_errno rc;
    cfg_val_type val_type = CVT_STRING;
    const char *device = dev + strlen("/dev/");
    char *value;

    rc = cfg_get_instance_fmt(&val_type, &value, "/local:%s/nvme:%s/nqn:",
                              agent, device);

    return rc == 0 ? value: "te_testing";
}

te_errno
tsapi_set_fdlimits(rcf_rpc_server *rpcs)
{
    struct tarpc_rlimit fdlimits;
    fdlimits.rlim_cur = FDLIMIT_CUR;
    fdlimits.rlim_max = FDLIMIT_MAX;

    if (rpc_setrlimit(rpcs, RPC_RLIMIT_NOFILE, &fdlimits) == -1)
        TEST_FAIL("Error while setrlimit on TA: %s", rpcs->ta);

    return 0;
}

tapi_nvme_target
tsapi_get_target_ta(const char *ta)
{
    te_errno rc;
    cfg_val_type val_type = CVT_STRING;
    char *target_type;

    rc = cfg_get_instance_fmt(&val_type, &target_type, "/local:%s/nvme:/tgt_type:", ta);
    if (ta != NULL && rc == 0)
    {
        if (strcmp(target_type, "kernel") == 0)
            return TAPI_NVME_KERN_TARGET;
        if (strcmp(target_type, "onvme") == 0)
            return TAPI_NVME_ONVME_TARGET;

        TEST_FAIL("Target type '%s' is not supported on agent `%s`",
                  target_type, ta);
    }
    else
    {
        TEST_FAIL("Cannot get target type on agent '%s', rc=%r", ta, rc);
    }

    /* cant't be reached */
    return TAPI_NVME_KERN_TARGET;
}

static te_bool
local_cfg_get_nvme_connect_opt(const char *ta, const char *name)
{
    cfg_val_type val_type = CVT_STRING;
    char *value;
    te_bool result;

    CHECK_RC(cfg_get_instance_fmt(&val_type, &value,
                                  "/local:%s/nvme:/connect:%s", ta, name));

    if (strcmp(value, "random") == 0)
        result = rand() % 2 == 0? FALSE: TRUE;
    else if (strcmp(value, "1") == 0)
        result = TRUE;
    else if (strcmp(value, "0") == 0)
        result = FALSE;
    else
    {
        TEST_FAIL("Cannot parse value %s of /local:%s/nvme:/connect:%s",
                  value, ta, name);
    }

    RING("Test run with %s = %d on ta '%s'", name, result, ta);

    return result;
}

void
tsapi_get_nvme_connect_opts(const char *ta, tapi_nvme_connect_opts *opts)
{
    assert(ta);
    assert(opts);

    opts->hdr_digest = local_cfg_get_nvme_connect_opt(ta, "hdr_digest");
    opts->data_digest = local_cfg_get_nvme_connect_opt(ta, "data_digest");
    opts->duplicate_connection = FALSE;
}

te_errno
tsapi_nvme_initiator_init(tsapi_nvme_initiator *initiator)
{
    return rcf_rpc_server_create_process(initiator->initiator.rpcs,
                                         "disconnect_rpcs", /*flags*/ 0,
                                         &initiator->disconnect_rpcs);
}

te_errno
tsapi_nvme_initiator_connect(tsapi_nvme_initiator *initiator,
                             const tapi_nvme_target *target)
{
    assert(initiator);
    return tapi_nvme_initiator_connect(&initiator->initiator, target);
}

te_errno
tsapi_nvme_initiator_connect_opts(tsapi_nvme_initiator *initiator,
                                  const tapi_nvme_target *target,
                                  const tapi_nvme_connect_opts *opts)
{
    assert(initiator);
    return tapi_nvme_initiator_connect_opts(&initiator->initiator, target, opts);
}

te_errno
tsapi_nvme_initiator_disconnect(tsapi_nvme_initiator *initiator)
{
    te_errno rc;
    rcf_rpc_server *old_rpcs = initiator->initiator.rpcs;
    static const unsigned wating_disconnect = TE_SEC2MS(5 * 60);

    assert(initiator);

    if (initiator->disconnect_rpcs)
    {
        initiator->initiator.rpcs = initiator->disconnect_rpcs;
        initiator->initiator.rpcs->def_timeout = wating_disconnect;
    }
    else
    {
        RING("There are no disconnect_rpcs available for %s."
             " Using default one.",
             initiator->initiator.admin_dev);
    }

    if ((rc = tapi_nvme_initiator_disconnect(&initiator->initiator)) != 0)
        return rc;

    /* Back state of initiator */
    initiator->initiator.rpcs = old_rpcs;

    return 0;
}

te_errno
tsapi_nvme_initiator_fini(tsapi_nvme_initiator *initiator)
{
    assert(initiator);

    return rcf_rpc_server_destroy(initiator->disconnect_rpcs);
}
