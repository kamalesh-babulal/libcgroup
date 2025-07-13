# SPDX-License-Identifier: LGPL-2.1-only
#
# Constants for the libcgroup functional tests (Ubuntu)
#
# Copyright (c) 2019-2025 Oracle and/or its affiliates.
#
# Author: Tom Hromatka <tom.hromatka@oracle.com>
# Author: Kamalesh Babulal <kamalesh.babulal@oracle.com>


EXPECTED_CPU_OUT_V1 = [
    """cpu.cfs_period_us: 100000
    cpu.stat: nr_periods 0
            nr_throttled 0
            throttled_time 0
    cpu.shares: 1024
    cpu.cfs_quota_us: -1
    cpu.uclamp.min: 0.00
    cpu.uclamp.max: max""",
    # cfs_bandwidth without cpu.stat nr_busts, burst_time
    """cpu.cfs_burst_us: 0
    cpu.cfs_period_us: 100000
    cpu.stat: nr_periods 0
            nr_throttled 0
            throttled_time 0
    cpu.shares: 1024
    cpu.idle: 0
    cpu.cfs_quota_us: -1
    cpu.uclamp.min: 0.00
    cpu.uclamp.max: max""",
    # cfs_bandwidth with cpu.stat nr_busts, burst_time
    """cpu.cfs_burst_us: 0
    cpu.cfs_period_us: 100000
    cpu.stat: nr_periods 0
            nr_throttled 0
            throttled_time 0
            nr_bursts 0
            burst_time 0
    cpu.shares: 1024
    cpu.idle: 0
    cpu.cfs_quota_us: -1
    cpu.uclamp.min: 0.00
    cpu.uclamp.max: max""",
    # cfs_bandwidth with cpu.stat nr_busts, burst_time
    """cpu.cfs_burst_us: 0
    cpu.cfs_period_us: 100000
    cpu.stat: nr_periods 0
            nr_throttled 0
            throttled_time 0
            nr_bursts 0
            burst_time 0
    cpu.shares: 1024
    cpu.idle: 0
    cpu.stat.local: throttled_time 0
    cpu.cfs_quota_us: -1
    cpu.uclamp.min: 0.00
    cpu.uclamp.max: max"""
]

EXPECTED_CPU_OUT_V2 = [
    """cpu.weight: 100
    cpu.stat: usage_usec 0
            user_usec 0
            system_usec 0
            nr_periods 0
            nr_throttled 0
            throttled_usec 0
    cpu.weight.nice: 0
    cpu.pressure: some avg10=0.00 avg60=0.00 avg300=0.00 total=0
    cpu.max: max 100000
    cpu.uclamp.min: 0.00
    cpu.uclamp.max: max""",
    # with PSI
    """cpu.weight: 100
    cpu.stat: usage_usec 0
            user_usec 0
            system_usec 0
            nr_periods 0
            nr_throttled 0
            throttled_usec 0
    cpu.weight.nice: 0
    cpu.pressure: some avg10=0.00 avg60=0.00 avg300=0.00 total=0
            full avg10=0.00 avg60=0.00 avg300=0.00 total=0
    cpu.max: max 100000
    cpu.uclamp.min: 0.00
    cpu.uclamp.max: max""",
    # with PSI, cfs_bandwidth without cpu.stat nr_busts, burst_time
    """cpu.weight: 100
    cpu.stat: usage_usec 0
            user_usec 0
            system_usec 0
            nr_periods 0
            nr_throttled 0
            throttled_usec 0
    cpu.weight.nice: 0
    cpu.pressure: some avg10=0.00 avg60=0.00 avg300=0.00 total=0
            full avg10=0.00 avg60=0.00 avg300=0.00 total=0
    cpu.idle: 0
    cpu.max.burst: 0
    cpu.max: max 100000
    cpu.uclamp.min: 0.00
    cpu.uclamp.max: max""",
    # with PSI, cfs_bandwidth with cpu.stat nr_busts, burst_time
    """cpu.weight: 100
    cpu.stat: usage_usec 0
            user_usec 0
            system_usec 0
            nr_periods 0
            nr_throttled 0
            throttled_usec 0
            nr_bursts 0
            burst_usec 0
    cpu.weight.nice: 0
    cpu.pressure: some avg10=0.00 avg60=0.00 avg300=0.00 total=0
            full avg10=0.00 avg60=0.00 avg300=0.00 total=0
    cpu.idle: 0
    cpu.max.burst: 0
    cpu.max: max 100000
    cpu.uclamp.min: 0.00
    cpu.uclamp.max: max""",
    # with PSI, cfs_bandwidth with cpu.stat nr_busts, burst_time, force_idle
    """cpu.weight: 100
    cpu.stat: usage_usec 0
            user_usec 0
            system_usec 0
            core_sched.force_idle_usec 0
            nr_periods 0
            nr_throttled 0
            throttled_usec 0
            nr_bursts 0
            burst_usec 0
    cpu.weight.nice: 0
    cpu.pressure: some avg10=0.00 avg60=0.00 avg300=0.00 total=0
            full avg10=0.00 avg60=0.00 avg300=0.00 total=0
    cpu.idle: 0
    cpu.max.burst: 0
    cpu.max: max 100000
    cpu.uclamp.min: 0.00
    cpu.uclamp.max: max""",
    # with PSI, cfs_bandwidth with cpu.stat nr_busts, burst_time, force_idle
    # cpu.stat.local
    """cpu.weight: 100
    cpu.stat: usage_usec 0
            user_usec 0
            system_usec 0
            core_sched.force_idle_usec 0
            nr_periods 0
            nr_throttled 0
            throttled_usec 0
            nr_bursts 0
            burst_usec 0
    cpu.weight.nice: 0
    cpu.pressure: some avg10=0.00 avg60=0.00 avg300=0.00 total=0
            full avg10=0.00 avg60=0.00 avg300=0.00 total=0
    cpu.idle: 0
    cpu.stat.local: throttled_usec 0
    cpu.max.burst: 0
    cpu.max: max 100000
    cpu.uclamp.min: 0.00
    cpu.uclamp.max: max""",
    # with PSI, cfs_bandwidth with cpu.stat nr_busts, burst_time, force_idle
    # cpu.stat.local, cgroup bstat with nice_usec
    """cpu.weight: 100
    cpu.stat: usage_usec 0
            user_usec 0
            system_usec 0
            nice_usec 0
            core_sched.force_idle_usec 0
            nr_periods 0
            nr_throttled 0
            throttled_usec 0
            nr_bursts 0
            burst_usec 0
    cpu.weight.nice: 0
    cpu.pressure: some avg10=0.00 avg60=0.00 avg300=0.00 total=0
            full avg10=0.00 avg60=0.00 avg300=0.00 total=0
    cpu.idle: 0
    cpu.stat.local: throttled_usec 0
    cpu.max.burst: 0
    cpu.max: max 100000
    cpu.uclamp.min: 0.00
    cpu.uclamp.max: max"""
]

# vim: set et ts=4 sw=4:
