#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/xattr.h>
#include <unistd.h>
#include <systemd/sd-bus.h>
#include <libcgroup.h>
#include <dirent.h>

#define CG_SYSTEMD_USER_SLICE_NAME "user.slice"
#define CG_SCOPE_TREE_ROOT "/sys/fs/cgroup/unified/"

enum SYSD_UNIT_MODE {SYSD_UNIT_MODE_FAIL, SYSD_UNIT_MODE_REPLACE, SYSD_UNIT_MODE_ISOLATE,
					SYSD_UNIT_MODE_IGN_DEPS, SYSD_UNIT_MODE_IGN_REQS};

char SYSD_UNIT_MODE_NAMES[5][20] = {"fail", "replace", "isolate","ignore-dependencies", "ignore-requirements"};

void permanent_sleep(){
	while (1){
		sleep(10000);
	}
}

int match_ends(char *a, char *b){
	int a_len = strlen(a), b_len = strlen(b);

	while((a_len >= 0) && (b_len >= 0)){
		if (a[a_len] != b[b_len]){
			return 0;
		}
		a_len = a_len - 1;
		b_len = b_len - 1;
	}
	return 1;
}

int find_path(char *file_name, char *path_name_in, char *located){

	char path_name_out[FILENAME_MAX], path_name_child[FILENAME_MAX];
	struct dirent *entry;
	int ret = 0;
	DIR *path;

	if (strlen(path_name_in) == 0){
		strncpy(path_name_out, "/sys/fs/cgroup/unified/", FILENAME_MAX);
	}
	else {
		strncpy(path_name_out, path_name_in, FILENAME_MAX);
	}

	path = opendir(path_name_out);
	if (path){
		while((entry = readdir(path)) != NULL){
			if (match_ends(file_name, entry->d_name)){
				strncpy(path_name_child, path_name_out, FILENAME_MAX);
				strncat(path_name_child, entry->d_name, strlen(entry->d_name));
				ret = 1;
				break;
			}
			else if (match_ends(entry->d_name, ".slice")){
				strncpy(path_name_child, path_name_out, FILENAME_MAX);
				strncat(path_name_child, entry->d_name, strlen(entry->d_name));
				strncat(path_name_child, "/", FILENAME_MAX - strlen(path_name_child));

				if(find_path(file_name, path_name_child, located)){
					ret = 2;
					break;
				}
				memset(path_name_child, 0, FILENAME_MAX);
			}
			else if (match_ends(entry->d_name, ".scope")){
				strncpy(path_name_child, path_name_out, FILENAME_MAX);
				strncat(path_name_child, entry->d_name, strlen(entry->d_name));
				strncat(path_name_child, "/", FILENAME_MAX - strlen(path_name_child));

				if(find_path(file_name, path_name_child, located)){
					ret = 2;
					break;
				}
				memset(path_name_child, 0, FILENAME_MAX);
			}
		}
		closedir(path);
	}

	if (ret == 1){
		strncpy(located, path_name_child, FILENAME_MAX);
	}
	else if (ret == 0){
		located = NULL;
	}

	return ret;
}

/*
 * @todo Improve error messages for create_scope_and_slice & create_scope_user_slice; look at cg_delegate.py
 */

int cgroup_create_scope_and_slice(char *scope_name, char *slice_name, int delegated, enum SYSD_UNIT_MODE mode){

	sd_bus_error error = SD_BUS_ERROR_NULL;
	sd_bus_message *m = NULL, *reply = NULL;
	sd_bus *bus = NULL;
	int ret = 0;

	int child_pid = fork();

	if (child_pid < 0){
		fprintf(stderr, "fork() failed: %d:%s\n", child_pid, strerror(-child_pid));
	}
	else if (child_pid == 0){
		permanent_sleep();
	}
	else{
		ret = sd_bus_open_system(&bus);
		if (ret<0){
			fprintf(stderr, "sd_bus_open_system failed: %d:%s\n", ret, strerror(-ret));
			goto out;
		}

		ret = sd_bus_message_new_method_call(bus, &m, "org.freedesktop.systemd1",
			"/org/freedesktop/systemd1", "org.freedesktop.systemd1.Manager",
			 "StartTransientUnit");
		if (ret<0){
			fprintf(stderr, "sd_bus_message_new_method_call failed: %d:%s\n", ret, strerror(-ret));
			goto out;
		}

		fprintf(stderr, "scope %s\n", scope_name);
		//ret = sd_bus_message_append(m, "ss", scope_name, SYSD_UNIT_MODE_NAMES[mode]);
		ret = sd_bus_message_append(m, "ss", scope_name, "fail");
		if (ret<0){
			fprintf(stderr, "1st sd_bus_message_append failed: %d:%s\n", ret, strerror(-ret));
			goto out;
		}

		ret = sd_bus_message_open_container(m, 'a', "(sv)");
		if (ret<0)
			{fprintf(stderr, "sd_bus_message_open_container failed: %d:%s\n", ret, strerror(-ret));
			goto out;
		}

		ret = sd_bus_message_append(m, "(sv)", "PIDs", "au", 1, (uint32_t) child_pid);
		if (ret<0){
			fprintf(stderr, "2nd sd_bus_message_append failed: %d:%s\n", ret, strerror(-ret));
			goto out;
		}

		if(delegated == 1){
			ret = sd_bus_message_append(m, "(sv)", "Delegate", "b", 1);
			if (ret<0){
				fprintf(stderr, "3rd sd_bus_message_append failed: %d:%s\n", ret, strerror(-ret));
				goto out;
			}
		}


		fprintf(stderr, "slice %s\n", slice_name);
		ret = sd_bus_message_append(m, "(sv)", "Slice", "s", slice_name);
		if (ret<0){
			fprintf(stderr, "4th sd_bus_message_append failed: %d:%s\n", ret, strerror(-ret));
			goto out;
		}

		ret = sd_bus_message_close_container(m);
		if (ret<0){
			fprintf(stderr, "sd_bus_message_close failed: %d:%s\n", ret, strerror(-ret));
			goto out;
		}

		ret = sd_bus_message_append(m, "a(sa(sv))", 0);
		if (ret<0){
			fprintf(stderr, "5th sd_bus_message_append failed: %d:%s\n", ret, strerror(-ret));
			goto out;
		}

		ret = sd_bus_call(bus, m, 0, &error, &reply);
		if (ret<0){
			fprintf(stderr, "sd_bus_call failed: %d:%s\n", ret, strerror(-ret));
			goto out;
		}

		ret = child_pid;
	}

out:
	if(m != NULL){
		sd_bus_message_unref(m);
	}
	if(reply != NULL){
		sd_bus_message_unref(reply);
	}
	if(bus != NULL){
		sd_bus_unref(bus);
	}

	return ret;
}


int cgroup_create_scope_user_slice(char *scope_name, int delegated, enum SYSD_UNIT_MODE mode){
	return cgroup_create_scope_and_slice(scope_name, CG_SYSTEMD_USER_SLICE_NAME, delegated, mode);
}
/*
int cgroup_create_scope_user_slice(char *scope_name, int delegated, enum SYSD_UNIT_MODE mode){

	sd_bus_error error = SD_BUS_ERROR_NULL;
	sd_bus_message *m = NULL, *reply = NULL;
	sd_bus *bus = NULL;
	int ret = 0;

	int child_pid = fork();

	if(child_pid < 0){
		fprintf(stderr, "fork() failed: %d:%s\n", child_pid, strerror(-child_pid));
	}
	else if (child_pid == 0){
		permanent_sleep();
	}
	else{
		ret = sd_bus_open_system(&bus);
		if (ret<0){
			fprintf(stderr, "sd_bus_open_system failed: %d:%s\n", ret, strerror(-ret));
			goto out;
		}

		ret = sd_bus_message_new_method_call(bus, &m, "org.freedesktop.systemd1",
			"/org/freedesktop/systemd1", "org.freedesktop.systemd1.Manager",
			 "StartTransientUnit");
		if (ret<0){
			fprintf(stderr, "sd_bus_message_new_method_call failed: %d:%s\n", ret, strerror(-ret));
			goto out;
		}

		ret = sd_bus_message_append(m, "ss", scope_name, SYSD_UNIT_MODE_NAMES[mode]);
		if (ret<0){
			fprintf(stderr, "1st sd_bus_message_append failed: %d:%s\n", ret, strerror(-ret));
			goto out;
		}

		ret = sd_bus_message_open_container(m, 'a', "(sv)");
		if (ret<0)
			{fprintf(stderr, "sd_bus_message_open_container failed: %d:%s\n", ret, strerror(-ret));
			goto out;
		}

		ret = sd_bus_message_append(m, "(sv)", "PIDs", "au", 1, (uint32_t) getpid());
		if (ret<0){
			fprintf(stderr, "2nd sd_bus_message_append failed: %d:%s\n", ret, strerror(-ret));
			goto out;
		}

		if(delegated == 1){
			ret = sd_bus_message_append(m, "(sv)", "Delegate", "b", 1);
			if (ret<0){
				fprintf(stderr, "3rd sd_bus_message_append failed: %d:%s\n", ret, strerror(-ret));
				goto out;
			}
		}

		ret = sd_bus_message_append(m, "(sv)", "Slice", "s", CG_SYSTEMD_USER_SLICE_NAME);
		if (ret<0){
			fprintf(stderr, "4th sd_bus_message_append failed: %d:%s\n", ret, strerror(-ret));
			goto out;
		}

		ret = sd_bus_message_close_container(m);
		if (ret<0){
			fprintf(stderr, "sd_bus_message_close failed: %d:%s\n", ret, strerror(-ret));
			goto out;
		}

		ret = sd_bus_message_append(m, "a(sa(sv))", 0);
		if (ret<0){
			fprintf(stderr, "5th sd_bus_message_append failed: %d:%s\n", ret, strerror(-ret));
			goto out;
		}

		ret = sd_bus_call(bus, m, 0, &error, &reply);
		if (ret<0){
			fprintf(stderr, "sd_bus_call failed: %d:%s\n", ret, strerror(-ret));
			goto out;
		}

		ret = child_pid;
	}

out:
	if(m != NULL){
		sd_bus_message_unref(m);
	}
	if(reply != NULL){
		sd_bus_message_unref(reply);
	}
	if(bus != NULL){
		sd_bus_unref(bus);
	}

	return ret;
}
*/

int cgroup_is_delegated(char *scope_name){

	char file_path[FILENAME_MAX], file_root[FILENAME_MAX], result[FILENAME_MAX];
	int ret = 0;

	find_path(scope_name, file_root, file_path);
	if(file_path[0] == 0){
		// @TODO Does this need a specific error message number to show desired scope doesn't exit?
		return ret;
	}

	ret = getxattr(scope_name, "trusted.delegate", result, FILENAME_MAX);
	if (ret<0){
		fprintf(stderr, "getxattr failed: %d:%s\n", ret, strerror(-ret));
		fflush(stderr);
	}
	else if (strncmp(result, "1", 1)==0){
		fprintf(stdout, "Successfully Delegated Scope!\n");
		ret=1;
	}

	return ret;
}

#if 0
int main (int argc, char* argv[]){

	char file_path[FILENAME_MAX], file_root[FILENAME_MAX];
	enum SYSD_UNIT_MODE mode = SYSD_UNIT_MODE_FAIL;
	int ret = 0;

	find_path("testing.scope",file_root, file_path);
	fprintf(stdout, "Find Path Succeeds Intentionally: %s\n", file_path);
	ret = cgroup_is_delegated("does-not-exist.scope");
	fprintf(stdout, "Cgroup Is Delegated Fails Intentionally: %d\n", ret);

	ret = cgroup_create_scope_and_slice("testing.scope","testing.slice", 1, mode);
	fprintf(stdout, "Create Scope And Slice: %d\n", ret);
	ret = cgroup_is_delegated("/sys/fs/cgroup/unified/testing.slice/testing.slice/testing.scope");
	fprintf(stdout, "Check Delegated: %d\n", ret);
	sleep(100); // Allow time to check manually

	ret = cgroup_create_scope_user_slice("user-delegated.scope", 1, mode);
	fprintf(stdout, "Create Scope User Slice: %d\n", ret);
	ret = cgroup_is_delegated("/sys/fs/cgroup/unified/user.slice/user-delegated.scope");
	fprintf(stdout, "Check Delegated: %d\n", ret);
	fprintf(stdout, "%d\n", getpid());

//	fprintf(stdout, "Sucess!\n");
	fflush(stdout);
	fflush(stderr);
	sleep(100); // Allow time to check manually

}
#endif
