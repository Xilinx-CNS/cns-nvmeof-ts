/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2018 - 2022 Xilinx, Inc. All rights reserved. */
/*
 * NVMoF Test Suite
 * Command test
 */

/** @page conf-list Check that list command works
 *
 * @objective Check that nvme commands works in parallel with traffic
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

#define TE_TEST_NAME  "conf/nvmecmd"

#include "nvmeof_test.h"

static struct te_vec *
test_artifacts(void)
{
    static te_vec artifacts = TE_VEC_INIT(te_string);
    return &artifacts;
}

static te_bool
test_artifacts_contain(const char *artifact)
{
    size_t i;

    for (i = 0; i < te_vec_size(test_artifacts()); i++)
    {
        te_string elem = TE_VEC_GET(te_string, test_artifacts(), i);
        if (strcmp(elem.ptr, artifact) == 0)
            return TRUE;
    }

    return FALSE;
}

static void
test_artifacts_cleanup(void)
{
    size_t i;

    for (i = 0; i < te_vec_size(test_artifacts()); i++)
        te_string_free(te_vec_get(test_artifacts(), i));

    te_vec_free(test_artifacts());
}

static void
test_artifact(const char *fmt, ...)
{
    te_string string = TE_STRING_INIT;
    va_list  ap;

    va_start(ap, fmt);
    CHECK_RC(te_string_append_va(&string, fmt, ap));
    va_end(ap);

    if (!test_artifacts_contain(string.ptr))
    {
        WARN_ARTIFACT("%s", string.ptr);
        CHECK_RC(TE_VEC_APPEND(test_artifacts(), string));
    }
}

typedef struct nvme_feature {
    int feat;
    const char *name;
    te_bool supported;
} nvme_feature;

#define DEF_FEATURE(_feature, _supported) \
{                                         \
    .feat = TAPI_NVME_##_feature,         \
    .name = #_feature,                    \
    .supported = _supported               \
}

static nvme_feature features[] = {
    DEF_FEATURE(FEAT_ASYNC_EVENT, TRUE),
    DEF_FEATURE(FEAT_ASYNC_EVENT, TRUE),
    DEF_FEATURE(FEAT_VOLATILE_WC, TRUE),
    DEF_FEATURE(FEAT_NUM_QUEUES, TRUE),
    DEF_FEATURE(FEAT_KATO, TRUE),
    DEF_FEATURE(FEAT_HOST_ID, TRUE),
    DEF_FEATURE(FEAT_WRITE_PROTECT, TRUE),
    DEF_FEATURE(FEAT_ARBITRATION, FALSE),
    DEF_FEATURE(FEAT_POWER_MGMT, FALSE),
    DEF_FEATURE(FEAT_TEMP_THRESH, FALSE),
    DEF_FEATURE(FEAT_ERR_RECOVERY, FALSE),
    DEF_FEATURE(FEAT_IRQ_COALESCE, FALSE),
    DEF_FEATURE(FEAT_IRQ_CONFIG, FALSE),
    DEF_FEATURE(FEAT_WRITE_ATOMIC, FALSE),
};

static te_errno
nvme_get_next_feature(tapi_nvme_host_ctrl *host_ctrl)
{
    te_errno rc;
    static size_t feature_index = 0;
    struct nvme_feature feature = features[feature_index];

    feature_index = (feature_index + 1) % TE_ARRAY_LEN(features);
    rc = tapi_nvme_initiator_get_feature(host_ctrl, feature.feat);

    if (feature.supported && rc != 0)
    {
        ERROR_VERDICT("get-feature '%s': supported but failed", feature.name);
        TEST_FAIL("get-feature '%s': supported but failed", feature.name);
    }
    else if (rc != 0)
    {
        test_artifact("get-feature %s: not supported", feature.name);
    }

    return 0;
}

static te_errno
nvme_flush_device(tapi_nvme_host_ctrl *host_ctrl)
{
    return tapi_nvme_initiator_flush(host_ctrl, NULL);
}

struct command {
    const char *name;
    te_errno (*func)(tapi_nvme_host_ctrl *);
};

static struct command commands[] = {
    {"list",         tapi_nvme_initiator_list       },
    {"id-ctrl",      tapi_nvme_initiator_id_ctrl    },
    {"id-ns",        tapi_nvme_initiator_id_ns      },
    {"show-regs",    tapi_nvme_initiator_show_regs  },
    {"fw-log",       tapi_nvme_initiator_fw_log     },
    {"smart-log",    tapi_nvme_initiator_smart_log  },
    {"error-log",    tapi_nvme_initiator_error_log  },
    {"get-feature",  nvme_get_next_feature          },
    {"flush",        nvme_flush_device              },
};

static te_bool
is_command_needed(const char *command_name, const char *commands_string)
{
    char *command, *token, *tofree;
    te_bool result = FALSE;

    if (commands_string == NULL)
        return TRUE;

    command = tofree = strdup(commands_string);
    while ((token = strsep(&command, ",")) != NULL)
    {
        if (strcmp(command_name, token) == 0)
        {
            result = TRUE;
            break;
        }
    }

    free(tofree);
    return result;
}

enum traversal_type {
    TRAVERSAL_TYPE_RAND,
    TRAVERSAL_TYPE_CONSISTENTLY,
};

#define TRAVERSAL_TYPE_MAPPING                                  \
    {"traversal_rand", TRAVERSAL_TYPE_RAND},                    \
    {"traversal_consistently", TRAVERSAL_TYPE_CONSISTENTLY}     \

static unsigned get_command_index(enum traversal_type ttype)
{
    static unsigned index = 0;

    switch (ttype) {
        case TRAVERSAL_TYPE_RAND:
            index = rand();
            break;
        case TRAVERSAL_TYPE_CONSISTENTLY:
            index++;
            break;
        default:
            TEST_FAIL("traversal type '%d' not supported", ttype);
    }

    return index;
}

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
    enum traversal_type ttype = TRAVERSAL_TYPE_CONSISTENTLY;
    const char *specific_commands = NULL;
    unsigned filtred_commad_size = 0;
    struct command filtred_commands[TE_ARRAY_LEN(commands)];
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
    TEST_GET_ENUM_PARAM(ttype, TRAVERSAL_TYPE_MAPPING);
    if (TEST_HAS_PARAM(specific_commands))
        TEST_GET_STRING_PARAM(specific_commands);

    target = tsapi_get_target_ta(tgt->ta);
    target.rpcs = tgt_rpcs;
    target.addr = tgt_addr;
    initiator.initiator.rpcs = init_rpcs;

    TEST_STEP("Read target options");
    tsapi_read_nvme_target_opts(&target, argc, argv);

    TEST_STEP("Setup target");
    tsapi_read_nvme_target_specific(&target, &target_opts, tgt_if->if_name,
                                    argc, argv);
    CHECK_RC(tapi_nvme_target_init(&target, target_opts));
    CHECK_RC(tapi_nvme_target_setup(&target));

    TEST_STEP("Setup initiator");
    CHECK_RC(tsapi_nvme_initiator_init(&initiator));
    CHECK_RC(tsapi_nvme_initiator_connect(&initiator, &target));

    TEST_STEP("Read and apply interface options");
    tsapi_read_if_feature_opts(&sys_opts, argc, argv);
    tsapi_apply_if_feature_opts(&sys_opts, init->ta, init_if->if_name);
    tsapi_apply_if_feature_opts(&sys_opts, tgt->ta, tgt_if->if_name);

    TEST_STEP("Prepare FIO run");
    tsapi_read_fio_opts(&opts, argc, argv);
    opts.filename = initiator.initiator.device;
    fio = tapi_fio_create(&opts, init_rpcs);

    CHECK_RC(tapi_fio_start(fio));

    TEST_STEP("Run %d nvme command(s)", repeat);
    for (i = 0; i < TE_ARRAY_LEN(commands); i++)
    {
        if (is_command_needed(commands[i].name, specific_commands))
            filtred_commands[filtred_commad_size++] = commands[i];
    }
    for (i = 0; i < repeat; i++)
    {
        unsigned command_index = get_command_index(ttype) % filtred_commad_size;
        struct command cmd = filtred_commands[command_index];
        if (cmd.func(&initiator.initiator) != 0)
            test_artifact("%s: not supported", cmd.name);
    }

    TEST_STEP("Start FIO and wait for completion");
    CHECK_RC(tapi_fio_wait(fio, TAPI_FIO_TIMEOUT_DEFAULT));
    CHECK_RC(tapi_fio_stop(fio));

    TEST_STEP("Get FIO report and log it");
    CHECK_RC(tapi_fio_get_report(fio, &report));

    tapi_fio_log_report(&report);

    TEST_SUCCESS;
cleanup:
    TEST_STEP("Disconnect the initiator and wipe target config.");
    free(target_opts);
    tapi_fio_destroy(fio);
    CLEANUP_CHECK_RC(tsapi_nvme_initiator_disconnect(&initiator));
    tapi_nvme_target_fini(&target);
    test_artifacts_cleanup();
    TEST_END;
}
