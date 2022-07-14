# SPDX-License-Identifier: Apache-2.0
# (c) Copyright 2018 - 2022 Xilinx, Inc. All rights reserved.
AM_CPPFLAGS = @TE_CPPFLAGS@

AM_CFLAGS = @TE_CFLAGS@ -Werror

noinst_HEADERS = \
    nvmeof_test.h \
    tsapi_opts.h \
    tsapi_sysstat.h

noinst_LIBRARIES = libts_nvmeof.a

libts_nvmeof_a_SOURCES = \
    nvmeof_ts.c \
    tsapi_opts.c \
    tsapi_sysstat.c
