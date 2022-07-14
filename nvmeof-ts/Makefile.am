# SPDX-License-Identifier: Apache-2.0
# (c) Copyright 2018 - 2022 Xilinx, Inc. All rights reserved.
SUBDIRS = lib . perf conf data stability

EXTRA_DIST = Doxyfile mainpage.dox

tetestdir=$(prefix)/@PACKAGE_NAME@/$(subdir)

dist_tetest_DATA = package.xml

AM_CPPFLAGS = @TS_CPPFLAGS@

AM_CFLAGS = @TS_CFLAGS@

AM_LDFLAGS = @TS_LDFLAGS@


MYLDADD = @TS_TE_LIBS@
MYDEPS = @TS_TE_DEPS@  lib/libts_nvmeof.a

tetest_PROGRAMS = prologue

prologue_SOURCES = prologue.c
prologue_LDADD = $(MYLDADD) -lts_nvmeof
prologue_DEPENDENCIES = $(MYDEPS)
