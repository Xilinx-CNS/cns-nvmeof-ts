# SPDX-License-Identifier: Apache-2.0
# (c) Copyright 2018 - 2022 Xilinx, Inc. All rights reserved.
project('nvmeof-ts', 'c',
    version : '1.0.0',
)

te_path = get_option('te_path')
te_libdir = get_option('te_libdir')
add_project_arguments(get_option('te_cflags').split(), language: 'c')
add_project_arguments('-Werror', language : 'c')
add_project_link_arguments(get_option('te_ldflags').split(), language: 'c')

cc = meson.get_compiler('c')

te_tests_info_sh = find_program(join_paths(te_path, 'te_tests_info.sh'))

test_deps = [ dependency('threads') ]

te_libs = [
    'tapi_env',
    'tapi_rpc',
    'rcfrpc',
    'tapi',
    'confapi',
    'conf_oid',
    'rcfapi',
    'rpcxdr',
    'rpc_types',
    'logger_ten',
    'ipc',
    'log_bufs',
    'tools',
    'tapi_tad',
    'ndn',
    'asn',
    'tapi_nvme',
    'tapi_fio',
    'tapi_performance',
    'tapi_job',
    'logger_core',

    # I'm not sure where to put it right.
    'm',
    'jansson',
]

foreach lib : te_libs
    test_deps += cc.find_library(lib, dirs: te_libdir)
endforeach

lib_dir = include_directories('lib')

subdir('lib')

test_deps += declare_dependency(include_directories: lib_dir,
                                link_with: [ tslib ])

tests = [ 'prologue' ]

package_dir = 'nvmeof-ts'
package_tests_c = []

foreach test : tests
    test_exe = test
    test_c = test + '.c'
    package_tests_c += [ test_c ]
    executable(test_exe, test_c, install: true, install_dir: package_dir,
               dependencies: test_deps)
endforeach

install_data([ 'mainpage.dox', 'package.xml' ],
             install_dir: package_dir)

packages = [
    'conf',
    'perf',
    'stability',
    'data',
]

mydir = package_dir
foreach package : packages
    package_dir = join_paths(mydir, package)
    package_tests_c = []
    subdir(package)
endforeach
