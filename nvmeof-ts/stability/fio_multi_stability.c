/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2018 - 2022 Xilinx, Inc. All rights reserved. */
/*
 * NVMoF Test Suite
 * Stability testing (multinode version)
 */

/** @page stability-fio_multi_stability FIO tests over bad link
 *
 * @objective Run FIO over a bad link that looses/corrupts the frames
 *
 * @type fio_multi_stability
 *
 * @ref fio_multi_stability-xml for interation details
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

#define TE_TEST_NAME  "stability/fio_multi_stability"

#include "nvmeof_test.h"
#include "tapi_cfg_qdisc.h"
#include "tapi_cfg_netem.h"


#define APPLY_NETEM_FOR_EACH_INITIATOR                              \
    do {                                                            \
        tsapi_for_each_initiator(initiator)                         \
        {                                                           \
            tsapi_apply_if_netem_opts(&netem_opts,                  \
                                      initiator->rpcs->ta,          \
                                      initiator->iface->if_name);   \
        }                                                           \
    } while(0)

int
main(int argc, char *argv[])
{
    tapi_nvme_target *target = NULL;
    tsapi_initiator *initiator = NULL;
    tapi_nvme_host_ctrl *host_ctrl = NULL;

    tapi_fio *fio = NULL;
    tapi_fio_report report;

    tapi_fio_opts opts = TAPI_FIO_OPTS_DEFAULTS;
    if_feature_opts sys_opts = IF_FEATURE_OPTS_DEFAULTS;
    if_netem_opts netem_opts = IF_NETEM_OPTS_DEFAULTS;

    unsigned ep;

    te_bool apply_netem_before_nvme_connect;

    TEST_START;

    apply_netem_before_nvme_connect = (rand() % 2 == 0 ? TRUE: FALSE);
    TEST_STEP("We will plan apply netem opts %s nvme connect",
              apply_netem_before_nvme_connect ? "before": "after");

    tsapi_read_if_netem_opts(&netem_opts, argc, argv);

    TEST_STEP("Read target options.");
    for (ep = 0; ep < TEST_NVME_MAX_HOSTS; ep++)
    {
        TEST_GET_INITIATOR(initiator, ep);
        TEST_GET_TARGET(target, ep, TAPI_NVME_KERN_TARGET);
    }

    TEST_STEP("Read and apply netem opts");
    if (apply_netem_before_nvme_connect)
        APPLY_NETEM_FOR_EACH_INITIATOR;

    TEST_STEP("Setup all targets described in the environment");
    tsapi_for_each_target(target)
    {
        tapi_nvme_target_init(target, NULL);
        tsapi_read_nvme_target_opts(target, argc, argv);
        CHECK_RC(tapi_nvme_target_setup(target));
    }

    TEST_STEP("Connecting all initiators to all targets (fixme)");
    tsapi_for_each_initiator(initiator)
    {
        tsapi_for_each_target(target)
        {
            TEST_SUBSTEP("For each init->tgt connection create a separate "
                         "RPC server process and issue a connect command.");
            host_ctrl = tsapi_initiator_add_host_ctrl(initiator,
                                                      target->rpcs->ta);
            CHECK_RC(tapi_nvme_initiator_connect(host_ctrl, target));
        }
    }

    if (!apply_netem_before_nvme_connect)
        APPLY_NETEM_FOR_EACH_INITIATOR;

    TEST_STEP("Load and apply interfaces configuration. "
              "Same on all interfaces.");
    tsapi_read_if_feature_opts(&sys_opts, argc, argv);
    tapi_env_foreach_if(&env, tsapi_env_apply_if_feature_opts, &sys_opts);

    TEST_STEP("Creating FIO RPC servers and structures");
    tsapi_read_fio_opts(&opts, argc, argv);
    tsapi_for_each_initiator(initiator)
    {
        tsapi_for_each_host_ctrl(initiator, host_ctrl)
        {
            TEST_SUBSTEP("For each initiator/targe prepare RPC server to "
                         "start FIO and FIO app");
            opts.filename = host_ctrl->device;
            tsapi_fio_add(tapi_fio_create(&opts, host_ctrl->rpcs));
        }
    }

    TEST_STEP("Starting FIO");
    tsapi_for_each_fio(fio)
    {
        CHECK_RC(tapi_fio_start(fio));
    }

    TEST_STEP("Waiting FIO to complete");
    tsapi_for_each_fio(fio)
    {
        CHECK_RC(tapi_fio_wait(fio, TAPI_FIO_TIMEOUT_DEFAULT));
    }

    TEST_STEP("Get FIO report (and stop if not complete)");
    tsapi_for_each_fio(fio)
    {
        CHECK_RC(tapi_fio_stop(fio));
        CHECK_RC(tapi_fio_get_report(fio, &report));
        tapi_fio_log_report(&report);
    }

    TEST_SUCCESS;
cleanup:
#if 0                           /* segfaults */
    tsapi_for_each_fio(fio)
    {
        tapi_fio_destroy(fio);
    }
#endif
    tsapi_for_each_initiator_and_host_ctrl(initiator, host_ctrl)
    {
        tapi_nvme_initiator_disconnect(host_ctrl);
    }

    TEST_STEP("Targets cleanup");
    tsapi_for_each_target(target)
    {
        tapi_nvme_target_fini(target);
    }

    TEST_END;
}
