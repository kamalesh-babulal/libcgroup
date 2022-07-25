#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/xattr.h>
#include <unistd.h>
#include <systemd/sd-bus.h>

#ifndef __LIBC_SYSTEMD
#define __LIBC_SYSTEMD
/**
 * Function to create a scope of specified name within the specified slice;
 * the scope is delegated if "delegated" = 1, and mode is expected to be one
 * of the defined unit modes.
 */
int cgroup_create_scope_and_slice(char *scope_name, char *slice_name, int delegated, char* mode);

/**
 * Function to create a scope of specified name within the user's slice
 * ("user.slice"); the scope is delegated if "delegated" = 1, and mode is
 * expected to be one of the defined unit modes.
 */
int cgroup_create_scope_user_slice(char *scope_name, int delegated, char *mode);
/**
 * Checks whether the slice/scope specified by the given path is delegated.
 * Returns 0 if the specified scope is not delegated, 1 if it is delegated,
 * and a negative error code if a failure occurs.
 */
int cgroup_is_delegated(char *path);
#endif
