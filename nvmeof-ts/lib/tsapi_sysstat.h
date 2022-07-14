/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2018 - 2022 Xilinx, Inc. All rights reserved. */
/** @file
 * @brief NVMeoF Test Suite
 *
 * Collecting system statistic
 *
 * @author Nikita Somenkov <Nikita.Somenkov@oktetlabs.ru>
 */

#ifndef __TSAPI_SYSSTAT_H__
#define __TSAPI_SYSSTAT_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "te_defs.h"
#include "te_errno.h"
#include "te_vector.h"
#include "te_string.h"
#include "rcf_rpc.h"
#include "tapi_job_factory_rpc.h"

struct tapi_job_t;

/** Avaible types for collection system statistic */
typedef enum tsapi_sysstat_stat_type {
    TSAPI_SYSSTAT_STAT_CPU,         /**< CPU statistics */
    TSAPI_SYSSTAT_STAT_IO,          /**< IO statistics */
} tsapi_sysstat_stat_type;

/** System statistic handle */
typedef struct tsapi_sysstat {
    tsapi_sysstat_stat_type type;           /**< Type of statistic */
    struct tapi_job_t *job;                 /**< Job for collecting statistic */
    struct tapi_job_factory_t *factory;     /**< Job factory */
} tsapi_sysstat;

/** Shortcut for creating instances of tsapi_sysstat */
#define TSAPI_SYSSTAT_INIT(_type) (struct tsapi_sysstat) {  \
    .type = TSAPI_SYSSTAT_STAT_##_type,                     \
    .job = NULL,                                            \
}

/**
 * Start collecting statistic (array version)
 *
 * @param stat      Array of system statistic handles
 * @param count     Count element of array @p stat
 * @param rpcs      RPC server
 *
 * @return Status code
 */
extern te_errno tsapi_sysstat_start_all(tsapi_sysstat *stat, size_t count,
                                        rcf_rpc_server *rpcs);

/**
 * Start collecting statistic
 *
 * @param stat      System statistic handles
 * @param rpcs      RPC server
 *
 * @return Status code
 */
static inline te_errno
tsapi_sysstat_start(tsapi_sysstat *stat, rcf_rpc_server *rpcs)
{
    return tsapi_sysstat_start_all(stat, 1, rpcs);
}

/**
 * Stop collecting statistic (array version)
 *
 * @param stat      Array of system statistic handles
 * @param count     Count element of array @p stat
 *
 * @return Status code
 */
extern te_errno tsapi_sysstat_stop_all(tsapi_sysstat *stat, size_t count);

/**
 * Stop collecting statistic
 *
 * @param stat      System statistic handles
 *
 * @return Status code
 */
static inline te_errno
tsapi_sysstat_stop(tsapi_sysstat *stat)
{
    return tsapi_sysstat_stop_all(stat, 1);
}

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* __TSAPI_SYSSTAT_H__ */
