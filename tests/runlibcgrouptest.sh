#!/bin/bash
# usage ./runlibcgrouptest.sh
# Copyright IBM Corporation. 2008
#
# Author:	Sudhir Kumar <skumar@linux.vnet.ibm.com>
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of version 2.1 of the GNU Lesser General Public License
# as published by the Free Software Foundation.
#
# This program is distributed in the hope that it would be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
#
# Description: This script runs the the basic tests for testing libcgroup apis.
#

DEBUG=false;		# for debug messages
FS_MOUNTED=0;		# 0 for not mounted, 1 for mounted, 2 for multimounted
MOUNTPOINT=/dev/cgroup_controllers;	# Just to initialize
TARGET=/dev/cgroup_controllers;
CONTROLLERS=cpu,memory;
NUM_MOUNT=1;		# Number of places to be mounted on

debug()
{
	# Function parameter is the string to print out
	if $DEBUG
	then
		echo SH:DBG: $1;
	fi
}

check_mount_fs ()
{
	local NUM_MOUNT=0;
	CGROUP=`cat /proc/mounts|grep -w cgroup|tr -s [:space:]|cut -d" " -f3`;

	# get first word in case of multiple mounts
	CGROUP=`echo $CGROUP|cut -d" " -f1`;

	debug "check_mount_fs(): CGROUP is $CGROUP";
	if [ "$CGROUP" = "cgroup" ]
	then
		NUM_MOUNT=`cat /proc/mounts|grep -w cgroup|wc -l`;
		debug "check_mount_fs(): fs mounted at $NUM_MOUNT places";

		if [ $NUM_MOUNT -eq 1 ]
		then
			FS_MOUNTED=1;
		else
			# Any number of mounts is multi mount
			FS_MOUNTED=2;
		fi;
		return 0;	# True
	else
		FS_MOUNTED=0;
		return 1;	# false
	fi
}

umount_fs ()
{
	while check_mount_fs
	do
		PROC_ENTRY=`cat /proc/mounts|grep cgroup|tr -s [:space:]|cut -d" " -f2`;
		# Get first mountpoint in case of multiple mounts
		PROC_ENTRY=`echo $PROC_ENTRY|cut -d" " -f1`;
		if [ -n "$PROC_ENTRY" ]
		then
			TARGET=$PROC_ENTRY;
			# Need to take care if there are tasks running in any group ??
			# Also need to care if cpu and memory are mounted at different places
			rmdir $TARGET/* 2> /dev/null ;
			umount $TARGET;
			rmdir  $TARGET;
			debug "umounted $TARGET";
		fi;
	done
	FS_MOUNTED=0;
	TARGET=/dev/cgroup_controllers;	#??
	echo "Cleanup done";
}

# Check if kernel is not having any of the controllers enabled
no_controllers()
{
	CPU="";
	MEMORY="";
	if [ -e /proc/cgroups ]
	then
		CPU=`cat /proc/cgroups|grep -w cpu|cut -f1`;
		MEMORY=`cat /proc/cgroups|grep -w memory|cut -f1`;
	fi;

	if [ -n $CPU ] && [ -n $MEMORY ]
	then
		CONTROLLERS=$CPU,$MEMORY ;
		return 1;	# false
	elif [ -n $CPU ]
	then
		CONTROLLERS=$CPU ;
		return 1;	# false
	elif [ -n $MEMORY ]
	then
		CONTROLLERS=$MEMORY ;
		return 1;	# false
	fi;
	# Kernel has no controllers enabled
	return 0;	# true
}


mount_fs ()
{
	local NUM_MOUNT=0;	# On how many places to mount on
	local CUR_MOUNT=1;
	FS_MOUNTED=0;

	# Check if kernel has controllers enabled
	if no_controllers
	then
		echo "Kernel has no controllers enabled";
		echo "Recompile your kernel with controllers enabled"
		echo "Exiting the tests.....";
		exit 1;
	fi;

	# At least one Controller is enabled. So proceed further.
	if [ -z $1 ]
	then
		echo "WARN: No parameter passed to function mount_fs";
		echo "taking default as 0....So not mounting cgroup fs";
	else
		NUM_MOUNT=$1;
		debug "mount_fs fs will be mounted on $NUM_MOUNT places";
	fi;

	# create so many directories i.e. mountpoints
	while [ $NUM_MOUNT -ge $CUR_MOUNT ]
	do
		NEWTARGET="$TARGET-$CUR_MOUNT";
		if [ -e $NEWTARGET ]
		then
			echo "WARN: $NEWTARGET already exist..overwriting";
			check_mount_fs;	# Possibly fs might be mounted on it
			if [ $FS_MOUNTED -gt 0 ]
			then
				umount_fs;
			else
				rmdir $NEWTARGET ;
			fi;
		fi;
		mkdir $NEWTARGET;

		# In case of multimount, mount controllers at diff points
		if $MULTIMOUNT ; then
			if [ $CPU ] && [ $MEMORY ] ; then
				if [ $CUR_MOUNT -eq 1 ] ; then
					CONTROLLERS="cpu";
				else
					CONTROLLERS="memory";
				fi;
			else
				echo "Only 1 controleer enabled in kernel";
				echo "So not running multiple mount testcases";
				exit 1;
			fi;
		fi;

		mount -t cgroup -o $CONTROLLERS cgroup $NEWTARGET; # 2> /dev/null?
		if [ $? -ne 0 ]
		then
			echo "ERROR: Could not mount cgroup filesystem on $NEWTARGET."
			echo "Exiting test";
			umount_fs;
			exit -1;
		fi;
		CUR_MOUNT=`expr $CUR_MOUNT + 1`;
		FS_MOUNTED=`expr $FS_MOUNTED + 1`;

		# Group created earlier may again be visible if not cleaned.
		# So clean them all
		if [ -e $NEWTARGET/group1 ] # first group that is created
		then
			# Need to handle if tasks are running in them
			rmdir $NEWTARGET/group*
			echo "WARN: Earlier groups found and removed...";
		fi;

		debug "cgroup filesystem mounted on $NEWTARGET  directory"
	done;

	if [ $FS_MOUNTED -gt 2 ]
	then
		FS_MOUNTED=2;
	fi;

}

get_mountpoint()
{
	# need to handle how to pass multiple mount points to C file
	# It will depend on the requirements if any
	MOUNTPOINT=`cat /proc/mounts|grep -w cgroup|tr -s [:space:]| \
							cut -d" " -f2`;
	debug "mountpoint is $MOUNTPOINT"
}
runtest()
{
	MOUNT_INFO=$1;
	TEST_EXEC=$2;
	if [ -f $TEST_EXEC ]
	then
		./$TEST_EXEC $MOUNT_INFO $MOUNTPOINT;
		if [ $? -ne 0 ]
		then
			echo Error in running ./$TEST_EXEC
			echo Exiting tests.
		else
			PID=$!;
		fi;
	else
		echo Sources not compiled. please run make;
	fi
}
# TestSet01: Run tests without mounting cgroup filesystem
	echo;
	echo Running first set of testcases;
	echo ==============================
	FS_MOUNTED=0;
	FILE=libcgrouptest01;
	check_mount_fs;
	# unmount fs if already mounted
	if [ $FS_MOUNTED -ne 0 ]
	then
		umount_fs;
	fi;
	debug "FS_MOUNTED = $FS_MOUNTED"
	runtest $FS_MOUNTED $FILE

	wait $PID;
	RC=$?;
	if [ $RC -ne 0 ]
	then
		echo Test binary $FILE exited abnormaly with return value $RC;
		# Do not exit here. Failure in this case does not imply
		# failure in other cases also
	fi;

# TestSet02: Run tests with mounting cgroup filesystem
	echo;
	echo Running second set of testcases;
	echo ==============================
	FILE=libcgrouptest01;
	check_mount_fs;
	# mount fs at one point if not already mounted or multimounted
	NUM_MOUNT=1;
	if [ $FS_MOUNTED -eq 0 ]
	then
		mount_fs $NUM_MOUNT;
	elif [ $FS_MOUNTED -gt 1 ]
	then
		umount_fs;
		mount_fs $NUM_MOUNT;
	fi;
	debug "FS_MOUNTED = $FS_MOUNTED"
	get_mountpoint;
	runtest $FS_MOUNTED $FILE

	wait $PID;
	RC=$?;
	if [ $RC -ne 0 ]
	then
		echo Test binary $FILE exited abnormaly with return value $RC;
	fi;
	umount_fs;


# TestSet03: Run tests with mounting cgroup filesystem at multiple points
	echo;
	echo Running third set of testcases;
	echo ==============================
	FILE=libcgrouptest01;
	check_mount_fs;
	# mount fs at multiple points
	MULTIMOUNT=true;
	NUM_MOUNT=2;
	if [ $FS_MOUNTED -eq 0 ]
	then
		mount_fs $NUM_MOUNT;
	elif [ $FS_MOUNTED -eq 1 ]
	then
		umount_fs;
		mount_fs $NUM_MOUNT;
	fi;
	debug "FS_MOUNTED = $FS_MOUNTED"
	get_mountpoint;
	runtest $FS_MOUNTED $FILE

	wait $PID;
	RC=$?;
	if [ $RC -ne 0 ]
	then
		echo Test binary $FILE exited abnormaly with return value $RC;
	fi;
	umount_fs;

	exit 0;
