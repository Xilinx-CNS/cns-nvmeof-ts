# SPDX-License-Identifier: Apache-2.0
# (c) Copyright 2018 - 2022 Xilinx, Inc. All rights reserved.

EXTRA_DIST = package.dox

tetestdir=$(prefix)/@PACKAGE_NAME@/$(subdir)

dist_tetest_DATA = package.xml

tetest_DATA = tests-info.xml

tests-info.xml: $(srcdir)/*.c
	@TE_PATH@/te_tests_info.sh $(srcdir) > tests-info.xml

CLEANFILES = tests-info.xml

AM_CPPFLAGS = @TS_CPPFLAGS@
AM_CFLAGS = @TS_CFLAGS@
AM_LDFLAGS = @TS_LDFLAGS@
LDADD = @TS_TE_LIBS@

tetest_PROGRAMS = \
	fio_pattern

fio_pattern_DEPENDENCIES = @TS_TE_DEPS@  ../lib/libts_nvmeof.a
fio_pattern_LDADD = @TS_TE_LIBS@ -lts_nvmeof
