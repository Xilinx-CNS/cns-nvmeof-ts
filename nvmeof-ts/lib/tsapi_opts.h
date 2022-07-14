/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2018 - 2022 Xilinx, Inc. All rights reserved. */
/** @file
 * @brief NVMeoF Opts procesing
 *
 * @author Konstantin Ushakov <Konstantin.Ushakvo@oktetlabs.ru>
 */

#ifndef __TS_TSAPI_OPTS_H__
#define __TS_TSAPI_OPTS_H__

#include "tapi_fio.h"
#include "tapi_nvme.h"
#include "tapi_env.h"
#include "tapi_nvme_kern_target.h"
#include "tapi_nvme_onvme_target.h"

/** System opts */
typedef struct if_feature_opts {
    int mtu;        /**< Maximum transmission unit */
    te_bool tso;    /**< TCP segmentation offload enabled */
    te_bool lro;    /**< Large receive offload enabled */
} if_feature_opts;

typedef struct if_netem_opts {
    uint32_t delay;    /**< Delay on interface in usec */
    double loss;       /**< Loss on interface in percent [0-100]% */
    double corruption; /**< Corruption on interface in percent [0-100]% */
} if_netem_opts;

/** Default value initialization */
#define IF_FEATURE_OPTS_DEFAULTS ((struct if_feature_opts) { \
    .mtu = 1500,    \
    .tso = FALSE,   \
    .lro = FALSE,   \
})

/** Default netem options */
#define IF_NETEM_OPTS_DEFAULTS (struct if_netem_opts) { \
    .delay = 0,                                         \
    .loss = 0.0,                                        \
    .corruption = 0.0,                                  \
}

/**
 * Set option as required
 *
 * @param opts_name_    name in structure
 * @param name_         name in package.xml
 * @param type_         type of option
 * @param ...           additional args for test_get_*_param
 */
#define REQUIRED(opts_name_, name_, type_, ...)

/**
 * Set option as option
 *
 * @param opts_name_    name in structure
 * @param name_         name in package.xml
 * @param type_         type of option
 * @param ...           additional args for test_get_*_param
 */
#define OPTIONAL(opts_name_, name_, type_, ...)

/** List of support FIO parameters in TS */
#define FIO_PARAMETERS \
    REQUIRED(blocksize, blocksize, uint);           \
    REQUIRED(ioengine, ioengine, enum, IOEGINES);   \
    REQUIRED(rwtype, rwtype, enum, RWTYPES);        \
    REQUIRED(runtime_sec, runtime_sec, int);        \
    REQUIRED(iodepth, iodepth, uint);               \
    REQUIRED(numjobs, numjobs, fio_numjobs);        \
    REQUIRED(rwmixread, rwmixread, uint);           \
    OPTIONAL(name, name, string);                   \
    OPTIONAL(filename, filename, string);           \
    OPTIONAL(user, user, string);

/** List of support if features parameters in TS */
#define IF_FEATURE_PARAMETERS \
    REQUIRED(mtu, mtu, int);            \
    REQUIRED(tso, tso, enum, ONOFF);    \
    REQUIRED(lro, lro, enum, ONOFF);

/** List of support nvme parameters in TS */
#define NVME_PARAMETERS \
    OPTIONAL(transport, transport, enum, TRANSPORTS);   \
    OPTIONAL(subnqn, nqn, string);                      \
    OPTIONAL(nvmet_port, port, uint);

/** List of support netem parameters in TS */
#define IF_NETEM_PARAMETERS \
    REQUIRED(delay, if_delay_msec, uint); \
    REQUIRED(loss, if_loss_proc, double); \
    REQUIRED(corruption, if_corruption_prob, uint);

/**
 * Fill fio option form package.xml
 *
 * @param opts FIO options
 * @param argc Count of arguments
 * @param argv List of arguments
 */
extern void tsapi_read_fio_opts(tapi_fio_opts *opts, int argc, char **argv);

/**
 * Fill NVMe option form package.xml
 *
 * @param opts Target options
 * @param argc Count of arguments
 * @param argv List of arguments
 */
extern void tsapi_read_nvme_target_opts(tapi_nvme_target *opts,
                                        int argc, char **argv);


/**
 * Read NVMe target specific opts from package.xml
 *
 * @param target        Target options
 * @param specific_opts Specfific options for target
 * @param ifname        Interface name
 * @param argc          Count of arguments
 * @param argv          List of arguments
 */
extern void tsapi_read_nvme_target_specific(tapi_nvme_target *target,
                                            void **specific_opts,
                                            const char *ifname,
                                            int argc, char **argv);

/**
 * Fill features option from package.xml
 *
 * @param opts Features options
 * @param argc Count of arguments
 * @param argv List of arguments
 */
extern void tsapi_read_if_feature_opts(if_feature_opts *opts, int argc,
                                       char **argv);

/**
 * Apply features option to system
 *
 * @param opts      Features options
 * @param agent     TA name
 * @param ifname    Interface name
 */
extern void tsapi_apply_if_feature_opts(const if_feature_opts *opts,
                                        const char *agent,
                                        const char *ifname);

/**
 * Fill netem option from package.xml
 *
 * @param opts NetEm options
 * @param argc Count of arguments
 * @param argv List of arguments
 */
extern void tsapi_read_if_netem_opts(if_netem_opts *opts, int argc,
                                     char **argv);

/**
 * Apply netem option to system
 *
 * @param opts NetEm options
 * @param agent     TA name
 * @param ifname    Interface name
 */
extern void tsapi_apply_if_netem_opts(const if_netem_opts *opts,
                                      const char *agent,
                                      const char *ifname);

/**
 * Check that target is ONVMe
 *
 * @param       target for check
 *
 * @return result of check
 */
extern te_bool tsapi_is_onvme_target(const tapi_nvme_target *target);


/**
 * Add for fio wrapper for collection throughput
 *
 * @param opts      fio options structure
 * @param ifname    interface name
 */
extern void tsapi_apply_fio_opts_throughput(tapi_fio_opts *opts,
                                            const char *ifname);

/**
 * Add for fio wrapper for collection latency
 *
 * @param opts      fio options structure
 * @param ifname    interface name
 */
extern void tsapi_apply_fio_opts_latency(tapi_fio_opts *opts,
                                         const char *ifname);

/**
 * Add for fio wrapper for run under numactl or taskset
 *
 * @param opts      fio options structure
 * @param ifname    interface name
 */
extern void tsapi_apply_perf_profiles_fio_opts(tapi_fio_opts *opts,
                                               const char *ifname,
                                               int argc, char **argv);

tapi_env_foreach_if_fn tsapi_env_apply_if_feature_opts;

#endif /* !__TS_NVMEOF_TEST_H__ */
