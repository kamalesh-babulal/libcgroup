// SPDX-License-Identifier: LGPL-2.1-only
/**
 * Libcgroup extended cgxset.  Works with both cgroup v1 and v2
 *
 * Copyright (c) 2021-2022 Oracle and/or its affiliates.
 * Author: Tom Hromatka <tom.hromatka@oracle.com>
 */
#include "tools-common.h"
#include "abstraction-common.h"

#include <libcgroup.h>
#include <libcgroup-internal.h>

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <errno.h>
#include <stdio.h>

#define FL_RULES	1
#define FL_COPY		2

enum {
	COPY_FROM_OPTION = CHAR_MAX + 1
};

#ifndef UNIT_TEST
static const struct option long_options[] = {
	{"v1",			      no_argument, NULL, '1'},
	{"v2",			      no_argument, NULL, '2'},
	{"ignore-unmappable",	      no_argument, NULL, 'i'},
	{"rule",		required_argument, NULL, 'r'},
	{"help",		      no_argument, NULL, 'h'},
	{"copy-from",		required_argument, NULL, COPY_FROM_OPTION},
	{NULL, 0, NULL, 0}
};

int flags; /* used input method */

static char *program_name;

/* cgroup.subtree_control -r name, value */
static struct control_value *cgrp_subtree_ctrl_val;

static struct cgroup *copy_name_value_from_cgroup(char src_cgrp_path[FILENAME_MAX])
{
	struct cgroup *src_cgrp;
	int ret = 0;

	/* create source cgroup */
	src_cgrp = cgroup_new_cgroup(src_cgrp_path);
	if (!src_cgrp) {
		err("can't create cgroup: %s\n", cgroup_strerror(ECGFAIL));
		goto scgrp_err;
	}

	/* copy the name-version values to the cgroup structure */
	ret = cgroup_get_cgroup(src_cgrp);
	if (ret != 0) {
		err("cgroup %s error: %s\n", src_cgrp_path, cgroup_strerror(ret));
		goto scgrp_err;
	}

	return src_cgrp;

scgrp_err:
	cgroup_free(&src_cgrp);

	return NULL;
}

static struct cgroup *create_and_copy_cgroup(struct cgroup *src_cgrp, const char * const new_cgrp)
{
	struct cgroup *cgrp;
	int ret = 0;

	/* create new cgroup */
	cgrp = cgroup_new_cgroup(new_cgrp);
	if (!cgrp) {
		err("%s: can't add new cgroup: %s\n", program_name, cgroup_strerror(ret));
		return NULL;
	}

	/* copy the values from the source cgroup to new one */
	ret = cgroup_copy_cgroup(cgrp, src_cgrp);
	if (ret != 0) {
		err("%s: cgroup %s error: %s\n", program_name, src_cgrp->name,
		    cgroup_strerror(ret));
		goto err;
	}

	return cgrp;

err:
	cgroup_free(&cgrp);
	return NULL;
}

static int cgroup_set_cgroup_values(struct cgroup *src_cgrp, const char * const new_cgrp,
				    bool ignore_unmappable, enum cg_version_t src_version)
{
	struct cgroup *cgrp, *converted_src_cgrp;
	int ret = ECGFAIL;

	/* subtree_cgrp (src_cgrp) can be empty */
	if (!src_cgrp)
		return 0;

	cgrp = create_and_copy_cgroup(src_cgrp, new_cgrp);
	if (!cgrp)
		return ret;

	converted_src_cgrp = cgroup_new_cgroup(cgrp->name);
	if (converted_src_cgrp == NULL) {
		ret = ECGCONTROLLERCREATEFAILED;
		goto err;
	}

	/*
	 * If a failure occurs between this comment and the "End of above comment"
	 * below, then we need to manually free converted_src_cgrp.
	 */

	ret = cgroup_convert_cgroup(converted_src_cgrp, CGROUP_DISK, src_cgrp, src_version);
	if ((ret && ret != ECGNOVERSIONCONVERT) ||
	    (ret == ECGNOVERSIONCONVERT && !ignore_unmappable)) {
		/*
		 * If the user not has specified that we ignore any errors
		 * due to being unable to map from v1 to v2 or vice versa,
		 * return error, else ignore the error and continue.
		 */
		cgroup_free(&converted_src_cgrp);
		goto err;
	}

	/*
	 * End of above comment about converted_src_cgrp needing to be manually freed.
	 */
	cgroup_free(&cgrp);
	cgrp = converted_src_cgrp;

	/* modify cgroup based on values of the new one */
	ret = cgroup_modify_cgroup(cgrp);
	if (ret) {
		err("%s: cgroup modify error: %s\n", program_name, cgroup_strerror(ret));
		goto err;
	}

err:
	cgroup_free(&cgrp);
	return ret;
}

static int is_leaf_node(void **handle)
{
	struct cgroup_tree_handle *entry;
	FTSENT *ent;

	entry = (struct  cgroup_tree_handle *)  *handle;
	ent = fts_children(entry->fts, 0);
	if (!ent)
		return errno;

	while (ent != NULL) {
		/* return on the first child cgroup */
		if (ent->fts_info == FTS_D)
			return 0;

		ent = ent->fts_link;
	}

	return 1;
}

static int _cgroup_set_cgroup_values_r(struct cgroup *src_cgrp, const char * const new_cgrp,
				    bool ignore_unmappable, enum cg_version_t src_version,
				       bool post_order_walk)
{
	struct cgroup_controller *ctrl = NULL;
	bool subtree_control = false;
	struct cgroup_file_info info;
	int prefix_len;
	void *handle;
	int lvl, ret;

	ctrl = src_cgrp->controller[0];

	if (!strcmp(ctrl->values[0]->name, "cgroup.subtree_control"))
		subtree_control = true;

	ret = cgroup_walk_tree_begin(ctrl->name, new_cgrp, 0, &handle, &info, &lvl);
	if (ret) {
		err("%s: failed to walk the tree for cgroup %s controller %s\n",
		    program_name, new_cgrp, src_cgrp->controller[0]->name);

		return ret;
	}

	if (post_order_walk) {
		ret = cgroup_walk_tree_set_flags(&handle, CGROUP_WALK_TYPE_POST_DIR);
		if (ret) {
			err("%s: failed to set CGROUP_WALK_TYPE_POST_DIR flag\n", program_name);
			goto err;
		}
	}

	prefix_len = strlen(info.full_path) - strlen(new_cgrp) - 1;

	/* In post order cgroup tree walk, parent should be modify last */
	if (!post_order_walk) {
		ret = cgroup_set_cgroup_values(src_cgrp, &info.full_path[prefix_len],
					       ignore_unmappable, src_version);
		if (ret)
			goto err;
	}

	while ((ret = cgroup_walk_tree_next(0, &handle, &info, lvl)) == 0) {
		if (info.type == CGROUP_FILE_TYPE_DIR) {

			/* skip modify subtree_control file for the leaf nodes */
			if (subtree_control && is_leaf_node(&handle))
				continue;

			ret = cgroup_set_cgroup_values(src_cgrp, &info.full_path[prefix_len],
						       ignore_unmappable, src_version);
			if (ret)
				goto err;
		}
	}

	if (post_order_walk) {
		ret = cgroup_set_cgroup_values(src_cgrp, &info.full_path[prefix_len],
					       ignore_unmappable, src_version);
		if (ret)
			goto err;
	}

err:
	if (ret == ECGEOF)
		ret = 0; /* we successfully walked the tree */

	cgroup_walk_tree_end(&handle);
	return ret;
}

static int cgroup_copy_controller_idx(struct cgroup *dst_cgrp, struct cgroup *src_cgrp, int idx)
{
	struct cgroup_controller *src_cgc = src_cgrp->controller[idx];
	struct cgroup_controller *dst_cgc = NULL;
	int ret, i;

	dst_cgc = cgroup_add_controller(dst_cgrp, src_cgc->name);
	if (!dst_cgc) {
		err("%s: failed to add controller %s to %s\n", program_name, src_cgc->name,
		    dst_cgrp->name);
		return ECGFAIL;
	}

	for (i = 0; i < src_cgc->index; i++) {
		ret = cgroup_add_value_string(dst_cgc, src_cgc->values[i]->name,
					      src_cgc->values[i]->value);
		if (ret) {
			err("%s: Failed to add value %s to cgroup %s controller %s\n",
			    program_name, src_cgc->values[i]->name, dst_cgrp->name, src_cgc->name);
			return ret;
		}
	}

	return 0;
}

static int cgroup_populate_cgroup_ctrl(const char * const new_cgrp, bool ignore_unmappable,
				       enum cg_version_t src_version)
{
	char *ctrl, *ctrl_list = cgrp_subtree_ctrl_val->value;
	struct cgroup_controller *cgc;
	struct cgroup *cgrp;
	int ret = ECGFAIL;

	/* create new cgroup */
	cgrp = cgroup_new_cgroup(new_cgrp);
	if (!cgrp) {
		err("%s: can't add new cgroup: %s\n", program_name, cgroup_strerror(ret));
		return ret;
	}

	cgc =  cgroup_add_controller(cgrp, "cgroup");
	if (!cgc)  {
		err("%s: failed to add controller cgroup to %s\n", program_name, cgrp->name);
		goto err;
	}

	/*
	 * this is need for adding the controller setting, which
	 * will be reset in the loop below.
	 */
	ret = cgroup_add_value_string(cgc, cgrp_subtree_ctrl_val->name,
			cgrp_subtree_ctrl_val->value);
	if (ret) {
		err("%s: failed to add value %s to cgroup %s controller cgroup\n", program_name,
				cgrp_subtree_ctrl_val->name, cgrp->name);
		goto err;
	}

	while ((ctrl = strtok(ctrl_list, " ")) != NULL) {
		ret = cgroup_set_value_string(cgc, cgrp_subtree_ctrl_val->name, ctrl);
		if (ret) {
			err("%s: failed to set value %s:%s to cgroup %s controller cgroup\n",
			    program_name, cgrp_subtree_ctrl_val->name, ctrl, cgrp->name);
			goto err;
		}

		if (ctrl[0] == '-')
			ret = _cgroup_set_cgroup_values_r(cgrp, new_cgrp, ignore_unmappable,
							  src_version, true);
		else if (ctrl[0] == '+')
			ret = _cgroup_set_cgroup_values_r(cgrp, new_cgrp, ignore_unmappable,
							  src_version, false);
		else
			ret = ECGFAIL;

		if (ret)
			goto err;

		ctrl_list = NULL;
	}

	ret = 0;

err:
	cgroup_free(&cgrp);
	return ret;
}

static int cgroup_set_cgroup_values_r(struct cgroup *src_cgrp, const char * const new_cgrp,
				    bool ignore_unmappable, enum cg_version_t src_version)
{
	struct cgroup *cgrp = NULL;
	int i, ret = ECGFAIL;

	if (cgrp_subtree_ctrl_val) {
		ret = cgroup_populate_cgroup_ctrl(new_cgrp, ignore_unmappable, src_version);
		if (ret)
			goto err;
	}

	if (is_cgroup_mode_unified()) {
		if (!src_cgrp->index)
			return 0;

		cgrp = create_and_copy_cgroup(src_cgrp, new_cgrp);
		if (!cgrp)
			return ret;

		ret = _cgroup_set_cgroup_values_r(cgrp, new_cgrp, ignore_unmappable,
						  src_version, false);
		goto err;
	}

	/*
	 * for non-unified mode, split every controller and walk the
	 * tree. Unlike unified mode the cgroups in the controller
	 * hierarchy might differ, hence walk per controller.
	 *
	 *          cpu       memory
	 *         /  \         |
	 *        a   a1        a
	 *        |           /   \
	 *        b1         b1   b2
	 *        |               |
	 *        c               c
	 */
	cgrp = cgroup_new_cgroup(new_cgrp);
	if (!cgrp) {
		err("%s: can't add new cgroup: %s\n", program_name, cgroup_strerror(ret));
		return ret;
	}

	for (i = 0; i < src_cgrp->index; i++) {
		cgroup_free_controllers(cgrp);

		ret = cgroup_copy_controller_idx(cgrp, src_cgrp, i);
		if (ret)
			goto err;

		ret = _cgroup_set_cgroup_values_r(cgrp, new_cgrp, ignore_unmappable,
						  src_version, false);
		if (ret)
			goto err;
	}

err:
	cgroup_free(&cgrp);
	return ret;
}

static void usage(int status)
{
	if (status != 0) {
		err("Wrong input parameters, ");
		err("try %s --help' for more information.\n", program_name);
		return;
	}

	info("Usage: %s [-r <name=value>] <cgroup_path> ...\n", program_name);
	info("   or: %s --copy-from <source_cgroup_path> <cgroup_path> ...\n", program_name);
	info("Set the parameters of given cgroup(s)\n");
	info("  -1, --v1                                Provided parameters are in v1 format\n");
	info("  -2, --v2                                Provided parameters are in v2 format\n");
	info("  -i, --ignore-unmappable                 Do not return an error for ");
	info("settings that cannot be converted\n");
	info("  -r, --variable <name>                   Define parameter to set\n");
	info("  --copy-from <source_cgroup_path>        Control group whose ");
	info("parameters will be copied\n");
#ifdef WITH_SYSTEMD
	info("  -b                                      Ignore default systemd ");
	info("delegate hierarchy\n");
	info("  -R                                      Recursively set variable(s)");
	info(" for cgroups under <cgroup_path>\n");
#endif
}
#endif /* !UNIT_TEST */

STATIC int parse_r_flag(const char * const program_name, const char * const name_value_str,
			struct control_value * const name_value)
{
	char *copy = NULL, *buf = NULL;
	int ret = 0;

	buf = strchr(name_value_str, '=');
	if (buf == NULL) {
		err("%s: wrong parameter of option -r: %s\n", program_name, optarg);
		ret = EXIT_BADARGS;
		goto err;
	}

	copy = strdup(name_value_str);
	if (copy == NULL) {
		err("%s: not enough memory\n", program_name);
		ret = -1;
		goto err;
	}

	/* parse optarg value */
	buf = strtok(copy, "=");
	if (buf == NULL) {
		err("%s: wrong parameter of option -r: %s\n", program_name, optarg);
		ret = EXIT_BADARGS;
		goto err;
	}

	strncpy(name_value->name, buf, FILENAME_MAX);
	name_value->name[FILENAME_MAX-1] = '\0';

	buf = strchr(name_value_str, '=');
	/*
	 * we don't need to check the return value of strchr because we
	 * know there's already an '=' character in the string.
	 */
	buf++;

	if (strlen(buf) == 0) {
		err("%s: wrong parameter of option -r: %s\n", program_name, optarg);
		ret = EXIT_BADARGS;
		goto err;
	}

	strncpy(name_value->value, buf, CG_CONTROL_VALUE_MAX);
	name_value->value[CG_CONTROL_VALUE_MAX-1] = '\0';

err:
	if (copy)
		free(copy);

	return ret;
}

#ifndef UNIT_TEST
static int add_subtree_control_name_value(struct control_value *name_value)
{
	if (cgrp_subtree_ctrl_val) {
		err("%s: duplicate -r %s option found\n", program_name, name_value->name);
		return ECGFAIL;
	}

	cgrp_subtree_ctrl_val = calloc(1, sizeof(struct control_value));
	if (!cgrp_subtree_ctrl_val) {
		err("%s: not enough memory\n", program_name);
		return ECGFAIL;
	}

	snprintf(cgrp_subtree_ctrl_val->name, FILENAME_MAX, "%s", name_value->name);
	snprintf(cgrp_subtree_ctrl_val->value, CG_CONTROL_VALUE_MAX, "%s", name_value->value);

	return 0;
}

int main(int argc, char *argv[])
{
#ifdef WITH_SYSTEMD
	int ignore_default_systemd_delegate_slice = 0;
#endif
	struct control_value *name_value = NULL;
	int nv_number = 0;
	int recursive = 0;
	int nv_max = 0;

	char src_cgrp_path[FILENAME_MAX] = "\0";
	struct cgroup *subtree_cgrp = NULL;
	struct cgroup *src_cgrp = NULL;

	enum cg_version_t src_version = CGROUP_UNK;
	bool ignore_unmappable = false;
	int ret = 0;
	int c;

	program_name = argv[0];

	/* no parameter on input */
	if (argc < 2) {
		err("Usage is %s -r <name=value> <relative path to cgroup>\n", program_name);
		return -1;
	}

	/* initialize libcgroup */
	ret = cgroup_init();
	if (ret) {
		err("%s: libcgroup initialization failed: %s\n", program_name,
		    cgroup_strerror(ret));
		goto err;
	}

#ifdef WITH_SYSTEMD
	/* parse arguments */
	while ((c = getopt_long (argc, argv, "r:h12ibR", long_options, NULL)) != -1) {
		switch (c) {
		case 'b':
			ignore_default_systemd_delegate_slice = 1;
			break;
#else
	while ((c = getopt_long (argc, argv, "r:h12iR", long_options, NULL)) != -1) {
		switch (c) {
#endif
		case 'h':
			usage(0);
			ret = 0;
			goto err;
		case 'r':
			if ((flags &  FL_COPY) != 0) {
				usage(1);
				ret = EXIT_BADARGS;
				goto err;
			}
			flags |= FL_RULES;

			/* add name-value pair to buffer (= name_value variable) */
			if (nv_number >= nv_max) {
				nv_max += CG_NV_MAX;
				name_value = (struct control_value *)
					realloc(name_value, nv_max * sizeof(struct control_value));
				if (!name_value) {
					err("%s: not enough memory\n", program_name);
					ret = -1;
					goto err;
				}
			}

			ret = parse_r_flag(program_name, optarg, &name_value[nv_number]);
			if (ret)
				goto err;

			if (!strcmp(name_value[nv_number].name, "cgroup.subtree_control")) {
				ret = add_subtree_control_name_value(&name_value[nv_number]);
				if (ret)
					goto err;
				memset(&name_value[nv_number], '\0', sizeof(struct control_value));
			} else {
				nv_number++;
			}

			break;
		case COPY_FROM_OPTION:
			if (flags != 0) {
				usage(1);
				ret = EXIT_BADARGS;
				goto err;
			}
			flags |= FL_COPY;
			strncpy(src_cgrp_path, optarg, FILENAME_MAX);
			src_cgrp_path[FILENAME_MAX-1] = '\0';
			break;
		case '1':
			src_version = CGROUP_V1;
			break;
		case '2':
			src_version = CGROUP_V2;
			break;
		case 'i':
			ignore_unmappable = true;
			break;
		case 'R':
			recursive = 1;
			break;
		default:
			usage(1);
			ret = EXIT_BADARGS;
			goto err;
		}
	}

	/* no cgroup name */
	if (!argv[optind]) {
		err("%s: no cgroup specified\n", program_name);
		ret = EXIT_BADARGS;
		goto err;
	}

	if (flags == 0) {
		err("%s: no name-value pair was set\n", program_name);
		ret = EXIT_BADARGS;
		goto err;
	}

	/* initialize libcgroup */
	ret = cgroup_init();
	if (ret) {
		err("%s: libcgroup initialization failed: %s\n", program_name,
		    cgroup_strerror(ret));
		goto err;
	}

#ifdef WITH_SYSTEMD
	if (!ignore_default_systemd_delegate_slice)
		cgroup_set_default_systemd_cgroup();
#endif

	/* copy the name-value pairs from -r options */
	if ((flags & FL_RULES) != 0) {
		src_cgrp = create_cgroup_from_name_value_pairs("tmp", name_value, nv_number);
		if (src_cgrp == NULL)
			goto err;

		if (cgrp_subtree_ctrl_val && !recursive) {
			subtree_cgrp = create_cgroup_from_name_value_pairs("tmp1",
							cgrp_subtree_ctrl_val, 1);
			if (!subtree_cgrp)
				goto err;
		}
	}

	/* copy the name-value from the given group */
	if ((flags & FL_COPY) != 0) {
		src_cgrp = copy_name_value_from_cgroup(src_cgrp_path);
		if (src_cgrp == NULL)
			goto err;
	}

	while (optind < argc) {

		if (recursive) {
			ret = cgroup_set_cgroup_values_r(src_cgrp, argv[optind],
							 ignore_unmappable, src_version);
		} else {
			ret = cgroup_set_cgroup_values(subtree_cgrp, argv[optind],
						       ignore_unmappable, src_version);
			if (ret)
				goto err;
			ret = cgroup_set_cgroup_values(src_cgrp, argv[optind],
						       ignore_unmappable, src_version);
		}
		if (ret)
			goto err;

		optind++;
	}

err:
	cgroup_free(&src_cgrp);
	cgroup_free(&subtree_cgrp);
	free(name_value);

	return ret;
}
#endif /* !UNIT_TEST */

#ifdef LIBCG_LIB
int cgroup_cgxset(const struct cgroup * const cgroup, enum cg_version_t version,
		  bool ignore_unmappable)
{
	struct cgroup *converted_cgroup;
	int ret;

	converted_cgroup = cgroup_new_cgroup(cgroup->name);
	if (converted_cgroup == NULL) {
		ret = ECGCONTROLLERCREATEFAILED;
		goto err;
	}

	ret = cgroup_convert_cgroup(converted_cgroup, CGROUP_DISK, cgroup, version);
	if (ret == ECGNOVERSIONCONVERT && ignore_unmappable)
		/*
		 * The user has specified that we should ignore any errors
		 * due to being unable to map from v1 to v2 or vice versa
		 */
		ret = 0;
	else if (ret)
		goto err;

	/* modify cgroup based on values of the new one */
	ret = cgroup_modify_cgroup(converted_cgroup);
	if (ret)
		goto err;

err:
	if (converted_cgroup)
		cgroup_free(&converted_cgroup);

	return ret;
}
#endif /* LIBCG_LIB */
