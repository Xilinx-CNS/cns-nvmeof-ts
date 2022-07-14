/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2018 - 2022 Xilinx, Inc. All rights reserved. */
/** @file
 * @brief Options processing suite
 *
 * Options processing code
 *
 * @author Nikita Somenkov <Nikita.Somenkov@oktetlabs.ru>
 */

#define TE_LGR_USER     "TS Options processing/handling"

#include "nvmeof_test.h"
#include "tapi_cfg_eth.h"
#include "tapi_cfg_base.h"
#include "tapi_cfg_netem.h"
#include "tapi_cfg_qdisc.h"

#define CREATE_MAPPING_LIST(list_...) \
    , ((struct param_map_entry[]) {list_, {NULL, 0}})

#define RWTYPES \
    CREATE_MAPPING_LIST(TAPI_FIO_RWTYPE_MAPPING_LIST)

#define IOEGINES \
    CREATE_MAPPING_LIST(TAPI_FIO_IOENGINE_MAPPING_LIST)

#define ONOFF \
    CREATE_MAPPING_LIST({"on", 1}, {"off", 0})

#define TRANSPORTS \
    CREATE_MAPPING_LIST(TAPI_NVME_TRANSPORT_MAPPING_LIST)

#undef OPTIONAL
#undef REQUIRED

#define OPTIONAL(opts_param_, param_, type_, ...) \
    if (TEST_HAS_PARAM(param_))                         \
        opts->opts_param_ = test_get_##type_##_param(   \
            argc, argv, #param_ __VA_ARGS__);

#define REQUIRED(opts_param_, param_, type_, ...) \
    OPTIONAL(opts_param_, param_, type_, __VA_ARGS__)     \
    else TEST_FAIL("Parameter %s required", #param_);

static int
shuffled_blocksize(int base_blocksize)
{
    /* Randomizes from 512 to 512 * 7 = 4k */
    const unsigned min_bs = 512;
    const unsigned max_mult = 7;

    int mult = 1 + rand() % (max_mult - 1);

    return base_blocksize + min_bs * mult;
}

/* See description in nvmeof_test.h */
void
tsapi_read_fio_opts(tapi_fio_opts *opts, int argc, char **argv)
{
    te_bool is_shuffle_blocksize = FALSE;

    FIO_PARAMETERS;

    if (TEST_HAS_PARAM(is_shuffle_blocksize))
        TEST_GET_BOOL_PARAM(is_shuffle_blocksize);

    if (is_shuffle_blocksize)
    {
        int new_blocksize = shuffled_blocksize(opts->blocksize);
        RING("Shuffled blocksize enabled, now we will use blocksize: %d",
             new_blocksize);
        opts->blocksize = new_blocksize;
    }

    opts->rand_gen = "tausworthe64";
}

void
tsapi_apply_fio_opts_throughput(tapi_fio_opts *opts, const char *ifname)
{
    te_string fio_prefix = TE_STRING_INIT;

    CHECK_RC(te_string_append(&fio_prefix, "numactl -N netdev:%s -m netdev:%s",
                              ifname, ifname));

    RING("For collect throughput we will run fio under %s", fio_prefix.ptr);

    if (opts->numjobs.value < 16 && opts->numjobs.factor == TAPI_FIO_NUMJOBS_WITHOUT_FACTOR)
        opts->prefix = fio_prefix.ptr;
    else
        opts->prefix = NULL;
}

void
tsapi_apply_fio_opts_latency(tapi_fio_opts *opts, const char *ifname)
{
    te_string fio_prefix = TE_STRING_INIT;

    CHECK_RC(te_string_append(&fio_prefix,
        "taskset -c `cat /sys/class/net/%s/device/local_cpulist"
        " | cut -d ',' -d '-' -f1` ", ifname));

    RING("For collect latency we will run fio under %s", fio_prefix.ptr);

    opts->prefix = fio_prefix.ptr;
}

void
tsapi_apply_perf_profiles_fio_opts(tapi_fio_opts *opts, const char *ifname,
                                   int argc, char **argv)
{
    const char *perf_type = NULL;

    if (!TEST_HAS_PARAM(perf_type))
        return;

    TEST_GET_STRING_PARAM(perf_type);
    if (strcmp(perf_type, "latency") == 0)
        tsapi_apply_fio_opts_latency(opts, ifname);
    else if (strcmp(perf_type, "throughput") == 0)
        tsapi_apply_fio_opts_throughput(opts, ifname);
}

/* See description in nvmeof_test.h */
void
tsapi_read_nvme_target_opts(tapi_nvme_target *opts, int argc, char **argv)
{
    NVME_PARAMETERS;

    /* first device by default */
    opts->device = tsapi_agent_get_device(opts->rpcs->ta, 0);
    opts->subnqn = tsapi_agent_get_nqn(opts->rpcs->ta, opts->device);
    if (opts->subnqn == NULL || strcmp(opts->subnqn, "") == 0)
    {
        opts->subnqn = "te_testing";
    }
}

static void
read_onvme_target_opts(tapi_nvme_target *target,
                       tapi_nvme_onvme_target_opts *opts,
                       const char *ifname, int argc, char **argv)
{
    const char *onvme_cores;
    const char *max_worker_conn;

    assert(opts);

    *opts = TAPI_NVME_ONVME_TARGET_OPTS_DEFAULTS;
    if (TEST_HAS_PARAM(max_worker_conn))
    {
        TEST_GET_STRING_PARAM(max_worker_conn);
        if (strcmp(max_worker_conn, "default") != 0)
        {
            REQUIRED(max_worker_conn, onvme_max_worker_conn, int);
        }
    }
    if (TEST_HAS_PARAM(onvme_cores))
    {
        TEST_GET_STRING_PARAM(onvme_cores);
        if (strcmp(onvme_cores, "default") != 0)
        {
            CHECK_RC(te_string_append(&opts->cores, "%s", onvme_cores));
        }
        else if (strcmp(onvme_cores, "numa-local") == 0)
        {
            CHECK_RC(te_string_append(&opts->cores, "netdev:%s", ifname));
        }
    }

    if (strstr(target->device, "nullb") != NULL)
        opts->is_nullblock = TRUE;
}

/* FIXME: We should check over variable... */
te_bool
tsapi_is_onvme_target(const tapi_nvme_target *target)
{
    static const tapi_nvme_target_methods onvme_methods =
        TAPI_NVME_ONVME_TARGET_METHODS;

    return target->methods.cleanup == onvme_methods.cleanup &&
           target->methods.setup == onvme_methods.setup &&
           target->methods.fini == onvme_methods.fini &&
           target->methods.init == onvme_methods.init;
}

/* See description in nvmeof_test.h */
void
tsapi_read_nvme_target_specific(tapi_nvme_target *target, void **specific_opts,
                                const char *ifname, int argc, char **argv)
{
    if (tsapi_is_onvme_target(target))
    {
        tapi_nvme_onvme_target_opts *opts = TE_ALLOC(
            sizeof(tapi_nvme_onvme_target_opts));

        read_onvme_target_opts(target, opts, ifname, argc, argv);
        *specific_opts = opts;
    }
}

/* See description in nvmeof_test.h */
void
tsapi_read_if_feature_opts(if_feature_opts *opts, int argc, char **argv)
{
    IF_FEATURE_PARAMETERS;
}

/* See description in nvmeof_test.h */
void
tsapi_apply_if_feature_opts(const if_feature_opts *opts, const char *agent,
                            const char *ifname)
{
    size_t i = 0;
    const char *tso[] = {
        "tx-tcp-segmentation",
        "tx-tcp-ecn-segmentation",
        "tx-tcp6-segmentation",
        "tx-tcp-mangleid-segmentation"
    };

    CHECK_RC(tapi_cfg_base_if_set_mtu(agent, ifname, opts->mtu, NULL));
    for (i = 0; i < TE_ARRAY_LEN(tso); i++)
        CHECK_RC(tapi_eth_feature_set(agent, ifname, tso[i], opts->lro));
    CHECK_RC(tapi_eth_feature_set(agent, ifname, "rx-lro", opts->lro));
}

/* See description in nvmeof_test.h */
void
tsapi_read_if_netem_opts(if_netem_opts *opts, int argc, char **argv)
{
    IF_NETEM_PARAMETERS;
}

/* See description in nvmeof_test.h */
void
tsapi_apply_if_netem_opts(const if_netem_opts *opts, const char *agent,
                          const char *ifname)
{
    CHECK_RC(tapi_cfg_qdisc_disable(agent, ifname));

    CHECK_RC(tapi_cfg_qdisc_set_kind(agent, ifname,
                                     TAPI_CFG_QDISC_KIND_NETEM));

    CHECK_RC(tapi_cfg_netem_set_delay(agent, ifname, TE_MS2US(opts->delay)));
    CHECK_RC(tapi_cfg_netem_set_loss(agent, ifname, opts->loss));
    CHECK_RC(tapi_cfg_netem_set_corruption_probability(
        agent, ifname, opts->corruption));

    CHECK_RC(tapi_cfg_qdisc_enable(agent, ifname));
}

void
tsapi_env_apply_if_feature_opts(tapi_env_if *iface, void *opaque)
{
    if_feature_opts *opts = (if_feature_opts *)opaque;

    tsapi_apply_if_feature_opts(opts, iface->host->ta,
                                iface->if_info.if_name);
}
