/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2018 - 2022 Xilinx, Inc. All rights reserved. */
/** @file
 * @brief NVMeoF Test Suite
 *
 * @author Konstantin Ushakov <Konstantin.Ushakvo@oktetlabs.ru>
 */

#ifndef __TS_NVMEOF_TEST_H__
#define __TS_NVMEOF_TEST_H__

#include "te_config.h"
#include "te_kvpair.h"
#include "te_string.h"
#include "te_log_stack.h"
#include "tsapi_opts.h"
#include "tsapi_sysstat.h"

/**
 * Maximum number of initiators/targets in the environment (not cumulative).
 */
#define TEST_NVME_MAX_HOSTS 6

#ifdef TEST_START_VARS
#error "Who changed TEST_START_VARS??"
#endif

/**
 * Test suite specific variables of the test @b main() function.
 */
#define TEST_START_VARS \
    TEST_START_ENV_VARS                                 \
    LIST_HEAD(tsapi_initiators_list, tsapi_initiator)                   \
        all_initiators __attribute__((unused)) =                        \
        LIST_HEAD_INITIALIZER(&all_initiators);                         \
    LIST_HEAD(tsapi_targets_list, tapi_nvme_target)                     \
      all_targets __attribute__((unused)) =                             \
        LIST_HEAD_INITIALIZER(&all_targets);                            \
    LIST_HEAD(tsapi_fio_list, tapi_fio)                                 \
      all_fios __attribute__((unused)) =                                \
        LIST_HEAD_INITIALIZER(&all_fios);

#ifdef TEST_START_SPECIFIC
#error "Who changed TEST_START_SPECIFIC??"
#endif

/**
 * Test suite specific the first actions of the test.
 */
#define TEST_START_SPECIFIC \
    do {                                                                \
        TEST_START_ENV;                                                 \
    } while (0)

#ifndef TEST_END_SPECIFIC
/**
 * Test suite specific part of the last action of the test @b main()
 * function.
 */
#define TEST_END_SPECIFIC \
    do {                                             \
        TEST_END_ENV;                                \
    } while (0)
#endif

#include "tapi_test.h"
#include "tapi_env.h"
#include "tapi_mem.h"
#include "tapi_sockaddr.h"
#include "tapi_rpcsock_macros.h"
#include "tapi_fio.h"
#include "tapi_nvme.h"

#include "te_sockaddr.h"
#include "te_vector.h"

/**
 * Get device from the agent block devices list by number.
 *
 * @param ta        TA name
 * @param number    Which one to take from the ordered list
 */
extern char * tsapi_agent_get_device(const char *agent, unsigned number);

/**
 * Get nqn from the agent for blockdev.
 *
 * @param agent     TA name
 * @param device    Device for get NQN
 */
extern char * tsapi_agent_get_nqn(const char *agent, const char *device);

/****************************** INITIATOR ********************************/

typedef struct tsapi_initiator {
    LIST_ENTRY(tsapi_initiator) list;

    LIST_HEAD(tsapi_host_ctrls, tapi_nvme_host_ctrl) host_ctrls;

    tapi_env_host *host;        /**< Host on which the initiator lives */
    const struct if_nameindex *iface; /**< Name of the interface via which
                                 * initiator will operate or NULL */
    rcf_rpc_server *rpcs;       /**< Root rpcs of the initiator */
} tsapi_initiator;

/**
 * Add initiator into the scope of the test.
 *
 * @param _target   Initiator to be added
 */
#define tsapi_nvme_ininitator_add(_initiator)              \
    do {                                                           \
        tsapi_initiator *the_initiator = (_initiator);             \
        CHECK_NOT_NULL(the_initiator);                             \
        RING("Adding initiator %p from host %s into the scope",    \
             the_initiator, the_initiator->host->ta);              \
        LIST_INSERT_HEAD(&all_initiators, the_initiator, list);    \
    } while(0)

/**
 * Loop overall each initiator from the scope of the test
 *
 * @param _initiator  Iterator
 */
#define tsapi_for_each_initiator(_initiator)            \
      LIST_FOREACH(_initiator, &all_initiators, list)

/**
 * Loop overall each host_ctrl from the given initiator
 *
 * @param _initiator     Which initiator
 * @param _host_ctrl     Iterator
 */
#define tsapi_for_each_host_ctrl(_initiator, _host_ctrl)        \
    LIST_FOREACH(_host_ctrl, &((_initiator)->host_ctrls), list)

/**
 * Loop overall each host_ctrl from all initiators
 *
 * @param _initiator  Initiator current target belongs to (also iterates!)
 * @param _target     Iterator
 */
#define tsapi_for_each_initiator_and_host_ctrl(_initiator, _host_ctrl)  \
    tsapi_for_each_initiator(_initiator)                                \
    tsapi_for_each_host_ctrl(_initiator, _host_ctrl)

/**
 * Get initiator from the environment and add it into the scope of the test.
 *
 * @param _var          Name of the variable to store the output.
 * @param _number       Net number from the env.
 */
#define TEST_GET_INITIATOR(_var, _number)                       \
    do {                                                        \
        tsapi_initiator *my_init;                               \
        tapi_env_host *host =                                           \
            tapi_env_get_host(&env,                                     \
                              te_string_fmt("init%d", (_number)));      \
        if (host == NULL)                                      \
        {                                                               \
            if (_number == 0)                                           \
                TEST_FAIL("You must have init0");                       \
            else                                                        \
                break;                                                  \
        }                                                               \
        my_init = calloc(1, sizeof(*my_init));                          \
        my_init->host = host;                                           \
        my_init->rpcs =                                                 \
            tapi_env_get_pco(&env,                                      \
                             te_string_fmt("init_rpcs%d", (_number)));  \
        if (my_init->rpcs == NULL)                                      \
            TEST_FAIL("If you have init%d you should have PCO %d",      \
                      (_number), (_number));                            \
        my_init->iface = tapi_env_get_if(&env,                          \
                                         te_string_fmt("init_if%d",     \
                                                       (_number)));     \
        LIST_INIT(&my_init->host_ctrls);                                \
        tsapi_nvme_ininitator_add(my_init);                             \
        _var = my_init;                                                 \
    } while(0)

static inline tapi_nvme_host_ctrl * tsapi_initiator_add_host_ctrl(
                                                tsapi_initiator *initiator,
                                                const char *target_id)
{
    tapi_nvme_host_ctrl *host_ctrl;

    host_ctrl = calloc(1, sizeof(*host_ctrl));
    tapi_nvme_initiator_init(host_ctrl);

    CHECK_RC(
        rcf_rpc_server_create_process(initiator->rpcs,
                                      te_string_fmt("%s_to_%s",
                                                    initiator->rpcs->ta,
                                                    target_id),
                                      0, &host_ctrl->rpcs));
    LIST_INSERT_HEAD(&initiator->host_ctrls, host_ctrl, list);

    return host_ctrl;
}

/****************************** TARGET ********************************/

/**
 * Add target into the scope of the test.
 *
 * @param _target   Target to be added
 */
#define tsapi_nvme_target_add(_target)                          \
    do {                                                        \
        tapi_nvme_target *the_target = (_target);               \
        CHECK_NOT_NULL(the_target);                             \
        RING("Adding target %p from host %s into the scope",    \
             the_target, the_target->rpcs->ta);                 \
        LIST_INSERT_HEAD(&all_targets, the_target, list);       \
    } while(0)

/**
 * Loop overall each target from the scope of the test
 *
 * @param _target     Iterator
 */
#define tsapi_for_each_target(_target)          \
    LIST_FOREACH(_target, &all_targets, list)   \

/**
 * Get target from the environment and add it into the scope of the test.
 *
 * @param _var          Name of the variable to store the output.
 * @param _number       Net number from the env.
 * @param _type         Type of targets.
 */
#define TEST_GET_TARGET(_var, _number, _type)                           \
    do {                                                                \
        tapi_nvme_target *my_target;                                    \
                                                                        \
        if (tapi_env_get_host(&env,                                     \
                              te_string_fmt("tgt%d", (_number))) == NULL) \
        {                                                               \
            if (_number == 0)                                           \
                TEST_FAIL("You must have tgt0");                        \
            else                                                        \
                break;                                                  \
        }                                                               \
                                                                        \
        CHECK_NOT_NULL(my_target = calloc(1, sizeof(*target)));         \
        *my_target = _type;                                             \
        my_target->rpcs = tapi_env_get_pco(&env,                        \
                                           te_string_fmt("tgt_rpcs%d",  \
                                                         (_number)));   \
        if (my_target->rpcs == NULL)                                    \
        {                                                               \
            TEST_FAIL("You need to specify tgt_rpcs%d PCO if "          \
                      "you're to use this API.", (_number));            \
        }                                                               \
                                                                        \
        my_target->addr = tapi_env_get_addr(&env,                       \
                                            te_string_fmt("tgt_addr%d", \
                                                          (_number)),   \
                                            NULL);                      \
        if (te_sockaddr_get_port_ptr(my_target->addr) != NULL)          \
            tapi_allocate_port_htons(my_target->rpcs,                   \
                                     te_sockaddr_get_port_ptr(my_target->addr)); \
        tsapi_nvme_target_add(my_target);                               \
        _var = my_target;                                               \
    } while(0)


/****************************** FIO ********************************/

/**
 * Add FIO instance in the scope of the test so it can iterated etc.
 */
#define tsapi_fio_add(_fio)                         \
    do {                                              \
        tapi_fio *my_fio = (_fio);                    \
                                                      \
        CHECK_NOT_NULL(my_fio);                       \
        LIST_INSERT_HEAD(&all_fios, my_fio, list);    \
    } while(0)


/**
 * Loop over each fio instance form the scope of the test. FIO instances
 * should be enqueued with tsapi_fio_add()
 *
 * @param _fio        Iterator
 */
#define tsapi_for_each_fio(_fio)        \
    LIST_FOREACH(_fio, &all_fios, list) \

extern te_errno tsapi_set_fdlimits(rcf_rpc_server *rpcs);

/**
 * Get target object, i.e. target type
 *
 * @param ta        Test Agent
 *
 * @return target type if target_type supported, overwise TEST_FAIL
 */
extern tapi_nvme_target tsapi_get_target_ta(const char *ta);

/**
 * Get specific nvme connect options
 *
 * @param ta        Test Agent
 * @param opts      NVMe connect specific option
 */
extern void tsapi_get_nvme_connect_opts(const char *ta,
                                        tapi_nvme_connect_opts *opts);

/** Initiator with custom disconnect RPC server */
typedef struct tsapi_nvme_initiator {
    rcf_rpc_server *disconnect_rpcs;    /**< RPC server for disconnect */
    tapi_nvme_host_ctrl initiator;      /**< NVMe initiator */
} tsapi_nvme_initiator;

/** Defaults for tsapi_nvme_initiator */
#define TSAPI_NVME_INITIATOR (tsapi_nvme_initiator) \
{                                                   \
    .initiator = TAPI_NVME_HOST_CTRL_DEFAULTS,      \
    .disconnect_rpcs = NULL,                        \
}

/**
 * Create disconnect RPC in initiator
 *
 * @param initiator     Initiator
 *
 * @return Status code
 */
extern te_errno tsapi_nvme_initiator_init(tsapi_nvme_initiator *initiator);

/**
 * Close disconnect RPC in initiator
 *
 * @param initiator     Initiator
 *
 * @return Status code
 */
extern te_errno tsapi_nvme_initiator_fini(tsapi_nvme_initiator *initiator);


/**
 * Wrapper for tapi_nvme_initiator_connect
 *
 * @param initiator     Initiator
 *
 * @return Status code
 */
extern te_errno tsapi_nvme_initiator_connect(tsapi_nvme_initiator *initiator,
                                             const tapi_nvme_target *target);

/**
 * Wrapper for tapi_nvme_initiator_connect
 *
 * @param initiator     Initiator
 * @param target        Target for connect
 * @param opts          Specific option for nvme connect
 *
 * @return Status code
 */
extern te_errno tsapi_nvme_initiator_connect_opts(
    tsapi_nvme_initiator *initiator, const tapi_nvme_target *target,
    const tapi_nvme_connect_opts *opts);

/**
 * Do disconnect from disconnect RPC server
 *
 * @param initiator     Initiator
 *
 * @return Status code
 */
extern te_errno tsapi_nvme_initiator_disconnect(tsapi_nvme_initiator *initiator);

#endif /* !__TS_NVMEOF_TEST_H__ */
