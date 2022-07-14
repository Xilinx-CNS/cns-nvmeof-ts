/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2018 - 2022 Xilinx, Inc. All rights reserved. */
/*
 * NVMeoF Test Suite prologue
 *
 * @author Konstantin Ushakov <Konstantin.Ushakov@oktetlabs.ru>
 */

/** @page prologue Prologue
 * @brief NVMeoF Test Suite prologue
 *
 * NVMeoF Test Suite prologue
 *
 * @par Scenario:
 */

#ifndef DOXYGEN_TEST_SPEC

/** Logging subsystem entity name */
#define TE_TEST_NAME    "prologue"

#include "nvmeof_test.h"

#include "tapi_cfg_net.h"
#include "tapi_cfg_modules.h"
#include "logger_ten.h"
#include "tapi_test.h"
#include "tapi_cfg.h"
#include "tapi_cfg_base.h"
#include "tapi_sh_env.h"

char *
tapi_cfg_agent_dir(char *agent_name)
{
    cfg_val_type val_type = CVT_STRING;
    char *agent_dir;

    CHECK_RC(cfg_get_instance_fmt(&val_type, &agent_dir,
                                  "/agent:%s/dir:", agent_name));
    return agent_dir;
}

typedef enum {
    TE_TOOL_EXEC,
    TE_TOOL_KMOD,
} te_tool_type;

/* we don't care about memory leaks */
static void
copy_tool(const char *tool_name, const char *remote_name,
          te_tool_type type)
{
    unsigned    n_tools;
    cfg_handle *tools = NULL;
    unsigned    i;

    if (remote_name == NULL)
        remote_name = tool_name;

    RING("Copy %s tool (type=%u)", tool_name, type);

    CHECK_RC(cfg_find_pattern_fmt(&n_tools, &tools,
                                  "/local:*/tools:/%s:", tool_name));
    RING("%s: find returned %u entries", __FUNCTION__, n_tools);

    for (i = 0; i < n_tools; ++i)
    {
        cfg_handle tool_inst = tools[i];
        cfg_oid *oid;
        cfg_val_type val_type;
        char *tool;
        char *local_file, *remote_file;
        char *agent_name;
        char *agent_dir;

        val_type = CVT_STRING;
        CHECK_RC(cfg_get_instance(tools[i], &val_type, &tool));

        if (strlen(tool) == 0) {
            CHECK_RC(cfg_del_instance(tools[i], FALSE));
            continue;
        }

        CHECK_RC(cfg_get_oid(tool_inst, &oid));
        agent_name = CFG_OID_GET_INST_NAME(oid, 1);

        agent_dir = tapi_cfg_agent_dir(agent_name);
        CHECK_RC(cfg_get_instance_fmt(&val_type, &agent_dir,
                                      "/agent:%s/dir:", agent_name));

        switch (type)
        {
            case TE_TOOL_EXEC:
                local_file = tool;
                remote_file = te_string_fmt("%s/%s",
                                            agent_dir, remote_name);
                break;
            case TE_TOOL_KMOD:
                local_file = te_string_fmt("%s.ko", tool);
                remote_file = te_string_fmt("%s/%s.ko",
                                            agent_dir, remote_name);
                break;
            default:
                TEST_FAIL("Unknown tool type %u", type);
        }

        RING("Store %s in %s", tool_name, remote_file);
        CHECK_RC(rcf_ta_put_file(agent_name, 0, local_file, remote_file));

        if (type == TE_TOOL_EXEC) {
            te_errno rc;
            CHECK_RC(rcf_ta_call(agent_name, 0, "shell",
                                 &rc, 3, TRUE, "chmod", "+s", remote_file));
            if (rc != 0)
                TEST_FAIL("Failed to 'chmod +s %s' on agent %s",
                          remote_file, agent_name);
        }
    }
}

struct te_tool {
    const char *tool_name;
    const char *remote_name;
    te_tool_type type;
};

static te_bool
is_tool_need_copy(const char *tool_name, const char *specific_tools)
{
    char *tools, *token, *tofree;
    te_bool result = FALSE;

    if (specific_tools == NULL)
        return TRUE;

    tools = tofree = strdup(specific_tools);
    while ((token = strsep(&tools, ",")) != NULL)
    {
        if (strcmp(tool_name, token) == 0)
        {
            result = TRUE;
            break;
        }
    }

    free(tofree);
    return result;
}

static void
copy_tools(const char *specific_tools)
{
    size_t i = 0;
    struct te_tool tools[] = {
        { "nvme-cli",       "nvme", TE_TOOL_EXEC },
        { "fio",            NULL,   TE_TOOL_EXEC },
        { "libcrc32c",      NULL,   TE_TOOL_KMOD },
        { "nvme-core",      NULL,   TE_TOOL_KMOD },
        { "nvme",           NULL,   TE_TOOL_KMOD },
        { "nvme-tcp",       NULL,   TE_TOOL_KMOD },
        { "nvme-fabrics",   NULL,   TE_TOOL_KMOD },
        { "nvmet",          NULL,   TE_TOOL_KMOD },
        { "nvmet-tcp",      NULL,   TE_TOOL_KMOD }
    };

    RING("Copy tools and modules to the agents");

    for (i = 0; i < TE_ARRAY_LEN(tools); i++)
        if (is_tool_need_copy(tools[i].tool_name, specific_tools))
            copy_tool(tools[i].tool_name, tools[i].remote_name, tools[i].type);

}

static void
unload_module(const char *mod_name, const char *lsmod_name)
{
    unsigned    n_mods;
    cfg_handle *mods = NULL;
    unsigned    i;

    CHECK_RC(cfg_find_pattern_fmt(&n_mods, &mods,
                                  "/local:*/tools:/%s:", mod_name));
    for (i = 0; i < n_mods; ++i)
    {
        cfg_handle mod_inst = mods[i];
        cfg_oid *oid;
        char *agent_name;
        unsigned    n_entries;
        cfg_handle *entries = NULL;
        te_errno rc;

        CHECK_RC(cfg_get_oid(mod_inst, &oid));
        agent_name = CFG_OID_GET_INST_NAME(oid, 1);

        RING("? Unloading %s module on agent %s getting "
             "/agent:%s/module:%s",
             mod_name, agent_name, agent_name, lsmod_name);
        CHECK_RC(cfg_find_pattern_fmt(&n_entries, &entries,
                                      "/agent:%s/module:%s",
                                      agent_name, lsmod_name));
        if (n_entries != 0) {
            RING("Unloading %s module on agent %s", mod_name, agent_name);
            (void)rcf_ta_call(agent_name, 0, "shell",
                              &rc, 2, TRUE, "rmmod", lsmod_name);
        }
    }

}

static void
unload_modules(void)
{

    unload_module("nvme-tcp", "nvme_tcp");
    unload_module("nvme-fabrics", "nvme_fabrics");
    unload_module("nvme", "nvme");
    unload_module("nvme-core", "nvme_core");

    unload_module("nvmet-tcp", "nvmet_tcp");
    unload_module("nvmet", "nvmet");

    /* we don't unload libcrc32c cause once you load it it's in use by other
     * modules */

    /* we need to sync because configurator state has changed */
    CHECK_RC(cfg_synchronize("/:", TRUE));
}

static void
load_module(const char *mod_name, const char *sysfs_name, te_bool can_fail)
{
    unsigned    n_mods;
    cfg_handle *mods = NULL;
    unsigned    i;

    RING("Load %s module", mod_name);

    CHECK_RC(cfg_find_pattern_fmt(&n_mods, &mods,
                                  "/local:*/tools:/%s:", mod_name));
    for (i = 0; i < n_mods; ++i)
    {
        cfg_handle mod_inst = mods[i];
        cfg_oid *oid;
        cfg_val_type val_type;
        char *mod;
        char *mod_file;
        char *agent_name;
        te_errno rc;

        val_type = CVT_STRING;
        CHECK_RC(cfg_get_instance(mods[i], &val_type, &mod));

        if (strlen(mod) == 0)
            CHECK_RC(cfg_del_instance(mods[i], FALSE));

        CHECK_RC(cfg_get_oid(mod_inst, &oid));
        agent_name = CFG_OID_GET_INST_NAME(oid, 1);

        RING("Loading %s module on agent %s", mod_name, agent_name);
        mod_file = te_string_fmt("%s/%s.ko",
                                 tapi_cfg_agent_dir(agent_name),
                                 mod_name);

        CHECK_RC(tapi_cfg_module_add(agent_name, sysfs_name, FALSE));
        CHECK_RC(tapi_cfg_module_filename_set(agent_name, sysfs_name,
                                              mod_file));
        rc = tapi_cfg_module_load(agent_name, sysfs_name);
        if (!can_fail && rc != 0)
            TEST_FAIL("Failed to 'insmod %s' on agent %s",
                      mod_file, agent_name);
    }
}

static void
load_modules(void)
{
    RING("Load modules specified in /local:<agent>/tools:/");

    load_module("libcrc32c",     "libcrc32c",   TRUE);

    load_module("nvme-core",    "nvme_core",    FALSE);
    load_module("nvme",         "nvme",         FALSE);
    load_module("nvme-fabrics", "nvme_fabrics", FALSE);
    load_module("nvme-tcp",     "nvme_tcp",     FALSE);

    load_module("nvmet",        "nvmet",        FALSE);
    load_module("nvmet-tcp",    "nvmet_tcp",    FALSE);
}

char *
local_nullblk_param(const char *agent_name,
                    const char *param_name)
{
    char *value;

    CHECK_RC(cfg_get_instance_fmt(NULL, &value,
                                  "/local:%s/nullblk:/parameter:%s",
                                  agent_name, param_name));
    /* we'll loose memory and we don't care */
    return value;
}

static void
apply_nullblk()
{
    unsigned n_nullblks = 0;
    cfg_handle *nullblks = NULL;
    unsigned i;

    CHECK_RC(cfg_find_pattern_fmt(&n_nullblks, &nullblks,
                                  "/local:*/nullblk:"));
    for (i = 0; i < n_nullblks; ++i)
    {
        cfg_handle inst = nullblks[i];
        cfg_oid *oid;
        char *agent_name;
        te_errno rc;

        CHECK_RC(cfg_get_oid(inst, &oid));
        agent_name = CFG_OID_GET_INST_NAME(oid, 1);

        CHECK_RC(tapi_cfg_module_add(agent_name, "null_blk", FALSE));
        rc = tapi_cfg_module_params_add(agent_name, "null_blk",
            "nr_devices", local_nullblk_param(agent_name, "nr_devices"),
            "submit_queues", local_nullblk_param(agent_name, "submit_queues"),
            "hw_queue_depth", local_nullblk_param(agent_name, "hw_queue_depth"),
            "gb", local_nullblk_param(agent_name, "gb"), NULL);

        /* We cannot change params of module if it already loaded, ignore */
        if (rc == TE_EEXIST)
        {
            RING("nullblk for TA '%s' already loaded", agent_name);
            continue;
        }

        CHECK_RC(rc);
        CHECK_RC(tapi_cfg_module_load(agent_name, "null_blk"));
    }
}

static void
update_agents_path(void)
{
    cfg_handle   *handles = NULL;
    unsigned int  num;
    unsigned      i;

    CHECK_RC(cfg_find_pattern("/agent:*", &num, &handles));
    for (i = 0; i < num; i++)
    {
        cfg_oid *oid = NULL;

        CHECK_RC(cfg_get_oid(handles[i], &oid));
        CHECK_RC(tapi_sh_env_ta_path_append(CFG_OID_GET_INST_NAME(oid, 1),
                                            "/usr/local/sbin/"));
        CHECK_RC(tapi_sh_env_ta_path_append(CFG_OID_GET_INST_NAME(oid, 1),
                                            "/usr/sbin/"));
    }
}

int
main(int argc, char **argv)
{
    char *opt;
/* Redefine as empty to avoid environment processing here */
#undef TEST_START_ENV_VARS
#define TEST_START_ENV_VARS
#undef TEST_START_SPECIFIC
#define TEST_START_SPECIFIC
#undef TEST_END_SPECIFIC
#define TEST_END_SPECIFIC

    TEST_START;

    te_log_stack_init();

    TEST_STEP("If SF_TOOLS_COPY is set copy tools to each agent.");
    if ((opt = getenv("SF_TOOLS_COPY")) && strcmp(opt, "true") == 0)
        copy_tools(getenv("SF_TOOLS"));

    TEST_STEP("Append /usr/local/sbin/, /usr/sbin directory with tools to PATH");
    update_agents_path();

    TEST_STEP("If SF_MODULES_LOAD is set load modules on each agent.");
    if ((opt = getenv("SF_MODULES_LOAD")) && strcmp(opt, "true") == 0)
    {
        TEST_SUBSTEP("Unload old modules");
        unload_modules();
        TEST_SUBSTEP("Load new modules.");
        load_modules();
    }

    TEST_STEP("Apply nullblk configuration.");
    apply_nullblk();

    TEST_STEP("Apply shell environment to the agent");
    CHECK_RC(tapi_cfg_env_local_to_agent());

    TEST_STEP("Remove nets that are not used in the current implementation");
    if ((rc = tapi_cfg_net_remove_empty()) != 0)
        TEST_FAIL("Failed to remove /net instances with empty interfaces");

    TEST_STEP("Reserver all interfaces that are mentioned in the network");
    rc = tapi_cfg_net_reserve_all();
    if (rc != 0)
        TEST_FAIL("Failed to reserve all interfaces mentioned in networks "
                  "configuration: %r", rc);

    /* FIXME: disable offloads on all mentioned interfaces */
#if 0
    rc = tapi_cfg_net_foreach_node(disable_offloads, NULL);
    if (rc != 0)
        TEST_FAIL("Failed to disable offloads on interfaces mentioned in "
                  "networks configuration: %r", rc);
#endif

    TEST_STEP("Put all interfaces in the UP state");
    rc = tapi_cfg_net_all_up(FALSE);
    if (rc != 0)
        TEST_FAIL("Failed to up all interfaces mentioned in networks "
                  "configuration: %r", rc);

    TEST_STEP("Cleanup pre-configured IP addresses from all "
              "network interfaces");
    rc = tapi_cfg_net_delete_all_ip4_addresses();
    if (rc != 0)
        TEST_FAIL("Failed to delete all IPv4 addresses from all "
                  "interfaces mentioned in networks configuration: %r",
                  rc);

    TEST_STEP("Assign new IP addresses based on nets configuration");
    rc = tapi_cfg_net_all_assign_ip(AF_INET);
    if (rc != 0)
        TEST_FAIL("Failed to assign IPv4 subnets: %r", rc);

    rc = tapi_cfg_net_all_check_mtu();
    if (rc != 0)
        TEST_FAIL("Failed to check all MTU: %r", rc);

    TEST_STEP("Start up basic RPCs servers");
    CHECK_RC(tapi_cfg_rpcs_local_to_agent());

    TEST_STEP("Commit changes and wait for it to be applied. "
              "Dump configuration tree");
    CFG_WAIT_CHANGES;
    CHECK_RC(rc = cfg_synchronize("/:", TRUE));
    CHECK_RC(rc = cfg_tree_print(NULL, TE_LL_RING, "/:"));

    TEST_SUCCESS;

cleanup:

    TEST_END;
}

#endif /* !DOXYGEN_TEST_SPEC */
