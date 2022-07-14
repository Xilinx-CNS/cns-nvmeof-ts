# SPDX-License-Identifier: Apache-2.0
# (c) Copyright 2018 - 2022 Xilinx, Inc. All rights reserved.
dnl Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([nvmeof-ts],[1.0.0],[Konstantin.Ushakov@oktetlabs.ru])

AC_CONFIG_SRCDIR([lib/nvmeof_test.h])

AC_CONFIG_AUX_DIR([auxdir])

AM_INIT_AUTOMAKE([1.11.0 -Wall foreign])

AC_CANONICAL_HOST
AC_SUBST([host])

dnl Checks for programs.
AC_PROG_CC
AM_PROG_AR
AC_PROG_RANLIB
AC_PROG_INSTALL

TS_CPPFLAGS="-I\$(srcdir)/lib -I\$(top_srcdir)/lib ${TE_CPPFLAGS}"
TS_CFLAGS="${TE_CFLAGS}"
TS_LDFLAGS="-L. -L./lib -L\$(top_builddir)/lib ${TE_LDFLAGS}"

AC_SUBST([TS_CPPFLAGS])
AC_SUBST([TS_CFLAGS])
AC_SUBST([TS_LDFLAGS])
AC_SUBST([TE_PATH])
AC_SUBST([TE_PLATFORM])

AC_ARG_VAR([TS_CPPFLAGS])
AC_ARG_VAR([TS_CFLAGS])
AC_ARG_VAR([TS_LDFLAGS])
AC_ARG_VAR([TE_CPPFLAGS])
AC_ARG_VAR([TE_CFLAGS])
AC_ARG_VAR([TE_LDFLAGS])
AC_ARG_VAR([TE_INSTALL])
AC_ARG_VAR([TE_PATH])
AC_ARG_VAR([TE_PLATFORM])

TS_TE_LIBS=" \
-ltapi_env -ltapi_rpc -lrcfrpc -ltapi \
-lconfapi -lconf_oid -lrcfapi -lrpcxdr -lrpc_types -llogger_ten -lipc \
-llog_bufs -ltools -ltapi_tad -lndn -lasn -lm -ltapi_fio -ltapi_nvme \
-ltapi_performance -ltapi_job -llogger_core"

TS_TE_DEPS=" \
\$(DESTDIR)/\$(prefix)/../${TE_PLATFORM}/lib/libipc.a \
\$(DESTDIR)/\$(prefix)/../${TE_PLATFORM}/lib/librcfapi.a \
\$(DESTDIR)/\$(prefix)/../${TE_PLATFORM}/lib/libconfapi.a \
\$(DESTDIR)/\$(prefix)/../${TE_PLATFORM}/lib/libconf_oid.a \
\$(DESTDIR)/\$(prefix)/../${TE_PLATFORM}/lib/librcfrpc.a \
\$(DESTDIR)/\$(prefix)/../${TE_PLATFORM}/lib/librpcxdr.a \
\$(DESTDIR)/\$(prefix)/../${TE_PLATFORM}/lib/librpc_types.a \
\$(DESTDIR)/\$(prefix)/../${TE_PLATFORM}/lib/liblogger_core.a \
\$(DESTDIR)/\$(prefix)/../${TE_PLATFORM}/lib/liblogger_ten.a \
\$(DESTDIR)/\$(prefix)/../${TE_PLATFORM}/lib/liblog_bufs.a \
\$(DESTDIR)/\$(prefix)/../${TE_PLATFORM}/lib/libtools.a \
\$(DESTDIR)/\$(prefix)/../${TE_PLATFORM}/lib/libtapi.a \
\$(DESTDIR)/\$(prefix)/../${TE_PLATFORM}/lib/libtapi_env.a \
\$(DESTDIR)/\$(prefix)/../${TE_PLATFORM}/lib/libtapi_rpc.a \
\$(DESTDIR)/\$(prefix)/../${TE_PLATFORM}/lib/libtapi_tad.a \
\$(DESTDIR)/\$(prefix)/../${TE_PLATFORM}/lib/libndn.a \
\$(DESTDIR)/\$(prefix)/../${TE_PLATFORM}/lib/libasn.a \
\$(DESTDIR)/\$(prefix)/../${TE_PLATFORM}/lib/libtapi_fio.a \
\$(DESTDIR)/\$(prefix)/../${TE_PLATFORM}/lib/libtapi_performance.a \
\$(DESTDIR)/\$(prefix)/../${TE_PLATFORM}/lib/libtapi_nvme.a \
\$(DESTDIR)/\$(prefix)/../${TE_PLATFORM}/lib/libtapi_job.a \
"

AC_SUBST([TS_TE_LIBS])
AC_SUBST([TS_TE_DEPS])

AC_CONFIG_HEADERS([package.h])

AC_CONFIG_FILES([\
Makefile \
lib/Makefile \
perf/Makefile \
conf/Makefile \
data/Makefile \
stability/Makefile
])

AC_OUTPUT
