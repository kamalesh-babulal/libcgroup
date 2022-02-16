#!/usr/bin/env python3
#
# Advanced cgset functionality test - set multiple cgroups' values
#
# Copyright (c) 2021 Oracle and/or its affiliates.
# Author: Tom Hromatka <tom.hromatka@oracle.com>
#

#
# This library is free software; you can redistribute it and/or modify it
# under the terms of version 2.1 of the GNU Lesser General Public License as
# published by the Free Software Foundation.
#
# This library is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License
# for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with this library; if not, see <http://www.gnu.org/licenses>.
#

from cgroup import Cgroup, CgroupVersion
import consts
import ftests
import sys
import os

CONTROLLER = 'memory'
CGNAME1 = '025cgset1'
CGNAME2 = '025cgset2'

SETTING = 'memory.swappiness'
VALUE = '42'


def prereqs(config):
    result = consts.TEST_PASSED
    cause = None

    if CgroupVersion.get_version('memory') != CgroupVersion.CGROUP_V1:
        result = consts.TEST_SKIPPED
        cause = 'This test requires the cgroup v1 memory controller'
        return result, cause

    return result, cause


def setup(config):
    Cgroup.create(config, CONTROLLER, CGNAME1)
    Cgroup.create(config, CONTROLLER, CGNAME2)


def test(config):
    Cgroup.set(config, cgname=[CGNAME1, CGNAME2], setting=SETTING, value=VALUE)

    Cgroup.get_and_validate(config, CGNAME1, SETTING, VALUE)
    Cgroup.get_and_validate(config, CGNAME2, SETTING, VALUE)

    return consts.TEST_PASSED, None


def teardown(config):
    Cgroup.delete(config, CONTROLLER, CGNAME1)
    Cgroup.delete(config, CONTROLLER, CGNAME2)


def main(config):
    [result, cause] = prereqs(config)
    if result != consts.TEST_PASSED:
        return [result, cause]

    setup(config)
    [result, cause] = test(config)
    teardown(config)

    return [result, cause]


if __name__ == '__main__':
    config = ftests.parse_args()
    # this test was invoked directly.  run only it
    config.args.num = int(os.path.basename(__file__).split('-')[0])
    sys.exit(ftests.main(config))

# vim: set et ts=4 sw=4:
