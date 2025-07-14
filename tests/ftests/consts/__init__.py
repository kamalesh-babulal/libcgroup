# SPDX-License-Identifier: LGPL-2.1-only
#
# Constants for the libcgroup functional tests
#
# Copyright (c) 2019-2025 Oracle and/or its affiliates.
# Author: Tom Hromatka <tom.hromatka@oracle.com>
# Author: Kamalesh Babulal <kamalesh.babulal@oracle.com>
#

from . import consts_ubuntu
import os

DEFAULT_LOG_FILE = 'libcgroup-ftests.log'

LOG_CRITICAL = 1
LOG_WARNING = 5
LOG_DEBUG = 8
DEFAULT_LOG_LEVEL = 5

ftest_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
tests_dir = os.path.dirname(ftest_dir)
LIBCG_MOUNT_POINT = os.path.dirname(tests_dir)

DEFAULT_CONTAINER_NAME = 'TestLibcg'
DEFAULT_CONTAINER_DISTRO = 'ubuntu'
DEFAULT_CONTAINER_RELEASE = '22.04'
DEFAULT_CONTAINER_ARCH = 'amd64'
DEFAULT_CONTAINER_STOP_TIMEOUT = 5

TESTS_RUN_ALL = -1
TESTS_RUN_ALL_SUITES = 'allsuites'
TEST_PASSED = 'passed'
TEST_FAILED = 'failed'
TEST_SKIPPED = 'skipped'

CGRULES_FILE = '/etc/cgrules.conf'

EXPECTED_PIDS_OUT = [
    """pids.current: 0
    pids.events: max 0
    pids.max: max
    """,
    """pids.current: 0
    pids.events: max 0
    pids.max: max
    pids.peak: 0
    """,
    # pids.events.local
    """pids.current: 0
    pids.events: max 0
    pids.max: max
    pids.events.local: max 0
    pids.peak: 0
    """
]

EXPECTED_CPU_OUT_V1 = consts_ubuntu.EXPECTED_CPU_OUT_V1
EXPECTED_CPU_OUT_V2 = consts_ubuntu.EXPECTED_CPU_OUT_V2


# get the current linux flavour
def get_distro(config):
    with open('/etc/os-release', 'r') as relfile:
        buf = relfile.read()
        if "Oracle Linux" in buf:
            return "oracle"
        elif "Ubuntu" in buf:
            return "ubuntu"

# vim: set et ts=4 sw=4:
