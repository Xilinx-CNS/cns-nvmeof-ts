#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# (c) Copyright 2018 - 2022 Xilinx, Inc. All rights reserved.

import argparse
import json
import sys
import re

"""
Script collect performance form log.txt generated by test-environment
Script generate structure like this:
{
    "poi1": {                                    # parameter of interest
        (p1,p2,p3, ...): {                       # test parameters
            "fields": (n1, n2, n3, ...),         # sequence of test parameters
            "reports": [                         # fio reports
                {"poi1": v1, "rlat": rlat, ...}, # fio report of one iteration
                {"poi1": v2, "rlat": rlat, ...},
            ]
        },
        ...
    },
    "poi2": {
        ...
    }
}
useful for printing in different formats
"""


def log_entries(log_path='log.txt') -> tuple:
    """
    Log entry generator

    :param log_path: path to te log
    :return: current log header and log message
    """
    log_type_re = r'^ERROR|WARN|RING|INFO|VERB|ENTRY/EXIT|PACKET'
    correct_header = ['Log report\n',
                      '~~~~~~~~~~\n']

    with open(log_path) as flog:
        file_header = [flog.readline(), flog.readline()]
        if file_header != correct_header:
            raise StopIteration('Incorrect file format {}'.format(log_path))

        header, message = flog.readline(), ''
        for line in flog:
            if re.match(log_type_re, line):
                yield header, message
                header, message = line, ''
                continue
            message += line


def get_test_info(message) -> dict:
    """
    Parse test info from Tester Control message

    :param message: Tester Control message
    :return: dict with fields:
                id    - [tuple] ID of parent and this node
                type  - [str]   TEST, PACKAGE, PASSED, FAILED, etc...
                name  - [str]   Test name
                tin   - [str]   Test TIN
                hash  - [str]   Test hash
                args  - [dict]  Test arguments
            or empty dict if parse failed
    """
    pattern = r'^(?P<id>(\d+\ )+\d+)\ ' \
              r'(?P<type>[A-Z]+)\ (?P<name>[a-zA-Z0-9\-\.\_]+)?' \
              r'(.+TIN\ (?P<tin>\d+)\ .+\ HASH\ (?P<hash>\w+)\ ' \
              r'ARGs\ (?P<args>.+))?'
    result = re.match(pattern, message)
    if not result:
        return dict()
    result = result.groupdict()

    result['id'] = tuple(map(int, result['id'].split()))
    if result['args']:
        args = dict()
        pattern = r'(?P<name>\w+)=\"(?P<value>[a-zA-Z0-9\.\-]+)\"\ ?'
        for arg in re.finditer(pattern, result['args']):
            arg_dict = arg.groupdict()
            args[arg_dict['name']] = arg_dict['value']
        result['args'] = args

    return result


def test_runs(log_entries) -> tuple:
    """
    Test entries generator

    :param log_entries: log entries generated by log_entries
    :return: current log of current test
    """
    test_log_entries = []
    test_info = dict()

    for header, message in log_entries:
        log_level, who1, who2, *other = header.split()

        if who1 + who2 == 'TesterControl':
            current_test_info = get_test_info(message)

            if current_test_info['type'] in {'PASSED', 'FAILED'}:
                if test_info['id'] == current_test_info['id']:
                    yield test_info, test_log_entries
            elif current_test_info['type'] == 'TEST':
                test_info = current_test_info

            test_log_entries.clear()

        test_log_entries.append((header, message))


def checked_json_loads(string):
    """
    Wrapper for json.loads with validate JSON

    :param string:
    :return: dict representation of JSON or None if JSON no valid
    """
    try:
        return json.loads(string)
    except ValueError:
        return


def get_fio_result(test_entries) -> dict:
    """
    Search in test entries fio report and return one

    :param test_entries: test entries generated by test_runs
    :return: dict with converted fio json output
             or None if fio report no found
    """
    fio_result_label = 'FIO result.json:\n'
    for header, message in test_entries:
        if fio_result_label in message:
            return checked_json_loads(message[len(fio_result_label):])


def process_fio_result(fio_result: dict) -> dict:
    """
    Read from fio output need values

    :param fio_result: fio output in
    :return: fio report as dict
    """
    first_job = fio_result['jobs'][0]
    result = dict()
    for io_type in 'read', 'write':
        values = first_job[io_type]
        if values['lat_ns']['mean'] == 0:
            continue
        for param in ['bw_mean']:
            result[io_type[0] + param] = values[param]
        for param in ['mean']:
            result[io_type[0] + 'lat_' + param] = values['lat_ns'][param]
    return result


def get_performance(runs, test_types: dict) -> dict:
    """
    Generate performance from te log

    :param runs:        test entries generator
    :param test_types:  dict to determine the type of test
    :return: performance object with the structure described above
    """
    perf = dict()
    for test_info, test_entries in runs:
        if test_info['name'] not in test_types:
            continue
        args = test_info['args']
        test_type = test_types[test_info['name']]
        if test_type not in args:
            continue
        fio_result = get_fio_result(test_entries)
        if not fio_result:
            print("No found fio_result for test {name}%{tin} with "
                  "hash={hash}".format(**test_info), file=sys.stderr)
            continue
        report = process_fio_result(fio_result)
        report[test_type] = args.pop(test_type)

        test_params_values = tuple(args.values())
        test_params_fields = args.keys()

        if test_type not in perf:
            perf[test_type] = dict()

        if test_params_values not in perf[test_type]:
            perf[test_type][test_params_values] = dict(
                fields=test_params_fields, reports=[report])
            continue
        perf[test_type][test_params_values]['reports'].append(report)

    return perf


def get_unit(field):
    if 'lat' in field:
        return 'usec', lambda x: x / 1000.0
    elif 'bw' in field:
        return 'MB/sec', lambda x: x / 1000.0


def print_performance_hr(perf) -> None:
    """
    Print performance as human-readable

    :param perf: performance object with the structure described above
    """
    cell_format = '|{:^11}'
    value_format = '{:.03f}'

    def make_header(reports, ttype, header_type):
        yield ttype if header_type == 'fields' else ''
        first_report = filter(lambda x: x != ttype, reports[0])
        for field in first_report:
            yield get_unit(field)[0] if header_type == 'units' else field

    def make_table(reports, ttype):
        for report in reports:
            row = [report[ttype]]
            row += (value_format.format(get_unit(f)[1](v))
                    for f, v in report.items() if f != ttype)
            yield row

    for test_type, runs in perf.items():
        print('\n')
        for params, data in runs.items():
            args_name_gen = map('{}={}'.format, data['fields'], params)
            print(' '.join(args_name_gen))

            reports = data['reports']
            fields_header = list(make_header(reports, test_type, 'fields'))
            unit_header = list(make_header(reports, test_type, 'units'))
            table = list(make_table(reports, test_type))

            row_cells = cell_format * len(fields_header)
            print(row_cells.format(*fields_header))
            print(row_cells.format(*unit_header))
            print('\n'.join(map(lambda row: row_cells.format(*row), table)))
            print()


def main(args):
    test_types = {
        'fio_1req_latency': 'iodepth',        
        'fio_simple_qd': 'iodepth',
        'fio_simple': 'iodepth',
        'fio_simple_base_throughput_reduced': 'numjobs',
        'fio_simple_base_throughput': 'numjobs',
        'fio_simple_threads_mixed_throughput': 'numjobs',
        'fio_simple_threads': 'numjobs',
        'fio_simple_bs_iterate_throughput': 'iodepth',
    }
    entries = log_entries(args.te_log)
    runs = test_runs(entries)
    perf = get_performance(runs, test_types)
    if args.mode == 'human_readable':
        print_performance_hr(perf)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument('-l', '--te_log', default='log.txt',
                        help='Path to TE log')
    parser.add_argument('-m', '--mode', help='Output mode type',
                        choices=['human_readable'], default='human_readable')
    main(parser.parse_args())
