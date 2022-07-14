/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2018 - 2022 Xilinx, Inc. All rights reserved. */
/** @file
 * @brief NVMeoF Test Suite
 *
 * Collecting system statistic
 *
 * @author Nikita Somenkov <Nikita.Somenkov@oktetlabs.ru>
 */

#include "te_defs.h"
#include "tapi_test.h"
#include "te_sleep.h"
#include "tapi_job.h"
#include "tsapi_sysstat.h"

#define RC(_expr)               \
    do {                        \
        te_errno rc;            \
        if ((rc = _expr) != 0)  \
            return rc;          \
    } while(0)

struct sysstat_command {
    const char **command;
    const char *regexp;
    const char *name;
};

static te_errno
sysstat_command_exec(struct sysstat_command *cmd, rcf_rpc_server *rpcs,
                     tsapi_sysstat *stat)
{
    tapi_job_channel_t *filter;
    tapi_job_channel_t *channels[2];

    RC(tapi_job_factory_rpc_create(rpcs, &stat->factory));
    RC(tapi_job_create(stat->factory, NULL, cmd->command[0],
                       cmd->command, NULL, &stat->job));
    RC(tapi_job_alloc_output_channels(stat->job, 2, channels));
    RC(tapi_job_attach_filter(TAPI_JOB_CHANNEL_SET(channels[0]),
                              cmd->name, FALSE, TE_LL_WARN, &filter));
    RC(tapi_job_attach_filter(TAPI_JOB_CHANNEL_SET(channels[1]),
                              "stderr-filter", FALSE, TE_LL_ERROR, NULL));
    RC(tapi_job_filter_add_regexp(filter, cmd->regexp, 0));
    RC(tapi_job_start(stat->job));

    return 0;
}

static te_errno
sysstat_command_stop(tsapi_sysstat *stat)
{
    size_t i;
    te_errno rc;
    const size_t max_attemps = 2;
    tapi_job_status_t status;

    RC(tapi_job_kill(stat->job, SIGINT));
    for (i = 0; i < max_attemps; i++)
    {
        te_motivated_sleep(1, "waiting while statistics will be collected");
        if ((rc = tapi_job_wait(stat->job, 1, &status)) == TE_EINPROGRESS)
            continue;
        else if (rc != 0)
            return rc;
        break;
    }
    RC(tapi_job_destroy(stat->job, 0));
    tapi_job_factory_destroy(stat->factory);

    return 0;
}

te_errno
sysstat_command_type2command(tsapi_sysstat_stat_type type, struct sysstat_command *command)
{
    switch (type) {
        case TSAPI_SYSSTAT_STAT_CPU:
            command->name = "average-cpu";
            command->command = (const char *[4]) { "sar", "-u", "1", NULL };
            command->regexp = "^Avarage:(.+)";
            break;
        case TSAPI_SYSSTAT_STAT_IO:
            command->name = "average-io";
            command->command = (const char *[5]) { "sar", "-d", "-p", "1", NULL };
            command->regexp = "^Avarage:(.+)";
            break;
        default:
            return TE_EINVAL;
    }
    return 0;
}

te_errno
tsapi_sysstat_start_all(tsapi_sysstat *stat, size_t count, rcf_rpc_server *rpcs)
{
    size_t i;
    te_errno rc;

    for (i = 0; i < count; i++)
    {
        struct sysstat_command cmd;

        if ((rc = sysstat_command_type2command(stat[i].type, &cmd)) != 0)
            return rc;

        if ((rc = sysstat_command_exec(&cmd, rpcs, stat + i)) != 0)
            return rc;
    }

    return 0;
}

te_errno
tsapi_sysstat_stop_all(tsapi_sysstat *stat, size_t count)
{
    size_t i;
    te_errno rc;

    for (i = 0; i < count; i++)
    {
        if ((rc = sysstat_command_stop(stat + i)) != 0)
            return rc;

        stat[i].job = NULL;
    }

    return 0;
}
