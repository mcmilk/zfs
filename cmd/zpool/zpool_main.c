// SPDX-License-Identifier: CDDL-1.0
/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or https://opensource.org/licenses/CDDL-1.0.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */

/*
 * Copyright (c) 2005, 2010, Oracle and/or its affiliates. All rights reserved.
 * Copyright 2011 Nexenta Systems, Inc. All rights reserved.
 * Copyright (c) 2011, 2024 by Delphix. All rights reserved.
 * Copyright (c) 2012 by Frederik Wessels. All rights reserved.
 * Copyright (c) 2012 by Cyril Plisko. All rights reserved.
 * Copyright (c) 2013 by Prasad Joshi (sTec). All rights reserved.
 * Copyright 2016 Igor Kozhukhov <ikozhukhov@gmail.com>.
 * Copyright (c) 2017 Datto Inc.
 * Copyright (c) 2017 Open-E, Inc. All Rights Reserved.
 * Copyright (c) 2017, Intel Corporation.
 * Copyright (c) 2019, loli10K <ezomori.nozomu@gmail.com>
 * Copyright (c) 2021, Colm Buckley <colm@tuatha.org>
 * Copyright (c) 2021, 2023, Klara Inc.
 * Copyright (c) 2021, 2025 Hewlett Packard Enterprise Development LP.
 */

#include <assert.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <libgen.h>
#include <libintl.h>
#include <libuutil.h>
#include <locale.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <thread_pool.h>
#include <time.h>
#include <unistd.h>
#include <pwd.h>
#include <zone.h>
#include <sys/wait.h>
#include <zfs_prop.h>
#include <sys/fs/zfs.h>
#include <sys/stat.h>
#include <sys/systeminfo.h>
#include <sys/fm/fs/zfs.h>
#include <sys/fm/util.h>
#include <sys/fm/protocol.h>
#include <sys/zfs_ioctl.h>
#include <sys/mount.h>
#include <sys/sysmacros.h>
#include <string.h>
#include <math.h>

#include <libzfs.h>
#include <libzutil.h>

#include "zpool_util.h"
#include "zfs_comutil.h"
#include "zfeature_common.h"
#include "zfs_valstr.h"

#include "statcommon.h"

libzfs_handle_t *g_zfs;

static int mount_tp_nthr = 512;  /* tpool threads for multi-threaded mounting */

static int zpool_do_create(int, char **);
static int zpool_do_destroy(int, char **);

static int zpool_do_add(int, char **);
static int zpool_do_remove(int, char **);
static int zpool_do_labelclear(int, char **);

static int zpool_do_checkpoint(int, char **);
static int zpool_do_prefetch(int, char **);

static int zpool_do_list(int, char **);
static int zpool_do_iostat(int, char **);
static int zpool_do_status(int, char **);

static int zpool_do_online(int, char **);
static int zpool_do_offline(int, char **);
static int zpool_do_clear(int, char **);
static int zpool_do_reopen(int, char **);

static int zpool_do_reguid(int, char **);

static int zpool_do_attach(int, char **);
static int zpool_do_detach(int, char **);
static int zpool_do_replace(int, char **);
static int zpool_do_split(int, char **);

static int zpool_do_initialize(int, char **);
static int zpool_do_scrub(int, char **);
static int zpool_do_resilver(int, char **);
static int zpool_do_trim(int, char **);

static int zpool_do_import(int, char **);
static int zpool_do_export(int, char **);

static int zpool_do_upgrade(int, char **);

static int zpool_do_history(int, char **);
static int zpool_do_events(int, char **);

static int zpool_do_get(int, char **);
static int zpool_do_set(int, char **);

static int zpool_do_sync(int, char **);

static int zpool_do_version(int, char **);

static int zpool_do_wait(int, char **);

static int zpool_do_ddt_prune(int, char **);

static int zpool_do_help(int argc, char **argv);

static zpool_compat_status_t zpool_do_load_compat(
    const char *, boolean_t *);

enum zpool_options {
	ZPOOL_OPTION_POWER = 1024,
	ZPOOL_OPTION_ALLOW_INUSE,
	ZPOOL_OPTION_ALLOW_REPLICATION_MISMATCH,
	ZPOOL_OPTION_ALLOW_ASHIFT_MISMATCH,
	ZPOOL_OPTION_POOL_KEY_GUID,
	ZPOOL_OPTION_JSON_NUMS_AS_INT,
	ZPOOL_OPTION_JSON_FLAT_VDEVS
};

/*
 * These libumem hooks provide a reasonable set of defaults for the allocator's
 * debugging facilities.
 */

#ifdef DEBUG
const char *
_umem_debug_init(void)
{
	return ("default,verbose"); /* $UMEM_DEBUG setting */
}

const char *
_umem_logging_init(void)
{
	return ("fail,contents"); /* $UMEM_LOGGING setting */
}
#endif

typedef enum {
	HELP_ADD,
	HELP_ATTACH,
	HELP_CLEAR,
	HELP_CREATE,
	HELP_CHECKPOINT,
	HELP_DDT_PRUNE,
	HELP_DESTROY,
	HELP_DETACH,
	HELP_EXPORT,
	HELP_HISTORY,
	HELP_IMPORT,
	HELP_IOSTAT,
	HELP_LABELCLEAR,
	HELP_LIST,
	HELP_OFFLINE,
	HELP_ONLINE,
	HELP_PREFETCH,
	HELP_REPLACE,
	HELP_REMOVE,
	HELP_INITIALIZE,
	HELP_SCRUB,
	HELP_RESILVER,
	HELP_TRIM,
	HELP_STATUS,
	HELP_UPGRADE,
	HELP_EVENTS,
	HELP_GET,
	HELP_SET,
	HELP_SPLIT,
	HELP_SYNC,
	HELP_REGUID,
	HELP_REOPEN,
	HELP_VERSION,
	HELP_WAIT
} zpool_help_t;


/*
 * Flags for stats to display with "zpool iostats"
 */
enum iostat_type {
	IOS_DEFAULT = 0,
	IOS_LATENCY = 1,
	IOS_QUEUES = 2,
	IOS_L_HISTO = 3,
	IOS_RQ_HISTO = 4,
	IOS_COUNT,	/* always last element */
};

/* iostat_type entries as bitmasks */
#define	IOS_DEFAULT_M	(1ULL << IOS_DEFAULT)
#define	IOS_LATENCY_M	(1ULL << IOS_LATENCY)
#define	IOS_QUEUES_M	(1ULL << IOS_QUEUES)
#define	IOS_L_HISTO_M	(1ULL << IOS_L_HISTO)
#define	IOS_RQ_HISTO_M	(1ULL << IOS_RQ_HISTO)

/* Mask of all the histo bits */
#define	IOS_ANYHISTO_M (IOS_L_HISTO_M | IOS_RQ_HISTO_M)

/*
 * Lookup table for iostat flags to nvlist names.  Basically a list
 * of all the nvlists a flag requires.  Also specifies the order in
 * which data gets printed in zpool iostat.
 */
static const char *vsx_type_to_nvlist[IOS_COUNT][15] = {
	[IOS_L_HISTO] = {
	    ZPOOL_CONFIG_VDEV_TOT_R_LAT_HISTO,
	    ZPOOL_CONFIG_VDEV_TOT_W_LAT_HISTO,
	    ZPOOL_CONFIG_VDEV_DISK_R_LAT_HISTO,
	    ZPOOL_CONFIG_VDEV_DISK_W_LAT_HISTO,
	    ZPOOL_CONFIG_VDEV_SYNC_R_LAT_HISTO,
	    ZPOOL_CONFIG_VDEV_SYNC_W_LAT_HISTO,
	    ZPOOL_CONFIG_VDEV_ASYNC_R_LAT_HISTO,
	    ZPOOL_CONFIG_VDEV_ASYNC_W_LAT_HISTO,
	    ZPOOL_CONFIG_VDEV_SCRUB_LAT_HISTO,
	    ZPOOL_CONFIG_VDEV_TRIM_LAT_HISTO,
	    ZPOOL_CONFIG_VDEV_REBUILD_LAT_HISTO,
	    NULL},
	[IOS_LATENCY] = {
	    ZPOOL_CONFIG_VDEV_TOT_R_LAT_HISTO,
	    ZPOOL_CONFIG_VDEV_TOT_W_LAT_HISTO,
	    ZPOOL_CONFIG_VDEV_DISK_R_LAT_HISTO,
	    ZPOOL_CONFIG_VDEV_DISK_W_LAT_HISTO,
	    ZPOOL_CONFIG_VDEV_TRIM_LAT_HISTO,
	    ZPOOL_CONFIG_VDEV_REBUILD_LAT_HISTO,
	    NULL},
	[IOS_QUEUES] = {
	    ZPOOL_CONFIG_VDEV_SYNC_R_ACTIVE_QUEUE,
	    ZPOOL_CONFIG_VDEV_SYNC_W_ACTIVE_QUEUE,
	    ZPOOL_CONFIG_VDEV_ASYNC_R_ACTIVE_QUEUE,
	    ZPOOL_CONFIG_VDEV_ASYNC_W_ACTIVE_QUEUE,
	    ZPOOL_CONFIG_VDEV_SCRUB_ACTIVE_QUEUE,
	    ZPOOL_CONFIG_VDEV_TRIM_ACTIVE_QUEUE,
	    ZPOOL_CONFIG_VDEV_REBUILD_ACTIVE_QUEUE,
	    NULL},
	[IOS_RQ_HISTO] = {
	    ZPOOL_CONFIG_VDEV_SYNC_IND_R_HISTO,
	    ZPOOL_CONFIG_VDEV_SYNC_AGG_R_HISTO,
	    ZPOOL_CONFIG_VDEV_SYNC_IND_W_HISTO,
	    ZPOOL_CONFIG_VDEV_SYNC_AGG_W_HISTO,
	    ZPOOL_CONFIG_VDEV_ASYNC_IND_R_HISTO,
	    ZPOOL_CONFIG_VDEV_ASYNC_AGG_R_HISTO,
	    ZPOOL_CONFIG_VDEV_ASYNC_IND_W_HISTO,
	    ZPOOL_CONFIG_VDEV_ASYNC_AGG_W_HISTO,
	    ZPOOL_CONFIG_VDEV_IND_SCRUB_HISTO,
	    ZPOOL_CONFIG_VDEV_AGG_SCRUB_HISTO,
	    ZPOOL_CONFIG_VDEV_IND_TRIM_HISTO,
	    ZPOOL_CONFIG_VDEV_AGG_TRIM_HISTO,
	    ZPOOL_CONFIG_VDEV_IND_REBUILD_HISTO,
	    ZPOOL_CONFIG_VDEV_AGG_REBUILD_HISTO,
	    NULL},
};

static const char *pool_scan_func_str[] = {
	"NONE",
	"SCRUB",
	"RESILVER",
	"ERRORSCRUB"
};

static const char *pool_scan_state_str[] = {
	"NONE",
	"SCANNING",
	"FINISHED",
	"CANCELED",
	"ERRORSCRUBBING"
};

static const char *vdev_rebuild_state_str[] = {
	"NONE",
	"ACTIVE",
	"CANCELED",
	"COMPLETE"
};

static const char *checkpoint_state_str[] = {
	"NONE",
	"EXISTS",
	"DISCARDING"
};

static const char *vdev_state_str[] = {
	"UNKNOWN",
	"CLOSED",
	"OFFLINE",
	"REMOVED",
	"CANT_OPEN",
	"FAULTED",
	"DEGRADED",
	"ONLINE"
};

static const char *vdev_aux_str[] = {
	"NONE",
	"OPEN_FAILED",
	"CORRUPT_DATA",
	"NO_REPLICAS",
	"BAD_GUID_SUM",
	"TOO_SMALL",
	"BAD_LABEL",
	"VERSION_NEWER",
	"VERSION_OLDER",
	"UNSUP_FEAT",
	"SPARED",
	"ERR_EXCEEDED",
	"IO_FAILURE",
	"BAD_LOG",
	"EXTERNAL",
	"SPLIT_POOL",
	"BAD_ASHIFT",
	"EXTERNAL_PERSIST",
	"ACTIVE",
	"CHILDREN_OFFLINE",
	"ASHIFT_TOO_BIG"
};

static const char *vdev_init_state_str[] = {
	"NONE",
	"ACTIVE",
	"CANCELED",
	"SUSPENDED",
	"COMPLETE"
};

static const char *vdev_trim_state_str[] = {
	"NONE",
	"ACTIVE",
	"CANCELED",
	"SUSPENDED",
	"COMPLETE"
};

#define	ZFS_NICE_TIMESTAMP	100

/*
 * Given a cb->cb_flags with a histogram bit set, return the iostat_type.
 * Right now, only one histo bit is ever set at one time, so we can
 * just do a highbit64(a)
 */
#define	IOS_HISTO_IDX(a)	(highbit64(a & IOS_ANYHISTO_M) - 1)

typedef struct zpool_command {
	const char	*name;
	int		(*func)(int, char **);
	zpool_help_t	usage;
} zpool_command_t;

/*
 * Master command table.  Each ZFS command has a name, associated function, and
 * usage message.  The usage messages need to be internationalized, so we have
 * to have a function to return the usage message based on a command index.
 *
 * These commands are organized according to how they are displayed in the usage
 * message.  An empty command (one with a NULL name) indicates an empty line in
 * the generic usage message.
 */
static zpool_command_t command_table[] = {
	{ "version",	zpool_do_version,	HELP_VERSION		},
	{ NULL },
	{ "create",	zpool_do_create,	HELP_CREATE		},
	{ "destroy",	zpool_do_destroy,	HELP_DESTROY		},
	{ NULL },
	{ "add",	zpool_do_add,		HELP_ADD		},
	{ "remove",	zpool_do_remove,	HELP_REMOVE		},
	{ NULL },
	{ "labelclear",	zpool_do_labelclear,	HELP_LABELCLEAR		},
	{ NULL },
	{ "checkpoint",	zpool_do_checkpoint,	HELP_CHECKPOINT		},
	{ "prefetch",	zpool_do_prefetch,	HELP_PREFETCH		},
	{ NULL },
	{ "list",	zpool_do_list,		HELP_LIST		},
	{ "iostat",	zpool_do_iostat,	HELP_IOSTAT		},
	{ "status",	zpool_do_status,	HELP_STATUS		},
	{ NULL },
	{ "online",	zpool_do_online,	HELP_ONLINE		},
	{ "offline",	zpool_do_offline,	HELP_OFFLINE		},
	{ "clear",	zpool_do_clear,		HELP_CLEAR		},
	{ "reopen",	zpool_do_reopen,	HELP_REOPEN		},
	{ NULL },
	{ "attach",	zpool_do_attach,	HELP_ATTACH		},
	{ "detach",	zpool_do_detach,	HELP_DETACH		},
	{ "replace",	zpool_do_replace,	HELP_REPLACE		},
	{ "split",	zpool_do_split,		HELP_SPLIT		},
	{ NULL },
	{ "initialize",	zpool_do_initialize,	HELP_INITIALIZE		},
	{ "resilver",	zpool_do_resilver,	HELP_RESILVER		},
	{ "scrub",	zpool_do_scrub,		HELP_SCRUB		},
	{ "trim",	zpool_do_trim,		HELP_TRIM		},
	{ NULL },
	{ "import",	zpool_do_import,	HELP_IMPORT		},
	{ "export",	zpool_do_export,	HELP_EXPORT		},
	{ "upgrade",	zpool_do_upgrade,	HELP_UPGRADE		},
	{ "reguid",	zpool_do_reguid,	HELP_REGUID		},
	{ NULL },
	{ "history",	zpool_do_history,	HELP_HISTORY		},
	{ "events",	zpool_do_events,	HELP_EVENTS		},
	{ NULL },
	{ "get",	zpool_do_get,		HELP_GET		},
	{ "set",	zpool_do_set,		HELP_SET		},
	{ "sync",	zpool_do_sync,		HELP_SYNC		},
	{ NULL },
	{ "wait",	zpool_do_wait,		HELP_WAIT		},
	{ NULL },
	{ "ddtprune",	zpool_do_ddt_prune,	HELP_DDT_PRUNE		},
};

#define	NCOMMAND	(ARRAY_SIZE(command_table))

#define	VDEV_ALLOC_CLASS_LOGS	"logs"

#define	MAX_CMD_LEN	256

static zpool_command_t *current_command;
static zfs_type_t current_prop_type = (ZFS_TYPE_POOL | ZFS_TYPE_VDEV);
static char history_str[HIS_MAX_RECORD_LEN];
static boolean_t log_history = B_TRUE;
static uint_t timestamp_fmt = NODATE;

static const char *
get_usage(zpool_help_t idx)
{
	switch (idx) {
	case HELP_ADD:
		return (gettext("\tadd [-afgLnP] [-o property=value] "
		    "<pool> <vdev> ...\n"));
	case HELP_ATTACH:
		return (gettext("\tattach [-fsw] [-o property=value] "
		    "<pool> <device> <new-device>\n"));
	case HELP_CLEAR:
		return (gettext("\tclear [[--power]|[-nF]] <pool> [device]\n"));
	case HELP_CREATE:
		return (gettext("\tcreate [-fnd] [-o property=value] ... \n"
		    "\t    [-O file-system-property=value] ... \n"
		    "\t    [-m mountpoint] [-R root] <pool> <vdev> ...\n"));
	case HELP_CHECKPOINT:
		return (gettext("\tcheckpoint [-d [-w]] <pool> ...\n"));
	case HELP_DESTROY:
		return (gettext("\tdestroy [-f] <pool>\n"));
	case HELP_DETACH:
		return (gettext("\tdetach <pool> <device>\n"));
	case HELP_EXPORT:
		return (gettext("\texport [-af] <pool> ...\n"));
	case HELP_HISTORY:
		return (gettext("\thistory [-il] [<pool>] ...\n"));
	case HELP_IMPORT:
		return (gettext("\timport [-d dir] [-D]\n"
		    "\timport [-o mntopts] [-o property=value] ... \n"
		    "\t    [-d dir | -c cachefile] [-D] [-l] [-f] [-m] [-N] "
		    "[-R root] [-F [-n]] -a\n"
		    "\timport [-o mntopts] [-o property=value] ... \n"
		    "\t    [-d dir | -c cachefile] [-D] [-l] [-f] [-m] [-N] "
		    "[-R root] [-F [-n]]\n"
		    "\t    [--rewind-to-checkpoint] <pool | id> [newpool]\n"));
	case HELP_IOSTAT:
		return (gettext("\tiostat [[[-c [script1,script2,...]"
		    "[-lq]]|[-rw]] [-T d | u] [-ghHLpPvy]\n"
		    "\t    [[pool ...]|[pool vdev ...]|[vdev ...]]"
		    " [[-n] interval [count]]\n"));
	case HELP_LABELCLEAR:
		return (gettext("\tlabelclear [-f] <vdev>\n"));
	case HELP_LIST:
		return (gettext("\tlist [-gHLpPv] [-o property[,...]] [-j "
		    "[--json-int, --json-pool-key-guid]] ...\n"
		    "\t    [-T d|u] [pool] [interval [count]]\n"));
	case HELP_PREFETCH:
		return (gettext("\tprefetch -t <type> [<type opts>] <pool>\n"
		    "\t    -t ddt <pool>\n"));
	case HELP_OFFLINE:
		return (gettext("\toffline [--power]|[[-f][-t]] <pool> "
		    "<device> ...\n"));
	case HELP_ONLINE:
		return (gettext("\tonline [--power][-e] <pool> <device> "
		    "...\n"));
	case HELP_REPLACE:
		return (gettext("\treplace [-fsw] [-o property=value] "
		    "<pool> <device> [new-device]\n"));
	case HELP_REMOVE:
		return (gettext("\tremove [-npsw] <pool> <device> ...\n"));
	case HELP_REOPEN:
		return (gettext("\treopen [-n] <pool>\n"));
	case HELP_INITIALIZE:
		return (gettext("\tinitialize [-c | -s | -u] [-w] <-a | <pool> "
		    "[<device> ...]>\n"));
	case HELP_SCRUB:
		return (gettext("\tscrub [-e | -s | -p | -C] [-w] <-a | "
		    "<pool> [<pool> ...]>\n"));
	case HELP_RESILVER:
		return (gettext("\tresilver <pool> ...\n"));
	case HELP_TRIM:
		return (gettext("\ttrim [-dw] [-r <rate>] [-c | -s] "
		    "<-a | <pool> [<device> ...]>\n"));
	case HELP_STATUS:
		return (gettext("\tstatus [-DdegiLPpstvx] "
		    "[-c script1[,script2,...]] ...\n"
		    "\t    [-j|--json [--json-flat-vdevs] [--json-int] "
		    "[--json-pool-key-guid]] ...\n"
		    "\t    [-T d|u] [--power] [pool] [interval [count]]\n"));
	case HELP_UPGRADE:
		return (gettext("\tupgrade\n"
		    "\tupgrade -v\n"
		    "\tupgrade [-V version] <-a | pool ...>\n"));
	case HELP_EVENTS:
		return (gettext("\tevents [-vHf [pool] | -c]\n"));
	case HELP_GET:
		return (gettext("\tget [-Hp] [-j [--json-int, "
		    "--json-pool-key-guid]] ...\n"
		    "\t    [-o \"all\" | field[,...]] "
		    "<\"all\" | property[,...]> <pool> ...\n"));
	case HELP_SET:
		return (gettext("\tset <property=value> <pool>\n"
		    "\tset <vdev_property=value> <pool> <vdev>\n"));
	case HELP_SPLIT:
		return (gettext("\tsplit [-gLnPl] [-R altroot] [-o mntopts]\n"
		    "\t    [-o property=value] <pool> <newpool> "
		    "[<device> ...]\n"));
	case HELP_REGUID:
		return (gettext("\treguid [-g guid] <pool>\n"));
	case HELP_SYNC:
		return (gettext("\tsync [pool] ...\n"));
	case HELP_VERSION:
		return (gettext("\tversion [-j]\n"));
	case HELP_WAIT:
		return (gettext("\twait [-Hp] [-T d|u] [-t <activity>[,...]] "
		    "<pool> [interval]\n"));
	case HELP_DDT_PRUNE:
		return (gettext("\tddtprune -d|-p <amount> <pool>\n"));
	default:
		__builtin_unreachable();
	}
}

/*
 * Callback routine that will print out a pool property value.
 */
static int
print_pool_prop_cb(int prop, void *cb)
{
	FILE *fp = cb;

	(void) fprintf(fp, "\t%-19s  ", zpool_prop_to_name(prop));

	if (zpool_prop_readonly(prop))
		(void) fprintf(fp, "  NO   ");
	else
		(void) fprintf(fp, " YES   ");

	if (zpool_prop_values(prop) == NULL)
		(void) fprintf(fp, "-\n");
	else
		(void) fprintf(fp, "%s\n", zpool_prop_values(prop));

	return (ZPROP_CONT);
}

/*
 * Callback routine that will print out a vdev property value.
 */
static int
print_vdev_prop_cb(int prop, void *cb)
{
	FILE *fp = cb;

	(void) fprintf(fp, "\t%-19s  ", vdev_prop_to_name(prop));

	if (vdev_prop_readonly(prop))
		(void) fprintf(fp, "  NO   ");
	else
		(void) fprintf(fp, " YES   ");

	if (vdev_prop_values(prop) == NULL)
		(void) fprintf(fp, "-\n");
	else
		(void) fprintf(fp, "%s\n", vdev_prop_values(prop));

	return (ZPROP_CONT);
}

/*
 * Given a leaf vdev name like 'L5' return its VDEV_CONFIG_PATH like
 * '/dev/disk/by-vdev/L5'.
 */
static const char *
vdev_name_to_path(zpool_handle_t *zhp, char *vdev)
{
	nvlist_t *vdev_nv = zpool_find_vdev(zhp, vdev, NULL, NULL, NULL);
	if (vdev_nv == NULL) {
		return (NULL);
	}
	return (fnvlist_lookup_string(vdev_nv, ZPOOL_CONFIG_PATH));
}

static int
zpool_power_on(zpool_handle_t *zhp, char *vdev)
{
	return (zpool_power(zhp, vdev, B_TRUE));
}

static int
zpool_power_on_and_disk_wait(zpool_handle_t *zhp, char *vdev)
{
	int rc;

	rc = zpool_power_on(zhp, vdev);
	if (rc != 0)
		return (rc);

	zpool_disk_wait(vdev_name_to_path(zhp, vdev));

	return (0);
}

static int
zpool_power_on_pool_and_wait_for_devices(zpool_handle_t *zhp)
{
	nvlist_t *nv;
	const char *path = NULL;
	int rc;

	/* Power up all the devices first */
	FOR_EACH_REAL_LEAF_VDEV(zhp, nv) {
		path = fnvlist_lookup_string(nv, ZPOOL_CONFIG_PATH);
		if (path != NULL) {
			rc = zpool_power_on(zhp, (char *)path);
			if (rc != 0) {
				return (rc);
			}
		}
	}

	/*
	 * Wait for their devices to show up.  Since we powered them on
	 * at roughly the same time, they should all come online around
	 * the same time.
	 */
	FOR_EACH_REAL_LEAF_VDEV(zhp, nv) {
		path = fnvlist_lookup_string(nv, ZPOOL_CONFIG_PATH);
		zpool_disk_wait(path);
	}

	return (0);
}

static int
zpool_power_off(zpool_handle_t *zhp, char *vdev)
{
	return (zpool_power(zhp, vdev, B_FALSE));
}

/*
 * Display usage message.  If we're inside a command, display only the usage for
 * that command.  Otherwise, iterate over the entire command table and display
 * a complete usage message.
 */
static __attribute__((noreturn)) void
usage(boolean_t requested)
{
	FILE *fp = requested ? stdout : stderr;

	if (current_command == NULL) {
		int i;

		(void) fprintf(fp, gettext("usage: zpool command args ...\n"));
		(void) fprintf(fp,
		    gettext("where 'command' is one of the following:\n\n"));

		for (i = 0; i < NCOMMAND; i++) {
			if (command_table[i].name == NULL)
				(void) fprintf(fp, "\n");
			else
				(void) fprintf(fp, "%s",
				    get_usage(command_table[i].usage));
		}

		(void) fprintf(fp,
		    gettext("\nFor further help on a command or topic, "
		    "run: %s\n"), "zpool help [<topic>]");
	} else {
		(void) fprintf(fp, gettext("usage:\n"));
		(void) fprintf(fp, "%s", get_usage(current_command->usage));
	}

	if (current_command != NULL &&
	    current_prop_type != (ZFS_TYPE_POOL | ZFS_TYPE_VDEV) &&
	    ((strcmp(current_command->name, "set") == 0) ||
	    (strcmp(current_command->name, "get") == 0) ||
	    (strcmp(current_command->name, "list") == 0))) {

		(void) fprintf(fp, "%s",
		    gettext("\nthe following properties are supported:\n"));

		(void) fprintf(fp, "\n\t%-19s  %s   %s\n\n",
		    "PROPERTY", "EDIT", "VALUES");

		/* Iterate over all properties */
		if (current_prop_type == ZFS_TYPE_POOL) {
			(void) zprop_iter(print_pool_prop_cb, fp, B_FALSE,
			    B_TRUE, current_prop_type);

			(void) fprintf(fp, "\t%-19s   ", "feature@...");
			(void) fprintf(fp, "YES   "
			    "disabled | enabled | active\n");

			(void) fprintf(fp, gettext("\nThe feature@ properties "
			    "must be appended with a feature name.\n"
			    "See zpool-features(7).\n"));
		} else if (current_prop_type == ZFS_TYPE_VDEV) {
			(void) zprop_iter(print_vdev_prop_cb, fp, B_FALSE,
			    B_TRUE, current_prop_type);
		}
	}

	/*
	 * See comments at end of main().
	 */
	if (getenv("ZFS_ABORT") != NULL) {
		(void) printf("dumping core by request\n");
		abort();
	}

	exit(requested ? 0 : 2);
}

/*
 * zpool initialize [-c | -s | -u] [-w] <pool> [<vdev> ...]
 * Initialize all unused blocks in the specified vdevs, or all vdevs in the pool
 * if none specified.
 *
 *	-c	Cancel. Ends active initializing.
 *	-s	Suspend. Initializing can then be restarted with no flags.
 *	-u	Uninitialize. Clears initialization state.
 *	-w	Wait. Blocks until initializing has completed.
 */
int
zpool_do_initialize(int argc, char **argv)
{
	int c;
	char *poolname;
	zpool_handle_t *zhp;
	int err = 0;
	boolean_t wait = B_FALSE;
	boolean_t initialize_all = B_FALSE;

	struct option long_options[] = {
		{"cancel",	no_argument,		NULL, 'c'},
		{"suspend",	no_argument,		NULL, 's'},
		{"uninit",	no_argument,		NULL, 'u'},
		{"wait",	no_argument,		NULL, 'w'},
		{"all", 	no_argument,		NULL, 'a'},
		{0, 0, 0, 0}
	};

	pool_initialize_func_t cmd_type = POOL_INITIALIZE_START;
	while ((c = getopt_long(argc, argv, "acsuw", long_options,
	    NULL)) != -1) {
		switch (c) {
		case 'a':
			initialize_all = B_TRUE;
			break;
		case 'c':
			if (cmd_type != POOL_INITIALIZE_START &&
			    cmd_type != POOL_INITIALIZE_CANCEL) {
				(void) fprintf(stderr, gettext("-c cannot be "
				    "combined with other options\n"));
				usage(B_FALSE);
			}
			cmd_type = POOL_INITIALIZE_CANCEL;
			break;
		case 's':
			if (cmd_type != POOL_INITIALIZE_START &&
			    cmd_type != POOL_INITIALIZE_SUSPEND) {
				(void) fprintf(stderr, gettext("-s cannot be "
				    "combined with other options\n"));
				usage(B_FALSE);
			}
			cmd_type = POOL_INITIALIZE_SUSPEND;
			break;
		case 'u':
			if (cmd_type != POOL_INITIALIZE_START &&
			    cmd_type != POOL_INITIALIZE_UNINIT) {
				(void) fprintf(stderr, gettext("-u cannot be "
				    "combined with other options\n"));
				usage(B_FALSE);
			}
			cmd_type = POOL_INITIALIZE_UNINIT;
			break;
		case 'w':
			wait = B_TRUE;
			break;
		case '?':
			if (optopt != 0) {
				(void) fprintf(stderr,
				    gettext("invalid option '%c'\n"), optopt);
			} else {
				(void) fprintf(stderr,
				    gettext("invalid option '%s'\n"),
				    argv[optind - 1]);
			}
			usage(B_FALSE);
		}
	}

	argc -= optind;
	argv += optind;

	initialize_cbdata_t cbdata = {
		.wait = wait,
		.cmd_type = cmd_type
	};

	if (initialize_all && argc > 0) {
		(void) fprintf(stderr, gettext("-a cannot be combined with "
		    "individual pools or vdevs\n"));
		usage(B_FALSE);
	}

	if (argc < 1 && !initialize_all) {
		(void) fprintf(stderr, gettext("missing pool name argument\n"));
		usage(B_FALSE);
		return (-1);
	}

	if (wait && (cmd_type != POOL_INITIALIZE_START)) {
		(void) fprintf(stderr, gettext("-w cannot be used with -c, -s"
		    "or -u\n"));
		usage(B_FALSE);
	}

	if (argc == 0 && initialize_all) {
		/* Initilize each pool recursively */
		err = for_each_pool(argc, argv, B_TRUE, NULL, ZFS_TYPE_POOL,
		    B_FALSE, zpool_initialize_one, &cbdata);
		return (err);
	} else if (argc == 1) {
		/* no individual leaf vdevs specified, initialize the pool */
		poolname = argv[0];
		zhp = zpool_open(g_zfs, poolname);
		if (zhp == NULL)
			return (-1);
		err = zpool_initialize_one(zhp, &cbdata);
	} else {
		/* individual leaf vdevs specified, initialize them */
		poolname = argv[0];
		zhp = zpool_open(g_zfs, poolname);
		if (zhp == NULL)
			return (-1);
		nvlist_t *vdevs = fnvlist_alloc();
		for (int i = 1; i < argc; i++) {
			fnvlist_add_boolean(vdevs, argv[i]);
		}
		if (wait)
			err = zpool_initialize_wait(zhp, cmd_type, vdevs);
		else
			err = zpool_initialize(zhp, cmd_type, vdevs);
		fnvlist_free(vdevs);
	}

	zpool_close(zhp);

	return (err);
}

/*
 * print a pool vdev config for dry runs
 */
static void
print_vdev_tree(zpool_handle_t *zhp, const char *name, nvlist_t *nv, int indent,
    const char *match, int name_flags)
{
	nvlist_t **child;
	uint_t c, children;
	char *vname;
	boolean_t printed = B_FALSE;

	if (nvlist_lookup_nvlist_array(nv, ZPOOL_CONFIG_CHILDREN,
	    &child, &children) != 0) {
		if (name != NULL)
			(void) printf("\t%*s%s\n", indent, "", name);
		return;
	}

	for (c = 0; c < children; c++) {
		uint64_t is_log = B_FALSE, is_hole = B_FALSE;
		const char *class = "";

		(void) nvlist_lookup_uint64(child[c], ZPOOL_CONFIG_IS_HOLE,
		    &is_hole);

		if (is_hole == B_TRUE) {
			continue;
		}

		(void) nvlist_lookup_uint64(child[c], ZPOOL_CONFIG_IS_LOG,
		    &is_log);
		if (is_log)
			class = VDEV_ALLOC_BIAS_LOG;
		(void) nvlist_lookup_string(child[c],
		    ZPOOL_CONFIG_ALLOCATION_BIAS, &class);
		if (strcmp(match, class) != 0)
			continue;

		if (!printed && name != NULL) {
			(void) printf("\t%*s%s\n", indent, "", name);
			printed = B_TRUE;
		}
		vname = zpool_vdev_name(g_zfs, zhp, child[c], name_flags);
		print_vdev_tree(zhp, vname, child[c], indent + 2, "",
		    name_flags);
		free(vname);
	}
}

/*
 * Print the list of l2cache devices for dry runs.
 */
static void
print_cache_list(nvlist_t *nv, int indent)
{
	nvlist_t **child;
	uint_t c, children;

	if (nvlist_lookup_nvlist_array(nv, ZPOOL_CONFIG_L2CACHE,
	    &child, &children) == 0 && children > 0) {
		(void) printf("\t%*s%s\n", indent, "", "cache");
	} else {
		return;
	}
	for (c = 0; c < children; c++) {
		char *vname;

		vname = zpool_vdev_name(g_zfs, NULL, child[c], 0);
		(void) printf("\t%*s%s\n", indent + 2, "", vname);
		free(vname);
	}
}

/*
 * Print the list of spares for dry runs.
 */
static void
print_spare_list(nvlist_t *nv, int indent)
{
	nvlist_t **child;
	uint_t c, children;

	if (nvlist_lookup_nvlist_array(nv, ZPOOL_CONFIG_SPARES,
	    &child, &children) == 0 && children > 0) {
		(void) printf("\t%*s%s\n", indent, "", "spares");
	} else {
		return;
	}
	for (c = 0; c < children; c++) {
		char *vname;

		vname = zpool_vdev_name(g_zfs, NULL, child[c], 0);
		(void) printf("\t%*s%s\n", indent + 2, "", vname);
		free(vname);
	}
}

typedef struct spare_cbdata {
	uint64_t	cb_guid;
	zpool_handle_t	*cb_zhp;
} spare_cbdata_t;

static boolean_t
find_vdev(nvlist_t *nv, uint64_t search)
{
	uint64_t guid;
	nvlist_t **child;
	uint_t c, children;

	if (nvlist_lookup_uint64(nv, ZPOOL_CONFIG_GUID, &guid) == 0 &&
	    search == guid)
		return (B_TRUE);

	if (nvlist_lookup_nvlist_array(nv, ZPOOL_CONFIG_CHILDREN,
	    &child, &children) == 0) {
		for (c = 0; c < children; c++)
			if (find_vdev(child[c], search))
				return (B_TRUE);
	}

	return (B_FALSE);
}

static int
find_spare(zpool_handle_t *zhp, void *data)
{
	spare_cbdata_t *cbp = data;
	nvlist_t *config, *nvroot;

	config = zpool_get_config(zhp, NULL);
	verify(nvlist_lookup_nvlist(config, ZPOOL_CONFIG_VDEV_TREE,
	    &nvroot) == 0);

	if (find_vdev(nvroot, cbp->cb_guid)) {
		cbp->cb_zhp = zhp;
		return (1);
	}

	zpool_close(zhp);
	return (0);
}

static void
nice_num_str_nvlist(nvlist_t *item, const char *key, uint64_t value,
    boolean_t literal, boolean_t as_int, int format)
{
	char buf[256];
	if (literal) {
		if (!as_int)
			snprintf(buf, 256, "%llu", (u_longlong_t)value);
	} else {
		switch (format) {
		case ZFS_NICENUM_1024:
			zfs_nicenum_format(value, buf, 256, ZFS_NICENUM_1024);
			break;
		case ZFS_NICENUM_BYTES:
			zfs_nicenum_format(value, buf, 256, ZFS_NICENUM_BYTES);
			break;
		case ZFS_NICENUM_TIME:
			zfs_nicenum_format(value, buf, 256, ZFS_NICENUM_TIME);
			break;
		case ZFS_NICE_TIMESTAMP:
			format_timestamp(value, buf, 256);
			break;
		default:
			fprintf(stderr, "Invalid number format");
			exit(1);
		}
	}
	if (as_int)
		fnvlist_add_uint64(item, key, value);
	else
		fnvlist_add_string(item, key, buf);
}

/*
 * Generates an nvlist with output version for every command based on params.
 * Purpose of this is to add a version of JSON output, considering the schema
 * format might be updated for each command in future.
 *
 * Schema:
 *
 * "output_version": {
 *    "command": string,
 *    "vers_major": integer,
 *    "vers_minor": integer,
 *  }
 */
static nvlist_t *
zpool_json_schema(int maj_v, int min_v)
{
	char cmd[MAX_CMD_LEN];
	nvlist_t *sch = fnvlist_alloc();
	nvlist_t *ov = fnvlist_alloc();

	snprintf(cmd, MAX_CMD_LEN, "zpool %s", current_command->name);
	fnvlist_add_string(ov, "command", cmd);
	fnvlist_add_uint32(ov, "vers_major", maj_v);
	fnvlist_add_uint32(ov, "vers_minor", min_v);
	fnvlist_add_nvlist(sch, "output_version", ov);
	fnvlist_free(ov);
	return (sch);
}

static void
fill_pool_info(nvlist_t *list, zpool_handle_t *zhp, boolean_t addtype,
    boolean_t as_int)
{
	nvlist_t *config = zpool_get_config(zhp, NULL);
	uint64_t guid = fnvlist_lookup_uint64(config, ZPOOL_CONFIG_POOL_GUID);
	uint64_t txg = fnvlist_lookup_uint64(config, ZPOOL_CONFIG_POOL_TXG);

	fnvlist_add_string(list, "name", zpool_get_name(zhp));
	if (addtype)
		fnvlist_add_string(list, "type", "POOL");
	fnvlist_add_string(list, "state", zpool_get_state_str(zhp));
	if (as_int) {
		if (guid)
			fnvlist_add_uint64(list, ZPOOL_CONFIG_POOL_GUID, guid);
		if (txg)
			fnvlist_add_uint64(list, ZPOOL_CONFIG_POOL_TXG, txg);
		fnvlist_add_uint64(list, "spa_version", SPA_VERSION);
		fnvlist_add_uint64(list, "zpl_version", ZPL_VERSION);
	} else {
		char value[ZFS_MAXPROPLEN];
		if (guid) {
			snprintf(value, ZFS_MAXPROPLEN, "%llu",
			    (u_longlong_t)guid);
			fnvlist_add_string(list, ZPOOL_CONFIG_POOL_GUID, value);
		}
		if (txg) {
			snprintf(value, ZFS_MAXPROPLEN, "%llu",
			    (u_longlong_t)txg);
			fnvlist_add_string(list, ZPOOL_CONFIG_POOL_TXG, value);
		}
		fnvlist_add_string(list, "spa_version", SPA_VERSION_STRING);
		fnvlist_add_string(list, "zpl_version", ZPL_VERSION_STRING);
	}
}

static void
used_by_other(zpool_handle_t *zhp, nvlist_t *nvdev, nvlist_t *list)
{
	spare_cbdata_t spare_cb;
	verify(nvlist_lookup_uint64(nvdev, ZPOOL_CONFIG_GUID,
	    &spare_cb.cb_guid) == 0);
	if (zpool_iter(g_zfs, find_spare, &spare_cb) == 1) {
		if (strcmp(zpool_get_name(spare_cb.cb_zhp),
		    zpool_get_name(zhp)) != 0) {
			fnvlist_add_string(list, "used_by",
			    zpool_get_name(spare_cb.cb_zhp));
		}
		zpool_close(spare_cb.cb_zhp);
	}
}

static void
fill_vdev_info(nvlist_t *list, zpool_handle_t *zhp, char *name,
    boolean_t addtype, boolean_t as_int)
{
	boolean_t l2c = B_FALSE;
	const char *path, *phys, *devid, *bias = NULL;
	uint64_t hole = 0, log = 0, spare = 0;
	vdev_stat_t *vs;
	uint_t c;
	nvlist_t *nvdev;
	nvlist_t *nvdev_parent = NULL;
	char *_name;

	if (strcmp(name, zpool_get_name(zhp)) != 0)
		_name = name;
	else
		_name = (char *)"root-0";

	nvdev = zpool_find_vdev(zhp, _name, NULL, &l2c, NULL);

	fnvlist_add_string(list, "name", name);
	if (addtype)
		fnvlist_add_string(list, "type", "VDEV");
	if (nvdev) {
		const char *type = fnvlist_lookup_string(nvdev,
		    ZPOOL_CONFIG_TYPE);
		if (type)
			fnvlist_add_string(list, "vdev_type", type);
		uint64_t guid = fnvlist_lookup_uint64(nvdev, ZPOOL_CONFIG_GUID);
		if (guid) {
			if (as_int) {
				fnvlist_add_uint64(list, "guid", guid);
			} else {
				char buf[ZFS_MAXPROPLEN];
				snprintf(buf, ZFS_MAXPROPLEN, "%llu",
				    (u_longlong_t)guid);
				fnvlist_add_string(list, "guid", buf);
			}
		}
		if (nvlist_lookup_string(nvdev, ZPOOL_CONFIG_PATH, &path) == 0)
			fnvlist_add_string(list, "path", path);
		if (nvlist_lookup_string(nvdev, ZPOOL_CONFIG_PHYS_PATH,
		    &phys) == 0)
			fnvlist_add_string(list, "phys_path", phys);
		if (nvlist_lookup_string(nvdev, ZPOOL_CONFIG_DEVID,
		    &devid) == 0)
			fnvlist_add_string(list, "devid", devid);
		(void) nvlist_lookup_uint64(nvdev, ZPOOL_CONFIG_IS_LOG, &log);
		(void) nvlist_lookup_uint64(nvdev, ZPOOL_CONFIG_IS_SPARE,
		    &spare);
		(void) nvlist_lookup_uint64(nvdev, ZPOOL_CONFIG_IS_HOLE, &hole);
		if (hole)
			fnvlist_add_string(list, "class", VDEV_TYPE_HOLE);
		else if (l2c)
			fnvlist_add_string(list, "class", VDEV_TYPE_L2CACHE);
		else if (spare)
			fnvlist_add_string(list, "class", VDEV_TYPE_SPARE);
		else if (log)
			fnvlist_add_string(list, "class", VDEV_TYPE_LOG);
		else {
			(void) nvlist_lookup_string(nvdev,
			    ZPOOL_CONFIG_ALLOCATION_BIAS, &bias);
			if (bias != NULL)
				fnvlist_add_string(list, "class", bias);
			else {
				nvdev_parent = NULL;
				nvdev_parent = zpool_find_parent_vdev(zhp,
				    _name, NULL, NULL, NULL);

				/*
				 * With a mirrored special device, the parent
				 * "mirror" vdev will have
				 * ZPOOL_CONFIG_ALLOCATION_BIAS set to "special"
				 * not the leaf vdevs.  If we're a leaf vdev
				 * in that case we need to look at our parent
				 * to see if they're "special" to know if we
				 * are "special" too.
				 */
				if (nvdev_parent) {
					(void) nvlist_lookup_string(
					    nvdev_parent,
					    ZPOOL_CONFIG_ALLOCATION_BIAS,
					    &bias);
				}
				if (bias != NULL)
					fnvlist_add_string(list, "class", bias);
				else
					fnvlist_add_string(list, "class",
					    "normal");
			}
		}
		if (nvlist_lookup_uint64_array(nvdev, ZPOOL_CONFIG_VDEV_STATS,
		    (uint64_t **)&vs, &c) == 0) {
			fnvlist_add_string(list, "state",
			    vdev_state_str[vs->vs_state]);
		}
	}
}

static boolean_t
prop_list_contains_feature(nvlist_t *proplist)
{
	nvpair_t *nvp;
	for (nvp = nvlist_next_nvpair(proplist, NULL); NULL != nvp;
	    nvp = nvlist_next_nvpair(proplist, nvp)) {
		if (zpool_prop_feature(nvpair_name(nvp)))
			return (B_TRUE);
	}
	return (B_FALSE);
}

/*
 * Add a property pair (name, string-value) into a property nvlist.
 */
static int
add_prop_list(const char *propname, const char *propval, nvlist_t **props,
    boolean_t poolprop)
{
	zpool_prop_t prop = ZPOOL_PROP_INVAL;
	nvlist_t *proplist;
	const char *normnm;
	const char *strval;

	if (*props == NULL &&
	    nvlist_alloc(props, NV_UNIQUE_NAME, 0) != 0) {
		(void) fprintf(stderr,
		    gettext("internal error: out of memory\n"));
		return (1);
	}

	proplist = *props;

	if (poolprop) {
		const char *vname = zpool_prop_to_name(ZPOOL_PROP_VERSION);
		const char *cname =
		    zpool_prop_to_name(ZPOOL_PROP_COMPATIBILITY);

		if ((prop = zpool_name_to_prop(propname)) == ZPOOL_PROP_INVAL &&
		    (!zpool_prop_feature(propname) &&
		    !zpool_prop_vdev(propname))) {
			(void) fprintf(stderr, gettext("property '%s' is "
			    "not a valid pool or vdev property\n"), propname);
			return (2);
		}

		/*
		 * feature@ properties and version should not be specified
		 * at the same time.
		 */
		if ((prop == ZPOOL_PROP_INVAL && zpool_prop_feature(propname) &&
		    nvlist_exists(proplist, vname)) ||
		    (prop == ZPOOL_PROP_VERSION &&
		    prop_list_contains_feature(proplist))) {
			(void) fprintf(stderr, gettext("'feature@' and "
			    "'version' properties cannot be specified "
			    "together\n"));
			return (2);
		}

		/*
		 * if version is specified, only "legacy" compatibility
		 * may be requested
		 */
		if ((prop == ZPOOL_PROP_COMPATIBILITY &&
		    strcmp(propval, ZPOOL_COMPAT_LEGACY) != 0 &&
		    nvlist_exists(proplist, vname)) ||
		    (prop == ZPOOL_PROP_VERSION &&
		    nvlist_exists(proplist, cname) &&
		    strcmp(fnvlist_lookup_string(proplist, cname),
		    ZPOOL_COMPAT_LEGACY) != 0)) {
			(void) fprintf(stderr, gettext("when 'version' is "
			    "specified, the 'compatibility' feature may only "
			    "be set to '" ZPOOL_COMPAT_LEGACY "'\n"));
			return (2);
		}

		if (zpool_prop_feature(propname) || zpool_prop_vdev(propname))
			normnm = propname;
		else
			normnm = zpool_prop_to_name(prop);
	} else {
		zfs_prop_t fsprop = zfs_name_to_prop(propname);

		if (zfs_prop_valid_for_type(fsprop, ZFS_TYPE_FILESYSTEM,
		    B_FALSE)) {
			normnm = zfs_prop_to_name(fsprop);
		} else if (zfs_prop_user(propname) ||
		    zfs_prop_userquota(propname)) {
			normnm = propname;
		} else {
			(void) fprintf(stderr, gettext("property '%s' is "
			    "not a valid filesystem property\n"), propname);
			return (2);
		}
	}

	if (nvlist_lookup_string(proplist, normnm, &strval) == 0 &&
	    prop != ZPOOL_PROP_CACHEFILE) {
		(void) fprintf(stderr, gettext("property '%s' "
		    "specified multiple times\n"), propname);
		return (2);
	}

	if (nvlist_add_string(proplist, normnm, propval) != 0) {
		(void) fprintf(stderr, gettext("internal "
		    "error: out of memory\n"));
		return (1);
	}

	return (0);
}

/*
 * Set a default property pair (name, string-value) in a property nvlist
 */
static int
add_prop_list_default(const char *propname, const char *propval,
    nvlist_t **props)
{
	const char *pval;

	if (nvlist_lookup_string(*props, propname, &pval) == 0)
		return (0);

	return (add_prop_list(propname, propval, props, B_TRUE));
}

/*
 * zpool add [-afgLnP] [-o property=value] <pool> <vdev> ...
 *
 *	-a	Disable the ashift validation checks
 *	-f	Force addition of devices, even if they appear in use
 *	-g	Display guid for individual vdev name.
 *	-L	Follow links when resolving vdev path name.
 *	-n	Do not add the devices, but display the resulting layout if
 *		they were to be added.
 *	-o	Set property=value.
 *	-P	Display full path for vdev name.
 *
 * Adds the given vdevs to 'pool'.  As with create, the bulk of this work is
 * handled by make_root_vdev(), which constructs the nvlist needed to pass to
 * libzfs.
 */
int
zpool_do_add(int argc, char **argv)
{
	boolean_t check_replication = B_TRUE;
	boolean_t check_inuse = B_TRUE;
	boolean_t dryrun = B_FALSE;
	boolean_t check_ashift = B_TRUE;
	boolean_t force = B_FALSE;
	int name_flags = 0;
	int c;
	nvlist_t *nvroot;
	char *poolname;
	int ret;
	zpool_handle_t *zhp;
	nvlist_t *config;
	nvlist_t *props = NULL;
	char *propval;

	struct option long_options[] = {
		{"allow-in-use", no_argument, NULL, ZPOOL_OPTION_ALLOW_INUSE},
		{"allow-replication-mismatch", no_argument, NULL,
		    ZPOOL_OPTION_ALLOW_REPLICATION_MISMATCH},
		{"allow-ashift-mismatch", no_argument, NULL,
		    ZPOOL_OPTION_ALLOW_ASHIFT_MISMATCH},
		{0, 0, 0, 0}
	};

	/* check options */
	while ((c = getopt_long(argc, argv, "fgLno:P", long_options, NULL))
	    != -1) {
		switch (c) {
		case 'f':
			force = B_TRUE;
			break;
		case 'g':
			name_flags |= VDEV_NAME_GUID;
			break;
		case 'L':
			name_flags |= VDEV_NAME_FOLLOW_LINKS;
			break;
		case 'n':
			dryrun = B_TRUE;
			break;
		case 'o':
			if ((propval = strchr(optarg, '=')) == NULL) {
				(void) fprintf(stderr, gettext("missing "
				    "'=' for -o option\n"));
				usage(B_FALSE);
			}
			*propval = '\0';
			propval++;

			if ((strcmp(optarg, ZPOOL_CONFIG_ASHIFT) != 0) ||
			    (add_prop_list(optarg, propval, &props, B_TRUE)))
				usage(B_FALSE);
			break;
		case 'P':
			name_flags |= VDEV_NAME_PATH;
			break;
		case ZPOOL_OPTION_ALLOW_INUSE:
			check_inuse = B_FALSE;
			break;
		case ZPOOL_OPTION_ALLOW_REPLICATION_MISMATCH:
			check_replication = B_FALSE;
			break;
		case ZPOOL_OPTION_ALLOW_ASHIFT_MISMATCH:
			check_ashift = B_FALSE;
			break;
		case '?':
			(void) fprintf(stderr, gettext("invalid option '%c'\n"),
			    optopt);
			usage(B_FALSE);
		}
	}

	argc -= optind;
	argv += optind;

	/* get pool name and check number of arguments */
	if (argc < 1) {
		(void) fprintf(stderr, gettext("missing pool name argument\n"));
		usage(B_FALSE);
	}
	if (argc < 2) {
		(void) fprintf(stderr, gettext("missing vdev specification\n"));
		usage(B_FALSE);
	}

	if (force) {
		if (!check_inuse || !check_replication || !check_ashift) {
			(void) fprintf(stderr, gettext("'-f' option is not "
			    "allowed with '--allow-replication-mismatch', "
			    "'--allow-ashift-mismatch', or "
			    "'--allow-in-use'\n"));
			usage(B_FALSE);
		}
		check_inuse = B_FALSE;
		check_replication = B_FALSE;
		check_ashift = B_FALSE;
	}

	poolname = argv[0];

	argc--;
	argv++;

	if ((zhp = zpool_open(g_zfs, poolname)) == NULL)
		return (1);

	if ((config = zpool_get_config(zhp, NULL)) == NULL) {
		(void) fprintf(stderr, gettext("pool '%s' is unavailable\n"),
		    poolname);
		zpool_close(zhp);
		return (1);
	}

	/* unless manually specified use "ashift" pool property (if set) */
	if (!nvlist_exists(props, ZPOOL_CONFIG_ASHIFT)) {
		int intval;
		zprop_source_t src;
		char strval[ZPOOL_MAXPROPLEN];

		intval = zpool_get_prop_int(zhp, ZPOOL_PROP_ASHIFT, &src);
		if (src != ZPROP_SRC_DEFAULT) {
			(void) sprintf(strval, "%" PRId32, intval);
			verify(add_prop_list(ZPOOL_CONFIG_ASHIFT, strval,
			    &props, B_TRUE) == 0);
		}
	}

	/* pass off to make_root_vdev for processing */
	nvroot = make_root_vdev(zhp, props, !check_inuse,
	    check_replication, B_FALSE, dryrun, argc, argv);
	if (nvroot == NULL) {
		zpool_close(zhp);
		return (1);
	}

	if (dryrun) {
		nvlist_t *poolnvroot;
		nvlist_t **l2child, **sparechild;
		uint_t l2children, sparechildren, c;
		char *vname;
		boolean_t hadcache = B_FALSE, hadspare = B_FALSE;

		verify(nvlist_lookup_nvlist(config, ZPOOL_CONFIG_VDEV_TREE,
		    &poolnvroot) == 0);

		(void) printf(gettext("would update '%s' to the following "
		    "configuration:\n\n"), zpool_get_name(zhp));

		/* print original main pool and new tree */
		print_vdev_tree(zhp, poolname, poolnvroot, 0, "",
		    name_flags | VDEV_NAME_TYPE_ID);
		print_vdev_tree(zhp, NULL, nvroot, 0, "", name_flags);

		/* print other classes: 'dedup', 'special', and 'log' */
		if (zfs_special_devs(poolnvroot, VDEV_ALLOC_BIAS_DEDUP)) {
			print_vdev_tree(zhp, "dedup", poolnvroot, 0,
			    VDEV_ALLOC_BIAS_DEDUP, name_flags);
			print_vdev_tree(zhp, NULL, nvroot, 0,
			    VDEV_ALLOC_BIAS_DEDUP, name_flags);
		} else if (zfs_special_devs(nvroot, VDEV_ALLOC_BIAS_DEDUP)) {
			print_vdev_tree(zhp, "dedup", nvroot, 0,
			    VDEV_ALLOC_BIAS_DEDUP, name_flags);
		}

		if (zfs_special_devs(poolnvroot, VDEV_ALLOC_BIAS_SPECIAL)) {
			print_vdev_tree(zhp, "special", poolnvroot, 0,
			    VDEV_ALLOC_BIAS_SPECIAL, name_flags);
			print_vdev_tree(zhp, NULL, nvroot, 0,
			    VDEV_ALLOC_BIAS_SPECIAL, name_flags);
		} else if (zfs_special_devs(nvroot, VDEV_ALLOC_BIAS_SPECIAL)) {
			print_vdev_tree(zhp, "special", nvroot, 0,
			    VDEV_ALLOC_BIAS_SPECIAL, name_flags);
		}

		if (num_logs(poolnvroot) > 0) {
			print_vdev_tree(zhp, "logs", poolnvroot, 0,
			    VDEV_ALLOC_BIAS_LOG, name_flags);
			print_vdev_tree(zhp, NULL, nvroot, 0,
			    VDEV_ALLOC_BIAS_LOG, name_flags);
		} else if (num_logs(nvroot) > 0) {
			print_vdev_tree(zhp, "logs", nvroot, 0,
			    VDEV_ALLOC_BIAS_LOG, name_flags);
		}

		/* Do the same for the caches */
		if (nvlist_lookup_nvlist_array(poolnvroot, ZPOOL_CONFIG_L2CACHE,
		    &l2child, &l2children) == 0 && l2children) {
			hadcache = B_TRUE;
			(void) printf(gettext("\tcache\n"));
			for (c = 0; c < l2children; c++) {
				vname = zpool_vdev_name(g_zfs, NULL,
				    l2child[c], name_flags);
				(void) printf("\t  %s\n", vname);
				free(vname);
			}
		}
		if (nvlist_lookup_nvlist_array(nvroot, ZPOOL_CONFIG_L2CACHE,
		    &l2child, &l2children) == 0 && l2children) {
			if (!hadcache)
				(void) printf(gettext("\tcache\n"));
			for (c = 0; c < l2children; c++) {
				vname = zpool_vdev_name(g_zfs, NULL,
				    l2child[c], name_flags);
				(void) printf("\t  %s\n", vname);
				free(vname);
			}
		}
		/* And finally the spares */
		if (nvlist_lookup_nvlist_array(poolnvroot, ZPOOL_CONFIG_SPARES,
		    &sparechild, &sparechildren) == 0 && sparechildren > 0) {
			hadspare = B_TRUE;
			(void) printf(gettext("\tspares\n"));
			for (c = 0; c < sparechildren; c++) {
				vname = zpool_vdev_name(g_zfs, NULL,
				    sparechild[c], name_flags);
				(void) printf("\t  %s\n", vname);
				free(vname);
			}
		}
		if (nvlist_lookup_nvlist_array(nvroot, ZPOOL_CONFIG_SPARES,
		    &sparechild, &sparechildren) == 0 && sparechildren > 0) {
			if (!hadspare)
				(void) printf(gettext("\tspares\n"));
			for (c = 0; c < sparechildren; c++) {
				vname = zpool_vdev_name(g_zfs, NULL,
				    sparechild[c], name_flags);
				(void) printf("\t  %s\n", vname);
				free(vname);
			}
		}

		ret = 0;
	} else {
		ret = (zpool_add(zhp, nvroot, check_ashift) != 0);
	}

	nvlist_free(props);
	nvlist_free(nvroot);
	zpool_close(zhp);

	return (ret);
}

/*
 * zpool remove [-npsw] <pool> <vdev> ...
 *
 * Removes the given vdev from the pool.
 */
int
zpool_do_remove(int argc, char **argv)
{
	char *poolname;
	int i, ret = 0;
	zpool_handle_t *zhp = NULL;
	boolean_t stop = B_FALSE;
	int c;
	boolean_t noop = B_FALSE;
	boolean_t parsable = B_FALSE;
	boolean_t wait = B_FALSE;

	/* check options */
	while ((c = getopt(argc, argv, "npsw")) != -1) {
		switch (c) {
		case 'n':
			noop = B_TRUE;
			break;
		case 'p':
			parsable = B_TRUE;
			break;
		case 's':
			stop = B_TRUE;
			break;
		case 'w':
			wait = B_TRUE;
			break;
		case '?':
			(void) fprintf(stderr, gettext("invalid option '%c'\n"),
			    optopt);
			usage(B_FALSE);
		}
	}

	argc -= optind;
	argv += optind;

	/* get pool name and check number of arguments */
	if (argc < 1) {
		(void) fprintf(stderr, gettext("missing pool name argument\n"));
		usage(B_FALSE);
	}

	poolname = argv[0];

	if ((zhp = zpool_open(g_zfs, poolname)) == NULL)
		return (1);

	if (stop && noop) {
		zpool_close(zhp);
		(void) fprintf(stderr, gettext("stop request ignored\n"));
		return (0);
	}

	if (stop) {
		if (argc > 1) {
			(void) fprintf(stderr, gettext("too many arguments\n"));
			usage(B_FALSE);
		}
		if (zpool_vdev_remove_cancel(zhp) != 0)
			ret = 1;
		if (wait) {
			(void) fprintf(stderr, gettext("invalid option "
			    "combination: -w cannot be used with -s\n"));
			usage(B_FALSE);
		}
	} else {
		if (argc < 2) {
			(void) fprintf(stderr, gettext("missing device\n"));
			usage(B_FALSE);
		}

		for (i = 1; i < argc; i++) {
			if (noop) {
				uint64_t size;

				if (zpool_vdev_indirect_size(zhp, argv[i],
				    &size) != 0) {
					ret = 1;
					break;
				}
				if (parsable) {
					(void) printf("%s %llu\n",
					    argv[i], (unsigned long long)size);
				} else {
					char valstr[32];
					zfs_nicenum(size, valstr,
					    sizeof (valstr));
					(void) printf("Memory that will be "
					    "used after removing %s: %s\n",
					    argv[i], valstr);
				}
			} else {
				if (zpool_vdev_remove(zhp, argv[i]) != 0)
					ret = 1;
			}
		}

		if (ret == 0 && wait)
			ret = zpool_wait(zhp, ZPOOL_WAIT_REMOVE);
	}
	zpool_close(zhp);

	return (ret);
}

/*
 * Return 1 if a vdev is active (being used in a pool)
 * Return 0 if a vdev is inactive (offlined or faulted, or not in active pool)
 *
 * This is useful for checking if a disk in an active pool is offlined or
 * faulted.
 */
static int
vdev_is_active(char *vdev_path)
{
	int fd;
	fd = open(vdev_path, O_EXCL);
	if (fd < 0) {
		return (1);   /* cant open O_EXCL - disk is active */
	}

	close(fd);
	return (0);   /* disk is inactive in the pool */
}

/*
 * zpool labelclear [-f] <vdev>
 *
 *	-f	Force clearing the label for the vdevs which are members of
 *		the exported or foreign pools.
 *
 * Verifies that the vdev is not active and zeros out the label information
 * on the device.
 */
int
zpool_do_labelclear(int argc, char **argv)
{
	char vdev[MAXPATHLEN];
	char *name = NULL;
	int c, fd, ret = 0;
	nvlist_t *config;
	pool_state_t state;
	boolean_t inuse = B_FALSE;
	boolean_t force = B_FALSE;

	/* check options */
	while ((c = getopt(argc, argv, "f")) != -1) {
		switch (c) {
		case 'f':
			force = B_TRUE;
			break;
		default:
			(void) fprintf(stderr, gettext("invalid option '%c'\n"),
			    optopt);
			usage(B_FALSE);
		}
	}

	argc -= optind;
	argv += optind;

	/* get vdev name */
	if (argc < 1) {
		(void) fprintf(stderr, gettext("missing vdev name\n"));
		usage(B_FALSE);
	}
	if (argc > 1) {
		(void) fprintf(stderr, gettext("too many arguments\n"));
		usage(B_FALSE);
	}

	(void) strlcpy(vdev, argv[0], sizeof (vdev));

	/*
	 * If we cannot open an absolute path, we quit.
	 * Otherwise if the provided vdev name doesn't point to a file,
	 * try prepending expected disk paths and partition numbers.
	 */
	if ((fd = open(vdev, O_RDWR)) < 0) {
		int error;
		if (vdev[0] == '/') {
			(void) fprintf(stderr, gettext("failed to open "
			    "%s: %s\n"), vdev, strerror(errno));
			return (1);
		}

		error = zfs_resolve_shortname(argv[0], vdev, MAXPATHLEN);
		if (error == 0 && zfs_dev_is_whole_disk(vdev)) {
			if (zfs_append_partition(vdev, MAXPATHLEN) == -1)
				error = ENOENT;
		}

		if (error || ((fd = open(vdev, O_RDWR)) < 0)) {
			if (errno == ENOENT) {
				(void) fprintf(stderr, gettext(
				    "failed to find device %s, try "
				    "specifying absolute path instead\n"),
				    argv[0]);
				return (1);
			}

			(void) fprintf(stderr, gettext("failed to open %s:"
			    " %s\n"), vdev, strerror(errno));
			return (1);
		}
	}

	/*
	 * Flush all dirty pages for the block device.  This should not be
	 * fatal when the device does not support BLKFLSBUF as would be the
	 * case for a file vdev.
	 */
	if ((zfs_dev_flush(fd) != 0) && (errno != ENOTTY))
		(void) fprintf(stderr, gettext("failed to invalidate "
		    "cache for %s: %s\n"), vdev, strerror(errno));

	if (zpool_read_label(fd, &config, NULL) != 0) {
		(void) fprintf(stderr,
		    gettext("failed to read label from %s\n"), vdev);
		ret = 1;
		goto errout;
	}
	nvlist_free(config);

	ret = zpool_in_use(g_zfs, fd, &state, &name, &inuse);
	if (ret != 0) {
		(void) fprintf(stderr,
		    gettext("failed to check state for %s\n"), vdev);
		ret = 1;
		goto errout;
	}

	if (!inuse)
		goto wipe_label;

	switch (state) {
	default:
	case POOL_STATE_ACTIVE:
	case POOL_STATE_SPARE:
	case POOL_STATE_L2CACHE:
		/*
		 * We allow the user to call 'zpool offline -f'
		 * on an offlined disk in an active pool. We can check if
		 * the disk is online by calling vdev_is_active().
		 */
		if (force && !vdev_is_active(vdev))
			break;

		(void) fprintf(stderr, gettext(
		    "%s is a member (%s) of pool \"%s\""),
		    vdev, zpool_pool_state_to_name(state), name);

		if (force) {
			(void) fprintf(stderr, gettext(
			    ". Offline the disk first to clear its label."));
		}
		printf("\n");
		ret = 1;
		goto errout;

	case POOL_STATE_EXPORTED:
		if (force)
			break;
		(void) fprintf(stderr, gettext(
		    "use '-f' to override the following error:\n"
		    "%s is a member of exported pool \"%s\"\n"),
		    vdev, name);
		ret = 1;
		goto errout;

	case POOL_STATE_POTENTIALLY_ACTIVE:
		if (force)
			break;
		(void) fprintf(stderr, gettext(
		    "use '-f' to override the following error:\n"
		    "%s is a member of potentially active pool \"%s\"\n"),
		    vdev, name);
		ret = 1;
		goto errout;

	case POOL_STATE_DESTROYED:
		/* inuse should never be set for a destroyed pool */
		assert(0);
		break;
	}

wipe_label:
	ret = zpool_clear_label(fd);
	if (ret != 0) {
		(void) fprintf(stderr,
		    gettext("failed to clear label for %s\n"), vdev);
	}

errout:
	free(name);
	(void) close(fd);

	return (ret);
}

/*
 * zpool create [-fnd] [-o property=value] ...
 *		[-O file-system-property=value] ...
 *		[-R root] [-m mountpoint] <pool> <dev> ...
 *
 *	-f	Force creation, even if devices appear in use
 *	-n	Do not create the pool, but display the resulting layout if it
 *		were to be created.
 *      -R	Create a pool under an alternate root
 *      -m	Set default mountpoint for the root dataset.  By default it's
 *		'/<pool>'
 *	-o	Set property=value.
 *	-o	Set feature@feature=enabled|disabled.
 *	-d	Don't automatically enable all supported pool features
 *		(individual features can be enabled with -o).
 *	-O	Set fsproperty=value in the pool's root file system
 *
 * Creates the named pool according to the given vdev specification.  The
 * bulk of the vdev processing is done in make_root_vdev() in zpool_vdev.c.
 * Once we get the nvlist back from make_root_vdev(), we either print out the
 * contents (if '-n' was specified), or pass it to libzfs to do the creation.
 */
int
zpool_do_create(int argc, char **argv)
{
	boolean_t force = B_FALSE;
	boolean_t dryrun = B_FALSE;
	boolean_t enable_pool_features = B_TRUE;

	int c;
	nvlist_t *nvroot = NULL;
	char *poolname;
	char *tname = NULL;
	int ret = 1;
	char *altroot = NULL;
	char *compat = NULL;
	char *mountpoint = NULL;
	nvlist_t *fsprops = NULL;
	nvlist_t *props = NULL;
	char *propval;

	/* check options */
	while ((c = getopt(argc, argv, ":fndR:m:o:O:t:")) != -1) {
		switch (c) {
		case 'f':
			force = B_TRUE;
			break;
		case 'n':
			dryrun = B_TRUE;
			break;
		case 'd':
			enable_pool_features = B_FALSE;
			break;
		case 'R':
			altroot = optarg;
			if (add_prop_list(zpool_prop_to_name(
			    ZPOOL_PROP_ALTROOT), optarg, &props, B_TRUE))
				goto errout;
			if (add_prop_list_default(zpool_prop_to_name(
			    ZPOOL_PROP_CACHEFILE), "none", &props))
				goto errout;
			break;
		case 'm':
			/* Equivalent to -O mountpoint=optarg */
			mountpoint = optarg;
			break;
		case 'o':
			if ((propval = strchr(optarg, '=')) == NULL) {
				(void) fprintf(stderr, gettext("missing "
				    "'=' for -o option\n"));
				goto errout;
			}
			*propval = '\0';
			propval++;

			if (add_prop_list(optarg, propval, &props, B_TRUE))
				goto errout;

			/*
			 * If the user is creating a pool that doesn't support
			 * feature flags, don't enable any features.
			 */
			if (zpool_name_to_prop(optarg) == ZPOOL_PROP_VERSION) {
				char *end;
				u_longlong_t ver;

				ver = strtoull(propval, &end, 0);
				if (*end == '\0' &&
				    ver < SPA_VERSION_FEATURES) {
					enable_pool_features = B_FALSE;
				}
			}
			if (zpool_name_to_prop(optarg) == ZPOOL_PROP_ALTROOT)
				altroot = propval;
			if (zpool_name_to_prop(optarg) ==
			    ZPOOL_PROP_COMPATIBILITY)
				compat = propval;
			break;
		case 'O':
			if ((propval = strchr(optarg, '=')) == NULL) {
				(void) fprintf(stderr, gettext("missing "
				    "'=' for -O option\n"));
				goto errout;
			}
			*propval = '\0';
			propval++;

			/*
			 * Mountpoints are checked and then added later.
			 * Uniquely among properties, they can be specified
			 * more than once, to avoid conflict with -m.
			 */
			if (0 == strcmp(optarg,
			    zfs_prop_to_name(ZFS_PROP_MOUNTPOINT))) {
				mountpoint = propval;
			} else if (add_prop_list(optarg, propval, &fsprops,
			    B_FALSE)) {
				goto errout;
			}
			break;
		case 't':
			/*
			 * Sanity check temporary pool name.
			 */
			if (strchr(optarg, '/') != NULL) {
				(void) fprintf(stderr, gettext("cannot create "
				    "'%s': invalid character '/' in temporary "
				    "name\n"), optarg);
				(void) fprintf(stderr, gettext("use 'zfs "
				    "create' to create a dataset\n"));
				goto errout;
			}

			if (add_prop_list(zpool_prop_to_name(
			    ZPOOL_PROP_TNAME), optarg, &props, B_TRUE))
				goto errout;
			if (add_prop_list_default(zpool_prop_to_name(
			    ZPOOL_PROP_CACHEFILE), "none", &props))
				goto errout;
			tname = optarg;
			break;
		case ':':
			(void) fprintf(stderr, gettext("missing argument for "
			    "'%c' option\n"), optopt);
			goto badusage;
		case '?':
			(void) fprintf(stderr, gettext("invalid option '%c'\n"),
			    optopt);
			goto badusage;
		}
	}

	argc -= optind;
	argv += optind;

	/* get pool name and check number of arguments */
	if (argc < 1) {
		(void) fprintf(stderr, gettext("missing pool name argument\n"));
		goto badusage;
	}
	if (argc < 2) {
		(void) fprintf(stderr, gettext("missing vdev specification\n"));
		goto badusage;
	}

	poolname = argv[0];

	/*
	 * As a special case, check for use of '/' in the name, and direct the
	 * user to use 'zfs create' instead.
	 */
	if (strchr(poolname, '/') != NULL) {
		(void) fprintf(stderr, gettext("cannot create '%s': invalid "
		    "character '/' in pool name\n"), poolname);
		(void) fprintf(stderr, gettext("use 'zfs create' to "
		    "create a dataset\n"));
		goto errout;
	}

	/* pass off to make_root_vdev for bulk processing */
	nvroot = make_root_vdev(NULL, props, force, !force, B_FALSE, dryrun,
	    argc - 1, argv + 1);
	if (nvroot == NULL)
		goto errout;

	/* make_root_vdev() allows 0 toplevel children if there are spares */
	if (!zfs_allocatable_devs(nvroot)) {
		(void) fprintf(stderr, gettext("invalid vdev "
		    "specification: at least one toplevel vdev must be "
		    "specified\n"));
		goto errout;
	}

	if (altroot != NULL && altroot[0] != '/') {
		(void) fprintf(stderr, gettext("invalid alternate root '%s': "
		    "must be an absolute path\n"), altroot);
		goto errout;
	}

	/*
	 * Check the validity of the mountpoint and direct the user to use the
	 * '-m' mountpoint option if it looks like its in use.
	 */
	if (mountpoint == NULL ||
	    (strcmp(mountpoint, ZFS_MOUNTPOINT_LEGACY) != 0 &&
	    strcmp(mountpoint, ZFS_MOUNTPOINT_NONE) != 0)) {
		char buf[MAXPATHLEN];
		DIR *dirp;

		if (mountpoint && mountpoint[0] != '/') {
			(void) fprintf(stderr, gettext("invalid mountpoint "
			    "'%s': must be an absolute path, 'legacy', or "
			    "'none'\n"), mountpoint);
			goto errout;
		}

		if (mountpoint == NULL) {
			if (altroot != NULL)
				(void) snprintf(buf, sizeof (buf), "%s/%s",
				    altroot, poolname);
			else
				(void) snprintf(buf, sizeof (buf), "/%s",
				    poolname);
		} else {
			if (altroot != NULL)
				(void) snprintf(buf, sizeof (buf), "%s%s",
				    altroot, mountpoint);
			else
				(void) snprintf(buf, sizeof (buf), "%s",
				    mountpoint);
		}

		if ((dirp = opendir(buf)) == NULL && errno != ENOENT) {
			(void) fprintf(stderr, gettext("mountpoint '%s' : "
			    "%s\n"), buf, strerror(errno));
			(void) fprintf(stderr, gettext("use '-m' "
			    "option to provide a different default\n"));
			goto errout;
		} else if (dirp) {
			int count = 0;

			while (count < 3 && readdir(dirp) != NULL)
				count++;
			(void) closedir(dirp);

			if (count > 2) {
				(void) fprintf(stderr, gettext("mountpoint "
				    "'%s' exists and is not empty\n"), buf);
				(void) fprintf(stderr, gettext("use '-m' "
				    "option to provide a "
				    "different default\n"));
				goto errout;
			}
		}
	}

	/*
	 * Now that the mountpoint's validity has been checked, ensure that
	 * the property is set appropriately prior to creating the pool.
	 */
	if (mountpoint != NULL) {
		ret = add_prop_list(zfs_prop_to_name(ZFS_PROP_MOUNTPOINT),
		    mountpoint, &fsprops, B_FALSE);
		if (ret != 0)
			goto errout;
	}

	ret = 1;
	if (dryrun) {
		/*
		 * For a dry run invocation, print out a basic message and run
		 * through all the vdevs in the list and print out in an
		 * appropriate hierarchy.
		 */
		(void) printf(gettext("would create '%s' with the "
		    "following layout:\n\n"), poolname);

		print_vdev_tree(NULL, poolname, nvroot, 0, "", 0);
		print_vdev_tree(NULL, "dedup", nvroot, 0,
		    VDEV_ALLOC_BIAS_DEDUP, 0);
		print_vdev_tree(NULL, "special", nvroot, 0,
		    VDEV_ALLOC_BIAS_SPECIAL, 0);
		print_vdev_tree(NULL, "logs", nvroot, 0,
		    VDEV_ALLOC_BIAS_LOG, 0);
		print_cache_list(nvroot, 0);
		print_spare_list(nvroot, 0);

		ret = 0;
	} else {
		/*
		 * Load in feature set.
		 * Note: if compatibility property not given, we'll have
		 * NULL, which means 'all features'.
		 */
		boolean_t requested_features[SPA_FEATURES];
		if (zpool_do_load_compat(compat, requested_features) !=
		    ZPOOL_COMPATIBILITY_OK)
			goto errout;

		/*
		 * props contains list of features to enable.
		 * For each feature:
		 *  - remove it if feature@name=disabled
		 *  - leave it there if feature@name=enabled
		 *  - add it if:
		 *    - enable_pool_features (ie: no '-d' or '-o version')
		 *    - it's supported by the kernel module
		 *    - it's in the requested feature set
		 *  - warn if it's enabled but not in compat
		 */
		for (spa_feature_t i = 0; i < SPA_FEATURES; i++) {
			char propname[MAXPATHLEN];
			const char *propval;
			zfeature_info_t *feat = &spa_feature_table[i];

			(void) snprintf(propname, sizeof (propname),
			    "feature@%s", feat->fi_uname);

			if (!nvlist_lookup_string(props, propname, &propval)) {
				if (strcmp(propval,
				    ZFS_FEATURE_DISABLED) == 0) {
					(void) nvlist_remove_all(props,
					    propname);
				} else if (strcmp(propval,
				    ZFS_FEATURE_ENABLED) == 0 &&
				    !requested_features[i]) {
					(void) fprintf(stderr, gettext(
					    "Warning: feature \"%s\" enabled "
					    "but is not in specified "
					    "'compatibility' feature set.\n"),
					    feat->fi_uname);
				}
			} else if (
			    enable_pool_features &&
			    feat->fi_zfs_mod_supported &&
			    requested_features[i]) {
				ret = add_prop_list(propname,
				    ZFS_FEATURE_ENABLED, &props, B_TRUE);
				if (ret != 0)
					goto errout;
			}
		}

		ret = 1;
		if (zpool_create(g_zfs, poolname,
		    nvroot, props, fsprops) == 0) {
			zfs_handle_t *pool = zfs_open(g_zfs,
			    tname ? tname : poolname, ZFS_TYPE_FILESYSTEM);
			if (pool != NULL) {
				if (zfs_mount(pool, NULL, 0) == 0) {
					ret = zfs_share(pool, NULL);
					zfs_commit_shares(NULL);
				}
				zfs_close(pool);
			}
		} else if (libzfs_errno(g_zfs) == EZFS_INVALIDNAME) {
			(void) fprintf(stderr, gettext("pool name may have "
			    "been omitted\n"));
		}
	}

errout:
	nvlist_free(nvroot);
	nvlist_free(fsprops);
	nvlist_free(props);
	return (ret);
badusage:
	nvlist_free(fsprops);
	nvlist_free(props);
	usage(B_FALSE);
	return (2);
}

/*
 * zpool destroy <pool>
 *
 * 	-f	Forcefully unmount any datasets
 *
 * Destroy the given pool.  Automatically unmounts any datasets in the pool.
 */
int
zpool_do_destroy(int argc, char **argv)
{
	boolean_t force = B_FALSE;
	int c;
	char *pool;
	zpool_handle_t *zhp;
	int ret;

	/* check options */
	while ((c = getopt(argc, argv, "f")) != -1) {
		switch (c) {
		case 'f':
			force = B_TRUE;
			break;
		case '?':
			(void) fprintf(stderr, gettext("invalid option '%c'\n"),
			    optopt);
			usage(B_FALSE);
		}
	}

	argc -= optind;
	argv += optind;

	/* check arguments */
	if (argc < 1) {
		(void) fprintf(stderr, gettext("missing pool argument\n"));
		usage(B_FALSE);
	}
	if (argc > 1) {
		(void) fprintf(stderr, gettext("too many arguments\n"));
		usage(B_FALSE);
	}

	pool = argv[0];

	if ((zhp = zpool_open_canfail(g_zfs, pool)) == NULL) {
		/*
		 * As a special case, check for use of '/' in the name, and
		 * direct the user to use 'zfs destroy' instead.
		 */
		if (strchr(pool, '/') != NULL)
			(void) fprintf(stderr, gettext("use 'zfs destroy' to "
			    "destroy a dataset\n"));
		return (1);
	}

	if (zpool_disable_datasets(zhp, force) != 0) {
		(void) fprintf(stderr, gettext("could not destroy '%s': "
		    "could not unmount datasets\n"), zpool_get_name(zhp));
		zpool_close(zhp);
		return (1);
	}

	/* The history must be logged as part of the export */
	log_history = B_FALSE;

	ret = (zpool_destroy(zhp, history_str) != 0);

	zpool_close(zhp);

	return (ret);
}

typedef struct export_cbdata {
	tpool_t *tpool;
	pthread_mutex_t mnttab_lock;
	boolean_t force;
	boolean_t hardforce;
	int retval;
} export_cbdata_t;


typedef struct {
	char *aea_poolname;
	export_cbdata_t	*aea_cbdata;
} async_export_args_t;

/*
 * Export one pool
 */
static int
zpool_export_one(zpool_handle_t *zhp, void *data)
{
	export_cbdata_t *cb = data;

	/*
	 * zpool_disable_datasets() is not thread-safe for mnttab access.
	 * So we serialize access here for 'zpool export -a' parallel case.
	 */
	if (cb->tpool != NULL)
		pthread_mutex_lock(&cb->mnttab_lock);

	int retval = zpool_disable_datasets(zhp, cb->force);

	if (cb->tpool != NULL)
		pthread_mutex_unlock(&cb->mnttab_lock);

	if (retval)
		return (1);

	if (cb->hardforce) {
		if (zpool_export_force(zhp, history_str) != 0)
			return (1);
	} else if (zpool_export(zhp, cb->force, history_str) != 0) {
		return (1);
	}

	return (0);
}

/*
 * Asynchronous export request
 */
static void
zpool_export_task(void *arg)
{
	async_export_args_t *aea = arg;

	zpool_handle_t *zhp = zpool_open(g_zfs, aea->aea_poolname);
	if (zhp != NULL) {
		int ret = zpool_export_one(zhp, aea->aea_cbdata);
		if (ret != 0)
			aea->aea_cbdata->retval = ret;
		zpool_close(zhp);
	} else {
		aea->aea_cbdata->retval = 1;
	}

	free(aea->aea_poolname);
	free(aea);
}

/*
 * Process an export request in parallel
 */
static int
zpool_export_one_async(zpool_handle_t *zhp, void *data)
{
	tpool_t *tpool = ((export_cbdata_t *)data)->tpool;
	async_export_args_t *aea = safe_malloc(sizeof (async_export_args_t));

	/* save pool name since zhp will go out of scope */
	aea->aea_poolname = strdup(zpool_get_name(zhp));
	aea->aea_cbdata = data;

	/* ship off actual export to another thread */
	if (tpool_dispatch(tpool, zpool_export_task, (void *)aea) != 0)
		return (errno);	/* unlikely */
	else
		return (0);
}

/*
 * zpool export [-f] <pool> ...
 *
 *	-a	Export all pools
 *	-f	Forcefully unmount datasets
 *
 * Export the given pools.  By default, the command will attempt to cleanly
 * unmount any active datasets within the pool.  If the '-f' flag is specified,
 * then the datasets will be forcefully unmounted.
 */
int
zpool_do_export(int argc, char **argv)
{
	export_cbdata_t cb;
	boolean_t do_all = B_FALSE;
	boolean_t force = B_FALSE;
	boolean_t hardforce = B_FALSE;
	int c, ret;

	/* check options */
	while ((c = getopt(argc, argv, "afF")) != -1) {
		switch (c) {
		case 'a':
			do_all = B_TRUE;
			break;
		case 'f':
			force = B_TRUE;
			break;
		case 'F':
			hardforce = B_TRUE;
			break;
		case '?':
			(void) fprintf(stderr, gettext("invalid option '%c'\n"),
			    optopt);
			usage(B_FALSE);
		}
	}

	cb.force = force;
	cb.hardforce = hardforce;
	cb.tpool = NULL;
	cb.retval = 0;
	argc -= optind;
	argv += optind;

	/* The history will be logged as part of the export itself */
	log_history = B_FALSE;

	if (do_all) {
		if (argc != 0) {
			(void) fprintf(stderr, gettext("too many arguments\n"));
			usage(B_FALSE);
		}

		cb.tpool = tpool_create(1, 5 * sysconf(_SC_NPROCESSORS_ONLN),
		    0, NULL);
		pthread_mutex_init(&cb.mnttab_lock, NULL);

		/* Asynchronously call zpool_export_one using thread pool */
		ret = for_each_pool(argc, argv, B_TRUE, NULL, ZFS_TYPE_POOL,
		    B_FALSE, zpool_export_one_async, &cb);

		tpool_wait(cb.tpool);
		tpool_destroy(cb.tpool);
		(void) pthread_mutex_destroy(&cb.mnttab_lock);

		return (ret | cb.retval);
	}

	/* check arguments */
	if (argc < 1) {
		(void) fprintf(stderr, gettext("missing pool argument\n"));
		usage(B_FALSE);
	}

	ret = for_each_pool(argc, argv, B_TRUE, NULL, ZFS_TYPE_POOL,
	    B_FALSE, zpool_export_one, &cb);

	return (ret);
}

/*
 * Given a vdev configuration, determine the maximum width needed for the device
 * name column.
 */
static int
max_width(zpool_handle_t *zhp, nvlist_t *nv, int depth, int max,
    int name_flags)
{
	static const char *const subtypes[] =
	    {ZPOOL_CONFIG_SPARES, ZPOOL_CONFIG_L2CACHE, ZPOOL_CONFIG_CHILDREN};

	char *name = zpool_vdev_name(g_zfs, zhp, nv, name_flags);
	max = MAX(strlen(name) + depth, max);
	free(name);

	nvlist_t **child;
	uint_t children;
	for (size_t i = 0; i < ARRAY_SIZE(subtypes); ++i)
		if (nvlist_lookup_nvlist_array(nv, subtypes[i],
		    &child, &children) == 0)
			for (uint_t c = 0; c < children; ++c)
				max = MAX(max_width(zhp, child[c], depth + 2,
				    max, name_flags), max);

	return (max);
}

typedef struct status_cbdata {
	int		cb_count;
	int		cb_name_flags;
	int		cb_namewidth;
	boolean_t	cb_allpools;
	boolean_t	cb_verbose;
	boolean_t	cb_literal;
	boolean_t	cb_explain;
	boolean_t	cb_first;
	boolean_t	cb_dedup_stats;
	boolean_t	cb_print_unhealthy;
	boolean_t	cb_print_status;
	boolean_t	cb_print_slow_ios;
	boolean_t	cb_print_dio_verify;
	boolean_t	cb_print_vdev_init;
	boolean_t	cb_print_vdev_trim;
	vdev_cmd_data_list_t	*vcdl;
	boolean_t	cb_print_power;
	boolean_t	cb_json;
	boolean_t	cb_flat_vdevs;
	nvlist_t	*cb_jsobj;
	boolean_t	cb_json_as_int;
	boolean_t	cb_json_pool_key_guid;
} status_cbdata_t;

/* Return 1 if string is NULL, empty, or whitespace; return 0 otherwise. */
static boolean_t
is_blank_str(const char *str)
{
	for (; str != NULL && *str != '\0'; ++str)
		if (!isblank(*str))
			return (B_FALSE);
	return (B_TRUE);
}

static void
zpool_nvlist_cmd(vdev_cmd_data_list_t *vcdl, const char *pool, const char *path,
    nvlist_t *item)
{
	vdev_cmd_data_t *data;
	int i, j, k = 1;
	char tmp[256];
	const char *val;

	for (i = 0; i < vcdl->count; i++) {
		if ((strcmp(vcdl->data[i].path, path) != 0) ||
		    (strcmp(vcdl->data[i].pool, pool) != 0))
			continue;

		data = &vcdl->data[i];
		for (j = 0; j < vcdl->uniq_cols_cnt; j++) {
			val = NULL;
			for (int k = 0; k < data->cols_cnt; k++) {
				if (strcmp(data->cols[k],
				    vcdl->uniq_cols[j]) == 0) {
					val = data->lines[k];
					break;
				}
			}
			if (val == NULL || is_blank_str(val))
				val = "-";
			fnvlist_add_string(item, vcdl->uniq_cols[j], val);
		}

		for (j = data->cols_cnt; j < data->lines_cnt; j++) {
			if (data->lines[j]) {
				snprintf(tmp, 256, "extra_%d", k++);
				fnvlist_add_string(item, tmp,
				    data->lines[j]);
			}
		}
		break;
	}
}

/* Print command output lines for specific vdev in a specific pool */
static void
zpool_print_cmd(vdev_cmd_data_list_t *vcdl, const char *pool, const char *path)
{
	vdev_cmd_data_t *data;
	int i, j;
	const char *val;

	for (i = 0; i < vcdl->count; i++) {
		if ((strcmp(vcdl->data[i].path, path) != 0) ||
		    (strcmp(vcdl->data[i].pool, pool) != 0)) {
			/* Not the vdev we're looking for */
			continue;
		}

		data = &vcdl->data[i];
		/* Print out all the output values for this vdev */
		for (j = 0; j < vcdl->uniq_cols_cnt; j++) {
			val = NULL;
			/* Does this vdev have values for this column? */
			for (int k = 0; k < data->cols_cnt; k++) {
				if (strcmp(data->cols[k],
				    vcdl->uniq_cols[j]) == 0) {
					/* yes it does, record the value */
					val = data->lines[k];
					break;
				}
			}
			/*
			 * Mark empty values with dashes to make output
			 * awk-able.
			 */
			if (val == NULL || is_blank_str(val))
				val = "-";

			printf("%*s", vcdl->uniq_cols_width[j], val);
			if (j < vcdl->uniq_cols_cnt - 1)
				fputs("  ", stdout);
		}

		/* Print out any values that aren't in a column at the end */
		for (j = data->cols_cnt; j < data->lines_cnt; j++) {
			/* Did we have any columns?  If so print a spacer. */
			if (vcdl->uniq_cols_cnt > 0)
				fputs("  ", stdout);

			val = data->lines[j];
			fputs(val ?: "", stdout);
		}
		break;
	}
}

/*
 * Print vdev initialization status for leaves
 */
static void
print_status_initialize(vdev_stat_t *vs, boolean_t verbose)
{
	if (verbose) {
		if ((vs->vs_initialize_state == VDEV_INITIALIZE_ACTIVE ||
		    vs->vs_initialize_state == VDEV_INITIALIZE_SUSPENDED ||
		    vs->vs_initialize_state == VDEV_INITIALIZE_COMPLETE) &&
		    !vs->vs_scan_removing) {
			char zbuf[1024];
			char tbuf[256];

			time_t t = vs->vs_initialize_action_time;
			int initialize_pct = 100;
			if (vs->vs_initialize_state !=
			    VDEV_INITIALIZE_COMPLETE) {
				initialize_pct = (vs->vs_initialize_bytes_done *
				    100 / (vs->vs_initialize_bytes_est + 1));
			}

			(void) ctime_r(&t, tbuf);
			tbuf[24] = 0;

			switch (vs->vs_initialize_state) {
			case VDEV_INITIALIZE_SUSPENDED:
				(void) snprintf(zbuf, sizeof (zbuf), ", %s %s",
				    gettext("suspended, started at"), tbuf);
				break;
			case VDEV_INITIALIZE_ACTIVE:
				(void) snprintf(zbuf, sizeof (zbuf), ", %s %s",
				    gettext("started at"), tbuf);
				break;
			case VDEV_INITIALIZE_COMPLETE:
				(void) snprintf(zbuf, sizeof (zbuf), ", %s %s",
				    gettext("completed at"), tbuf);
				break;
			}

			(void) printf(gettext("  (%d%% initialized%s)"),
			    initialize_pct, zbuf);
		} else {
			(void) printf(gettext("  (uninitialized)"));
		}
	} else if (vs->vs_initialize_state == VDEV_INITIALIZE_ACTIVE) {
		(void) printf(gettext("  (initializing)"));
	}
}

/*
 * Print vdev TRIM status for leaves
 */
static void
print_status_trim(vdev_stat_t *vs, boolean_t verbose)
{
	if (verbose) {
		if ((vs->vs_trim_state == VDEV_TRIM_ACTIVE ||
		    vs->vs_trim_state == VDEV_TRIM_SUSPENDED ||
		    vs->vs_trim_state == VDEV_TRIM_COMPLETE) &&
		    !vs->vs_scan_removing) {
			char zbuf[1024];
			char tbuf[256];

			time_t t = vs->vs_trim_action_time;
			int trim_pct = 100;
			if (vs->vs_trim_state != VDEV_TRIM_COMPLETE) {
				trim_pct = (vs->vs_trim_bytes_done *
				    100 / (vs->vs_trim_bytes_est + 1));
			}

			(void) ctime_r(&t, tbuf);
			tbuf[24] = 0;

			switch (vs->vs_trim_state) {
			case VDEV_TRIM_SUSPENDED:
				(void) snprintf(zbuf, sizeof (zbuf), ", %s %s",
				    gettext("suspended, started at"), tbuf);
				break;
			case VDEV_TRIM_ACTIVE:
				(void) snprintf(zbuf, sizeof (zbuf), ", %s %s",
				    gettext("started at"), tbuf);
				break;
			case VDEV_TRIM_COMPLETE:
				(void) snprintf(zbuf, sizeof (zbuf), ", %s %s",
				    gettext("completed at"), tbuf);
				break;
			}

			(void) printf(gettext("  (%d%% trimmed%s)"),
			    trim_pct, zbuf);
		} else if (vs->vs_trim_notsup) {
			(void) printf(gettext("  (trim unsupported)"));
		} else {
			(void) printf(gettext("  (untrimmed)"));
		}
	} else if (vs->vs_trim_state == VDEV_TRIM_ACTIVE) {
		(void) printf(gettext("  (trimming)"));
	}
}

/*
 * Return the color associated with a health string.  This includes returning
 * NULL for no color change.
 */
static const char *
health_str_to_color(const char *health)
{
	if (strcmp(health, gettext("FAULTED")) == 0 ||
	    strcmp(health, gettext("SUSPENDED")) == 0 ||
	    strcmp(health, gettext("UNAVAIL")) == 0) {
		return (ANSI_RED);
	}

	if (strcmp(health, gettext("OFFLINE")) == 0 ||
	    strcmp(health, gettext("DEGRADED")) == 0 ||
	    strcmp(health, gettext("REMOVED")) == 0) {
		return (ANSI_YELLOW);
	}

	return (NULL);
}

/*
 * Called for each leaf vdev.  Returns 0 if the vdev is healthy.
 * A vdev is unhealthy if any of the following are true:
 * 1) there are read, write, or checksum errors,
 * 2) its state is not ONLINE, or
 * 3) slow IO reporting was requested (-s) and there are slow IOs.
 */
static int
vdev_health_check_cb(void *hdl_data, nvlist_t *nv, void *data)
{
	status_cbdata_t *cb = data;
	vdev_stat_t *vs;
	uint_t vsc;
	(void) hdl_data;

	if (nvlist_lookup_uint64_array(nv, ZPOOL_CONFIG_VDEV_STATS,
	    (uint64_t **)&vs, &vsc) != 0)
		return (1);

	if (vs->vs_checksum_errors || vs->vs_read_errors ||
	    vs->vs_write_errors || vs->vs_state != VDEV_STATE_HEALTHY)
		return (1);

	if (cb->cb_print_slow_ios && vs->vs_slow_ios)
		return (1);

	return (0);
}

/*
 * Print out configuration state as requested by status_callback.
 */
static void
print_status_config(zpool_handle_t *zhp, status_cbdata_t *cb, const char *name,
    nvlist_t *nv, int depth, boolean_t isspare, vdev_rebuild_stat_t *vrs)
{
	nvlist_t **child, *root;
	uint_t c, i, vsc, children;
	pool_scan_stat_t *ps = NULL;
	vdev_stat_t *vs;
	char rbuf[6], wbuf[6], cbuf[6], dbuf[6];
	char *vname;
	uint64_t notpresent;
	spare_cbdata_t spare_cb;
	const char *state;
	const char *type;
	const char *path = NULL;
	const char *rcolor = NULL, *wcolor = NULL, *ccolor = NULL,
	    *scolor = NULL;

	if (nvlist_lookup_nvlist_array(nv, ZPOOL_CONFIG_CHILDREN,
	    &child, &children) != 0)
		children = 0;

	verify(nvlist_lookup_uint64_array(nv, ZPOOL_CONFIG_VDEV_STATS,
	    (uint64_t **)&vs, &vsc) == 0);

	verify(nvlist_lookup_string(nv, ZPOOL_CONFIG_TYPE, &type) == 0);

	if (strcmp(type, VDEV_TYPE_INDIRECT) == 0)
		return;

	state = zpool_state_to_name(vs->vs_state, vs->vs_aux);

	if (isspare) {
		/*
		 * For hot spares, we use the terms 'INUSE' and 'AVAILABLE' for
		 * online drives.
		 */
		if (vs->vs_aux == VDEV_AUX_SPARED)
			state = gettext("INUSE");
		else if (vs->vs_state == VDEV_STATE_HEALTHY)
			state = gettext("AVAIL");
	}

	/*
	 * If '-e' is specified then top-level vdevs and their children
	 * can be pruned if all of their leaves are healthy.
	 */
	if (cb->cb_print_unhealthy && depth > 0 &&
	    for_each_vdev_in_nvlist(nv, vdev_health_check_cb, cb) == 0) {
		return;
	}

	printf_color(health_str_to_color(state),
	    "\t%*s%-*s  %-8s", depth, "", cb->cb_namewidth - depth,
	    name, state);

	if (!isspare) {
		if (vs->vs_read_errors)
			rcolor = ANSI_RED;

		if (vs->vs_write_errors)
			wcolor = ANSI_RED;

		if (vs->vs_checksum_errors)
			ccolor = ANSI_RED;

		if (vs->vs_slow_ios)
			scolor = ANSI_BLUE;

		if (cb->cb_literal) {
			fputc(' ', stdout);
			printf_color(rcolor, "%5llu",
			    (u_longlong_t)vs->vs_read_errors);
			fputc(' ', stdout);
			printf_color(wcolor, "%5llu",
			    (u_longlong_t)vs->vs_write_errors);
			fputc(' ', stdout);
			printf_color(ccolor, "%5llu",
			    (u_longlong_t)vs->vs_checksum_errors);
		} else {
			zfs_nicenum(vs->vs_read_errors, rbuf, sizeof (rbuf));
			zfs_nicenum(vs->vs_write_errors, wbuf, sizeof (wbuf));
			zfs_nicenum(vs->vs_checksum_errors, cbuf,
			    sizeof (cbuf));
			fputc(' ', stdout);
			printf_color(rcolor, "%5s", rbuf);
			fputc(' ', stdout);
			printf_color(wcolor, "%5s", wbuf);
			fputc(' ', stdout);
			printf_color(ccolor, "%5s", cbuf);
		}
		if (cb->cb_print_slow_ios) {
			if (children == 0)  {
				/* Only leafs vdevs have slow IOs */
				zfs_nicenum(vs->vs_slow_ios, rbuf,
				    sizeof (rbuf));
			} else {
				snprintf(rbuf, sizeof (rbuf), "-");
			}

			if (cb->cb_literal)
				printf_color(scolor, " %5llu",
				    (u_longlong_t)vs->vs_slow_ios);
			else
				printf_color(scolor, " %5s", rbuf);
		}
		if (cb->cb_print_power) {
			if (children == 0)  {
				/* Only leaf vdevs have physical slots */
				switch (zpool_power_current_state(zhp, (char *)
				    fnvlist_lookup_string(nv,
				    ZPOOL_CONFIG_PATH))) {
				case 0:
					printf_color(ANSI_RED, " %5s",
					    gettext("off"));
					break;
				case 1:
					printf(" %5s", gettext("on"));
					break;
				default:
					printf(" %5s", "-");
				}
			} else {
				printf(" %5s", "-");
			}
		}
		if (VDEV_STAT_VALID(vs_dio_verify_errors, vsc) &&
		    cb->cb_print_dio_verify) {
			zfs_nicenum(vs->vs_dio_verify_errors, dbuf,
			    sizeof (dbuf));

			if (cb->cb_literal)
				printf(" %5llu",
				    (u_longlong_t)vs->vs_dio_verify_errors);
			else
				printf(" %5s", dbuf);
		}
	}

	if (nvlist_lookup_uint64(nv, ZPOOL_CONFIG_NOT_PRESENT,
	    &notpresent) == 0) {
		verify(nvlist_lookup_string(nv, ZPOOL_CONFIG_PATH, &path) == 0);
		(void) printf("  %s %s", gettext("was"), path);
	} else if (vs->vs_aux != 0) {
		(void) printf("  ");
		color_start(ANSI_RED);
		switch (vs->vs_aux) {
		case VDEV_AUX_OPEN_FAILED:
			(void) printf(gettext("cannot open"));
			break;

		case VDEV_AUX_BAD_GUID_SUM:
			(void) printf(gettext("missing device"));
			break;

		case VDEV_AUX_NO_REPLICAS:
			(void) printf(gettext("insufficient replicas"));
			break;

		case VDEV_AUX_VERSION_NEWER:
			(void) printf(gettext("newer version"));
			break;

		case VDEV_AUX_UNSUP_FEAT:
			(void) printf(gettext("unsupported feature(s)"));
			break;

		case VDEV_AUX_ASHIFT_TOO_BIG:
			(void) printf(gettext("unsupported minimum blocksize"));
			break;

		case VDEV_AUX_SPARED:
			verify(nvlist_lookup_uint64(nv, ZPOOL_CONFIG_GUID,
			    &spare_cb.cb_guid) == 0);
			if (zpool_iter(g_zfs, find_spare, &spare_cb) == 1) {
				if (strcmp(zpool_get_name(spare_cb.cb_zhp),
				    zpool_get_name(zhp)) == 0)
					(void) printf(gettext("currently in "
					    "use"));
				else
					(void) printf(gettext("in use by "
					    "pool '%s'"),
					    zpool_get_name(spare_cb.cb_zhp));
				zpool_close(spare_cb.cb_zhp);
			} else {
				(void) printf(gettext("currently in use"));
			}
			break;

		case VDEV_AUX_ERR_EXCEEDED:
			if (vs->vs_read_errors + vs->vs_write_errors +
			    vs->vs_checksum_errors == 0 && children == 0 &&
			    vs->vs_slow_ios > 0) {
				(void) printf(gettext("too many slow I/Os"));
			} else {
				(void) printf(gettext("too many errors"));
			}
			break;

		case VDEV_AUX_IO_FAILURE:
			(void) printf(gettext("experienced I/O failures"));
			break;

		case VDEV_AUX_BAD_LOG:
			(void) printf(gettext("bad intent log"));
			break;

		case VDEV_AUX_EXTERNAL:
			(void) printf(gettext("external device fault"));
			break;

		case VDEV_AUX_SPLIT_POOL:
			(void) printf(gettext("split into new pool"));
			break;

		case VDEV_AUX_ACTIVE:
			(void) printf(gettext("currently in use"));
			break;

		case VDEV_AUX_CHILDREN_OFFLINE:
			(void) printf(gettext("all children offline"));
			break;

		case VDEV_AUX_BAD_LABEL:
			(void) printf(gettext("invalid label"));
			break;

		default:
			(void) printf(gettext("corrupted data"));
			break;
		}
		color_end();
	} else if (children == 0 && !isspare &&
	    getenv("ZPOOL_STATUS_NON_NATIVE_ASHIFT_IGNORE") == NULL &&
	    VDEV_STAT_VALID(vs_physical_ashift, vsc) &&
	    vs->vs_configured_ashift < vs->vs_physical_ashift) {
		(void) printf(
		    gettext("  block size: %dB configured, %dB native"),
		    1 << vs->vs_configured_ashift, 1 << vs->vs_physical_ashift);
	}

	if (vs->vs_scan_removing != 0) {
		(void) printf(gettext("  (removing)"));
	} else if (VDEV_STAT_VALID(vs_noalloc, vsc) && vs->vs_noalloc != 0) {
		(void) printf(gettext("  (non-allocating)"));
	}

	/* The root vdev has the scrub/resilver stats */
	root = fnvlist_lookup_nvlist(zpool_get_config(zhp, NULL),
	    ZPOOL_CONFIG_VDEV_TREE);
	(void) nvlist_lookup_uint64_array(root, ZPOOL_CONFIG_SCAN_STATS,
	    (uint64_t **)&ps, &c);

	/*
	 * If you force fault a drive that's resilvering, its scan stats can
	 * get frozen in time, giving the false impression that it's
	 * being resilvered.  That's why we check the state to see if the vdev
	 * is healthy before reporting "resilvering" or "repairing".
	 */
	if (ps != NULL && ps->pss_state == DSS_SCANNING && children == 0 &&
	    vs->vs_state == VDEV_STATE_HEALTHY) {
		if (vs->vs_scan_processed != 0) {
			(void) printf(gettext("  (%s)"),
			    (ps->pss_func == POOL_SCAN_RESILVER) ?
			    "resilvering" : "repairing");
		} else if (vs->vs_resilver_deferred) {
			(void) printf(gettext("  (awaiting resilver)"));
		}
	}

	/* The top-level vdevs have the rebuild stats */
	if (vrs != NULL && vrs->vrs_state == VDEV_REBUILD_ACTIVE &&
	    children == 0 && vs->vs_state == VDEV_STATE_HEALTHY) {
		if (vs->vs_rebuild_processed != 0) {
			(void) printf(gettext("  (resilvering)"));
		}
	}

	if (cb->vcdl != NULL) {
		if (nvlist_lookup_string(nv, ZPOOL_CONFIG_PATH, &path) == 0) {
			printf("  ");
			zpool_print_cmd(cb->vcdl, zpool_get_name(zhp), path);
		}
	}

	/* Display vdev initialization and trim status for leaves. */
	if (children == 0) {
		print_status_initialize(vs, cb->cb_print_vdev_init);
		print_status_trim(vs, cb->cb_print_vdev_trim);
	}

	(void) printf("\n");

	for (c = 0; c < children; c++) {
		uint64_t islog = B_FALSE, ishole = B_FALSE;

		/* Don't print logs or holes here */
		(void) nvlist_lookup_uint64(child[c], ZPOOL_CONFIG_IS_LOG,
		    &islog);
		(void) nvlist_lookup_uint64(child[c], ZPOOL_CONFIG_IS_HOLE,
		    &ishole);
		if (islog || ishole)
			continue;
		/* Only print normal classes here */
		if (nvlist_exists(child[c], ZPOOL_CONFIG_ALLOCATION_BIAS))
			continue;

		/* Provide vdev_rebuild_stats to children if available */
		if (vrs == NULL) {
			(void) nvlist_lookup_uint64_array(nv,
			    ZPOOL_CONFIG_REBUILD_STATS,
			    (uint64_t **)&vrs, &i);
		}

		vname = zpool_vdev_name(g_zfs, zhp, child[c],
		    cb->cb_name_flags | VDEV_NAME_TYPE_ID);
		print_status_config(zhp, cb, vname, child[c], depth + 2,
		    isspare, vrs);
		free(vname);
	}
}

/*
 * Print the configuration of an exported pool.  Iterate over all vdevs in the
 * pool, printing out the name and status for each one.
 */
static void
print_import_config(status_cbdata_t *cb, const char *name, nvlist_t *nv,
    int depth)
{
	nvlist_t **child;
	uint_t c, children;
	vdev_stat_t *vs;
	const char *type;
	char *vname;

	verify(nvlist_lookup_string(nv, ZPOOL_CONFIG_TYPE, &type) == 0);
	if (strcmp(type, VDEV_TYPE_MISSING) == 0 ||
	    strcmp(type, VDEV_TYPE_HOLE) == 0)
		return;

	verify(nvlist_lookup_uint64_array(nv, ZPOOL_CONFIG_VDEV_STATS,
	    (uint64_t **)&vs, &c) == 0);

	(void) printf("\t%*s%-*s", depth, "", cb->cb_namewidth - depth, name);
	(void) printf("  %s", zpool_state_to_name(vs->vs_state, vs->vs_aux));

	if (vs->vs_aux != 0) {
		(void) printf("  ");

		switch (vs->vs_aux) {
		case VDEV_AUX_OPEN_FAILED:
			(void) printf(gettext("cannot open"));
			break;

		case VDEV_AUX_BAD_GUID_SUM:
			(void) printf(gettext("missing device"));
			break;

		case VDEV_AUX_NO_REPLICAS:
			(void) printf(gettext("insufficient replicas"));
			break;

		case VDEV_AUX_VERSION_NEWER:
			(void) printf(gettext("newer version"));
			break;

		case VDEV_AUX_UNSUP_FEAT:
			(void) printf(gettext("unsupported feature(s)"));
			break;

		case VDEV_AUX_ERR_EXCEEDED:
			(void) printf(gettext("too many errors"));
			break;

		case VDEV_AUX_ACTIVE:
			(void) printf(gettext("currently in use"));
			break;

		case VDEV_AUX_CHILDREN_OFFLINE:
			(void) printf(gettext("all children offline"));
			break;

		case VDEV_AUX_BAD_LABEL:
			(void) printf(gettext("invalid label"));
			break;

		default:
			(void) printf(gettext("corrupted data"));
			break;
		}
	}
	(void) printf("\n");

	if (nvlist_lookup_nvlist_array(nv, ZPOOL_CONFIG_CHILDREN,
	    &child, &children) != 0)
		return;

	for (c = 0; c < children; c++) {
		uint64_t is_log = B_FALSE;

		(void) nvlist_lookup_uint64(child[c], ZPOOL_CONFIG_IS_LOG,
		    &is_log);
		if (is_log)
			continue;
		if (nvlist_exists(child[c], ZPOOL_CONFIG_ALLOCATION_BIAS))
			continue;

		vname = zpool_vdev_name(g_zfs, NULL, child[c],
		    cb->cb_name_flags | VDEV_NAME_TYPE_ID);
		print_import_config(cb, vname, child[c], depth + 2);
		free(vname);
	}

	if (nvlist_lookup_nvlist_array(nv, ZPOOL_CONFIG_L2CACHE,
	    &child, &children) == 0) {
		(void) printf(gettext("\tcache\n"));
		for (c = 0; c < children; c++) {
			vname = zpool_vdev_name(g_zfs, NULL, child[c],
			    cb->cb_name_flags);
			(void) printf("\t  %s\n", vname);
			free(vname);
		}
	}

	if (nvlist_lookup_nvlist_array(nv, ZPOOL_CONFIG_SPARES,
	    &child, &children) == 0) {
		(void) printf(gettext("\tspares\n"));
		for (c = 0; c < children; c++) {
			vname = zpool_vdev_name(g_zfs, NULL, child[c],
			    cb->cb_name_flags);
			(void) printf("\t  %s\n", vname);
			free(vname);
		}
	}
}

/*
 * Print specialized class vdevs.
 *
 * These are recorded as top level vdevs in the main pool child array
 * but with "is_log" set to 1 or an "alloc_bias" string. We use either
 * print_status_config() or print_import_config() to print the top level
 * class vdevs then any of their children (eg mirrored slogs) are printed
 * recursively - which works because only the top level vdev is marked.
 */
static void
print_class_vdevs(zpool_handle_t *zhp, status_cbdata_t *cb, nvlist_t *nv,
    const char *class)
{
	uint_t c, children;
	nvlist_t **child;
	boolean_t printed = B_FALSE;

	assert(zhp != NULL || !cb->cb_verbose);

	if (nvlist_lookup_nvlist_array(nv, ZPOOL_CONFIG_CHILDREN, &child,
	    &children) != 0)
		return;

	for (c = 0; c < children; c++) {
		uint64_t is_log = B_FALSE;
		const char *bias = NULL;
		const char *type = NULL;

		(void) nvlist_lookup_uint64(child[c], ZPOOL_CONFIG_IS_LOG,
		    &is_log);

		if (is_log) {
			bias = (char *)VDEV_ALLOC_CLASS_LOGS;
		} else {
			(void) nvlist_lookup_string(child[c],
			    ZPOOL_CONFIG_ALLOCATION_BIAS, &bias);
			(void) nvlist_lookup_string(child[c],
			    ZPOOL_CONFIG_TYPE, &type);
		}

		if (bias == NULL || strcmp(bias, class) != 0)
			continue;
		if (!is_log && strcmp(type, VDEV_TYPE_INDIRECT) == 0)
			continue;

		if (!printed) {
			(void) printf("\t%s\t\n", gettext(class));
			printed = B_TRUE;
		}

		char *name = zpool_vdev_name(g_zfs, zhp, child[c],
		    cb->cb_name_flags | VDEV_NAME_TYPE_ID);
		if (cb->cb_print_status)
			print_status_config(zhp, cb, name, child[c], 2,
			    B_FALSE, NULL);
		else
			print_import_config(cb, name, child[c], 2);
		free(name);
	}
}

/*
 * Display the status for the given pool.
 */
static int
show_import(nvlist_t *config, boolean_t report_error)
{
	uint64_t pool_state;
	vdev_stat_t *vs;
	const char *name;
	uint64_t guid;
	uint64_t hostid = 0;
	const char *msgid;
	const char *hostname = "unknown";
	nvlist_t *nvroot, *nvinfo;
	zpool_status_t reason;
	zpool_errata_t errata;
	const char *health;
	uint_t vsc;
	const char *comment;
	const char *indent;
	char buf[2048];
	status_cbdata_t cb = { 0 };

	verify(nvlist_lookup_string(config, ZPOOL_CONFIG_POOL_NAME,
	    &name) == 0);
	verify(nvlist_lookup_uint64(config, ZPOOL_CONFIG_POOL_GUID,
	    &guid) == 0);
	verify(nvlist_lookup_uint64(config, ZPOOL_CONFIG_POOL_STATE,
	    &pool_state) == 0);
	verify(nvlist_lookup_nvlist(config, ZPOOL_CONFIG_VDEV_TREE,
	    &nvroot) == 0);

	verify(nvlist_lookup_uint64_array(nvroot, ZPOOL_CONFIG_VDEV_STATS,
	    (uint64_t **)&vs, &vsc) == 0);
	health = zpool_state_to_name(vs->vs_state, vs->vs_aux);

	reason = zpool_import_status(config, &msgid, &errata);

	/*
	 * If we're importing using a cachefile, then we won't report any
	 * errors unless we are in the scan phase of the import.
	 */
	if (reason != ZPOOL_STATUS_OK && !report_error)
		return (reason);

	if (nvlist_lookup_string(config, ZPOOL_CONFIG_COMMENT, &comment) == 0) {
		indent = " ";
	} else {
		comment = NULL;
		indent = "";
	}

	(void) printf(gettext("%s  pool: %s\n"), indent, name);
	(void) printf(gettext("%s    id: %llu\n"), indent, (u_longlong_t)guid);
	(void) printf(gettext("%s state: %s"), indent, health);
	if (pool_state == POOL_STATE_DESTROYED)
		(void) printf(gettext(" (DESTROYED)"));
	(void) printf("\n");

	if (reason != ZPOOL_STATUS_OK) {
		(void) printf("%s", indent);
		printf_color(ANSI_BOLD, gettext("status: "));
	}
	switch (reason) {
	case ZPOOL_STATUS_MISSING_DEV_R:
	case ZPOOL_STATUS_MISSING_DEV_NR:
	case ZPOOL_STATUS_BAD_GUID_SUM:
		printf_color(ANSI_YELLOW, gettext("One or more devices are "
		    "missing from the system.\n"));
		break;

	case ZPOOL_STATUS_CORRUPT_LABEL_R:
	case ZPOOL_STATUS_CORRUPT_LABEL_NR:
		printf_color(ANSI_YELLOW, gettext("One or more devices "
		    "contains corrupted data.\n"));
		break;

	case ZPOOL_STATUS_CORRUPT_DATA:
		printf_color(ANSI_YELLOW, gettext("The pool data is "
		    "corrupted.\n"));
		break;

	case ZPOOL_STATUS_OFFLINE_DEV:
		printf_color(ANSI_YELLOW, gettext("One or more devices "
		    "are offlined.\n"));
		break;

	case ZPOOL_STATUS_CORRUPT_POOL:
		printf_color(ANSI_YELLOW, gettext("The pool metadata is "
		    "corrupted.\n"));
		break;

	case ZPOOL_STATUS_VERSION_OLDER:
		printf_color(ANSI_YELLOW, gettext("The pool is formatted using "
		    "a legacy on-disk version.\n"));
		break;

	case ZPOOL_STATUS_VERSION_NEWER:
		printf_color(ANSI_YELLOW, gettext("The pool is formatted using "
		    "an incompatible version.\n"));
		break;

	case ZPOOL_STATUS_FEAT_DISABLED:
		printf_color(ANSI_YELLOW, gettext("Some supported "
		    "features are not enabled on the pool.\n"
		    "\t%s(Note that they may be intentionally disabled if the\n"
		    "\t%s'compatibility' property is set.)\n"), indent, indent);
		break;

	case ZPOOL_STATUS_COMPATIBILITY_ERR:
		printf_color(ANSI_YELLOW, gettext("Error reading or parsing "
		    "the file(s) indicated by the 'compatibility'\n"
		    "\t%sproperty.\n"), indent);
		break;

	case ZPOOL_STATUS_INCOMPATIBLE_FEAT:
		printf_color(ANSI_YELLOW, gettext("One or more features "
		    "are enabled on the pool despite not being\n"
		    "\t%srequested by the 'compatibility' property.\n"),
		    indent);
		break;

	case ZPOOL_STATUS_UNSUP_FEAT_READ:
		printf_color(ANSI_YELLOW, gettext("The pool uses the following "
		    "feature(s) not supported on this system:\n"));
		color_start(ANSI_YELLOW);
		zpool_collect_unsup_feat(config, buf, 2048);
		(void) printf("%s", buf);
		color_end();
		break;

	case ZPOOL_STATUS_UNSUP_FEAT_WRITE:
		printf_color(ANSI_YELLOW, gettext("The pool can only be "
		    "accessed in read-only mode on this system. It\n"
		    "\t%scannot be accessed in read-write mode because it uses "
		    "the following\n"
		    "\t%sfeature(s) not supported on this system:\n"),
		    indent, indent);
		color_start(ANSI_YELLOW);
		zpool_collect_unsup_feat(config, buf, 2048);
		(void) printf("%s", buf);
		color_end();
		break;

	case ZPOOL_STATUS_HOSTID_ACTIVE:
		printf_color(ANSI_YELLOW, gettext("The pool is currently "
		    "imported by another system.\n"));
		break;

	case ZPOOL_STATUS_HOSTID_REQUIRED:
		printf_color(ANSI_YELLOW, gettext("The pool has the "
		    "multihost property on.  It cannot\n"
		    "\t%sbe safely imported when the system hostid is not "
		    "set.\n"), indent);
		break;

	case ZPOOL_STATUS_HOSTID_MISMATCH:
		printf_color(ANSI_YELLOW, gettext("The pool was last accessed "
		    "by another system.\n"));
		break;

	case ZPOOL_STATUS_FAULTED_DEV_R:
	case ZPOOL_STATUS_FAULTED_DEV_NR:
		printf_color(ANSI_YELLOW, gettext("One or more devices are "
		    "faulted.\n"));
		break;

	case ZPOOL_STATUS_BAD_LOG:
		printf_color(ANSI_YELLOW, gettext("An intent log record cannot "
		    "be read.\n"));
		break;

	case ZPOOL_STATUS_RESILVERING:
	case ZPOOL_STATUS_REBUILDING:
		printf_color(ANSI_YELLOW, gettext("One or more devices were "
		    "being resilvered.\n"));
		break;

	case ZPOOL_STATUS_ERRATA:
		printf_color(ANSI_YELLOW, gettext("Errata #%d detected.\n"),
		    errata);
		break;

	case ZPOOL_STATUS_NON_NATIVE_ASHIFT:
		printf_color(ANSI_YELLOW, gettext("One or more devices are "
		    "configured to use a non-native block size.\n"
		    "\t%sExpect reduced performance.\n"), indent);
		break;

	default:
		/*
		 * No other status can be seen when importing pools.
		 */
		assert(reason == ZPOOL_STATUS_OK);
	}

	/*
	 * Print out an action according to the overall state of the pool.
	 */
	if (vs->vs_state != VDEV_STATE_HEALTHY ||
	    reason != ZPOOL_STATUS_ERRATA || errata != ZPOOL_ERRATA_NONE) {
		(void) printf("%s", indent);
		(void) printf(gettext("action: "));
	}
	if (vs->vs_state == VDEV_STATE_HEALTHY) {
		if (reason == ZPOOL_STATUS_VERSION_OLDER ||
		    reason == ZPOOL_STATUS_FEAT_DISABLED) {
			(void) printf(gettext("The pool can be imported using "
			    "its name or numeric identifier, though\n"
			    "\t%ssome features will not be available without "
			    "an explicit 'zpool upgrade'.\n"), indent);
		} else if (reason == ZPOOL_STATUS_COMPATIBILITY_ERR) {
			(void) printf(gettext("The pool can be imported using "
			    "its name or numeric\n"
			    "\t%sidentifier, though the file(s) indicated by "
			    "its 'compatibility'\n"
			    "\t%sproperty cannot be parsed at this time.\n"),
			    indent, indent);
		} else if (reason == ZPOOL_STATUS_HOSTID_MISMATCH) {
			(void) printf(gettext("The pool can be imported using "
			    "its name or numeric identifier and\n"
			    "\t%sthe '-f' flag.\n"), indent);
		} else if (reason == ZPOOL_STATUS_ERRATA) {
			switch (errata) {
			case ZPOOL_ERRATA_ZOL_2094_SCRUB:
				(void) printf(gettext("The pool can be "
				    "imported using its name or numeric "
				    "identifier,\n"
				    "\t%showever there is a compatibility "
				    "issue which should be corrected\n"
				    "\t%sby running 'zpool scrub'\n"),
				    indent, indent);
				break;

			case ZPOOL_ERRATA_ZOL_2094_ASYNC_DESTROY:
				(void) printf(gettext("The pool cannot be "
				    "imported with this version of ZFS due to\n"
				    "\t%san active asynchronous destroy. "
				    "Revert to an earlier version\n"
				    "\t%sand allow the destroy to complete "
				    "before updating.\n"), indent, indent);
				break;

			case ZPOOL_ERRATA_ZOL_6845_ENCRYPTION:
				(void) printf(gettext("Existing encrypted "
				    "datasets contain an on-disk "
				    "incompatibility, which\n"
				    "\t%sneeds to be corrected. Backup these "
				    "datasets to new encrypted datasets\n"
				    "\t%sand destroy the old ones.\n"),
				    indent, indent);
				break;

			case ZPOOL_ERRATA_ZOL_8308_ENCRYPTION:
				(void) printf(gettext("Existing encrypted "
				    "snapshots and bookmarks contain an "
				    "on-disk\n"
				    "\t%sincompatibility. This may cause "
				    "on-disk corruption if they are used\n"
				    "\t%swith 'zfs recv'. To correct the "
				    "issue, enable the bookmark_v2 feature.\n"
				    "\t%sNo additional action is needed if "
				    "there are no encrypted snapshots or\n"
				    "\t%sbookmarks. If preserving the "
				    "encrypted snapshots and bookmarks is\n"
				    "\t%srequired, use a non-raw send to "
				    "backup and restore them. Alternately,\n"
				    "\t%sthey may be removed to resolve the "
				    "incompatibility.\n"), indent, indent,
				    indent, indent, indent, indent);
				break;
			default:
				/*
				 * All errata must contain an action message.
				 */
				assert(errata == ZPOOL_ERRATA_NONE);
			}
		} else {
			(void) printf(gettext("The pool can be imported using "
			    "its name or numeric identifier.\n"));
		}
	} else if (vs->vs_state == VDEV_STATE_DEGRADED) {
		(void) printf(gettext("The pool can be imported despite "
		    "missing or damaged devices.  The\n"
		    "\t%sfault tolerance of the pool may be compromised if "
		    "imported.\n"), indent);
	} else {
		switch (reason) {
		case ZPOOL_STATUS_VERSION_NEWER:
			(void) printf(gettext("The pool cannot be imported.  "
			    "Access the pool on a system running newer\n"
			    "\t%ssoftware, or recreate the pool from "
			    "backup.\n"), indent);
			break;
		case ZPOOL_STATUS_UNSUP_FEAT_READ:
			(void) printf(gettext("The pool cannot be imported. "
			    "Access the pool on a system that supports\n"
			    "\t%sthe required feature(s), or recreate the pool "
			    "from backup.\n"), indent);
			break;
		case ZPOOL_STATUS_UNSUP_FEAT_WRITE:
			(void) printf(gettext("The pool cannot be imported in "
			    "read-write mode. Import the pool with\n"
			    "\t%s'-o readonly=on', access the pool on a system "
			    "that supports the\n"
			    "\t%srequired feature(s), or recreate the pool "
			    "from backup.\n"), indent, indent);
			break;
		case ZPOOL_STATUS_MISSING_DEV_R:
		case ZPOOL_STATUS_MISSING_DEV_NR:
		case ZPOOL_STATUS_BAD_GUID_SUM:
			(void) printf(gettext("The pool cannot be imported. "
			    "Attach the missing\n"
			    "\t%sdevices and try again.\n"), indent);
			break;
		case ZPOOL_STATUS_HOSTID_ACTIVE:
			VERIFY0(nvlist_lookup_nvlist(config,
			    ZPOOL_CONFIG_LOAD_INFO, &nvinfo));

			if (nvlist_exists(nvinfo, ZPOOL_CONFIG_MMP_HOSTNAME))
				hostname = fnvlist_lookup_string(nvinfo,
				    ZPOOL_CONFIG_MMP_HOSTNAME);

			if (nvlist_exists(nvinfo, ZPOOL_CONFIG_MMP_HOSTID))
				hostid = fnvlist_lookup_uint64(nvinfo,
				    ZPOOL_CONFIG_MMP_HOSTID);

			(void) printf(gettext("The pool must be exported from "
			    "%s (hostid=%"PRIx64")\n"
			    "\t%sbefore it can be safely imported.\n"),
			    hostname, hostid, indent);
			break;
		case ZPOOL_STATUS_HOSTID_REQUIRED:
			(void) printf(gettext("Set a unique system hostid with "
			    "the zgenhostid(8) command.\n"));
			break;
		default:
			(void) printf(gettext("The pool cannot be imported due "
			    "to damaged devices or data.\n"));
		}
	}

	/* Print the comment attached to the pool. */
	if (comment != NULL)
		(void) printf(gettext("comment: %s\n"), comment);

	/*
	 * If the state is "closed" or "can't open", and the aux state
	 * is "corrupt data":
	 */
	if ((vs->vs_state == VDEV_STATE_CLOSED ||
	    vs->vs_state == VDEV_STATE_CANT_OPEN) &&
	    vs->vs_aux == VDEV_AUX_CORRUPT_DATA) {
		if (pool_state == POOL_STATE_DESTROYED)
			(void) printf(gettext("\t%sThe pool was destroyed, "
			    "but can be imported using the '-Df' flags.\n"),
			    indent);
		else if (pool_state != POOL_STATE_EXPORTED)
			(void) printf(gettext("\t%sThe pool may be active on "
			    "another system, but can be imported using\n"
			    "\t%sthe '-f' flag.\n"), indent, indent);
	}

	if (msgid != NULL) {
		(void) printf(gettext("%s   see: "
		    "https://openzfs.github.io/openzfs-docs/msg/%s\n"),
		    indent, msgid);
	}

	(void) printf(gettext("%sconfig:\n\n"), indent);

	cb.cb_namewidth = max_width(NULL, nvroot, 0, strlen(name),
	    VDEV_NAME_TYPE_ID);
	if (cb.cb_namewidth < 10)
		cb.cb_namewidth = 10;

	print_import_config(&cb, name, nvroot, 0);

	print_class_vdevs(NULL, &cb, nvroot, VDEV_ALLOC_BIAS_DEDUP);
	print_class_vdevs(NULL, &cb, nvroot, VDEV_ALLOC_BIAS_SPECIAL);
	print_class_vdevs(NULL, &cb, nvroot, VDEV_ALLOC_CLASS_LOGS);

	if (reason == ZPOOL_STATUS_BAD_GUID_SUM) {
		(void) printf(gettext("\n\t%sAdditional devices are known to "
		    "be part of this pool, though their\n"
		    "\t%sexact configuration cannot be determined.\n"),
		    indent, indent);
	}
	return (0);
}

static boolean_t
zfs_force_import_required(nvlist_t *config)
{
	uint64_t state;
	uint64_t hostid = 0;
	nvlist_t *nvinfo;

	state = fnvlist_lookup_uint64(config, ZPOOL_CONFIG_POOL_STATE);
	nvinfo = fnvlist_lookup_nvlist(config, ZPOOL_CONFIG_LOAD_INFO);

	/*
	 * The hostid on LOAD_INFO comes from the MOS label via
	 * spa_tryimport(). If its not there then we're likely talking to an
	 * older kernel, so use the top one, which will be from the label
	 * discovered in zpool_find_import(), or if a cachefile is in use, the
	 * local hostid.
	 */
	if (nvlist_lookup_uint64(nvinfo, ZPOOL_CONFIG_HOSTID, &hostid) != 0)
		(void) nvlist_lookup_uint64(config, ZPOOL_CONFIG_HOSTID,
		    &hostid);

	if (state != POOL_STATE_EXPORTED && hostid != get_system_hostid())
		return (B_TRUE);

	if (nvlist_exists(nvinfo, ZPOOL_CONFIG_MMP_STATE)) {
		mmp_state_t mmp_state = fnvlist_lookup_uint64(nvinfo,
		    ZPOOL_CONFIG_MMP_STATE);

		if (mmp_state != MMP_STATE_INACTIVE)
			return (B_TRUE);
	}

	return (B_FALSE);
}

/*
 * Perform the import for the given configuration.  This passes the heavy
 * lifting off to zpool_import_props(), and then mounts the datasets contained
 * within the pool.
 */
static int
do_import(nvlist_t *config, const char *newname, const char *mntopts,
    nvlist_t *props, int flags, uint_t mntthreads)
{
	int ret = 0;
	int ms_status = 0;
	zpool_handle_t *zhp;
	const char *name;
	uint64_t version;

	name = fnvlist_lookup_string(config, ZPOOL_CONFIG_POOL_NAME);
	version = fnvlist_lookup_uint64(config, ZPOOL_CONFIG_VERSION);

	if (!SPA_VERSION_IS_SUPPORTED(version)) {
		(void) fprintf(stderr, gettext("cannot import '%s': pool "
		    "is formatted using an unsupported ZFS version\n"), name);
		return (1);
	} else if (zfs_force_import_required(config) &&
	    !(flags & ZFS_IMPORT_ANY_HOST)) {
		mmp_state_t mmp_state = MMP_STATE_INACTIVE;
		nvlist_t *nvinfo;

		nvinfo = fnvlist_lookup_nvlist(config, ZPOOL_CONFIG_LOAD_INFO);
		if (nvlist_exists(nvinfo, ZPOOL_CONFIG_MMP_STATE))
			mmp_state = fnvlist_lookup_uint64(nvinfo,
			    ZPOOL_CONFIG_MMP_STATE);

		if (mmp_state == MMP_STATE_ACTIVE) {
			const char *hostname = "<unknown>";
			uint64_t hostid = 0;

			if (nvlist_exists(nvinfo, ZPOOL_CONFIG_MMP_HOSTNAME))
				hostname = fnvlist_lookup_string(nvinfo,
				    ZPOOL_CONFIG_MMP_HOSTNAME);

			if (nvlist_exists(nvinfo, ZPOOL_CONFIG_MMP_HOSTID))
				hostid = fnvlist_lookup_uint64(nvinfo,
				    ZPOOL_CONFIG_MMP_HOSTID);

			(void) fprintf(stderr, gettext("cannot import '%s': "
			    "pool is imported on %s (hostid: "
			    "0x%"PRIx64")\nExport the pool on the other "
			    "system, then run 'zpool import'.\n"),
			    name, hostname, hostid);
		} else if (mmp_state == MMP_STATE_NO_HOSTID) {
			(void) fprintf(stderr, gettext("Cannot import '%s': "
			    "pool has the multihost property on and the\n"
			    "system's hostid is not set. Set a unique hostid "
			    "with the zgenhostid(8) command.\n"), name);
		} else {
			const char *hostname = "<unknown>";
			time_t timestamp = 0;
			uint64_t hostid = 0;

			if (nvlist_exists(nvinfo, ZPOOL_CONFIG_HOSTNAME))
				hostname = fnvlist_lookup_string(nvinfo,
				    ZPOOL_CONFIG_HOSTNAME);
			else if (nvlist_exists(config, ZPOOL_CONFIG_HOSTNAME))
				hostname = fnvlist_lookup_string(config,
				    ZPOOL_CONFIG_HOSTNAME);

			if (nvlist_exists(config, ZPOOL_CONFIG_TIMESTAMP))
				timestamp = fnvlist_lookup_uint64(config,
				    ZPOOL_CONFIG_TIMESTAMP);

			if (nvlist_exists(nvinfo, ZPOOL_CONFIG_HOSTID))
				hostid = fnvlist_lookup_uint64(nvinfo,
				    ZPOOL_CONFIG_HOSTID);
			else if (nvlist_exists(config, ZPOOL_CONFIG_HOSTID))
				hostid = fnvlist_lookup_uint64(config,
				    ZPOOL_CONFIG_HOSTID);

			(void) fprintf(stderr, gettext("cannot import '%s': "
			    "pool was previously in use from another system.\n"
			    "Last accessed by %s (hostid=%"PRIx64") at %s"
			    "The pool can be imported, use 'zpool import -f' "
			    "to import the pool.\n"), name, hostname,
			    hostid, ctime(&timestamp));
		}

		return (1);
	}

	if (zpool_import_props(g_zfs, config, newname, props, flags) != 0)
		return (1);

	if (newname != NULL)
		name = newname;

	if ((zhp = zpool_open_canfail(g_zfs, name)) == NULL)
		return (1);

	/*
	 * Loading keys is best effort. We don't want to return immediately
	 * if it fails but we do want to give the error to the caller.
	 */
	if (flags & ZFS_IMPORT_LOAD_KEYS &&
	    zfs_crypto_attempt_load_keys(g_zfs, name) != 0)
			ret = 1;

	if (zpool_get_state(zhp) != POOL_STATE_UNAVAIL &&
	    !(flags & ZFS_IMPORT_ONLY)) {
		ms_status = zpool_enable_datasets(zhp, mntopts, 0, mntthreads);
		if (ms_status == EZFS_SHAREFAILED) {
			(void) fprintf(stderr, gettext("Import was "
			    "successful, but unable to share some datasets\n"));
		} else if (ms_status == EZFS_MOUNTFAILED) {
			(void) fprintf(stderr, gettext("Import was "
			    "successful, but unable to mount some datasets\n"));
		}
	}

	zpool_close(zhp);
	return (ret);
}

typedef struct import_parameters {
	nvlist_t *ip_config;
	const char *ip_mntopts;
	nvlist_t *ip_props;
	int ip_flags;
	uint_t ip_mntthreads;
	int *ip_err;
} import_parameters_t;

static void
do_import_task(void *arg)
{
	import_parameters_t *ip = arg;
	*ip->ip_err |= do_import(ip->ip_config, NULL, ip->ip_mntopts,
	    ip->ip_props, ip->ip_flags, ip->ip_mntthreads);
	free(ip);
}


static int
import_pools(nvlist_t *pools, nvlist_t *props, char *mntopts, int flags,
    char *orig_name, char *new_name, importargs_t *import)
{
	nvlist_t *config = NULL;
	nvlist_t *found_config = NULL;
	uint64_t pool_state;
	boolean_t pool_specified = (import->poolname != NULL ||
	    import->guid != 0);
	uint_t npools = 0;


	tpool_t *tp = NULL;
	if (import->do_all) {
		tp = tpool_create(1, 5 * sysconf(_SC_NPROCESSORS_ONLN),
		    0, NULL);
	}

	/*
	 * At this point we have a list of import candidate configs. Even if
	 * we were searching by pool name or guid, we still need to
	 * post-process the list to deal with pool state and possible
	 * duplicate names.
	 */
	int err = 0;
	nvpair_t *elem = NULL;
	boolean_t first = B_TRUE;
	if (!pool_specified && import->do_all) {
		while ((elem = nvlist_next_nvpair(pools, elem)) != NULL)
			npools++;
	}
	while ((elem = nvlist_next_nvpair(pools, elem)) != NULL) {

		verify(nvpair_value_nvlist(elem, &config) == 0);

		verify(nvlist_lookup_uint64(config, ZPOOL_CONFIG_POOL_STATE,
		    &pool_state) == 0);
		if (!import->do_destroyed &&
		    pool_state == POOL_STATE_DESTROYED)
			continue;
		if (import->do_destroyed &&
		    pool_state != POOL_STATE_DESTROYED)
			continue;

		verify(nvlist_add_nvlist(config, ZPOOL_LOAD_POLICY,
		    import->policy) == 0);

		if (!pool_specified) {
			if (first)
				first = B_FALSE;
			else if (!import->do_all)
				(void) fputc('\n', stdout);

			if (import->do_all) {
				import_parameters_t *ip = safe_malloc(
				    sizeof (import_parameters_t));

				ip->ip_config = config;
				ip->ip_mntopts = mntopts;
				ip->ip_props = props;
				ip->ip_flags = flags;
				ip->ip_mntthreads = mount_tp_nthr / npools;
				ip->ip_err = &err;

				(void) tpool_dispatch(tp, do_import_task,
				    (void *)ip);
			} else {
				/*
				 * If we're importing from cachefile, then
				 * we don't want to report errors until we
				 * are in the scan phase of the import. If
				 * we get an error, then we return that error
				 * to invoke the scan phase.
				 */
				if (import->cachefile && !import->scan)
					err = show_import(config, B_FALSE);
				else
					(void) show_import(config, B_TRUE);
			}
		} else if (import->poolname != NULL) {
			const char *name;

			/*
			 * We are searching for a pool based on name.
			 */
			verify(nvlist_lookup_string(config,
			    ZPOOL_CONFIG_POOL_NAME, &name) == 0);

			if (strcmp(name, import->poolname) == 0) {
				if (found_config != NULL) {
					(void) fprintf(stderr, gettext(
					    "cannot import '%s': more than "
					    "one matching pool\n"),
					    import->poolname);
					(void) fprintf(stderr, gettext(
					    "import by numeric ID instead\n"));
					err = B_TRUE;
				}
				found_config = config;
			}
		} else {
			uint64_t guid;

			/*
			 * Search for a pool by guid.
			 */
			verify(nvlist_lookup_uint64(config,
			    ZPOOL_CONFIG_POOL_GUID, &guid) == 0);

			if (guid == import->guid)
				found_config = config;
		}
	}
	if (import->do_all) {
		tpool_wait(tp);
		tpool_destroy(tp);
	}

	/*
	 * If we were searching for a specific pool, verify that we found a
	 * pool, and then do the import.
	 */
	if (pool_specified && err == 0) {
		if (found_config == NULL) {
			(void) fprintf(stderr, gettext("cannot import '%s': "
			    "no such pool available\n"), orig_name);
			err = B_TRUE;
		} else {
			err |= do_import(found_config, new_name,
			    mntopts, props, flags, mount_tp_nthr);
		}
	}

	/*
	 * If we were just looking for pools, report an error if none were
	 * found.
	 */
	if (!pool_specified && first)
		(void) fprintf(stderr,
		    gettext("no pools available to import\n"));
	return (err);
}

typedef struct target_exists_args {
	const char	*poolname;
	uint64_t	poolguid;
} target_exists_args_t;

static int
name_or_guid_exists(zpool_handle_t *zhp, void *data)
{
	target_exists_args_t *args = data;
	nvlist_t *config = zpool_get_config(zhp, NULL);
	int found = 0;

	if (config == NULL)
		return (0);

	if (args->poolname != NULL) {
		const char *pool_name;

		verify(nvlist_lookup_string(config, ZPOOL_CONFIG_POOL_NAME,
		    &pool_name) == 0);
		if (strcmp(pool_name, args->poolname) == 0)
			found = 1;
	} else {
		uint64_t pool_guid;

		verify(nvlist_lookup_uint64(config, ZPOOL_CONFIG_POOL_GUID,
		    &pool_guid) == 0);
		if (pool_guid == args->poolguid)
			found = 1;
	}
	zpool_close(zhp);

	return (found);
}
/*
 * zpool checkpoint <pool>
 *       checkpoint --discard <pool>
 *
 *       -d         Discard the checkpoint from a checkpointed
 *       --discard  pool.
 *
 *       -w         Wait for discarding a checkpoint to complete.
 *       --wait
 *
 * Checkpoints the specified pool, by taking a "snapshot" of its
 * current state. A pool can only have one checkpoint at a time.
 */
int
zpool_do_checkpoint(int argc, char **argv)
{
	boolean_t discard, wait;
	char *pool;
	zpool_handle_t *zhp;
	int c, err;

	struct option long_options[] = {
		{"discard", no_argument, NULL, 'd'},
		{"wait", no_argument, NULL, 'w'},
		{0, 0, 0, 0}
	};

	discard = B_FALSE;
	wait = B_FALSE;
	while ((c = getopt_long(argc, argv, ":dw", long_options, NULL)) != -1) {
		switch (c) {
		case 'd':
			discard = B_TRUE;
			break;
		case 'w':
			wait = B_TRUE;
			break;
		case '?':
			(void) fprintf(stderr, gettext("invalid option '%c'\n"),
			    optopt);
			usage(B_FALSE);
		}
	}

	if (wait && !discard) {
		(void) fprintf(stderr, gettext("--wait only valid when "
		    "--discard also specified\n"));
		usage(B_FALSE);
	}

	argc -= optind;
	argv += optind;

	if (argc < 1) {
		(void) fprintf(stderr, gettext("missing pool argument\n"));
		usage(B_FALSE);
	}

	if (argc > 1) {
		(void) fprintf(stderr, gettext("too many arguments\n"));
		usage(B_FALSE);
	}

	pool = argv[0];

	if ((zhp = zpool_open(g_zfs, pool)) == NULL) {
		/* As a special case, check for use of '/' in the name */
		if (strchr(pool, '/') != NULL)
			(void) fprintf(stderr, gettext("'zpool checkpoint' "
			    "doesn't work on datasets. To save the state "
			    "of a dataset from a specific point in time "
			    "please use 'zfs snapshot'\n"));
		return (1);
	}

	if (discard) {
		err = (zpool_discard_checkpoint(zhp) != 0);
		if (err == 0 && wait)
			err = zpool_wait(zhp, ZPOOL_WAIT_CKPT_DISCARD);
	} else {
		err = (zpool_checkpoint(zhp) != 0);
	}

	zpool_close(zhp);

	return (err);
}

#define	CHECKPOINT_OPT	1024

/*
 * zpool prefetch <type> [<type opts>] <pool>
 *
 * Prefetchs a particular type of data in the specified pool.
 */
int
zpool_do_prefetch(int argc, char **argv)
{
	int c;
	char *poolname;
	char *typestr = NULL;
	zpool_prefetch_type_t type;
	zpool_handle_t *zhp;
	int err = 0;

	while ((c = getopt(argc, argv, "t:")) != -1) {
		switch (c) {
		case 't':
			typestr = optarg;
			break;
		case ':':
			(void) fprintf(stderr, gettext("missing argument for "
			    "'%c' option\n"), optopt);
			usage(B_FALSE);
			break;
		case '?':
			(void) fprintf(stderr, gettext("invalid option '%c'\n"),
			    optopt);
			usage(B_FALSE);
		}
	}
	argc -= optind;
	argv += optind;

	if (argc < 1) {
		(void) fprintf(stderr, gettext("missing pool name argument\n"));
		usage(B_FALSE);
	}

	if (argc > 1) {
		(void) fprintf(stderr, gettext("too many arguments\n"));
		usage(B_FALSE);
	}

	poolname = argv[0];

	argc--;
	argv++;

	if (strcmp(typestr, "ddt") == 0) {
		type = ZPOOL_PREFETCH_DDT;
	} else {
		(void) fprintf(stderr, gettext("unsupported prefetch type\n"));
		usage(B_FALSE);
	}

	if ((zhp = zpool_open(g_zfs, poolname)) == NULL)
		return (1);

	err = zpool_prefetch(zhp, type);

	zpool_close(zhp);

	return (err);
}

/*
 * zpool import [-d dir] [-D]
 *       import [-o mntopts] [-o prop=value] ... [-R root] [-D] [-l]
 *              [-d dir | -c cachefile | -s] [-f] -a
 *       import [-o mntopts] [-o prop=value] ... [-R root] [-D] [-l]
 *              [-d dir | -c cachefile | -s] [-f] [-n] [-F] <pool | id>
 *              [newpool]
 *
 *	-c	Read pool information from a cachefile instead of searching
 *		devices. If importing from a cachefile config fails, then
 *		fallback to searching for devices only in the directories that
 *		exist in the cachefile.
 *
 *	-d	Scan in a specific directory, other than /dev/.  More than
 *		one directory can be specified using multiple '-d' options.
 *
 *	-D	Scan for previously destroyed pools or import all or only
 *		specified destroyed pools.
 *
 *	-R	Temporarily import the pool, with all mountpoints relative to
 *		the given root.  The pool will remain exported when the machine
 *		is rebooted.
 *
 *	-V	Import even in the presence of faulted vdevs.  This is an
 *		intentionally undocumented option for testing purposes, and
 *		treats the pool configuration as complete, leaving any bad
 *		vdevs in the FAULTED state. In other words, it does verbatim
 *		import.
 *
 *	-f	Force import, even if it appears that the pool is active.
 *
 *	-F	Attempt rewind if necessary.
 *
 *	-n	See if rewind would work, but don't actually rewind.
 *
 *	-N	Import the pool but don't mount datasets.
 *
 *	-T	Specify a starting txg to use for import. This option is
 *		intentionally undocumented option for testing purposes.
 *
 *	-a	Import all pools found.
 *
 *	-l	Load encryption keys while importing.
 *
 *	-o	Set property=value and/or temporary mount options (without '=').
 *
 *	-s	Scan using the default search path, the libblkid cache will
 *		not be consulted.
 *
 *	--rewind-to-checkpoint
 *		Import the pool and revert back to the checkpoint.
 *
 * The import command scans for pools to import, and import pools based on pool
 * name and GUID.  The pool can also be renamed as part of the import process.
 */
int
zpool_do_import(int argc, char **argv)
{
	char **searchdirs = NULL;
	char *env, *envdup = NULL;
	int nsearch = 0;
	int c;
	int err = 0;
	nvlist_t *pools = NULL;
	boolean_t do_all = B_FALSE;
	boolean_t do_destroyed = B_FALSE;
	char *mntopts = NULL;
	uint64_t searchguid = 0;
	char *searchname = NULL;
	char *propval;
	nvlist_t *policy = NULL;
	nvlist_t *props = NULL;
	int flags = ZFS_IMPORT_NORMAL;
	uint32_t rewind_policy = ZPOOL_NO_REWIND;
	boolean_t dryrun = B_FALSE;
	boolean_t do_rewind = B_FALSE;
	boolean_t xtreme_rewind = B_FALSE;
	boolean_t do_scan = B_FALSE;
	boolean_t pool_exists = B_FALSE;
	uint64_t txg = -1ULL;
	char *cachefile = NULL;
	importargs_t idata = { 0 };
	char *endptr;

	struct option long_options[] = {
		{"rewind-to-checkpoint", no_argument, NULL, CHECKPOINT_OPT},
		{0, 0, 0, 0}
	};

	/* check options */
	while ((c = getopt_long(argc, argv, ":aCc:d:DEfFlmnNo:R:stT:VX",
	    long_options, NULL)) != -1) {
		switch (c) {
		case 'a':
			do_all = B_TRUE;
			break;
		case 'c':
			cachefile = optarg;
			break;
		case 'd':
			searchdirs = safe_realloc(searchdirs,
			    (nsearch + 1) * sizeof (char *));
			searchdirs[nsearch++] = optarg;
			break;
		case 'D':
			do_destroyed = B_TRUE;
			break;
		case 'f':
			flags |= ZFS_IMPORT_ANY_HOST;
			break;
		case 'F':
			do_rewind = B_TRUE;
			break;
		case 'l':
			flags |= ZFS_IMPORT_LOAD_KEYS;
			break;
		case 'm':
			flags |= ZFS_IMPORT_MISSING_LOG;
			break;
		case 'n':
			dryrun = B_TRUE;
			break;
		case 'N':
			flags |= ZFS_IMPORT_ONLY;
			break;
		case 'o':
			if ((propval = strchr(optarg, '=')) != NULL) {
				*propval = '\0';
				propval++;
				if (add_prop_list(optarg, propval,
				    &props, B_TRUE))
					goto error;
			} else {
				mntopts = optarg;
			}
			break;
		case 'R':
			if (add_prop_list(zpool_prop_to_name(
			    ZPOOL_PROP_ALTROOT), optarg, &props, B_TRUE))
				goto error;
			if (add_prop_list_default(zpool_prop_to_name(
			    ZPOOL_PROP_CACHEFILE), "none", &props))
				goto error;
			break;
		case 's':
			do_scan = B_TRUE;
			break;
		case 't':
			flags |= ZFS_IMPORT_TEMP_NAME;
			if (add_prop_list_default(zpool_prop_to_name(
			    ZPOOL_PROP_CACHEFILE), "none", &props))
				goto error;
			break;

		case 'T':
			errno = 0;
			txg = strtoull(optarg, &endptr, 0);
			if (errno != 0 || *endptr != '\0') {
				(void) fprintf(stderr,
				    gettext("invalid txg value\n"));
				usage(B_FALSE);
			}
			rewind_policy = ZPOOL_DO_REWIND | ZPOOL_EXTREME_REWIND;
			break;
		case 'V':
			flags |= ZFS_IMPORT_VERBATIM;
			break;
		case 'X':
			xtreme_rewind = B_TRUE;
			break;
		case CHECKPOINT_OPT:
			flags |= ZFS_IMPORT_CHECKPOINT;
			break;
		case ':':
			(void) fprintf(stderr, gettext("missing argument for "
			    "'%c' option\n"), optopt);
			usage(B_FALSE);
			break;
		case '?':
			(void) fprintf(stderr, gettext("invalid option '%c'\n"),
			    optopt);
			usage(B_FALSE);
		}
	}

	argc -= optind;
	argv += optind;

	if (cachefile && nsearch != 0) {
		(void) fprintf(stderr, gettext("-c is incompatible with -d\n"));
		usage(B_FALSE);
	}

	if (cachefile && do_scan) {
		(void) fprintf(stderr, gettext("-c is incompatible with -s\n"));
		usage(B_FALSE);
	}

	if ((flags & ZFS_IMPORT_LOAD_KEYS) && (flags & ZFS_IMPORT_ONLY)) {
		(void) fprintf(stderr, gettext("-l is incompatible with -N\n"));
		usage(B_FALSE);
	}

	if ((flags & ZFS_IMPORT_LOAD_KEYS) && !do_all && argc == 0) {
		(void) fprintf(stderr, gettext("-l is only meaningful during "
		    "an import\n"));
		usage(B_FALSE);
	}

	if ((dryrun || xtreme_rewind) && !do_rewind) {
		(void) fprintf(stderr,
		    gettext("-n or -X only meaningful with -F\n"));
		usage(B_FALSE);
	}
	if (dryrun)
		rewind_policy = ZPOOL_TRY_REWIND;
	else if (do_rewind)
		rewind_policy = ZPOOL_DO_REWIND;
	if (xtreme_rewind)
		rewind_policy |= ZPOOL_EXTREME_REWIND;

	/* In the future, we can capture further policy and include it here */
	if (nvlist_alloc(&policy, NV_UNIQUE_NAME, 0) != 0 ||
	    nvlist_add_uint64(policy, ZPOOL_LOAD_REQUEST_TXG, txg) != 0 ||
	    nvlist_add_uint32(policy, ZPOOL_LOAD_REWIND_POLICY,
	    rewind_policy) != 0)
		goto error;

	/* check argument count */
	if (do_all) {
		if (argc != 0) {
			(void) fprintf(stderr, gettext("too many arguments\n"));
			usage(B_FALSE);
		}
	} else {
		if (argc > 2) {
			(void) fprintf(stderr, gettext("too many arguments\n"));
			usage(B_FALSE);
		}
	}

	/*
	 * Check for the effective uid.  We do this explicitly here because
	 * otherwise any attempt to discover pools will silently fail.
	 */
	if (argc == 0 && geteuid() != 0) {
		(void) fprintf(stderr, gettext("cannot "
		    "discover pools: permission denied\n"));

		free(searchdirs);
		nvlist_free(props);
		nvlist_free(policy);
		return (1);
	}

	/*
	 * Depending on the arguments given, we do one of the following:
	 *
	 *	<none>	Iterate through all pools and display information about
	 *		each one.
	 *
	 *	-a	Iterate through all pools and try to import each one.
	 *
	 *	<id>	Find the pool that corresponds to the given GUID/pool
	 *		name and import that one.
	 *
	 *	-D	Above options applies only to destroyed pools.
	 */
	if (argc != 0) {
		char *endptr;

		errno = 0;
		searchguid = strtoull(argv[0], &endptr, 10);
		if (errno != 0 || *endptr != '\0') {
			searchname = argv[0];
			searchguid = 0;
		}

		/*
		 * User specified a name or guid.  Ensure it's unique.
		 */
		target_exists_args_t search = {searchname, searchguid};
		pool_exists = zpool_iter(g_zfs, name_or_guid_exists, &search);
	}

	/*
	 * Check the environment for the preferred search path.
	 */
	if ((searchdirs == NULL) && (env = getenv("ZPOOL_IMPORT_PATH"))) {
		char *dir, *tmp = NULL;

		envdup = strdup(env);

		for (dir = strtok_r(envdup, ":", &tmp);
		    dir != NULL;
		    dir = strtok_r(NULL, ":", &tmp)) {
			searchdirs = safe_realloc(searchdirs,
			    (nsearch + 1) * sizeof (char *));
			searchdirs[nsearch++] = dir;
		}
	}

	idata.path = searchdirs;
	idata.paths = nsearch;
	idata.poolname = searchname;
	idata.guid = searchguid;
	idata.cachefile = cachefile;
	idata.scan = do_scan;
	idata.policy = policy;
	idata.do_destroyed = do_destroyed;
	idata.do_all = do_all;

	libpc_handle_t lpch = {
		.lpc_lib_handle = g_zfs,
		.lpc_ops = &libzfs_config_ops,
		.lpc_printerr = B_TRUE
	};
	pools = zpool_search_import(&lpch, &idata);

	if (pools != NULL && pool_exists &&
	    (argc == 1 || strcmp(argv[0], argv[1]) == 0)) {
		(void) fprintf(stderr, gettext("cannot import '%s': "
		    "a pool with that name already exists\n"),
		    argv[0]);
		(void) fprintf(stderr, gettext("use the form '%s "
		    "<pool | id> <newpool>' to give it a new name\n"),
		    "zpool import");
		err = 1;
	} else if (pools == NULL && pool_exists) {
		(void) fprintf(stderr, gettext("cannot import '%s': "
		    "a pool with that name is already created/imported,\n"),
		    argv[0]);
		(void) fprintf(stderr, gettext("and no additional pools "
		    "with that name were found\n"));
		err = 1;
	} else if (pools == NULL) {
		if (argc != 0) {
			(void) fprintf(stderr, gettext("cannot import '%s': "
			    "no such pool available\n"), argv[0]);
		}
		err = 1;
	}

	if (err == 1) {
		free(searchdirs);
		free(envdup);
		nvlist_free(policy);
		nvlist_free(pools);
		nvlist_free(props);
		return (1);
	}

	err = import_pools(pools, props, mntopts, flags,
	    argc >= 1 ? argv[0] : NULL, argc >= 2 ? argv[1] : NULL, &idata);

	/*
	 * If we're using the cachefile and we failed to import, then
	 * fallback to scanning the directory for pools that match
	 * those in the cachefile.
	 */
	if (err != 0 && cachefile != NULL) {
		(void) printf(gettext("cachefile import failed, retrying\n"));

		/*
		 * We use the scan flag to gather the directories that exist
		 * in the cachefile. If we need to fallback to searching for
		 * the pool config, we will only search devices in these
		 * directories.
		 */
		idata.scan = B_TRUE;
		nvlist_free(pools);
		pools = zpool_search_import(&lpch, &idata);

		err = import_pools(pools, props, mntopts, flags,
		    argc >= 1 ? argv[0] : NULL, argc >= 2 ? argv[1] : NULL,
		    &idata);
	}

error:
	nvlist_free(props);
	nvlist_free(pools);
	nvlist_free(policy);
	free(searchdirs);
	free(envdup);

	return (err ? 1 : 0);
}

/*
 * zpool sync [-f] [pool] ...
 *
 * -f (undocumented) force uberblock (and config including zpool cache file)
 *    update.
 *
 * Sync the specified pool(s).
 * Without arguments "zpool sync" will sync all pools.
 * This command initiates TXG sync(s) and will return after the TXG(s) commit.
 *
 */
static int
zpool_do_sync(int argc, char **argv)
{
	int ret;
	boolean_t force = B_FALSE;

	/* check options */
	while ((ret  = getopt(argc, argv, "f")) != -1) {
		switch (ret) {
		case 'f':
			force = B_TRUE;
			break;
		case '?':
			(void) fprintf(stderr, gettext("invalid option '%c'\n"),
			    optopt);
			usage(B_FALSE);
		}
	}

	argc -= optind;
	argv += optind;

	/* if argc == 0 we will execute zpool_sync_one on all pools */
	ret = for_each_pool(argc, argv, B_FALSE, NULL, ZFS_TYPE_POOL,
	    B_FALSE, zpool_sync_one, &force);

	return (ret);
}

typedef struct iostat_cbdata {
	uint64_t cb_flags;
	int cb_namewidth;
	int cb_iteration;
	boolean_t cb_verbose;
	boolean_t cb_literal;
	boolean_t cb_scripted;
	zpool_list_t *cb_list;
	vdev_cmd_data_list_t *vcdl;
	vdev_cbdata_t cb_vdevs;
} iostat_cbdata_t;

/*  iostat labels */
typedef struct name_and_columns {
	const char *name;	/* Column name */
	unsigned int columns;	/* Center name to this number of columns */
} name_and_columns_t;

#define	IOSTAT_MAX_LABELS	15	/* Max number of labels on one line */

static const name_and_columns_t iostat_top_labels[][IOSTAT_MAX_LABELS] =
{
	[IOS_DEFAULT] = {{"capacity", 2}, {"operations", 2}, {"bandwidth", 2},
	    {NULL}},
	[IOS_LATENCY] = {{"total_wait", 2}, {"disk_wait", 2}, {"syncq_wait", 2},
	    {"asyncq_wait", 2}, {"scrub", 1}, {"trim", 1}, {"rebuild", 1},
	    {NULL}},
	[IOS_QUEUES] = {{"syncq_read", 2}, {"syncq_write", 2},
	    {"asyncq_read", 2}, {"asyncq_write", 2}, {"scrubq_read", 2},
	    {"trimq_write", 2}, {"rebuildq_write", 2}, {NULL}},
	[IOS_L_HISTO] = {{"total_wait", 2}, {"disk_wait", 2}, {"syncq_wait", 2},
	    {"asyncq_wait", 2}, {NULL}},
	[IOS_RQ_HISTO] = {{"sync_read", 2}, {"sync_write", 2},
	    {"async_read", 2}, {"async_write", 2}, {"scrub", 2},
	    {"trim", 2}, {"rebuild", 2}, {NULL}},
};

/* Shorthand - if "columns" field not set, default to 1 column */
static const name_and_columns_t iostat_bottom_labels[][IOSTAT_MAX_LABELS] =
{
	[IOS_DEFAULT] = {{"alloc"}, {"free"}, {"read"}, {"write"}, {"read"},
	    {"write"}, {NULL}},
	[IOS_LATENCY] = {{"read"}, {"write"}, {"read"}, {"write"}, {"read"},
	    {"write"}, {"read"}, {"write"}, {"wait"}, {"wait"}, {"wait"},
	    {NULL}},
	[IOS_QUEUES] = {{"pend"}, {"activ"}, {"pend"}, {"activ"}, {"pend"},
	    {"activ"}, {"pend"}, {"activ"}, {"pend"}, {"activ"},
	    {"pend"}, {"activ"}, {"pend"}, {"activ"}, {NULL}},
	[IOS_L_HISTO] = {{"read"}, {"write"}, {"read"}, {"write"}, {"read"},
	    {"write"}, {"read"}, {"write"}, {"scrub"}, {"trim"}, {"rebuild"},
	    {NULL}},
	[IOS_RQ_HISTO] = {{"ind"}, {"agg"}, {"ind"}, {"agg"}, {"ind"}, {"agg"},
	    {"ind"}, {"agg"}, {"ind"}, {"agg"}, {"ind"}, {"agg"},
	    {"ind"}, {"agg"}, {NULL}},
};

static const char *histo_to_title[] = {
	[IOS_L_HISTO] = "latency",
	[IOS_RQ_HISTO] = "req_size",
};

/*
 * Return the number of labels in a null-terminated name_and_columns_t
 * array.
 *
 */
static unsigned int
label_array_len(const name_and_columns_t *labels)
{
	int i = 0;

	while (labels[i].name)
		i++;

	return (i);
}

/*
 * Return the number of strings in a null-terminated string array.
 * For example:
 *
 *     const char foo[] = {"bar", "baz", NULL}
 *
 * returns 2
 */
static uint64_t
str_array_len(const char *array[])
{
	uint64_t i = 0;
	while (array[i])
		i++;

	return (i);
}


/*
 * Return a default column width for default/latency/queue columns. This does
 * not include histograms, which have their columns autosized.
 */
static unsigned int
default_column_width(iostat_cbdata_t *cb, enum iostat_type type)
{
	unsigned long column_width = 5; /* Normal niceprint */
	static unsigned long widths[] = {
		/*
		 * Choose some sane default column sizes for printing the
		 * raw numbers.
		 */
		[IOS_DEFAULT] = 15, /* 1PB capacity */
		[IOS_LATENCY] = 10, /* 1B ns = 10sec */
		[IOS_QUEUES] = 6,   /* 1M queue entries */
		[IOS_L_HISTO] = 10, /* 1B ns = 10sec */
		[IOS_RQ_HISTO] = 6, /* 1M queue entries */
	};

	if (cb->cb_literal)
		column_width = widths[type];

	return (column_width);
}

/*
 * Print the column labels, i.e:
 *
 *   capacity     operations     bandwidth
 * alloc   free   read  write   read  write  ...
 *
 * If force_column_width is set, use it for the column width.  If not set, use
 * the default column width.
 */
static void
print_iostat_labels(iostat_cbdata_t *cb, unsigned int force_column_width,
    const name_and_columns_t labels[][IOSTAT_MAX_LABELS])
{
	int i, idx, s;
	int text_start, rw_column_width, spaces_to_end;
	uint64_t flags = cb->cb_flags;
	uint64_t f;
	unsigned int column_width = force_column_width;

	/* For each bit set in flags */
	for (f = flags; f; f &= ~(1ULL << idx)) {
		idx = lowbit64(f) - 1;
		if (!force_column_width)
			column_width = default_column_width(cb, idx);
		/* Print our top labels centered over "read  write" label. */
		for (i = 0; i < label_array_len(labels[idx]); i++) {
			const char *name = labels[idx][i].name;
			/*
			 * We treat labels[][].columns == 0 as shorthand
			 * for one column.  It makes writing out the label
			 * tables more concise.
			 */
			unsigned int columns = MAX(1, labels[idx][i].columns);
			unsigned int slen = strlen(name);

			rw_column_width = (column_width * columns) +
			    (2 * (columns - 1));

			text_start = (int)((rw_column_width) / columns -
			    slen / columns);
			if (text_start < 0)
				text_start = 0;

			printf("  ");	/* Two spaces between columns */

			/* Space from beginning of column to label */
			for (s = 0; s < text_start; s++)
				printf(" ");

			printf("%s", name);

			/* Print space after label to end of column */
			spaces_to_end = rw_column_width - text_start - slen;
			if (spaces_to_end < 0)
				spaces_to_end = 0;

			for (s = 0; s < spaces_to_end; s++)
				printf(" ");
		}
	}
}


/*
 * print_cmd_columns - Print custom column titles from -c
 *
 * If the user specified the "zpool status|iostat -c" then print their custom
 * column titles in the header.  For example, print_cmd_columns() would print
 * the "  col1  col2" part of this:
 *
 * $ zpool iostat -vc 'echo col1=val1; echo col2=val2'
 * ...
 *	      capacity     operations     bandwidth
 * pool        alloc   free   read  write   read  write  col1  col2
 * ----------  -----  -----  -----  -----  -----  -----  ----  ----
 * mypool       269K  1008M      0      0    107    946
 *   mirror     269K  1008M      0      0    107    946
 *     sdb         -      -      0      0    102    473  val1  val2
 *     sdc         -      -      0      0      5    473  val1  val2
 * ----------  -----  -----  -----  -----  -----  -----  ----  ----
 */
static void
print_cmd_columns(vdev_cmd_data_list_t *vcdl, int use_dashes)
{
	int i, j;
	vdev_cmd_data_t *data = &vcdl->data[0];

	if (vcdl->count == 0 || data == NULL)
		return;

	/*
	 * Each vdev cmd should have the same column names unless the user did
	 * something weird with their cmd.  Just take the column names from the
	 * first vdev and assume it works for all of them.
	 */
	for (i = 0; i < vcdl->uniq_cols_cnt; i++) {
		printf("  ");
		if (use_dashes) {
			for (j = 0; j < vcdl->uniq_cols_width[i]; j++)
				printf("-");
		} else {
			printf_color(ANSI_BOLD, "%*s", vcdl->uniq_cols_width[i],
			    vcdl->uniq_cols[i]);
		}
	}
}


/*
 * Utility function to print out a line of dashes like:
 *
 * 	--------------------------------  -----  -----  -----  -----  -----
 *
 * ...or a dashed named-row line like:
 *
 * 	logs                                  -      -      -      -      -
 *
 * @cb:				iostat data
 *
 * @force_column_width		If non-zero, use the value as the column width.
 * 				Otherwise use the default column widths.
 *
 * @name:			Print a dashed named-row line starting
 * 				with @name.  Otherwise, print a regular
 * 				dashed line.
 */
static void
print_iostat_dashes(iostat_cbdata_t *cb, unsigned int force_column_width,
    const char *name)
{
	int i;
	unsigned int namewidth;
	uint64_t flags = cb->cb_flags;
	uint64_t f;
	int idx;
	const name_and_columns_t *labels;
	const char *title;


	if (cb->cb_flags & IOS_ANYHISTO_M) {
		title = histo_to_title[IOS_HISTO_IDX(cb->cb_flags)];
	} else if (cb->cb_vdevs.cb_names_count) {
		title = "vdev";
	} else  {
		title = "pool";
	}

	namewidth = MAX(MAX(strlen(title), cb->cb_namewidth),
	    name ? strlen(name) : 0);


	if (name) {
		printf("%-*s", namewidth, name);
	} else {
		for (i = 0; i < namewidth; i++)
			(void) printf("-");
	}

	/* For each bit in flags */
	for (f = flags; f; f &= ~(1ULL << idx)) {
		unsigned int column_width;
		idx = lowbit64(f) - 1;
		if (force_column_width)
			column_width = force_column_width;
		else
			column_width = default_column_width(cb, idx);

		labels = iostat_bottom_labels[idx];
		for (i = 0; i < label_array_len(labels); i++) {
			if (name)
				printf("  %*s-", column_width - 1, " ");
			else
				printf("  %.*s", column_width,
				    "--------------------");
		}
	}
}


static void
print_iostat_separator_impl(iostat_cbdata_t *cb,
    unsigned int force_column_width)
{
	print_iostat_dashes(cb, force_column_width, NULL);
}

static void
print_iostat_separator(iostat_cbdata_t *cb)
{
	print_iostat_separator_impl(cb, 0);
}

static void
print_iostat_header_impl(iostat_cbdata_t *cb, unsigned int force_column_width,
    const char *histo_vdev_name)
{
	unsigned int namewidth;
	const char *title;

	color_start(ANSI_BOLD);

	if (cb->cb_flags & IOS_ANYHISTO_M) {
		title = histo_to_title[IOS_HISTO_IDX(cb->cb_flags)];
	} else if (cb->cb_vdevs.cb_names_count) {
		title = "vdev";
	} else  {
		title = "pool";
	}

	namewidth = MAX(MAX(strlen(title), cb->cb_namewidth),
	    histo_vdev_name ? strlen(histo_vdev_name) : 0);

	if (histo_vdev_name)
		printf("%-*s", namewidth, histo_vdev_name);
	else
		printf("%*s", namewidth, "");


	print_iostat_labels(cb, force_column_width, iostat_top_labels);
	printf("\n");

	printf("%-*s", namewidth, title);

	print_iostat_labels(cb, force_column_width, iostat_bottom_labels);
	if (cb->vcdl != NULL)
		print_cmd_columns(cb->vcdl, 0);

	printf("\n");

	print_iostat_separator_impl(cb, force_column_width);

	if (cb->vcdl != NULL)
		print_cmd_columns(cb->vcdl, 1);

	color_end();

	printf("\n");
}

static void
print_iostat_header(iostat_cbdata_t *cb)
{
	print_iostat_header_impl(cb, 0, NULL);
}

/*
 * Prints a size string (i.e. 120M) with the suffix ("M") colored
 * by order of magnitude. Uses column_size to add padding.
 */
static void
print_stat_color(const char *statbuf, unsigned int column_size)
{
	fputs("  ", stdout);
	size_t len = strlen(statbuf);
	while (len < column_size) {
		fputc(' ', stdout);
		column_size--;
	}
	if (*statbuf == '0') {
		color_start(ANSI_GRAY);
		fputc('0', stdout);
	} else {
		for (; *statbuf; statbuf++) {
			if (*statbuf == 'K') color_start(ANSI_GREEN);
			else if (*statbuf == 'M') color_start(ANSI_YELLOW);
			else if (*statbuf == 'G') color_start(ANSI_RED);
			else if (*statbuf == 'T') color_start(ANSI_BOLD_BLUE);
			else if (*statbuf == 'P') color_start(ANSI_MAGENTA);
			else if (*statbuf == 'E') color_start(ANSI_CYAN);
			fputc(*statbuf, stdout);
			if (--column_size <= 0)
				break;
		}
	}
	color_end();
}

/*
 * Display a single statistic.
 */
static void
print_one_stat(uint64_t value, enum zfs_nicenum_format format,
    unsigned int column_size, boolean_t scripted)
{
	char buf[64];

	zfs_nicenum_format(value, buf, sizeof (buf), format);

	if (scripted)
		printf("\t%s", buf);
	else
		print_stat_color(buf, column_size);
}

/*
 * Calculate the default vdev stats
 *
 * Subtract oldvs from newvs, apply a scaling factor, and save the resulting
 * stats into calcvs.
 */
static void
calc_default_iostats(vdev_stat_t *oldvs, vdev_stat_t *newvs,
    vdev_stat_t *calcvs)
{
	int i;

	memcpy(calcvs, newvs, sizeof (*calcvs));
	for (i = 0; i < ARRAY_SIZE(calcvs->vs_ops); i++)
		calcvs->vs_ops[i] = (newvs->vs_ops[i] - oldvs->vs_ops[i]);

	for (i = 0; i < ARRAY_SIZE(calcvs->vs_bytes); i++)
		calcvs->vs_bytes[i] = (newvs->vs_bytes[i] - oldvs->vs_bytes[i]);
}

/*
 * Internal representation of the extended iostats data.
 *
 * The extended iostat stats are exported in nvlists as either uint64_t arrays
 * or single uint64_t's.  We make both look like arrays to make them easier
 * to process.  In order to make single uint64_t's look like arrays, we set
 * __data to the stat data, and then set *data = &__data with count = 1.  Then,
 * we can just use *data and count.
 */
struct stat_array {
	uint64_t *data;
	uint_t count;	/* Number of entries in data[] */
	uint64_t __data; /* Only used when data is a single uint64_t */
};

static uint64_t
stat_histo_max(struct stat_array *nva, unsigned int len)
{
	uint64_t max = 0;
	int i;
	for (i = 0; i < len; i++)
		max = MAX(max, array64_max(nva[i].data, nva[i].count));

	return (max);
}

/*
 * Helper function to lookup a uint64_t array or uint64_t value and store its
 * data as a stat_array.  If the nvpair is a single uint64_t value, then we make
 * it look like a one element array to make it easier to process.
 */
static int
nvpair64_to_stat_array(nvlist_t *nvl, const char *name,
    struct stat_array *nva)
{
	nvpair_t *tmp;
	int ret;

	verify(nvlist_lookup_nvpair(nvl, name, &tmp) == 0);
	switch (nvpair_type(tmp)) {
	case DATA_TYPE_UINT64_ARRAY:
		ret = nvpair_value_uint64_array(tmp, &nva->data, &nva->count);
		break;
	case DATA_TYPE_UINT64:
		ret = nvpair_value_uint64(tmp, &nva->__data);
		nva->data = &nva->__data;
		nva->count = 1;
		break;
	default:
		/* Not a uint64_t */
		ret = EINVAL;
		break;
	}

	return (ret);
}

/*
 * Given a list of nvlist names, look up the extended stats in newnv and oldnv,
 * subtract them, and return the results in a newly allocated stat_array.
 * You must free the returned array after you are done with it with
 * free_calc_stats().
 *
 * Additionally, you can set "oldnv" to NULL if you simply want the newnv
 * values.
 */
static struct stat_array *
calc_and_alloc_stats_ex(const char **names, unsigned int len, nvlist_t *oldnv,
    nvlist_t *newnv)
{
	nvlist_t *oldnvx = NULL, *newnvx;
	struct stat_array *oldnva, *newnva, *calcnva;
	int i, j;
	unsigned int alloc_size = (sizeof (struct stat_array)) * len;

	/* Extract our extended stats nvlist from the main list */
	verify(nvlist_lookup_nvlist(newnv, ZPOOL_CONFIG_VDEV_STATS_EX,
	    &newnvx) == 0);
	if (oldnv) {
		verify(nvlist_lookup_nvlist(oldnv, ZPOOL_CONFIG_VDEV_STATS_EX,
		    &oldnvx) == 0);
	}

	newnva = safe_malloc(alloc_size);
	oldnva = safe_malloc(alloc_size);
	calcnva = safe_malloc(alloc_size);

	for (j = 0; j < len; j++) {
		verify(nvpair64_to_stat_array(newnvx, names[j],
		    &newnva[j]) == 0);
		calcnva[j].count = newnva[j].count;
		alloc_size = calcnva[j].count * sizeof (calcnva[j].data[0]);
		calcnva[j].data = safe_malloc(alloc_size);
		memcpy(calcnva[j].data, newnva[j].data, alloc_size);

		if (oldnvx) {
			verify(nvpair64_to_stat_array(oldnvx, names[j],
			    &oldnva[j]) == 0);
			for (i = 0; i < oldnva[j].count; i++)
				calcnva[j].data[i] -= oldnva[j].data[i];
		}
	}
	free(newnva);
	free(oldnva);
	return (calcnva);
}

static void
free_calc_stats(struct stat_array *nva, unsigned int len)
{
	int i;
	for (i = 0; i < len; i++)
		free(nva[i].data);

	free(nva);
}

static void
print_iostat_histo(struct stat_array *nva, unsigned int len,
    iostat_cbdata_t *cb, unsigned int column_width, unsigned int namewidth,
    double scale)
{
	int i, j;
	char buf[6];
	uint64_t val;
	enum zfs_nicenum_format format;
	unsigned int buckets;
	unsigned int start_bucket;

	if (cb->cb_literal)
		format = ZFS_NICENUM_RAW;
	else
		format = ZFS_NICENUM_1024;

	/* All these histos are the same size, so just use nva[0].count */
	buckets = nva[0].count;

	if (cb->cb_flags & IOS_RQ_HISTO_M) {
		/* Start at 512 - req size should never be lower than this */
		start_bucket = 9;
	} else {
		start_bucket = 0;
	}

	for (j = start_bucket; j < buckets; j++) {
		/* Print histogram bucket label */
		if (cb->cb_flags & IOS_L_HISTO_M) {
			/* Ending range of this bucket */
			val = (1UL << (j + 1)) - 1;
			zfs_nicetime(val, buf, sizeof (buf));
		} else {
			/* Request size (starting range of bucket) */
			val = (1UL << j);
			zfs_nicenum(val, buf, sizeof (buf));
		}

		if (cb->cb_scripted)
			printf("%llu", (u_longlong_t)val);
		else
			printf("%-*s", namewidth, buf);

		/* Print the values on the line */
		for (i = 0; i < len; i++) {
			print_one_stat(nva[i].data[j] * scale, format,
			    column_width, cb->cb_scripted);
		}
		printf("\n");
	}
}

static void
print_solid_separator(unsigned int length)
{
	while (length--)
		printf("-");
	printf("\n");
}

static void
print_iostat_histos(iostat_cbdata_t *cb, nvlist_t *oldnv,
    nvlist_t *newnv, double scale, const char *name)
{
	unsigned int column_width;
	unsigned int namewidth;
	unsigned int entire_width;
	enum iostat_type type;
	struct stat_array *nva;
	const char **names;
	unsigned int names_len;

	/* What type of histo are we? */
	type = IOS_HISTO_IDX(cb->cb_flags);

	/* Get NULL-terminated array of nvlist names for our histo */
	names = vsx_type_to_nvlist[type];
	names_len = str_array_len(names); /* num of names */

	nva = calc_and_alloc_stats_ex(names, names_len, oldnv, newnv);

	if (cb->cb_literal) {
		column_width = MAX(5,
		    (unsigned int) log10(stat_histo_max(nva, names_len)) + 1);
	} else {
		column_width = 5;
	}

	namewidth = MAX(cb->cb_namewidth,
	    strlen(histo_to_title[IOS_HISTO_IDX(cb->cb_flags)]));

	/*
	 * Calculate the entire line width of what we're printing.  The
	 * +2 is for the two spaces between columns:
	 */
	/*	 read  write				*/
	/*	-----  -----				*/
	/*	|___|  <---------- column_width		*/
	/*						*/
	/*	|__________|  <--- entire_width		*/
	/*						*/
	entire_width = namewidth + (column_width + 2) *
	    label_array_len(iostat_bottom_labels[type]);

	if (cb->cb_scripted)
		printf("%s\n", name);
	else
		print_iostat_header_impl(cb, column_width, name);

	print_iostat_histo(nva, names_len, cb, column_width,
	    namewidth, scale);

	free_calc_stats(nva, names_len);
	if (!cb->cb_scripted)
		print_solid_separator(entire_width);
}

/*
 * Calculate the average latency of a power-of-two latency histogram
 */
static uint64_t
single_histo_average(uint64_t *histo, unsigned int buckets)
{
	int i;
	uint64_t count = 0, total = 0;

	for (i = 0; i < buckets; i++) {
		/*
		 * Our buckets are power-of-two latency ranges.  Use the
		 * midpoint latency of each bucket to calculate the average.
		 * For example:
		 *
		 * Bucket          Midpoint
		 * 8ns-15ns:       12ns
		 * 16ns-31ns:      24ns
		 * ...
		 */
		if (histo[i] != 0) {
			total += histo[i] * (((1UL << i) + ((1UL << i)/2)));
			count += histo[i];
		}
	}

	/* Prevent divide by zero */
	return (count == 0 ? 0 : total / count);
}

static void
print_iostat_queues(iostat_cbdata_t *cb, nvlist_t *newnv)
{
	const char *names[] = {
		ZPOOL_CONFIG_VDEV_SYNC_R_PEND_QUEUE,
		ZPOOL_CONFIG_VDEV_SYNC_R_ACTIVE_QUEUE,
		ZPOOL_CONFIG_VDEV_SYNC_W_PEND_QUEUE,
		ZPOOL_CONFIG_VDEV_SYNC_W_ACTIVE_QUEUE,
		ZPOOL_CONFIG_VDEV_ASYNC_R_PEND_QUEUE,
		ZPOOL_CONFIG_VDEV_ASYNC_R_ACTIVE_QUEUE,
		ZPOOL_CONFIG_VDEV_ASYNC_W_PEND_QUEUE,
		ZPOOL_CONFIG_VDEV_ASYNC_W_ACTIVE_QUEUE,
		ZPOOL_CONFIG_VDEV_SCRUB_PEND_QUEUE,
		ZPOOL_CONFIG_VDEV_SCRUB_ACTIVE_QUEUE,
		ZPOOL_CONFIG_VDEV_TRIM_PEND_QUEUE,
		ZPOOL_CONFIG_VDEV_TRIM_ACTIVE_QUEUE,
		ZPOOL_CONFIG_VDEV_REBUILD_PEND_QUEUE,
		ZPOOL_CONFIG_VDEV_REBUILD_ACTIVE_QUEUE,
	};

	struct stat_array *nva;

	unsigned int column_width = default_column_width(cb, IOS_QUEUES);
	enum zfs_nicenum_format format;

	nva = calc_and_alloc_stats_ex(names, ARRAY_SIZE(names), NULL, newnv);

	if (cb->cb_literal)
		format = ZFS_NICENUM_RAW;
	else
		format = ZFS_NICENUM_1024;

	for (int i = 0; i < ARRAY_SIZE(names); i++) {
		uint64_t val = nva[i].data[0];
		print_one_stat(val, format, column_width, cb->cb_scripted);
	}

	free_calc_stats(nva, ARRAY_SIZE(names));
}

static void
print_iostat_latency(iostat_cbdata_t *cb, nvlist_t *oldnv,
    nvlist_t *newnv)
{
	int i;
	uint64_t val;
	const char *names[] = {
		ZPOOL_CONFIG_VDEV_TOT_R_LAT_HISTO,
		ZPOOL_CONFIG_VDEV_TOT_W_LAT_HISTO,
		ZPOOL_CONFIG_VDEV_DISK_R_LAT_HISTO,
		ZPOOL_CONFIG_VDEV_DISK_W_LAT_HISTO,
		ZPOOL_CONFIG_VDEV_SYNC_R_LAT_HISTO,
		ZPOOL_CONFIG_VDEV_SYNC_W_LAT_HISTO,
		ZPOOL_CONFIG_VDEV_ASYNC_R_LAT_HISTO,
		ZPOOL_CONFIG_VDEV_ASYNC_W_LAT_HISTO,
		ZPOOL_CONFIG_VDEV_SCRUB_LAT_HISTO,
		ZPOOL_CONFIG_VDEV_TRIM_LAT_HISTO,
		ZPOOL_CONFIG_VDEV_REBUILD_LAT_HISTO,
	};
	struct stat_array *nva;

	unsigned int column_width = default_column_width(cb, IOS_LATENCY);
	enum zfs_nicenum_format format;

	nva = calc_and_alloc_stats_ex(names, ARRAY_SIZE(names), oldnv, newnv);

	if (cb->cb_literal)
		format = ZFS_NICENUM_RAWTIME;
	else
		format = ZFS_NICENUM_TIME;

	/* Print our avg latencies on the line */
	for (i = 0; i < ARRAY_SIZE(names); i++) {
		/* Compute average latency for a latency histo */
		val = single_histo_average(nva[i].data, nva[i].count);
		print_one_stat(val, format, column_width, cb->cb_scripted);
	}
	free_calc_stats(nva, ARRAY_SIZE(names));
}

/*
 * Print default statistics (capacity/operations/bandwidth)
 */
static void
print_iostat_default(vdev_stat_t *vs, iostat_cbdata_t *cb, double scale)
{
	unsigned int column_width = default_column_width(cb, IOS_DEFAULT);
	enum zfs_nicenum_format format;
	char na;	/* char to print for "not applicable" values */

	if (cb->cb_literal) {
		format = ZFS_NICENUM_RAW;
		na = '0';
	} else {
		format = ZFS_NICENUM_1024;
		na = '-';
	}

	/* only toplevel vdevs have capacity stats */
	if (vs->vs_space == 0) {
		if (cb->cb_scripted)
			printf("\t%c\t%c", na, na);
		else
			printf("  %*c  %*c", column_width, na, column_width,
			    na);
	} else {
		print_one_stat(vs->vs_alloc, format, column_width,
		    cb->cb_scripted);
		print_one_stat(vs->vs_space - vs->vs_alloc, format,
		    column_width, cb->cb_scripted);
	}

	print_one_stat((uint64_t)(vs->vs_ops[ZIO_TYPE_READ] * scale),
	    format, column_width, cb->cb_scripted);
	print_one_stat((uint64_t)(vs->vs_ops[ZIO_TYPE_WRITE] * scale),
	    format, column_width, cb->cb_scripted);
	print_one_stat((uint64_t)(vs->vs_bytes[ZIO_TYPE_READ] * scale),
	    format, column_width, cb->cb_scripted);
	print_one_stat((uint64_t)(vs->vs_bytes[ZIO_TYPE_WRITE] * scale),
	    format, column_width, cb->cb_scripted);
}

static const char *const class_name[] = {
	VDEV_ALLOC_BIAS_DEDUP,
	VDEV_ALLOC_BIAS_SPECIAL,
	VDEV_ALLOC_CLASS_LOGS
};

/*
 * Print out all the statistics for the given vdev.  This can either be the
 * toplevel configuration, or called recursively.  If 'name' is NULL, then this
 * is a verbose output, and we don't want to display the toplevel pool stats.
 *
 * Returns the number of stat lines printed.
 */
static unsigned int
print_vdev_stats(zpool_handle_t *zhp, const char *name, nvlist_t *oldnv,
    nvlist_t *newnv, iostat_cbdata_t *cb, int depth)
{
	nvlist_t **oldchild, **newchild;
	uint_t c, children, oldchildren;
	vdev_stat_t *oldvs, *newvs, *calcvs;
	vdev_stat_t zerovs = { 0 };
	char *vname;
	int i;
	int ret = 0;
	uint64_t tdelta;
	double scale;

	if (strcmp(name, VDEV_TYPE_INDIRECT) == 0)
		return (ret);

	calcvs = safe_malloc(sizeof (*calcvs));

	if (oldnv != NULL) {
		verify(nvlist_lookup_uint64_array(oldnv,
		    ZPOOL_CONFIG_VDEV_STATS, (uint64_t **)&oldvs, &c) == 0);
	} else {
		oldvs = &zerovs;
	}

	/* Do we only want to see a specific vdev? */
	for (i = 0; i < cb->cb_vdevs.cb_names_count; i++) {
		/* Yes we do.  Is this the vdev? */
		if (strcmp(name, cb->cb_vdevs.cb_names[i]) == 0) {
			/*
			 * This is our vdev.  Since it is the only vdev we
			 * will be displaying, make depth = 0 so that it
			 * doesn't get indented.
			 */
			depth = 0;
			break;
		}
	}

	if (cb->cb_vdevs.cb_names_count && (i == cb->cb_vdevs.cb_names_count)) {
		/* Couldn't match the name */
		goto children;
	}


	verify(nvlist_lookup_uint64_array(newnv, ZPOOL_CONFIG_VDEV_STATS,
	    (uint64_t **)&newvs, &c) == 0);

	/*
	 * Print the vdev name unless it's is a histogram.  Histograms
	 * display the vdev name in the header itself.
	 */
	if (!(cb->cb_flags & IOS_ANYHISTO_M)) {
		if (cb->cb_scripted) {
			printf("%s", name);
		} else {
			if (strlen(name) + depth > cb->cb_namewidth)
				(void) printf("%*s%s", depth, "", name);
			else
				(void) printf("%*s%s%*s", depth, "", name,
				    (int)(cb->cb_namewidth - strlen(name) -
				    depth), "");
		}
	}

	/* Calculate our scaling factor */
	tdelta = newvs->vs_timestamp - oldvs->vs_timestamp;
	if ((oldvs->vs_timestamp == 0) && (cb->cb_flags & IOS_ANYHISTO_M)) {
		/*
		 * If we specify printing histograms with no time interval, then
		 * print the histogram numbers over the entire lifetime of the
		 * vdev.
		 */
		scale = 1;
	} else {
		if (tdelta == 0)
			scale = 1.0;
		else
			scale = (double)NANOSEC / tdelta;
	}

	if (cb->cb_flags & IOS_DEFAULT_M) {
		calc_default_iostats(oldvs, newvs, calcvs);
		print_iostat_default(calcvs, cb, scale);
	}
	if (cb->cb_flags & IOS_LATENCY_M)
		print_iostat_latency(cb, oldnv, newnv);
	if (cb->cb_flags & IOS_QUEUES_M)
		print_iostat_queues(cb, newnv);
	if (cb->cb_flags & IOS_ANYHISTO_M) {
		printf("\n");
		print_iostat_histos(cb, oldnv, newnv, scale, name);
	}

	if (cb->vcdl != NULL) {
		const char *path;
		if (nvlist_lookup_string(newnv, ZPOOL_CONFIG_PATH,
		    &path) == 0) {
			printf("  ");
			zpool_print_cmd(cb->vcdl, zpool_get_name(zhp), path);
		}
	}

	if (!(cb->cb_flags & IOS_ANYHISTO_M))
		printf("\n");

	ret++;

children:

	free(calcvs);

	if (!cb->cb_verbose)
		return (ret);

	if (nvlist_lookup_nvlist_array(newnv, ZPOOL_CONFIG_CHILDREN,
	    &newchild, &children) != 0)
		return (ret);

	if (oldnv) {
		if (nvlist_lookup_nvlist_array(oldnv, ZPOOL_CONFIG_CHILDREN,
		    &oldchild, &oldchildren) != 0)
			return (ret);

		children = MIN(oldchildren, children);
	}

	/*
	 * print normal top-level devices
	 */
	for (c = 0; c < children; c++) {
		uint64_t ishole = B_FALSE, islog = B_FALSE;

		(void) nvlist_lookup_uint64(newchild[c], ZPOOL_CONFIG_IS_HOLE,
		    &ishole);

		(void) nvlist_lookup_uint64(newchild[c], ZPOOL_CONFIG_IS_LOG,
		    &islog);

		if (ishole || islog)
			continue;

		if (nvlist_exists(newchild[c], ZPOOL_CONFIG_ALLOCATION_BIAS))
			continue;

		vname = zpool_vdev_name(g_zfs, zhp, newchild[c],
		    cb->cb_vdevs.cb_name_flags | VDEV_NAME_TYPE_ID);
		ret += print_vdev_stats(zhp, vname, oldnv ? oldchild[c] : NULL,
		    newchild[c], cb, depth + 2);
		free(vname);
	}

	/*
	 * print all other top-level devices
	 */
	for (uint_t n = 0; n < ARRAY_SIZE(class_name); n++) {
		boolean_t printed = B_FALSE;

		for (c = 0; c < children; c++) {
			uint64_t islog = B_FALSE;
			const char *bias = NULL;
			const char *type = NULL;

			(void) nvlist_lookup_uint64(newchild[c],
			    ZPOOL_CONFIG_IS_LOG, &islog);
			if (islog) {
				bias = VDEV_ALLOC_CLASS_LOGS;
			} else {
				(void) nvlist_lookup_string(newchild[c],
				    ZPOOL_CONFIG_ALLOCATION_BIAS, &bias);
				(void) nvlist_lookup_string(newchild[c],
				    ZPOOL_CONFIG_TYPE, &type);
			}
			if (bias == NULL || strcmp(bias, class_name[n]) != 0)
				continue;
			if (!islog && strcmp(type, VDEV_TYPE_INDIRECT) == 0)
				continue;

			if (!printed) {
				if ((!(cb->cb_flags & IOS_ANYHISTO_M)) &&
				    !cb->cb_scripted &&
				    !cb->cb_vdevs.cb_names) {
					print_iostat_dashes(cb, 0,
					    class_name[n]);
				}
				printf("\n");
				printed = B_TRUE;
			}

			vname = zpool_vdev_name(g_zfs, zhp, newchild[c],
			    cb->cb_vdevs.cb_name_flags | VDEV_NAME_TYPE_ID);
			ret += print_vdev_stats(zhp, vname, oldnv ?
			    oldchild[c] : NULL, newchild[c], cb, depth + 2);
			free(vname);
		}
	}

	/*
	 * Include level 2 ARC devices in iostat output
	 */
	if (nvlist_lookup_nvlist_array(newnv, ZPOOL_CONFIG_L2CACHE,
	    &newchild, &children) != 0)
		return (ret);

	if (oldnv) {
		if (nvlist_lookup_nvlist_array(oldnv, ZPOOL_CONFIG_L2CACHE,
		    &oldchild, &oldchildren) != 0)
			return (ret);

		children = MIN(oldchildren, children);
	}

	if (children > 0) {
		if ((!(cb->cb_flags & IOS_ANYHISTO_M)) && !cb->cb_scripted &&
		    !cb->cb_vdevs.cb_names) {
			print_iostat_dashes(cb, 0, "cache");
		}
		printf("\n");

		for (c = 0; c < children; c++) {
			vname = zpool_vdev_name(g_zfs, zhp, newchild[c],
			    cb->cb_vdevs.cb_name_flags);
			ret += print_vdev_stats(zhp, vname, oldnv ? oldchild[c]
			    : NULL, newchild[c], cb, depth + 2);
			free(vname);
		}
	}

	return (ret);
}

static int
refresh_iostat(zpool_handle_t *zhp, void *data)
{
	iostat_cbdata_t *cb = data;
	boolean_t missing;

	/*
	 * If the pool has disappeared, remove it from the list and continue.
	 */
	if (zpool_refresh_stats(zhp, &missing) != 0)
		return (-1);

	if (missing)
		pool_list_remove(cb->cb_list, zhp);

	return (0);
}

/*
 * Callback to print out the iostats for the given pool.
 */
static int
print_iostat(zpool_handle_t *zhp, void *data)
{
	iostat_cbdata_t *cb = data;
	nvlist_t *oldconfig, *newconfig;
	nvlist_t *oldnvroot, *newnvroot;
	int ret;

	newconfig = zpool_get_config(zhp, &oldconfig);

	if (cb->cb_iteration == 1)
		oldconfig = NULL;

	verify(nvlist_lookup_nvlist(newconfig, ZPOOL_CONFIG_VDEV_TREE,
	    &newnvroot) == 0);

	if (oldconfig == NULL)
		oldnvroot = NULL;
	else
		verify(nvlist_lookup_nvlist(oldconfig, ZPOOL_CONFIG_VDEV_TREE,
		    &oldnvroot) == 0);

	ret = print_vdev_stats(zhp, zpool_get_name(zhp), oldnvroot, newnvroot,
	    cb, 0);
	if ((ret != 0) && !(cb->cb_flags & IOS_ANYHISTO_M) &&
	    !cb->cb_scripted && cb->cb_verbose &&
	    !cb->cb_vdevs.cb_names_count) {
		print_iostat_separator(cb);
		if (cb->vcdl != NULL) {
			print_cmd_columns(cb->vcdl, 1);
		}
		printf("\n");
	}

	return (ret);
}

static int
get_columns(void)
{
	struct winsize ws;
	int columns = 80;
	int error;

	if (isatty(STDOUT_FILENO)) {
		error = ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws);
		if (error == 0)
			columns = ws.ws_col;
	} else {
		columns = 999;
	}

	return (columns);
}

/*
 * Return the required length of the pool/vdev name column.  The minimum
 * allowed width and output formatting flags must be provided.
 */
static int
get_namewidth(zpool_handle_t *zhp, int min_width, int flags, boolean_t verbose)
{
	nvlist_t *config, *nvroot;
	int width = min_width;

	if ((config = zpool_get_config(zhp, NULL)) != NULL) {
		verify(nvlist_lookup_nvlist(config, ZPOOL_CONFIG_VDEV_TREE,
		    &nvroot) == 0);
		size_t poolname_len = strlen(zpool_get_name(zhp));
		if (verbose == B_FALSE) {
			width = MAX(poolname_len, min_width);
		} else {
			width = MAX(poolname_len,
			    max_width(zhp, nvroot, 0, min_width, flags));
		}
	}

	return (width);
}

/*
 * Parse the input string, get the 'interval' and 'count' value if there is one.
 */
static void
get_interval_count(int *argcp, char **argv, float *iv,
    unsigned long *cnt)
{
	float interval = 0;
	unsigned long count = 0;
	int argc = *argcp;

	/*
	 * Determine if the last argument is an integer or a pool name
	 */
	if (argc > 0 && zfs_isnumber(argv[argc - 1])) {
		char *end;

		errno = 0;
		interval = strtof(argv[argc - 1], &end);

		if (*end == '\0' && errno == 0) {
			if (interval == 0) {
				(void) fprintf(stderr, gettext(
				    "interval cannot be zero\n"));
				usage(B_FALSE);
			}
			/*
			 * Ignore the last parameter
			 */
			argc--;
		} else {
			/*
			 * If this is not a valid number, just plow on.  The
			 * user will get a more informative error message later
			 * on.
			 */
			interval = 0;
		}
	}

	/*
	 * If the last argument is also an integer, then we have both a count
	 * and an interval.
	 */
	if (argc > 0 && zfs_isnumber(argv[argc - 1])) {
		char *end;

		errno = 0;
		count = interval;
		interval = strtof(argv[argc - 1], &end);

		if (*end == '\0' && errno == 0) {
			if (interval == 0) {
				(void) fprintf(stderr, gettext(
				    "interval cannot be zero\n"));
				usage(B_FALSE);
			}

			/*
			 * Ignore the last parameter
			 */
			argc--;
		} else {
			interval = 0;
		}
	}

	*iv = interval;
	*cnt = count;
	*argcp = argc;
}

static void
get_timestamp_arg(char c)
{
	if (c == 'u')
		timestamp_fmt = UDATE;
	else if (c == 'd')
		timestamp_fmt = DDATE;
	else
		usage(B_FALSE);
}

/*
 * Return stat flags that are supported by all pools by both the module and
 * zpool iostat.  "*data" should be initialized to all 0xFFs before running.
 * It will get ANDed down until only the flags that are supported on all pools
 * remain.
 */
static int
get_stat_flags_cb(zpool_handle_t *zhp, void *data)
{
	uint64_t *mask = data;
	nvlist_t *config, *nvroot, *nvx;
	uint64_t flags = 0;
	int i, j;

	config = zpool_get_config(zhp, NULL);
	verify(nvlist_lookup_nvlist(config, ZPOOL_CONFIG_VDEV_TREE,
	    &nvroot) == 0);

	/* Default stats are always supported, but for completeness.. */
	if (nvlist_exists(nvroot, ZPOOL_CONFIG_VDEV_STATS))
		flags |= IOS_DEFAULT_M;

	/* Get our extended stats nvlist from the main list */
	if (nvlist_lookup_nvlist(nvroot, ZPOOL_CONFIG_VDEV_STATS_EX,
	    &nvx) != 0) {
		/*
		 * No extended stats; they're probably running an older
		 * module.  No big deal, we support that too.
		 */
		goto end;
	}

	/* For each extended stat, make sure all its nvpairs are supported */
	for (j = 0; j < ARRAY_SIZE(vsx_type_to_nvlist); j++) {
		if (!vsx_type_to_nvlist[j][0])
			continue;

		/* Start off by assuming the flag is supported, then check */
		flags |= (1ULL << j);
		for (i = 0; vsx_type_to_nvlist[j][i]; i++) {
			if (!nvlist_exists(nvx, vsx_type_to_nvlist[j][i])) {
				/* flag isn't supported */
				flags = flags & ~(1ULL  << j);
				break;
			}
		}
	}
end:
	*mask = *mask & flags;
	return (0);
}

/*
 * Return a bitmask of stats that are supported on all pools by both the module
 * and zpool iostat.
 */
static uint64_t
get_stat_flags(zpool_list_t *list)
{
	uint64_t mask = -1;

	/*
	 * get_stat_flags_cb() will lop off bits from "mask" until only the
	 * flags that are supported on all pools remain.
	 */
	pool_list_iter(list, B_FALSE, get_stat_flags_cb, &mask);
	return (mask);
}

/*
 * Return 1 if cb_data->cb_names[0] is this vdev's name, 0 otherwise.
 */
static int
is_vdev_cb(void *zhp_data, nvlist_t *nv, void *cb_data)
{
	uint64_t guid;
	vdev_cbdata_t *cb = cb_data;
	zpool_handle_t *zhp = zhp_data;

	if (nvlist_lookup_uint64(nv, ZPOOL_CONFIG_GUID, &guid) != 0)
		return (0);

	return (guid == zpool_vdev_path_to_guid(zhp, cb->cb_names[0]));
}

/*
 * Returns 1 if cb_data->cb_names[0] is a vdev name, 0 otherwise.
 */
static int
is_vdev(zpool_handle_t *zhp, void *cb_data)
{
	return (for_each_vdev(zhp, is_vdev_cb, cb_data));
}

/*
 * Check if vdevs are in a pool
 *
 * Return 1 if all argv[] strings are vdev names in pool "pool_name". Otherwise
 * return 0.  If pool_name is NULL, then search all pools.
 */
static int
are_vdevs_in_pool(int argc, char **argv, char *pool_name,
    vdev_cbdata_t *cb)
{
	char **tmp_name;
	int ret = 0;
	int i;
	int pool_count = 0;

	if ((argc == 0) || !*argv)
		return (0);

	if (pool_name)
		pool_count = 1;

	/* Temporarily hijack cb_names for a second... */
	tmp_name = cb->cb_names;

	/* Go though our list of prospective vdev names */
	for (i = 0; i < argc; i++) {
		cb->cb_names = argv + i;

		/* Is this name a vdev in our pools? */
		ret = for_each_pool(pool_count, &pool_name, B_TRUE, NULL,
		    ZFS_TYPE_POOL, B_FALSE, is_vdev, cb);
		if (!ret) {
			/* No match */
			break;
		}
	}

	cb->cb_names = tmp_name;

	return (ret);
}

static int
is_pool_cb(zpool_handle_t *zhp, void *data)
{
	char *name = data;
	if (strcmp(name, zpool_get_name(zhp)) == 0)
		return (1);

	return (0);
}

/*
 * Do we have a pool named *name?  If so, return 1, otherwise 0.
 */
static int
is_pool(char *name)
{
	return (for_each_pool(0, NULL, B_TRUE, NULL, ZFS_TYPE_POOL, B_FALSE,
	    is_pool_cb, name));
}

/* Are all our argv[] strings pool names?  If so return 1, 0 otherwise. */
static int
are_all_pools(int argc, char **argv)
{
	if ((argc == 0) || !*argv)
		return (0);

	while (--argc >= 0)
		if (!is_pool(argv[argc]))
			return (0);

	return (1);
}

/*
 * Helper function to print out vdev/pool names we can't resolve.  Used for an
 * error message.
 */
static void
error_list_unresolved_vdevs(int argc, char **argv, char *pool_name,
    vdev_cbdata_t *cb)
{
	int i;
	char *name;
	char *str;
	for (i = 0; i < argc; i++) {
		name = argv[i];

		if (is_pool(name))
			str = gettext("pool");
		else if (are_vdevs_in_pool(1, &name, pool_name, cb))
			str = gettext("vdev in this pool");
		else if (are_vdevs_in_pool(1, &name, NULL, cb))
			str = gettext("vdev in another pool");
		else
			str = gettext("unknown");

		fprintf(stderr, "\t%s (%s)\n", name, str);
	}
}

/*
 * Same as get_interval_count(), but with additional checks to not misinterpret
 * guids as interval/count values.  Assumes VDEV_NAME_GUID is set in
 * cb.cb_vdevs.cb_name_flags.
 */
static void
get_interval_count_filter_guids(int *argc, char **argv, float *interval,
    unsigned long *count, iostat_cbdata_t *cb)
{
	int argc_for_interval = 0;

	/* Is the last arg an interval value?  Or a guid? */
	if (*argc >= 1 && !are_vdevs_in_pool(1, &argv[*argc - 1], NULL,
	    &cb->cb_vdevs)) {
		/*
		 * The last arg is not a guid, so it's probably an
		 * interval value.
		 */
		argc_for_interval++;

		if (*argc >= 2 &&
		    !are_vdevs_in_pool(1, &argv[*argc - 2], NULL,
		    &cb->cb_vdevs)) {
			/*
			 * The 2nd to last arg is not a guid, so it's probably
			 * an interval value.
			 */
			argc_for_interval++;
		}
	}

	/* Point to our list of possible intervals */
	char **tmpargv = &argv[*argc - argc_for_interval];

	*argc = *argc - argc_for_interval;
	get_interval_count(&argc_for_interval, tmpargv,
	    interval, count);
}

/*
 * Terminal height, in rows. Returns -1 if stdout is not connected to a TTY or
 * if we were unable to determine its size.
 */
static int
terminal_height(void)
{
	struct winsize win;

	if (isatty(STDOUT_FILENO) == 0)
		return (-1);

	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &win) != -1 && win.ws_row > 0)
		return (win.ws_row);

	return (-1);
}

/*
 * Run one of the zpool status/iostat -c scripts with the help (-h) option and
 * print the result.
 *
 * name:	Short name of the script ('iostat').
 * path:	Full path to the script ('/usr/local/etc/zfs/zpool.d/iostat');
 */
static void
print_zpool_script_help(char *name, char *path)
{
	char *argv[] = {path, (char *)"-h", NULL};
	char **lines = NULL;
	int lines_cnt = 0;
	int rc;

	rc = libzfs_run_process_get_stdout_nopath(path, argv, NULL, &lines,
	    &lines_cnt);
	if (rc != 0 || lines == NULL || lines_cnt <= 0) {
		if (lines != NULL)
			libzfs_free_str_array(lines, lines_cnt);
		return;
	}

	for (int i = 0; i < lines_cnt; i++)
		if (!is_blank_str(lines[i]))
			printf("  %-14s  %s\n", name, lines[i]);

	libzfs_free_str_array(lines, lines_cnt);
}

/*
 * Go though the zpool status/iostat -c scripts in the user's path, run their
 * help option (-h), and print out the results.
 */
static void
print_zpool_dir_scripts(char *dirpath)
{
	DIR *dir;
	struct dirent *ent;
	char fullpath[MAXPATHLEN];
	struct stat dir_stat;

	if ((dir = opendir(dirpath)) != NULL) {
		/* print all the files and directories within directory */
		while ((ent = readdir(dir)) != NULL) {
			if (snprintf(fullpath, sizeof (fullpath), "%s/%s",
			    dirpath, ent->d_name) >= sizeof (fullpath)) {
				(void) fprintf(stderr,
				    gettext("internal error: "
				    "ZPOOL_SCRIPTS_PATH too large.\n"));
				exit(1);
			}

			/* Print the scripts */
			if (stat(fullpath, &dir_stat) == 0)
				if (dir_stat.st_mode & S_IXUSR &&
				    S_ISREG(dir_stat.st_mode))
					print_zpool_script_help(ent->d_name,
					    fullpath);
		}
		closedir(dir);
	}
}

/*
 * Print out help text for all zpool status/iostat -c scripts.
 */
static void
print_zpool_script_list(const char *subcommand)
{
	char *dir, *sp, *tmp;

	printf(gettext("Available 'zpool %s -c' commands:\n"), subcommand);

	sp = zpool_get_cmd_search_path();
	if (sp == NULL)
		return;

	for (dir = strtok_r(sp, ":", &tmp);
	    dir != NULL;
	    dir = strtok_r(NULL, ":", &tmp))
		print_zpool_dir_scripts(dir);

	free(sp);
}

/*
 * Set the minimum pool/vdev name column width.  The width must be at least 10,
 * but may be as large as the column width - 42 so it still fits on one line.
 * NOTE: 42 is the width of the default capacity/operations/bandwidth output
 */
static int
get_namewidth_iostat(zpool_handle_t *zhp, void *data)
{
	iostat_cbdata_t *cb = data;
	int width, available_width;

	/*
	 * get_namewidth() returns the maximum width of any name in that column
	 * for any pool/vdev/device line that will be output.
	 */
	width = get_namewidth(zhp, cb->cb_namewidth,
	    cb->cb_vdevs.cb_name_flags | VDEV_NAME_TYPE_ID, cb->cb_verbose);

	/*
	 * The width we are calculating is the width of the header and also the
	 * padding width for names that are less than maximum width.  The stats
	 * take up 42 characters, so the width available for names is:
	 */
	available_width = get_columns() - 42;

	/*
	 * If the maximum width fits on a screen, then great!  Make everything
	 * line up by justifying all lines to the same width.  If that max
	 * width is larger than what's available, the name plus stats won't fit
	 * on one line, and justifying to that width would cause every line to
	 * wrap on the screen.  We only want lines with long names to wrap.
	 * Limit the padding to what won't wrap.
	 */
	if (width > available_width)
		width = available_width;

	/*
	 * And regardless of whatever the screen width is (get_columns can
	 * return 0 if the width is not known or less than 42 for a narrow
	 * terminal) have the width be a minimum of 10.
	 */
	if (width < 10)
		width = 10;

	/* Save the calculated width */
	cb->cb_namewidth = width;

	return (0);
}

/*
 * zpool iostat [[-c [script1,script2,...]] [-lq]|[-rw]] [-ghHLpPvy] [-n name]
 *              [-T d|u] [[ pool ...]|[pool vdev ...]|[vdev ...]]
 *              [interval [count]]
 *
 *	-c CMD  For each vdev, run command CMD
 *	-g	Display guid for individual vdev name.
 *	-L	Follow links when resolving vdev path name.
 *	-P	Display full path for vdev name.
 *	-v	Display statistics for individual vdevs
 *	-h	Display help
 *	-p	Display values in parsable (exact) format.
 *	-H	Scripted mode.  Don't display headers, and separate properties
 *		by a single tab.
 *	-l	Display average latency
 *	-q	Display queue depths
 *	-w	Display latency histograms
 *	-r	Display request size histogram
 *	-T	Display a timestamp in date(1) or Unix format
 *	-n	Only print headers once
 *
 * This command can be tricky because we want to be able to deal with pool
 * creation/destruction as well as vdev configuration changes.  The bulk of this
 * processing is handled by the pool_list_* routines in zpool_iter.c.  We rely
 * on pool_list_update() to detect the addition of new pools.  Configuration
 * changes are all handled within libzfs.
 */
int
zpool_do_iostat(int argc, char **argv)
{
	int c;
	int ret;
	int npools;
	float interval = 0;
	unsigned long count = 0;
	zpool_list_t *list;
	boolean_t verbose = B_FALSE;
	boolean_t latency = B_FALSE, l_histo = B_FALSE, rq_histo = B_FALSE;
	boolean_t queues = B_FALSE, parsable = B_FALSE, scripted = B_FALSE;
	boolean_t omit_since_boot = B_FALSE;
	boolean_t guid = B_FALSE;
	boolean_t follow_links = B_FALSE;
	boolean_t full_name = B_FALSE;
	boolean_t headers_once = B_FALSE;
	iostat_cbdata_t cb = { 0 };
	char *cmd = NULL;

	/* Used for printing error message */
	const char flag_to_arg[] = {[IOS_LATENCY] = 'l', [IOS_QUEUES] = 'q',
	    [IOS_L_HISTO] = 'w', [IOS_RQ_HISTO] = 'r'};

	uint64_t unsupported_flags;

	/* check options */
	while ((c = getopt(argc, argv, "c:gLPT:vyhplqrwnH")) != -1) {
		switch (c) {
		case 'c':
			if (cmd != NULL) {
				fprintf(stderr,
				    gettext("Can't set -c flag twice\n"));
				exit(1);
			}

			if (getenv("ZPOOL_SCRIPTS_ENABLED") != NULL &&
			    !libzfs_envvar_is_set("ZPOOL_SCRIPTS_ENABLED")) {
				fprintf(stderr, gettext(
				    "Can't run -c, disabled by "
				    "ZPOOL_SCRIPTS_ENABLED.\n"));
				exit(1);
			}

			if ((getuid() <= 0 || geteuid() <= 0) &&
			    !libzfs_envvar_is_set("ZPOOL_SCRIPTS_AS_ROOT")) {
				fprintf(stderr, gettext(
				    "Can't run -c with root privileges "
				    "unless ZPOOL_SCRIPTS_AS_ROOT is set.\n"));
				exit(1);
			}
			cmd = optarg;
			verbose = B_TRUE;
			break;
		case 'g':
			guid = B_TRUE;
			break;
		case 'L':
			follow_links = B_TRUE;
			break;
		case 'P':
			full_name = B_TRUE;
			break;
		case 'T':
			get_timestamp_arg(*optarg);
			break;
		case 'v':
			verbose = B_TRUE;
			break;
		case 'p':
			parsable = B_TRUE;
			break;
		case 'l':
			latency = B_TRUE;
			break;
		case 'q':
			queues = B_TRUE;
			break;
		case 'H':
			scripted = B_TRUE;
			break;
		case 'w':
			l_histo = B_TRUE;
			break;
		case 'r':
			rq_histo = B_TRUE;
			break;
		case 'y':
			omit_since_boot = B_TRUE;
			break;
		case 'n':
			headers_once = B_TRUE;
			break;
		case 'h':
			usage(B_FALSE);
			break;
		case '?':
			if (optopt == 'c') {
				print_zpool_script_list("iostat");
				exit(0);
			} else {
				fprintf(stderr,
				    gettext("invalid option '%c'\n"), optopt);
			}
			usage(B_FALSE);
		}
	}

	argc -= optind;
	argv += optind;

	cb.cb_literal = parsable;
	cb.cb_scripted = scripted;

	if (guid)
		cb.cb_vdevs.cb_name_flags |= VDEV_NAME_GUID;
	if (follow_links)
		cb.cb_vdevs.cb_name_flags |= VDEV_NAME_FOLLOW_LINKS;
	if (full_name)
		cb.cb_vdevs.cb_name_flags |= VDEV_NAME_PATH;
	cb.cb_iteration = 0;
	cb.cb_namewidth = 0;
	cb.cb_verbose = verbose;

	/* Get our interval and count values (if any) */
	if (guid) {
		get_interval_count_filter_guids(&argc, argv, &interval,
		    &count, &cb);
	} else {
		get_interval_count(&argc, argv, &interval, &count);
	}

	if (argc == 0) {
		/* No args, so just print the defaults. */
	} else if (are_all_pools(argc, argv)) {
		/* All the args are pool names */
	} else if (are_vdevs_in_pool(argc, argv, NULL, &cb.cb_vdevs)) {
		/* All the args are vdevs */
		cb.cb_vdevs.cb_names = argv;
		cb.cb_vdevs.cb_names_count = argc;
		argc = 0; /* No pools to process */
	} else if (are_all_pools(1, argv)) {
		/* The first arg is a pool name */
		if (are_vdevs_in_pool(argc - 1, argv + 1, argv[0],
		    &cb.cb_vdevs)) {
			/* ...and the rest are vdev names */
			cb.cb_vdevs.cb_names = argv + 1;
			cb.cb_vdevs.cb_names_count = argc - 1;
			argc = 1; /* One pool to process */
		} else {
			fprintf(stderr, gettext("Expected either a list of "));
			fprintf(stderr, gettext("pools, or list of vdevs in"));
			fprintf(stderr, " \"%s\", ", argv[0]);
			fprintf(stderr, gettext("but got:\n"));
			error_list_unresolved_vdevs(argc - 1, argv + 1,
			    argv[0], &cb.cb_vdevs);
			fprintf(stderr, "\n");
			usage(B_FALSE);
			return (1);
		}
	} else {
		/*
		 * The args don't make sense. The first arg isn't a pool name,
		 * nor are all the args vdevs.
		 */
		fprintf(stderr, gettext("Unable to parse pools/vdevs list.\n"));
		fprintf(stderr, "\n");
		return (1);
	}

	if (cb.cb_vdevs.cb_names_count != 0) {
		/*
		 * If user specified vdevs, it implies verbose.
		 */
		cb.cb_verbose = B_TRUE;
	}

	/*
	 * Construct the list of all interesting pools.
	 */
	ret = 0;
	if ((list = pool_list_get(argc, argv, NULL, ZFS_TYPE_POOL, parsable,
	    &ret)) == NULL)
		return (1);

	if (pool_list_count(list) == 0 && argc != 0) {
		pool_list_free(list);
		return (1);
	}

	if (pool_list_count(list) == 0 && interval == 0) {
		pool_list_free(list);
		(void) fprintf(stderr, gettext("no pools available\n"));
		return (1);
	}

	if ((l_histo || rq_histo) && (cmd != NULL || latency || queues)) {
		pool_list_free(list);
		(void) fprintf(stderr,
		    gettext("[-r|-w] isn't allowed with [-c|-l|-q]\n"));
		usage(B_FALSE);
		return (1);
	}

	if (l_histo && rq_histo) {
		pool_list_free(list);
		(void) fprintf(stderr,
		    gettext("Only one of [-r|-w] can be passed at a time\n"));
		usage(B_FALSE);
		return (1);
	}

	/*
	 * Enter the main iostat loop.
	 */
	cb.cb_list = list;

	if (l_histo) {
		/*
		 * Histograms tables look out of place when you try to display
		 * them with the other stats, so make a rule that you can only
		 * print histograms by themselves.
		 */
		cb.cb_flags = IOS_L_HISTO_M;
	} else if (rq_histo) {
		cb.cb_flags = IOS_RQ_HISTO_M;
	} else {
		cb.cb_flags = IOS_DEFAULT_M;
		if (latency)
			cb.cb_flags |= IOS_LATENCY_M;
		if (queues)
			cb.cb_flags |= IOS_QUEUES_M;
	}

	/*
	 * See if the module supports all the stats we want to display.
	 */
	unsupported_flags = cb.cb_flags & ~get_stat_flags(list);
	if (unsupported_flags) {
		uint64_t f;
		int idx;
		fprintf(stderr,
		    gettext("The loaded zfs module doesn't support:"));

		/* for each bit set in unsupported_flags */
		for (f = unsupported_flags; f; f &= ~(1ULL << idx)) {
			idx = lowbit64(f) - 1;
			fprintf(stderr, " -%c", flag_to_arg[idx]);
		}

		fprintf(stderr, ".  Try running a newer module.\n");
		pool_list_free(list);

		return (1);
	}

	for (;;) {
		if ((npools = pool_list_count(list)) == 0)
			(void) fprintf(stderr, gettext("no pools available\n"));
		else {
			/*
			 * If this is the first iteration and -y was supplied
			 * we skip any printing.
			 */
			boolean_t skip = (omit_since_boot &&
			    cb.cb_iteration == 0);

			/*
			 * Refresh all statistics.  This is done as an
			 * explicit step before calculating the maximum name
			 * width, so that any * configuration changes are
			 * properly accounted for.
			 */
			(void) pool_list_iter(list, B_FALSE, refresh_iostat,
			    &cb);

			/*
			 * Iterate over all pools to determine the maximum width
			 * for the pool / device name column across all pools.
			 */
			cb.cb_namewidth = 0;
			(void) pool_list_iter(list, B_FALSE,
			    get_namewidth_iostat, &cb);

			if (timestamp_fmt != NODATE)
				print_timestamp(timestamp_fmt);

			if (cmd != NULL && cb.cb_verbose &&
			    !(cb.cb_flags & IOS_ANYHISTO_M)) {
				cb.vcdl = all_pools_for_each_vdev_run(argc,
				    argv, cmd, g_zfs, cb.cb_vdevs.cb_names,
				    cb.cb_vdevs.cb_names_count,
				    cb.cb_vdevs.cb_name_flags);
			} else {
				cb.vcdl = NULL;
			}


			/*
			 * Check terminal size so we can print headers
			 * even when terminal window has its height
			 * changed.
			 */
			int winheight = terminal_height();
			/*
			 * Are we connected to TTY? If not, headers_once
			 * should be true, to avoid breaking scripts.
			 */
			if (winheight < 0)
				headers_once = B_TRUE;

			/*
			 * If it's the first time and we're not skipping it,
			 * or either skip or verbose mode, print the header.
			 *
			 * The histogram code explicitly prints its header on
			 * every vdev, so skip this for histograms.
			 */
			if (((++cb.cb_iteration == 1 && !skip) ||
			    (skip != verbose) ||
			    (!headers_once &&
			    (cb.cb_iteration % winheight) == 0)) &&
			    (!(cb.cb_flags & IOS_ANYHISTO_M)) &&
			    !cb.cb_scripted)
				print_iostat_header(&cb);

			if (skip) {
				(void) fflush(stdout);
				(void) fsleep(interval);
				continue;
			}

			pool_list_iter(list, B_FALSE, print_iostat, &cb);

			/*
			 * If there's more than one pool, and we're not in
			 * verbose mode (which prints a separator for us),
			 * then print a separator.
			 *
			 * In addition, if we're printing specific vdevs then
			 * we also want an ending separator.
			 */
			if (((npools > 1 && !verbose &&
			    !(cb.cb_flags & IOS_ANYHISTO_M)) ||
			    (!(cb.cb_flags & IOS_ANYHISTO_M) &&
			    cb.cb_vdevs.cb_names_count)) &&
			    !cb.cb_scripted) {
				print_iostat_separator(&cb);
				if (cb.vcdl != NULL)
					print_cmd_columns(cb.vcdl, 1);
				printf("\n");
			}

			if (cb.vcdl != NULL)
				free_vdev_cmd_data_list(cb.vcdl);

		}

		if (interval == 0)
			break;

		if (count != 0 && --count == 0)
			break;

		(void) fflush(stdout);
		(void) fsleep(interval);
	}

	pool_list_free(list);

	return (ret);
}

typedef struct list_cbdata {
	boolean_t	cb_verbose;
	int		cb_name_flags;
	int		cb_namewidth;
	boolean_t	cb_json;
	boolean_t	cb_scripted;
	zprop_list_t	*cb_proplist;
	boolean_t	cb_literal;
	nvlist_t	*cb_jsobj;
	boolean_t	cb_json_as_int;
	boolean_t	cb_json_pool_key_guid;
} list_cbdata_t;


/*
 * Given a list of columns to display, output appropriate headers for each one.
 */
static void
print_header(list_cbdata_t *cb)
{
	zprop_list_t *pl = cb->cb_proplist;
	char headerbuf[ZPOOL_MAXPROPLEN];
	const char *header;
	boolean_t first = B_TRUE;
	boolean_t right_justify;
	size_t width = 0;

	for (; pl != NULL; pl = pl->pl_next) {
		width = pl->pl_width;
		if (first && cb->cb_verbose) {
			/*
			 * Reset the width to accommodate the verbose listing
			 * of devices.
			 */
			width = cb->cb_namewidth;
		}

		if (!first)
			(void) fputs("  ", stdout);
		else
			first = B_FALSE;

		right_justify = B_FALSE;
		if (pl->pl_prop != ZPROP_USERPROP) {
			header = zpool_prop_column_name(pl->pl_prop);
			right_justify = zpool_prop_align_right(pl->pl_prop);
		} else {
			int i;

			for (i = 0; pl->pl_user_prop[i] != '\0'; i++)
				headerbuf[i] = toupper(pl->pl_user_prop[i]);
			headerbuf[i] = '\0';
			header = headerbuf;
		}

		if (pl->pl_next == NULL && !right_justify)
			(void) fputs(header, stdout);
		else if (right_justify)
			(void) printf("%*s", (int)width, header);
		else
			(void) printf("%-*s", (int)width, header);
	}

	(void) fputc('\n', stdout);
}

/*
 * Given a pool and a list of properties, print out all the properties according
 * to the described layout. Used by zpool_do_list().
 */
static void
collect_pool(zpool_handle_t *zhp, list_cbdata_t *cb)
{
	zprop_list_t *pl = cb->cb_proplist;
	boolean_t first = B_TRUE;
	char property[ZPOOL_MAXPROPLEN];
	const char *propstr;
	boolean_t right_justify;
	size_t width;
	zprop_source_t sourcetype = ZPROP_SRC_NONE;
	nvlist_t *item, *d, *props;
	item = d = props = NULL;

	if (cb->cb_json) {
		item = fnvlist_alloc();
		props = fnvlist_alloc();
		d = fnvlist_lookup_nvlist(cb->cb_jsobj, "pools");
		if (d == NULL) {
			fprintf(stderr, "pools obj not found.\n");
			exit(1);
		}
		fill_pool_info(item, zhp, B_TRUE, cb->cb_json_as_int);
	}

	for (; pl != NULL; pl = pl->pl_next) {

		width = pl->pl_width;
		if (first && cb->cb_verbose) {
			/*
			 * Reset the width to accommodate the verbose listing
			 * of devices.
			 */
			width = cb->cb_namewidth;
		}

		if (!cb->cb_json && !first) {
			if (cb->cb_scripted)
				(void) fputc('\t', stdout);
			else
				(void) fputs("  ", stdout);
		} else {
			first = B_FALSE;
		}

		right_justify = B_FALSE;
		if (pl->pl_prop != ZPROP_USERPROP) {
			if (zpool_get_prop(zhp, pl->pl_prop, property,
			    sizeof (property), &sourcetype,
			    cb->cb_literal) != 0)
				propstr = "-";
			else
				propstr = property;

			right_justify = zpool_prop_align_right(pl->pl_prop);
		} else if ((zpool_prop_feature(pl->pl_user_prop) ||
		    zpool_prop_unsupported(pl->pl_user_prop)) &&
		    zpool_prop_get_feature(zhp, pl->pl_user_prop, property,
		    sizeof (property)) == 0) {
			propstr = property;
			sourcetype = ZPROP_SRC_LOCAL;
		} else if (zfs_prop_user(pl->pl_user_prop) &&
		    zpool_get_userprop(zhp, pl->pl_user_prop, property,
		    sizeof (property), &sourcetype) == 0) {
			propstr = property;
		} else {
			propstr = "-";
		}

		if (cb->cb_json) {
			if (pl->pl_prop == ZPOOL_PROP_NAME)
				continue;
			const char *prop_name;
			if (pl->pl_prop != ZPROP_USERPROP)
				prop_name = zpool_prop_to_name(pl->pl_prop);
			else
				prop_name = pl->pl_user_prop;
			(void) zprop_nvlist_one_property(
			    prop_name, propstr,
			    sourcetype, NULL, NULL, props, cb->cb_json_as_int);
		} else {
			/*
			 * If this is being called in scripted mode, or if this
			 * is the last column and it is left-justified, don't
			 * include a width format specifier.
			 */
			if (cb->cb_scripted || (pl->pl_next == NULL &&
			    !right_justify))
				(void) fputs(propstr, stdout);
			else if (right_justify)
				(void) printf("%*s", (int)width, propstr);
			else
				(void) printf("%-*s", (int)width, propstr);
		}
	}

	if (cb->cb_json) {
		fnvlist_add_nvlist(item, "properties", props);
		if (cb->cb_json_pool_key_guid) {
			char pool_guid[256];
			uint64_t guid = fnvlist_lookup_uint64(
			    zpool_get_config(zhp, NULL),
			    ZPOOL_CONFIG_POOL_GUID);
			snprintf(pool_guid, 256, "%llu",
			    (u_longlong_t)guid);
			fnvlist_add_nvlist(d, pool_guid, item);
		} else {
			fnvlist_add_nvlist(d, zpool_get_name(zhp),
			    item);
		}
		fnvlist_free(props);
		fnvlist_free(item);
	} else
		(void) fputc('\n', stdout);
}

static void
collect_vdev_prop(zpool_prop_t prop, uint64_t value, const char *str,
    boolean_t scripted, boolean_t valid, enum zfs_nicenum_format format,
    boolean_t json, nvlist_t *nvl, boolean_t as_int)
{
	char propval[64];
	boolean_t fixed;
	size_t width = zprop_width(prop, &fixed, ZFS_TYPE_POOL);

	switch (prop) {
	case ZPOOL_PROP_SIZE:
	case ZPOOL_PROP_EXPANDSZ:
	case ZPOOL_PROP_CHECKPOINT:
	case ZPOOL_PROP_DEDUPRATIO:
	case ZPOOL_PROP_DEDUPCACHED:
		if (value == 0)
			(void) strlcpy(propval, "-", sizeof (propval));
		else
			zfs_nicenum_format(value, propval, sizeof (propval),
			    format);
		break;
	case ZPOOL_PROP_FRAGMENTATION:
		if (value == ZFS_FRAG_INVALID) {
			(void) strlcpy(propval, "-", sizeof (propval));
		} else if (format == ZFS_NICENUM_RAW) {
			(void) snprintf(propval, sizeof (propval), "%llu",
			    (unsigned long long)value);
		} else {
			(void) snprintf(propval, sizeof (propval), "%llu%%",
			    (unsigned long long)value);
		}
		break;
	case ZPOOL_PROP_CAPACITY:
		/* capacity value is in parts-per-10,000 (aka permyriad) */
		if (format == ZFS_NICENUM_RAW)
			(void) snprintf(propval, sizeof (propval), "%llu",
			    (unsigned long long)value / 100);
		else
			(void) snprintf(propval, sizeof (propval),
			    value < 1000 ? "%1.2f%%" : value < 10000 ?
			    "%2.1f%%" : "%3.0f%%", value / 100.0);
		break;
	case ZPOOL_PROP_HEALTH:
		width = 8;
		(void) strlcpy(propval, str, sizeof (propval));
		break;
	default:
		zfs_nicenum_format(value, propval, sizeof (propval), format);
	}

	if (!valid)
		(void) strlcpy(propval, "-", sizeof (propval));

	if (json) {
		zprop_nvlist_one_property(zpool_prop_to_name(prop), propval,
		    ZPROP_SRC_NONE, NULL, NULL, nvl, as_int);
	} else {
		if (scripted)
			(void) printf("\t%s", propval);
		else
			(void) printf("  %*s", (int)width, propval);
	}
}

/*
 * print static default line per vdev
 * not compatible with '-o' <proplist> option
 */
static void
collect_list_stats(zpool_handle_t *zhp, const char *name, nvlist_t *nv,
    list_cbdata_t *cb, int depth, boolean_t isspare, nvlist_t *item)
{
	nvlist_t **child;
	vdev_stat_t *vs;
	uint_t c, children = 0;
	char *vname;
	boolean_t scripted = cb->cb_scripted;
	uint64_t islog = B_FALSE;
	nvlist_t *props, *ent, *ch, *obj, *l2c, *sp;
	props = ent = ch = obj = sp = l2c = NULL;
	const char *dashes = "%-*s      -      -      -        -         "
	    "-      -      -      -         -\n";

	verify(nvlist_lookup_uint64_array(nv, ZPOOL_CONFIG_VDEV_STATS,
	    (uint64_t **)&vs, &c) == 0);

	if (name != NULL) {
		boolean_t toplevel = (vs->vs_space != 0);
		uint64_t cap;
		enum zfs_nicenum_format format;
		const char *state;

		if (cb->cb_literal)
			format = ZFS_NICENUM_RAW;
		else
			format = ZFS_NICENUM_1024;

		if (strcmp(name, VDEV_TYPE_INDIRECT) == 0)
			return;

		if (cb->cb_json) {
			props = fnvlist_alloc();
			ent = fnvlist_alloc();
			fill_vdev_info(ent, zhp, (char *)name, B_FALSE,
			    cb->cb_json_as_int);
		} else {
			if (scripted)
				(void) printf("\t%s", name);
			else if (strlen(name) + depth > cb->cb_namewidth)
				(void) printf("%*s%s", depth, "", name);
			else
				(void) printf("%*s%s%*s", depth, "", name,
				    (int)(cb->cb_namewidth - strlen(name) -
				    depth), "");
		}

		/*
		 * Print the properties for the individual vdevs. Some
		 * properties are only applicable to toplevel vdevs. The
		 * 'toplevel' boolean value is passed to the print_one_column()
		 * to indicate that the value is valid.
		 */
		if (VDEV_STAT_VALID(vs_pspace, c) && vs->vs_pspace) {
			collect_vdev_prop(ZPOOL_PROP_SIZE, vs->vs_pspace, NULL,
			    scripted, B_TRUE, format, cb->cb_json, props,
			    cb->cb_json_as_int);
		} else {
			collect_vdev_prop(ZPOOL_PROP_SIZE, vs->vs_space, NULL,
			    scripted, toplevel, format, cb->cb_json, props,
			    cb->cb_json_as_int);
		}
		collect_vdev_prop(ZPOOL_PROP_ALLOCATED, vs->vs_alloc, NULL,
		    scripted, toplevel, format, cb->cb_json, props,
		    cb->cb_json_as_int);
		collect_vdev_prop(ZPOOL_PROP_FREE, vs->vs_space - vs->vs_alloc,
		    NULL, scripted, toplevel, format, cb->cb_json, props,
		    cb->cb_json_as_int);
		collect_vdev_prop(ZPOOL_PROP_CHECKPOINT,
		    vs->vs_checkpoint_space, NULL, scripted, toplevel, format,
		    cb->cb_json, props, cb->cb_json_as_int);
		collect_vdev_prop(ZPOOL_PROP_EXPANDSZ, vs->vs_esize, NULL,
		    scripted, B_TRUE, format, cb->cb_json, props,
		    cb->cb_json_as_int);
		collect_vdev_prop(ZPOOL_PROP_FRAGMENTATION,
		    vs->vs_fragmentation, NULL, scripted,
		    (vs->vs_fragmentation != ZFS_FRAG_INVALID && toplevel),
		    format, cb->cb_json, props, cb->cb_json_as_int);
		cap = (vs->vs_space == 0) ? 0 :
		    (vs->vs_alloc * 10000 / vs->vs_space);
		collect_vdev_prop(ZPOOL_PROP_CAPACITY, cap, NULL,
		    scripted, toplevel, format, cb->cb_json, props,
		    cb->cb_json_as_int);
		collect_vdev_prop(ZPOOL_PROP_DEDUPRATIO, 0, NULL,
		    scripted, toplevel, format, cb->cb_json, props,
		    cb->cb_json_as_int);
		state = zpool_state_to_name(vs->vs_state, vs->vs_aux);
		if (isspare) {
			if (vs->vs_aux == VDEV_AUX_SPARED)
				state = "INUSE";
			else if (vs->vs_state == VDEV_STATE_HEALTHY)
				state = "AVAIL";
		}
		collect_vdev_prop(ZPOOL_PROP_HEALTH, 0, state, scripted,
		    B_TRUE, format, cb->cb_json, props, cb->cb_json_as_int);

		if (cb->cb_json) {
			fnvlist_add_nvlist(ent, "properties", props);
			fnvlist_free(props);
		} else
			(void) fputc('\n', stdout);
	}

	if (nvlist_lookup_nvlist_array(nv, ZPOOL_CONFIG_CHILDREN,
	    &child, &children) != 0) {
		if (cb->cb_json) {
			fnvlist_add_nvlist(item, name, ent);
			fnvlist_free(ent);
		}
		return;
	}

	if (cb->cb_json) {
		ch = fnvlist_alloc();
	}

	/* list the normal vdevs first */
	for (c = 0; c < children; c++) {
		uint64_t ishole = B_FALSE;

		if (nvlist_lookup_uint64(child[c],
		    ZPOOL_CONFIG_IS_HOLE, &ishole) == 0 && ishole)
			continue;

		if (nvlist_lookup_uint64(child[c],
		    ZPOOL_CONFIG_IS_LOG, &islog) == 0 && islog)
			continue;

		if (nvlist_exists(child[c], ZPOOL_CONFIG_ALLOCATION_BIAS))
			continue;

		vname = zpool_vdev_name(g_zfs, zhp, child[c],
		    cb->cb_name_flags | VDEV_NAME_TYPE_ID);

		if (name == NULL || cb->cb_json != B_TRUE)
			collect_list_stats(zhp, vname, child[c], cb, depth + 2,
			    B_FALSE, item);
		else if (cb->cb_json) {
			collect_list_stats(zhp, vname, child[c], cb, depth + 2,
			    B_FALSE, ch);
		}
		free(vname);
	}

	if (cb->cb_json) {
		if (!nvlist_empty(ch))
			fnvlist_add_nvlist(ent, "vdevs", ch);
		fnvlist_free(ch);
	}

	/* list the classes: 'logs', 'dedup', and 'special' */
	for (uint_t n = 0; n < ARRAY_SIZE(class_name); n++) {
		boolean_t printed = B_FALSE;
		if (cb->cb_json)
			obj = fnvlist_alloc();
		for (c = 0; c < children; c++) {
			const char *bias = NULL;
			const char *type = NULL;

			if (nvlist_lookup_uint64(child[c], ZPOOL_CONFIG_IS_LOG,
			    &islog) == 0 && islog) {
				bias = VDEV_ALLOC_CLASS_LOGS;
			} else {
				(void) nvlist_lookup_string(child[c],
				    ZPOOL_CONFIG_ALLOCATION_BIAS, &bias);
				(void) nvlist_lookup_string(child[c],
				    ZPOOL_CONFIG_TYPE, &type);
			}
			if (bias == NULL || strcmp(bias, class_name[n]) != 0)
				continue;
			if (!islog && strcmp(type, VDEV_TYPE_INDIRECT) == 0)
				continue;

			if (!printed && !cb->cb_json) {
				/* LINTED E_SEC_PRINTF_VAR_FMT */
				(void) printf(dashes, cb->cb_namewidth,
				    class_name[n]);
				printed = B_TRUE;
			}
			vname = zpool_vdev_name(g_zfs, zhp, child[c],
			    cb->cb_name_flags | VDEV_NAME_TYPE_ID);
			collect_list_stats(zhp, vname, child[c], cb, depth + 2,
			    B_FALSE, obj);
			free(vname);
		}
		if (cb->cb_json) {
			if (!nvlist_empty(obj))
				fnvlist_add_nvlist(item, class_name[n], obj);
			fnvlist_free(obj);
		}
	}

	if (nvlist_lookup_nvlist_array(nv, ZPOOL_CONFIG_L2CACHE,
	    &child, &children) == 0 && children > 0) {
		if (cb->cb_json) {
			l2c = fnvlist_alloc();
		} else {
			/* LINTED E_SEC_PRINTF_VAR_FMT */
			(void) printf(dashes, cb->cb_namewidth, "cache");
		}
		for (c = 0; c < children; c++) {
			vname = zpool_vdev_name(g_zfs, zhp, child[c],
			    cb->cb_name_flags);
			collect_list_stats(zhp, vname, child[c], cb, depth + 2,
			    B_FALSE, l2c);
			free(vname);
		}
		if (cb->cb_json) {
			if (!nvlist_empty(l2c))
				fnvlist_add_nvlist(item, "l2cache", l2c);
			fnvlist_free(l2c);
		}
	}

	if (nvlist_lookup_nvlist_array(nv, ZPOOL_CONFIG_SPARES, &child,
	    &children) == 0 && children > 0) {
		if (cb->cb_json) {
			sp = fnvlist_alloc();
		} else {
			/* LINTED E_SEC_PRINTF_VAR_FMT */
			(void) printf(dashes, cb->cb_namewidth, "spare");
		}
		for (c = 0; c < children; c++) {
			vname = zpool_vdev_name(g_zfs, zhp, child[c],
			    cb->cb_name_flags);
			collect_list_stats(zhp, vname, child[c], cb, depth + 2,
			    B_TRUE, sp);
			free(vname);
		}
		if (cb->cb_json) {
			if (!nvlist_empty(sp))
				fnvlist_add_nvlist(item, "spares", sp);
			fnvlist_free(sp);
		}
	}

	if (name != NULL && cb->cb_json) {
		fnvlist_add_nvlist(item, name, ent);
		fnvlist_free(ent);
	}
}

/*
 * Generic callback function to list a pool.
 */
static int
list_callback(zpool_handle_t *zhp, void *data)
{
	nvlist_t *p, *d, *nvdevs;
	uint64_t guid;
	char pool_guid[256];
	const char *pool_name = zpool_get_name(zhp);
	list_cbdata_t *cbp = data;
	p = d = nvdevs = NULL;

	collect_pool(zhp, cbp);

	if (cbp->cb_verbose) {
		nvlist_t *config, *nvroot;
		config = zpool_get_config(zhp, NULL);
		verify(nvlist_lookup_nvlist(config, ZPOOL_CONFIG_VDEV_TREE,
		    &nvroot) == 0);
		if (cbp->cb_json) {
			d = fnvlist_lookup_nvlist(cbp->cb_jsobj,
			    "pools");
			if (cbp->cb_json_pool_key_guid) {
				guid = fnvlist_lookup_uint64(config,
				    ZPOOL_CONFIG_POOL_GUID);
				snprintf(pool_guid, 256, "%llu",
				    (u_longlong_t)guid);
				p = fnvlist_lookup_nvlist(d, pool_guid);
			} else {
				p = fnvlist_lookup_nvlist(d, pool_name);
			}
			nvdevs = fnvlist_alloc();
		}
		collect_list_stats(zhp, NULL, nvroot, cbp, 0, B_FALSE, nvdevs);
		if (cbp->cb_json) {
			fnvlist_add_nvlist(p, "vdevs", nvdevs);
			if (cbp->cb_json_pool_key_guid)
				fnvlist_add_nvlist(d, pool_guid, p);
			else
				fnvlist_add_nvlist(d, pool_name, p);
			fnvlist_add_nvlist(cbp->cb_jsobj, "pools", d);
			fnvlist_free(nvdevs);
		}
	}

	return (0);
}

/*
 * Set the minimum pool/vdev name column width.  The width must be at least 9,
 * but may be as large as needed.
 */
static int
get_namewidth_list(zpool_handle_t *zhp, void *data)
{
	list_cbdata_t *cb = data;
	int width;

	width = get_namewidth(zhp, cb->cb_namewidth,
	    cb->cb_name_flags | VDEV_NAME_TYPE_ID, cb->cb_verbose);

	if (width < 9)
		width = 9;

	cb->cb_namewidth = width;

	return (0);
}

/*
 * zpool list [-gHLpP] [-o prop[,prop]*] [-T d|u] [pool] ... [interval [count]]
 *
 *	-g	Display guid for individual vdev name.
 *	-H	Scripted mode.  Don't display headers, and separate properties
 *		by a single tab.
 *	-L	Follow links when resolving vdev path name.
 *	-o	List of properties to display.  Defaults to
 *		"name,size,allocated,free,expandsize,fragmentation,capacity,"
 *		"dedupratio,health,altroot"
 *	-p	Display values in parsable (exact) format.
 *	-P	Display full path for vdev name.
 *	-T	Display a timestamp in date(1) or Unix format
 *	-j	Display the output in JSON format
 *	--json-int	Display the numbers as integer instead of strings.
 *	--json-pool-key-guid  Set pool GUID as key for pool objects.
 *
 * List all pools in the system, whether or not they're healthy.  Output space
 * statistics for each one, as well as health status summary.
 */
int
zpool_do_list(int argc, char **argv)
{
	int c;
	int ret = 0;
	list_cbdata_t cb = { 0 };
	static char default_props[] =
	    "name,size,allocated,free,checkpoint,expandsize,fragmentation,"
	    "capacity,dedupratio,health,altroot";
	char *props = default_props;
	float interval = 0;
	unsigned long count = 0;
	zpool_list_t *list;
	boolean_t first = B_TRUE;
	nvlist_t *data = NULL;
	current_prop_type = ZFS_TYPE_POOL;

	struct option long_options[] = {
		{"json", no_argument, NULL, 'j'},
		{"json-int", no_argument, NULL, ZPOOL_OPTION_JSON_NUMS_AS_INT},
		{"json-pool-key-guid", no_argument, NULL,
		    ZPOOL_OPTION_POOL_KEY_GUID},
		{0, 0, 0, 0}
	};

	/* check options */
	while ((c = getopt_long(argc, argv, ":gjHLo:pPT:v", long_options,
	    NULL)) != -1) {
		switch (c) {
		case 'g':
			cb.cb_name_flags |= VDEV_NAME_GUID;
			break;
		case 'H':
			cb.cb_scripted = B_TRUE;
			break;
		case 'L':
			cb.cb_name_flags |= VDEV_NAME_FOLLOW_LINKS;
			break;
		case 'o':
			props = optarg;
			break;
		case 'P':
			cb.cb_name_flags |= VDEV_NAME_PATH;
			break;
		case 'p':
			cb.cb_literal = B_TRUE;
			break;
		case 'j':
			cb.cb_json = B_TRUE;
			break;
		case ZPOOL_OPTION_JSON_NUMS_AS_INT:
			cb.cb_json_as_int = B_TRUE;
			cb.cb_literal = B_TRUE;
			break;
		case ZPOOL_OPTION_POOL_KEY_GUID:
			cb.cb_json_pool_key_guid = B_TRUE;
			break;
		case 'T':
			get_timestamp_arg(*optarg);
			break;
		case 'v':
			cb.cb_verbose = B_TRUE;
			cb.cb_namewidth = 8;	/* 8 until precalc is avail */
			break;
		case ':':
			(void) fprintf(stderr, gettext("missing argument for "
			    "'%c' option\n"), optopt);
			usage(B_FALSE);
			break;
		case '?':
			(void) fprintf(stderr, gettext("invalid option '%c'\n"),
			    optopt);
			usage(B_FALSE);
		}
	}

	argc -= optind;
	argv += optind;

	if (!cb.cb_json && cb.cb_json_as_int) {
		(void) fprintf(stderr, gettext("'--json-int' only works with"
		    " '-j' option\n"));
		usage(B_FALSE);
	}

	if (!cb.cb_json && cb.cb_json_pool_key_guid) {
		(void) fprintf(stderr, gettext("'json-pool-key-guid' only"
		    " works with '-j' option\n"));
		usage(B_FALSE);
	}

	get_interval_count(&argc, argv, &interval, &count);

	if (zprop_get_list(g_zfs, props, &cb.cb_proplist, ZFS_TYPE_POOL) != 0)
		usage(B_FALSE);

	for (;;) {
		if ((list = pool_list_get(argc, argv, &cb.cb_proplist,
		    ZFS_TYPE_POOL, cb.cb_literal, &ret)) == NULL)
			return (1);

		if (pool_list_count(list) == 0)
			break;

		if (cb.cb_json) {
			cb.cb_jsobj = zpool_json_schema(0, 1);
			data = fnvlist_alloc();
			fnvlist_add_nvlist(cb.cb_jsobj, "pools", data);
			fnvlist_free(data);
		}

		cb.cb_namewidth = 0;
		(void) pool_list_iter(list, B_FALSE, get_namewidth_list, &cb);

		if (timestamp_fmt != NODATE) {
			if (cb.cb_json) {
				if (cb.cb_json_as_int) {
					fnvlist_add_uint64(cb.cb_jsobj, "time",
					    time(NULL));
				} else {
					char ts[128];
					get_timestamp(timestamp_fmt, ts, 128);
					fnvlist_add_string(cb.cb_jsobj, "time",
					    ts);
				}
			} else
				print_timestamp(timestamp_fmt);
		}

		if (!cb.cb_scripted && (first || cb.cb_verbose) &&
		    !cb.cb_json) {
			print_header(&cb);
			first = B_FALSE;
		}
		ret = pool_list_iter(list, B_TRUE, list_callback, &cb);

		if (ret == 0 && cb.cb_json)
			zcmd_print_json(cb.cb_jsobj);
		else if (ret != 0 && cb.cb_json)
			nvlist_free(cb.cb_jsobj);

		if (interval == 0)
			break;

		if (count != 0 && --count == 0)
			break;

		pool_list_free(list);

		(void) fflush(stdout);
		(void) fsleep(interval);
	}

	if (argc == 0 && !cb.cb_scripted && !cb.cb_json &&
	    pool_list_count(list) == 0) {
		(void) printf(gettext("no pools available\n"));
		ret = 0;
	}

	pool_list_free(list);
	zprop_free_list(cb.cb_proplist);
	return (ret);
}

static int
zpool_do_attach_or_replace(int argc, char **argv, int replacing)
{
	boolean_t force = B_FALSE;
	boolean_t rebuild = B_FALSE;
	boolean_t wait = B_FALSE;
	int c;
	nvlist_t *nvroot;
	char *poolname, *old_disk, *new_disk;
	zpool_handle_t *zhp;
	nvlist_t *props = NULL;
	char *propval;
	int ret;

	/* check options */
	while ((c = getopt(argc, argv, "fo:sw")) != -1) {
		switch (c) {
		case 'f':
			force = B_TRUE;
			break;
		case 'o':
			if ((propval = strchr(optarg, '=')) == NULL) {
				(void) fprintf(stderr, gettext("missing "
				    "'=' for -o option\n"));
				usage(B_FALSE);
			}
			*propval = '\0';
			propval++;

			if ((strcmp(optarg, ZPOOL_CONFIG_ASHIFT) != 0) ||
			    (add_prop_list(optarg, propval, &props, B_TRUE)))
				usage(B_FALSE);
			break;
		case 's':
			rebuild = B_TRUE;
			break;
		case 'w':
			wait = B_TRUE;
			break;
		case '?':
			(void) fprintf(stderr, gettext("invalid option '%c'\n"),
			    optopt);
			usage(B_FALSE);
		}
	}

	argc -= optind;
	argv += optind;

	/* get pool name and check number of arguments */
	if (argc < 1) {
		(void) fprintf(stderr, gettext("missing pool name argument\n"));
		usage(B_FALSE);
	}

	poolname = argv[0];

	if (argc < 2) {
		(void) fprintf(stderr,
		    gettext("missing <device> specification\n"));
		usage(B_FALSE);
	}

	old_disk = argv[1];

	if (argc < 3) {
		if (!replacing) {
			(void) fprintf(stderr,
			    gettext("missing <new_device> specification\n"));
			usage(B_FALSE);
		}
		new_disk = old_disk;
		argc -= 1;
		argv += 1;
	} else {
		new_disk = argv[2];
		argc -= 2;
		argv += 2;
	}

	if (argc > 1) {
		(void) fprintf(stderr, gettext("too many arguments\n"));
		usage(B_FALSE);
	}

	if ((zhp = zpool_open(g_zfs, poolname)) == NULL) {
		nvlist_free(props);
		return (1);
	}

	if (zpool_get_config(zhp, NULL) == NULL) {
		(void) fprintf(stderr, gettext("pool '%s' is unavailable\n"),
		    poolname);
		zpool_close(zhp);
		nvlist_free(props);
		return (1);
	}

	/* unless manually specified use "ashift" pool property (if set) */
	if (!nvlist_exists(props, ZPOOL_CONFIG_ASHIFT)) {
		int intval;
		zprop_source_t src;
		char strval[ZPOOL_MAXPROPLEN];

		intval = zpool_get_prop_int(zhp, ZPOOL_PROP_ASHIFT, &src);
		if (src != ZPROP_SRC_DEFAULT) {
			(void) sprintf(strval, "%" PRId32, intval);
			verify(add_prop_list(ZPOOL_CONFIG_ASHIFT, strval,
			    &props, B_TRUE) == 0);
		}
	}

	nvroot = make_root_vdev(zhp, props, force, B_FALSE, replacing, B_FALSE,
	    argc, argv);
	if (nvroot == NULL) {
		zpool_close(zhp);
		nvlist_free(props);
		return (1);
	}

	ret = zpool_vdev_attach(zhp, old_disk, new_disk, nvroot, replacing,
	    rebuild);

	if (ret == 0 && wait) {
		zpool_wait_activity_t activity = ZPOOL_WAIT_RESILVER;
		char raidz_prefix[] = "raidz";
		if (replacing) {
			activity = ZPOOL_WAIT_REPLACE;
		} else if (strncmp(old_disk,
		    raidz_prefix, strlen(raidz_prefix)) == 0) {
			activity = ZPOOL_WAIT_RAIDZ_EXPAND;
		}
		ret = zpool_wait(zhp, activity);
	}

	nvlist_free(props);
	nvlist_free(nvroot);
	zpool_close(zhp);

	return (ret);
}

/*
 * zpool replace [-fsw] [-o property=value] <pool> <device> <new_device>
 *
 *	-f	Force attach, even if <new_device> appears to be in use.
 *	-s	Use sequential instead of healing reconstruction for resilver.
 *	-o	Set property=value.
 *	-w	Wait for replacing to complete before returning
 *
 * Replace <device> with <new_device>.
 */
int
zpool_do_replace(int argc, char **argv)
{
	return (zpool_do_attach_or_replace(argc, argv, B_TRUE));
}

/*
 * zpool attach [-fsw] [-o property=value] <pool> <device>|<vdev> <new_device>
 *
 *	-f	Force attach, even if <new_device> appears to be in use.
 *	-s	Use sequential instead of healing reconstruction for resilver.
 *	-o	Set property=value.
 *	-w	Wait for resilvering (mirror) or expansion (raidz) to complete
 *		before returning.
 *
 * Attach <new_device> to a <device> or <vdev>, where the vdev can be of type
 * mirror or raidz. If <device> is not part of a mirror, then <device> will
 * be transformed into a mirror of <device> and <new_device>. When a mirror
 * is involved, <new_device> will begin life with a DTL of [0, now], and will
 * immediately begin to resilver itself. For the raidz case, a expansion will
 * commence and reflow the raidz data across all the disks including the
 * <new_device>.
 */
int
zpool_do_attach(int argc, char **argv)
{
	return (zpool_do_attach_or_replace(argc, argv, B_FALSE));
}

/*
 * zpool detach [-f] <pool> <device>
 *
 *	-f	Force detach of <device>, even if DTLs argue against it
 *		(not supported yet)
 *
 * Detach a device from a mirror.  The operation will be refused if <device>
 * is the last device in the mirror, or if the DTLs indicate that this device
 * has the only valid copy of some data.
 */
int
zpool_do_detach(int argc, char **argv)
{
	int c;
	char *poolname, *path;
	zpool_handle_t *zhp;
	int ret;

	/* check options */
	while ((c = getopt(argc, argv, "")) != -1) {
		switch (c) {
		case '?':
			(void) fprintf(stderr, gettext("invalid option '%c'\n"),
			    optopt);
			usage(B_FALSE);
		}
	}

	argc -= optind;
	argv += optind;

	/* get pool name and check number of arguments */
	if (argc < 1) {
		(void) fprintf(stderr, gettext("missing pool name argument\n"));
		usage(B_FALSE);
	}

	if (argc < 2) {
		(void) fprintf(stderr,
		    gettext("missing <device> specification\n"));
		usage(B_FALSE);
	}

	poolname = argv[0];
	path = argv[1];

	if ((zhp = zpool_open(g_zfs, poolname)) == NULL)
		return (1);

	ret = zpool_vdev_detach(zhp, path);

	zpool_close(zhp);

	return (ret);
}

/*
 * zpool split [-gLnP] [-o prop=val] ...
 *		[-o mntopt] ...
 *		[-R altroot] <pool> <newpool> [<device> ...]
 *
 *	-g      Display guid for individual vdev name.
 *	-L	Follow links when resolving vdev path name.
 *	-n	Do not split the pool, but display the resulting layout if
 *		it were to be split.
 *	-o	Set property=value, or set mount options.
 *	-P	Display full path for vdev name.
 *	-R	Mount the split-off pool under an alternate root.
 *	-l	Load encryption keys while importing.
 *
 * Splits the named pool and gives it the new pool name.  Devices to be split
 * off may be listed, provided that no more than one device is specified
 * per top-level vdev mirror.  The newly split pool is left in an exported
 * state unless -R is specified.
 *
 * Restrictions: the top-level of the pool pool must only be made up of
 * mirrors; all devices in the pool must be healthy; no device may be
 * undergoing a resilvering operation.
 */
int
zpool_do_split(int argc, char **argv)
{
	char *srcpool, *newpool, *propval;
	char *mntopts = NULL;
	splitflags_t flags;
	int c, ret = 0;
	int ms_status = 0;
	boolean_t loadkeys = B_FALSE;
	zpool_handle_t *zhp;
	nvlist_t *config, *props = NULL;

	flags.dryrun = B_FALSE;
	flags.import = B_FALSE;
	flags.name_flags = 0;

	/* check options */
	while ((c = getopt(argc, argv, ":gLR:lno:P")) != -1) {
		switch (c) {
		case 'g':
			flags.name_flags |= VDEV_NAME_GUID;
			break;
		case 'L':
			flags.name_flags |= VDEV_NAME_FOLLOW_LINKS;
			break;
		case 'R':
			flags.import = B_TRUE;
			if (add_prop_list(
			    zpool_prop_to_name(ZPOOL_PROP_ALTROOT), optarg,
			    &props, B_TRUE) != 0) {
				nvlist_free(props);
				usage(B_FALSE);
			}
			break;
		case 'l':
			loadkeys = B_TRUE;
			break;
		case 'n':
			flags.dryrun = B_TRUE;
			break;
		case 'o':
			if ((propval = strchr(optarg, '=')) != NULL) {
				*propval = '\0';
				propval++;
				if (add_prop_list(optarg, propval,
				    &props, B_TRUE) != 0) {
					nvlist_free(props);
					usage(B_FALSE);
				}
			} else {
				mntopts = optarg;
			}
			break;
		case 'P':
			flags.name_flags |= VDEV_NAME_PATH;
			break;
		case ':':
			(void) fprintf(stderr, gettext("missing argument for "
			    "'%c' option\n"), optopt);
			usage(B_FALSE);
			break;
		case '?':
			(void) fprintf(stderr, gettext("invalid option '%c'\n"),
			    optopt);
			usage(B_FALSE);
			break;
		}
	}

	if (!flags.import && mntopts != NULL) {
		(void) fprintf(stderr, gettext("setting mntopts is only "
		    "valid when importing the pool\n"));
		usage(B_FALSE);
	}

	if (!flags.import && loadkeys) {
		(void) fprintf(stderr, gettext("loading keys is only "
		    "valid when importing the pool\n"));
		usage(B_FALSE);
	}

	argc -= optind;
	argv += optind;

	if (argc < 1) {
		(void) fprintf(stderr, gettext("Missing pool name\n"));
		usage(B_FALSE);
	}
	if (argc < 2) {
		(void) fprintf(stderr, gettext("Missing new pool name\n"));
		usage(B_FALSE);
	}

	srcpool = argv[0];
	newpool = argv[1];

	argc -= 2;
	argv += 2;

	if ((zhp = zpool_open(g_zfs, srcpool)) == NULL) {
		nvlist_free(props);
		return (1);
	}

	config = split_mirror_vdev(zhp, newpool, props, flags, argc, argv);
	if (config == NULL) {
		ret = 1;
	} else {
		if (flags.dryrun) {
			(void) printf(gettext("would create '%s' with the "
			    "following layout:\n\n"), newpool);
			print_vdev_tree(NULL, newpool, config, 0, "",
			    flags.name_flags);
			print_vdev_tree(NULL, "dedup", config, 0,
			    VDEV_ALLOC_BIAS_DEDUP, 0);
			print_vdev_tree(NULL, "special", config, 0,
			    VDEV_ALLOC_BIAS_SPECIAL, 0);
		}
	}

	zpool_close(zhp);

	if (ret != 0 || flags.dryrun || !flags.import) {
		nvlist_free(config);
		nvlist_free(props);
		return (ret);
	}

	/*
	 * The split was successful. Now we need to open the new
	 * pool and import it.
	 */
	if ((zhp = zpool_open_canfail(g_zfs, newpool)) == NULL) {
		nvlist_free(config);
		nvlist_free(props);
		return (1);
	}

	if (loadkeys) {
		ret = zfs_crypto_attempt_load_keys(g_zfs, newpool);
		if (ret != 0)
			ret = 1;
	}

	if (zpool_get_state(zhp) != POOL_STATE_UNAVAIL) {
		ms_status = zpool_enable_datasets(zhp, mntopts, 0,
		    mount_tp_nthr);
		if (ms_status == EZFS_SHAREFAILED) {
			(void) fprintf(stderr, gettext("Split was successful, "
			    "datasets are mounted but sharing of some datasets "
			    "has failed\n"));
		} else if (ms_status == EZFS_MOUNTFAILED) {
			(void) fprintf(stderr, gettext("Split was successful"
			    ", but some datasets could not be mounted\n"));
			(void) fprintf(stderr, gettext("Try doing '%s' with a "
			    "different altroot\n"), "zpool import");
		}
	}
	zpool_close(zhp);
	nvlist_free(config);
	nvlist_free(props);

	return (ret);
}


/*
 * zpool online [--power] <pool> <device> ...
 *
 * --power: Power on the enclosure slot to the drive (if possible)
 */
int
zpool_do_online(int argc, char **argv)
{
	int c, i;
	char *poolname;
	zpool_handle_t *zhp;
	int ret = 0;
	vdev_state_t newstate;
	int flags = 0;
	boolean_t is_power_on = B_FALSE;
	struct option long_options[] = {
		{"power", no_argument, NULL, ZPOOL_OPTION_POWER},
		{0, 0, 0, 0}
	};

	/* check options */
	while ((c = getopt_long(argc, argv, "e", long_options, NULL)) != -1) {
		switch (c) {
		case 'e':
			flags |= ZFS_ONLINE_EXPAND;
			break;
		case ZPOOL_OPTION_POWER:
			is_power_on = B_TRUE;
			break;
		case '?':
			(void) fprintf(stderr, gettext("invalid option '%c'\n"),
			    optopt);
			usage(B_FALSE);
		}
	}

	if (libzfs_envvar_is_set("ZPOOL_AUTO_POWER_ON_SLOT"))
		is_power_on = B_TRUE;

	argc -= optind;
	argv += optind;

	/* get pool name and check number of arguments */
	if (argc < 1) {
		(void) fprintf(stderr, gettext("missing pool name\n"));
		usage(B_FALSE);
	}
	if (argc < 2) {
		(void) fprintf(stderr, gettext("missing device name\n"));
		usage(B_FALSE);
	}

	poolname = argv[0];

	if ((zhp = zpool_open(g_zfs, poolname)) == NULL) {
		(void) fprintf(stderr, gettext("failed to open pool "
		    "\"%s\""), poolname);
		return (1);
	}

	for (i = 1; i < argc; i++) {
		vdev_state_t oldstate;
		boolean_t avail_spare, l2cache;
		int rc;

		if (is_power_on) {
			rc = zpool_power_on_and_disk_wait(zhp, argv[i]);
			if (rc == ENOTSUP) {
				(void) fprintf(stderr,
				    gettext("Power control not supported\n"));
			}
			if (rc != 0)
				return (rc);
		}

		nvlist_t *tgt = zpool_find_vdev(zhp, argv[i], &avail_spare,
		    &l2cache, NULL);
		if (tgt == NULL) {
			ret = 1;
			(void) fprintf(stderr, gettext("couldn't find device "
			"\"%s\" in pool \"%s\"\n"), argv[i], poolname);
			continue;
		}
		uint_t vsc;
		oldstate = ((vdev_stat_t *)fnvlist_lookup_uint64_array(tgt,
		    ZPOOL_CONFIG_VDEV_STATS, &vsc))->vs_state;
		if ((rc = zpool_vdev_online(zhp, argv[i], flags,
		    &newstate)) == 0) {
			if (newstate != VDEV_STATE_HEALTHY) {
				(void) printf(gettext("warning: device '%s' "
				    "onlined, but remains in faulted state\n"),
				    argv[i]);
				if (newstate == VDEV_STATE_FAULTED)
					(void) printf(gettext("use 'zpool "
					    "clear' to restore a faulted "
					    "device\n"));
				else
					(void) printf(gettext("use 'zpool "
					    "replace' to replace devices "
					    "that are no longer present\n"));
				if ((flags & ZFS_ONLINE_EXPAND)) {
					(void) printf(gettext("%s: failed "
					    "to expand usable space on "
					    "unhealthy device '%s'\n"),
					    (oldstate >= VDEV_STATE_DEGRADED ?
					    "error" : "warning"), argv[i]);
					if (oldstate >= VDEV_STATE_DEGRADED) {
						ret = 1;
						break;
					}
				}
			}
		} else {
			(void) fprintf(stderr, gettext("Failed to online "
			    "\"%s\" in pool \"%s\": %d\n"),
			    argv[i], poolname, rc);
			ret = 1;
		}
	}

	zpool_close(zhp);

	return (ret);
}

/*
 * zpool offline [-ft]|[--power] <pool> <device> ...
 *
 *
 *	-f	Force the device into a faulted state.
 *
 *	-t	Only take the device off-line temporarily.  The offline/faulted
 *		state will not be persistent across reboots.
 *
 *	--power Power off the enclosure slot to the drive (if possible)
 */
int
zpool_do_offline(int argc, char **argv)
{
	int c, i;
	char *poolname;
	zpool_handle_t *zhp;
	int ret = 0;
	boolean_t istmp = B_FALSE;
	boolean_t fault = B_FALSE;
	boolean_t is_power_off = B_FALSE;

	struct option long_options[] = {
		{"power", no_argument, NULL, ZPOOL_OPTION_POWER},
		{0, 0, 0, 0}
	};

	/* check options */
	while ((c = getopt_long(argc, argv, "ft", long_options, NULL)) != -1) {
		switch (c) {
		case 'f':
			fault = B_TRUE;
			break;
		case 't':
			istmp = B_TRUE;
			break;
		case ZPOOL_OPTION_POWER:
			is_power_off = B_TRUE;
			break;
		case '?':
			(void) fprintf(stderr, gettext("invalid option '%c'\n"),
			    optopt);
			usage(B_FALSE);
		}
	}

	if (is_power_off && fault) {
		(void) fprintf(stderr,
		    gettext("-0 and -f cannot be used together\n"));
		usage(B_FALSE);
		return (1);
	}

	if (is_power_off && istmp) {
		(void) fprintf(stderr,
		    gettext("-0 and -t cannot be used together\n"));
		usage(B_FALSE);
		return (1);
	}

	argc -= optind;
	argv += optind;

	/* get pool name and check number of arguments */
	if (argc < 1) {
		(void) fprintf(stderr, gettext("missing pool name\n"));
		usage(B_FALSE);
	}
	if (argc < 2) {
		(void) fprintf(stderr, gettext("missing device name\n"));
		usage(B_FALSE);
	}

	poolname = argv[0];

	if ((zhp = zpool_open(g_zfs, poolname)) == NULL) {
		(void) fprintf(stderr, gettext("failed to open pool "
		    "\"%s\""), poolname);
		return (1);
	}

	for (i = 1; i < argc; i++) {
		uint64_t guid = zpool_vdev_path_to_guid(zhp, argv[i]);
		if (is_power_off) {
			/*
			 * Note: we have to power off first, then set REMOVED,
			 * or else zpool_vdev_set_removed_state() returns
			 * EAGAIN.
			 */
			ret = zpool_power_off(zhp, argv[i]);
			if (ret != 0) {
				(void) fprintf(stderr, "%s %s %d\n",
				    gettext("unable to power off slot for"),
				    argv[i], ret);
			}
			zpool_vdev_set_removed_state(zhp, guid, VDEV_AUX_NONE);

		} else if (fault) {
			vdev_aux_t aux;
			if (istmp == B_FALSE) {
				/* Force the fault to persist across imports */
				aux = VDEV_AUX_EXTERNAL_PERSIST;
			} else {
				aux = VDEV_AUX_EXTERNAL;
			}

			if (guid == 0 || zpool_vdev_fault(zhp, guid, aux) != 0)
				ret = 1;
		} else {
			if (zpool_vdev_offline(zhp, argv[i], istmp) != 0)
				ret = 1;
		}
	}

	zpool_close(zhp);

	return (ret);
}

/*
 * zpool clear [-nF]|[--power] <pool> [device]
 *
 * Clear all errors associated with a pool or a particular device.
 */
int
zpool_do_clear(int argc, char **argv)
{
	int c;
	int ret = 0;
	boolean_t dryrun = B_FALSE;
	boolean_t do_rewind = B_FALSE;
	boolean_t xtreme_rewind = B_FALSE;
	boolean_t is_power_on = B_FALSE;
	uint32_t rewind_policy = ZPOOL_NO_REWIND;
	nvlist_t *policy = NULL;
	zpool_handle_t *zhp;
	char *pool, *device;

	struct option long_options[] = {
		{"power", no_argument, NULL, ZPOOL_OPTION_POWER},
		{0, 0, 0, 0}
	};

	/* check options */
	while ((c = getopt_long(argc, argv, "FnX", long_options,
	    NULL)) != -1) {
		switch (c) {
		case 'F':
			do_rewind = B_TRUE;
			break;
		case 'n':
			dryrun = B_TRUE;
			break;
		case 'X':
			xtreme_rewind = B_TRUE;
			break;
		case ZPOOL_OPTION_POWER:
			is_power_on = B_TRUE;
			break;
		case '?':
			(void) fprintf(stderr, gettext("invalid option '%c'\n"),
			    optopt);
			usage(B_FALSE);
		}
	}

	if (libzfs_envvar_is_set("ZPOOL_AUTO_POWER_ON_SLOT"))
		is_power_on = B_TRUE;

	argc -= optind;
	argv += optind;

	if (argc < 1) {
		(void) fprintf(stderr, gettext("missing pool name\n"));
		usage(B_FALSE);
	}

	if (argc > 2) {
		(void) fprintf(stderr, gettext("too many arguments\n"));
		usage(B_FALSE);
	}

	if ((dryrun || xtreme_rewind) && !do_rewind) {
		(void) fprintf(stderr,
		    gettext("-n or -X only meaningful with -F\n"));
		usage(B_FALSE);
	}
	if (dryrun)
		rewind_policy = ZPOOL_TRY_REWIND;
	else if (do_rewind)
		rewind_policy = ZPOOL_DO_REWIND;
	if (xtreme_rewind)
		rewind_policy |= ZPOOL_EXTREME_REWIND;

	/* In future, further rewind policy choices can be passed along here */
	if (nvlist_alloc(&policy, NV_UNIQUE_NAME, 0) != 0 ||
	    nvlist_add_uint32(policy, ZPOOL_LOAD_REWIND_POLICY,
	    rewind_policy) != 0) {
		return (1);
	}

	pool = argv[0];
	device = argc == 2 ? argv[1] : NULL;

	if ((zhp = zpool_open_canfail(g_zfs, pool)) == NULL) {
		nvlist_free(policy);
		return (1);
	}

	if (is_power_on) {
		if (device == NULL) {
			zpool_power_on_pool_and_wait_for_devices(zhp);
		} else {
			zpool_power_on_and_disk_wait(zhp, device);
		}
	}

	if (zpool_clear(zhp, device, policy) != 0)
		ret = 1;

	zpool_close(zhp);

	nvlist_free(policy);

	return (ret);
}

/*
 * zpool reguid [-g <guid>] <pool>
 */
int
zpool_do_reguid(int argc, char **argv)
{
	uint64_t guid;
	uint64_t *guidp = NULL;
	int c;
	char *endptr;
	char *poolname;
	zpool_handle_t *zhp;
	int ret = 0;

	/* check options */
	while ((c = getopt(argc, argv, "g:")) != -1) {
		switch (c) {
		case 'g':
			errno = 0;
			guid = strtoull(optarg, &endptr, 10);
			if (errno != 0 || *endptr != '\0') {
				(void) fprintf(stderr,
				    gettext("invalid GUID: %s\n"), optarg);
				usage(B_FALSE);
			}
			guidp = &guid;
			break;
		case '?':
			(void) fprintf(stderr, gettext("invalid option '%c'\n"),
			    optopt);
			usage(B_FALSE);
		}
	}

	argc -= optind;
	argv += optind;

	/* get pool name and check number of arguments */
	if (argc < 1) {
		(void) fprintf(stderr, gettext("missing pool name\n"));
		usage(B_FALSE);
	}

	if (argc > 1) {
		(void) fprintf(stderr, gettext("too many arguments\n"));
		usage(B_FALSE);
	}

	poolname = argv[0];
	if ((zhp = zpool_open(g_zfs, poolname)) == NULL)
		return (1);

	ret = zpool_set_guid(zhp, guidp);

	zpool_close(zhp);
	return (ret);
}


/*
 * zpool reopen <pool>
 *
 * Reopen the pool so that the kernel can update the sizes of all vdevs.
 */
int
zpool_do_reopen(int argc, char **argv)
{
	int c;
	int ret = 0;
	boolean_t scrub_restart = B_TRUE;

	/* check options */
	while ((c = getopt(argc, argv, "n")) != -1) {
		switch (c) {
		case 'n':
			scrub_restart = B_FALSE;
			break;
		case '?':
			(void) fprintf(stderr, gettext("invalid option '%c'\n"),
			    optopt);
			usage(B_FALSE);
		}
	}

	argc -= optind;
	argv += optind;

	/* if argc == 0 we will execute zpool_reopen_one on all pools */
	ret = for_each_pool(argc, argv, B_TRUE, NULL, ZFS_TYPE_POOL,
	    B_FALSE, zpool_reopen_one, &scrub_restart);

	return (ret);
}

typedef struct scrub_cbdata {
	int	cb_type;
	pool_scrub_cmd_t cb_scrub_cmd;
} scrub_cbdata_t;

static boolean_t
zpool_has_checkpoint(zpool_handle_t *zhp)
{
	nvlist_t *config, *nvroot;

	config = zpool_get_config(zhp, NULL);

	if (config != NULL) {
		pool_checkpoint_stat_t *pcs = NULL;
		uint_t c;

		nvroot = fnvlist_lookup_nvlist(config, ZPOOL_CONFIG_VDEV_TREE);
		(void) nvlist_lookup_uint64_array(nvroot,
		    ZPOOL_CONFIG_CHECKPOINT_STATS, (uint64_t **)&pcs, &c);

		if (pcs == NULL || pcs->pcs_state == CS_NONE)
			return (B_FALSE);

		assert(pcs->pcs_state == CS_CHECKPOINT_EXISTS ||
		    pcs->pcs_state == CS_CHECKPOINT_DISCARDING);
		return (B_TRUE);
	}

	return (B_FALSE);
}

static int
scrub_callback(zpool_handle_t *zhp, void *data)
{
	scrub_cbdata_t *cb = data;
	int err;

	/*
	 * Ignore faulted pools.
	 */
	if (zpool_get_state(zhp) == POOL_STATE_UNAVAIL) {
		(void) fprintf(stderr, gettext("cannot scan '%s': pool is "
		    "currently unavailable\n"), zpool_get_name(zhp));
		return (1);
	}

	err = zpool_scan(zhp, cb->cb_type, cb->cb_scrub_cmd);

	if (err == 0 && zpool_has_checkpoint(zhp) &&
	    cb->cb_type == POOL_SCAN_SCRUB) {
		(void) printf(gettext("warning: will not scrub state that "
		    "belongs to the checkpoint of pool '%s'\n"),
		    zpool_get_name(zhp));
	}

	return (err != 0);
}

static int
wait_callback(zpool_handle_t *zhp, void *data)
{
	zpool_wait_activity_t *act = data;
	return (zpool_wait(zhp, *act));
}

/*
 * zpool scrub [-e | -s | -p | -C] [-w] <pool> ...
 *
 *	-e	Only scrub blocks in the error log.
 *	-s	Stop.  Stops any in-progress scrub.
 *	-p	Pause. Pause in-progress scrub.
 *	-w	Wait.  Blocks until scrub has completed.
 *	-C	Scrub from last saved txg.
 */
int
zpool_do_scrub(int argc, char **argv)
{
	int c;
	scrub_cbdata_t cb;
	boolean_t wait = B_FALSE;
	int error;

	cb.cb_type = POOL_SCAN_SCRUB;
	cb.cb_scrub_cmd = POOL_SCRUB_NORMAL;

	boolean_t is_error_scrub = B_FALSE;
	boolean_t is_pause = B_FALSE;
	boolean_t is_stop = B_FALSE;
	boolean_t is_txg_continue = B_FALSE;
	boolean_t scrub_all = B_FALSE;

	/* check options */
	while ((c = getopt(argc, argv, "aspweC")) != -1) {
		switch (c) {
		case 'a':
			scrub_all = B_TRUE;
			break;
		case 'e':
			is_error_scrub = B_TRUE;
			break;
		case 's':
			is_stop = B_TRUE;
			break;
		case 'p':
			is_pause = B_TRUE;
			break;
		case 'w':
			wait = B_TRUE;
			break;
		case 'C':
			is_txg_continue = B_TRUE;
			break;
		case '?':
			(void) fprintf(stderr, gettext("invalid option '%c'\n"),
			    optopt);
			usage(B_FALSE);
		}
	}

	if (is_pause && is_stop) {
		(void) fprintf(stderr, gettext("invalid option "
		    "combination: -s and -p are mutually exclusive\n"));
		usage(B_FALSE);
	} else if (is_pause && is_txg_continue) {
		(void) fprintf(stderr, gettext("invalid option "
		    "combination: -p and -C are mutually exclusive\n"));
		usage(B_FALSE);
	} else if (is_stop && is_txg_continue) {
		(void) fprintf(stderr, gettext("invalid option "
		    "combination: -s and -C are mutually exclusive\n"));
		usage(B_FALSE);
	} else if (is_error_scrub && is_txg_continue) {
		(void) fprintf(stderr, gettext("invalid option "
		    "combination: -e and -C are mutually exclusive\n"));
		usage(B_FALSE);
	} else {
		if (is_error_scrub)
			cb.cb_type = POOL_SCAN_ERRORSCRUB;

		if (is_pause) {
			cb.cb_scrub_cmd = POOL_SCRUB_PAUSE;
		} else if (is_stop) {
			cb.cb_type = POOL_SCAN_NONE;
		} else if (is_txg_continue) {
			cb.cb_scrub_cmd = POOL_SCRUB_FROM_LAST_TXG;
		} else {
			cb.cb_scrub_cmd = POOL_SCRUB_NORMAL;
		}
	}

	if (wait && (cb.cb_type == POOL_SCAN_NONE ||
	    cb.cb_scrub_cmd == POOL_SCRUB_PAUSE)) {
		(void) fprintf(stderr, gettext("invalid option combination: "
		    "-w cannot be used with -p or -s\n"));
		usage(B_FALSE);
	}

	argc -= optind;
	argv += optind;

	if (argc < 1 && !scrub_all) {
		(void) fprintf(stderr, gettext("missing pool name argument\n"));
		usage(B_FALSE);
	}

	error = for_each_pool(argc, argv, B_TRUE, NULL, ZFS_TYPE_POOL,
	    B_FALSE, scrub_callback, &cb);

	if (wait && !error) {
		zpool_wait_activity_t act = ZPOOL_WAIT_SCRUB;
		error = for_each_pool(argc, argv, B_TRUE, NULL, ZFS_TYPE_POOL,
		    B_FALSE, wait_callback, &act);
	}

	return (error);
}

/*
 * zpool resilver <pool> ...
 *
 *	Restarts any in-progress resilver
 */
int
zpool_do_resilver(int argc, char **argv)
{
	int c;
	scrub_cbdata_t cb;

	cb.cb_type = POOL_SCAN_RESILVER;
	cb.cb_scrub_cmd = POOL_SCRUB_NORMAL;

	/* check options */
	while ((c = getopt(argc, argv, "")) != -1) {
		switch (c) {
		case '?':
			(void) fprintf(stderr, gettext("invalid option '%c'\n"),
			    optopt);
			usage(B_FALSE);
		}
	}

	argc -= optind;
	argv += optind;

	if (argc < 1) {
		(void) fprintf(stderr, gettext("missing pool name argument\n"));
		usage(B_FALSE);
	}

	return (for_each_pool(argc, argv, B_TRUE, NULL, ZFS_TYPE_POOL,
	    B_FALSE, scrub_callback, &cb));
}

/*
 * zpool trim [-d] [-r <rate>] [-c | -s] <pool> [<device> ...]
 *
 *	-c		Cancel. Ends any in-progress trim.
 *	-d		Secure trim.  Requires kernel and device support.
 *	-r <rate>	Sets the TRIM rate in bytes (per second). Supports
 *			adding a multiplier suffix such as 'k' or 'm'.
 *	-s		Suspend. TRIM can then be restarted with no flags.
 *	-w		Wait. Blocks until trimming has completed.
 */
int
zpool_do_trim(int argc, char **argv)
{
	struct option long_options[] = {
		{"cancel",	no_argument,		NULL,	'c'},
		{"secure",	no_argument,		NULL,	'd'},
		{"rate",	required_argument,	NULL,	'r'},
		{"suspend",	no_argument,		NULL,	's'},
		{"wait",	no_argument,		NULL,	'w'},
		{"all",		no_argument,		NULL,	'a'},
		{0, 0, 0, 0}
	};

	pool_trim_func_t cmd_type = POOL_TRIM_START;
	uint64_t rate = 0;
	boolean_t secure = B_FALSE;
	boolean_t wait = B_FALSE;
	boolean_t trimall = B_FALSE;
	int error;

	int c;
	while ((c = getopt_long(argc, argv, "acdr:sw", long_options, NULL))
	    != -1) {
		switch (c) {
		case 'a':
			trimall = B_TRUE;
			break;
		case 'c':
			if (cmd_type != POOL_TRIM_START &&
			    cmd_type != POOL_TRIM_CANCEL) {
				(void) fprintf(stderr, gettext("-c cannot be "
				    "combined with other options\n"));
				usage(B_FALSE);
			}
			cmd_type = POOL_TRIM_CANCEL;
			break;
		case 'd':
			if (cmd_type != POOL_TRIM_START) {
				(void) fprintf(stderr, gettext("-d cannot be "
				    "combined with the -c or -s options\n"));
				usage(B_FALSE);
			}
			secure = B_TRUE;
			break;
		case 'r':
			if (cmd_type != POOL_TRIM_START) {
				(void) fprintf(stderr, gettext("-r cannot be "
				    "combined with the -c or -s options\n"));
				usage(B_FALSE);
			}
			if (zfs_nicestrtonum(g_zfs, optarg, &rate) == -1) {
				(void) fprintf(stderr, "%s: %s\n",
				    gettext("invalid value for rate"),
				    libzfs_error_description(g_zfs));
				usage(B_FALSE);
			}
			break;
		case 's':
			if (cmd_type != POOL_TRIM_START &&
			    cmd_type != POOL_TRIM_SUSPEND) {
				(void) fprintf(stderr, gettext("-s cannot be "
				    "combined with other options\n"));
				usage(B_FALSE);
			}
			cmd_type = POOL_TRIM_SUSPEND;
			break;
		case 'w':
			wait = B_TRUE;
			break;
		case '?':
			if (optopt != 0) {
				(void) fprintf(stderr,
				    gettext("invalid option '%c'\n"), optopt);
			} else {
				(void) fprintf(stderr,
				    gettext("invalid option '%s'\n"),
				    argv[optind - 1]);
			}
			usage(B_FALSE);
		}
	}

	argc -= optind;
	argv += optind;

	trimflags_t trim_flags = {
		.secure = secure,
		.rate = rate,
		.wait = wait,
	};

	trim_cbdata_t cbdata = {
		.trim_flags = trim_flags,
		.cmd_type = cmd_type
	};

	if (argc < 1 && !trimall) {
		(void) fprintf(stderr, gettext("missing pool name argument\n"));
		usage(B_FALSE);
		return (-1);
	}

	if (wait && (cmd_type != POOL_TRIM_START)) {
		(void) fprintf(stderr, gettext("-w cannot be used with -c or "
		    "-s options\n"));
		usage(B_FALSE);
	}

	if (trimall && argc > 0) {
		(void) fprintf(stderr, gettext("-a cannot be combined with "
		    "individual zpools or vdevs\n"));
		usage(B_FALSE);
	}

	if (argc == 0 && trimall) {
		cbdata.trim_flags.fullpool = B_TRUE;
		/* Trim each pool recursively */
		error = for_each_pool(argc, argv, B_TRUE, NULL, ZFS_TYPE_POOL,
		    B_FALSE, zpool_trim_one, &cbdata);
	} else if (argc == 1) {
		char *poolname = argv[0];
		zpool_handle_t *zhp = zpool_open(g_zfs, poolname);
		if (zhp == NULL)
			return (-1);
		/* no individual leaf vdevs specified, so add them all */
		error = zpool_trim_one(zhp, &cbdata);
		zpool_close(zhp);
	} else {
		char *poolname = argv[0];
		zpool_handle_t *zhp = zpool_open(g_zfs, poolname);
		if (zhp == NULL)
			return (-1);
		/* leaf vdevs specified, trim only those */
		cbdata.trim_flags.fullpool = B_FALSE;
		nvlist_t *vdevs = fnvlist_alloc();
		for (int i = 1; i < argc; i++) {
			fnvlist_add_boolean(vdevs, argv[i]);
		}
		error = zpool_trim(zhp, cbdata.cmd_type, vdevs,
		    &cbdata.trim_flags);
		fnvlist_free(vdevs);
		zpool_close(zhp);
	}

	return (error);
}

/*
 * Converts a total number of seconds to a human readable string broken
 * down in to days/hours/minutes/seconds.
 */
static void
secs_to_dhms(uint64_t total, char *buf)
{
	uint64_t days = total / 60 / 60 / 24;
	uint64_t hours = (total / 60 / 60) % 24;
	uint64_t mins = (total / 60) % 60;
	uint64_t secs = (total % 60);

	if (days > 0) {
		(void) sprintf(buf, "%llu days %02llu:%02llu:%02llu",
		    (u_longlong_t)days, (u_longlong_t)hours,
		    (u_longlong_t)mins, (u_longlong_t)secs);
	} else {
		(void) sprintf(buf, "%02llu:%02llu:%02llu",
		    (u_longlong_t)hours, (u_longlong_t)mins,
		    (u_longlong_t)secs);
	}
}

/*
 * Print out detailed error scrub status.
 */
static void
print_err_scrub_status(pool_scan_stat_t *ps)
{
	time_t start, end, pause;
	uint64_t total_secs_left;
	uint64_t secs_left, mins_left, hours_left, days_left;
	uint64_t examined, to_be_examined;

	if (ps == NULL || ps->pss_error_scrub_func != POOL_SCAN_ERRORSCRUB) {
		return;
	}

	(void) printf(gettext(" scrub: "));

	start = ps->pss_error_scrub_start;
	end = ps->pss_error_scrub_end;
	pause = ps->pss_pass_error_scrub_pause;
	examined = ps->pss_error_scrub_examined;
	to_be_examined = ps->pss_error_scrub_to_be_examined;

	assert(ps->pss_error_scrub_func == POOL_SCAN_ERRORSCRUB);

	if (ps->pss_error_scrub_state == DSS_FINISHED) {
		total_secs_left = end - start;
		days_left = total_secs_left / 60 / 60 / 24;
		hours_left = (total_secs_left / 60 / 60) % 24;
		mins_left = (total_secs_left / 60) % 60;
		secs_left = (total_secs_left % 60);

		(void) printf(gettext("scrubbed %llu error blocks in %llu days "
		    "%02llu:%02llu:%02llu on %s"), (u_longlong_t)examined,
		    (u_longlong_t)days_left, (u_longlong_t)hours_left,
		    (u_longlong_t)mins_left, (u_longlong_t)secs_left,
		    ctime(&end));

		return;
	} else if (ps->pss_error_scrub_state == DSS_CANCELED) {
		(void) printf(gettext("error scrub canceled on %s"),
		    ctime(&end));
		return;
	}
	assert(ps->pss_error_scrub_state == DSS_ERRORSCRUBBING);

	/* Error scrub is in progress. */
	if (pause == 0) {
		(void) printf(gettext("error scrub in progress since %s"),
		    ctime(&start));
	} else {
		(void) printf(gettext("error scrub paused since %s"),
		    ctime(&pause));
		(void) printf(gettext("\terror scrub started on %s"),
		    ctime(&start));
	}

	double fraction_done = (double)examined / (to_be_examined + examined);
	(void) printf(gettext("\t%.2f%% done, issued I/O for %llu error"
	    " blocks"), 100 * fraction_done, (u_longlong_t)examined);

	(void) printf("\n");
}

/*
 * Print out detailed scrub status.
 */
static void
print_scan_scrub_resilver_status(pool_scan_stat_t *ps)
{
	time_t start, end, pause;
	uint64_t pass_scanned, scanned, pass_issued, issued, total_s, total_i;
	uint64_t elapsed, scan_rate, issue_rate;
	double fraction_done;
	char processed_buf[7], scanned_buf[7], issued_buf[7], total_s_buf[7];
	char total_i_buf[7], srate_buf[7], irate_buf[7], time_buf[32];

	printf("  ");
	printf_color(ANSI_BOLD, gettext("scan:"));
	printf(" ");

	/* If there's never been a scan, there's not much to say. */
	if (ps == NULL || ps->pss_func == POOL_SCAN_NONE ||
	    ps->pss_func >= POOL_SCAN_FUNCS) {
		(void) printf(gettext("none requested\n"));
		return;
	}

	start = ps->pss_start_time;
	end = ps->pss_end_time;
	pause = ps->pss_pass_scrub_pause;

	zfs_nicebytes(ps->pss_processed, processed_buf, sizeof (processed_buf));

	int is_resilver = ps->pss_func == POOL_SCAN_RESILVER;
	int is_scrub = ps->pss_func == POOL_SCAN_SCRUB;
	assert(is_resilver || is_scrub);

	/* Scan is finished or canceled. */
	if (ps->pss_state == DSS_FINISHED) {
		secs_to_dhms(end - start, time_buf);

		if (is_scrub) {
			(void) printf(gettext("scrub repaired %s "
			    "in %s with %llu errors on %s"), processed_buf,
			    time_buf, (u_longlong_t)ps->pss_errors,
			    ctime(&end));
		} else if (is_resilver) {
			(void) printf(gettext("resilvered %s "
			    "in %s with %llu errors on %s"), processed_buf,
			    time_buf, (u_longlong_t)ps->pss_errors,
			    ctime(&end));
		}
		return;
	} else if (ps->pss_state == DSS_CANCELED) {
		if (is_scrub) {
			(void) printf(gettext("scrub canceled on %s"),
			    ctime(&end));
		} else if (is_resilver) {
			(void) printf(gettext("resilver canceled on %s"),
			    ctime(&end));
		}
		return;
	}

	assert(ps->pss_state == DSS_SCANNING);

	/* Scan is in progress. Resilvers can't be paused. */
	if (is_scrub) {
		if (pause == 0) {
			(void) printf(gettext("scrub in progress since %s"),
			    ctime(&start));
		} else {
			(void) printf(gettext("scrub paused since %s"),
			    ctime(&pause));
			(void) printf(gettext("\tscrub started on %s"),
			    ctime(&start));
		}
	} else if (is_resilver) {
		(void) printf(gettext("resilver in progress since %s"),
		    ctime(&start));
	}

	scanned = ps->pss_examined;
	pass_scanned = ps->pss_pass_exam;
	issued = ps->pss_issued;
	pass_issued = ps->pss_pass_issued;
	total_s = ps->pss_to_examine;
	total_i = ps->pss_to_examine - ps->pss_skipped;

	/* we are only done with a block once we have issued the IO for it */
	fraction_done = (double)issued / total_i;

	/* elapsed time for this pass, rounding up to 1 if it's 0 */
	elapsed = time(NULL) - ps->pss_pass_start;
	elapsed -= ps->pss_pass_scrub_spent_paused;
	elapsed = (elapsed != 0) ? elapsed : 1;

	scan_rate = pass_scanned / elapsed;
	issue_rate = pass_issued / elapsed;

	/* format all of the numbers we will be reporting */
	zfs_nicebytes(scanned, scanned_buf, sizeof (scanned_buf));
	zfs_nicebytes(issued, issued_buf, sizeof (issued_buf));
	zfs_nicebytes(total_s, total_s_buf, sizeof (total_s_buf));
	zfs_nicebytes(total_i, total_i_buf, sizeof (total_i_buf));

	/* do not print estimated time if we have a paused scrub */
	(void) printf(gettext("\t%s / %s scanned"), scanned_buf, total_s_buf);
	if (pause == 0 && scan_rate > 0) {
		zfs_nicebytes(scan_rate, srate_buf, sizeof (srate_buf));
		(void) printf(gettext(" at %s/s"), srate_buf);
	}
	(void) printf(gettext(", %s / %s issued"), issued_buf, total_i_buf);
	if (pause == 0 && issue_rate > 0) {
		zfs_nicebytes(issue_rate, irate_buf, sizeof (irate_buf));
		(void) printf(gettext(" at %s/s"), irate_buf);
	}
	(void) printf(gettext("\n"));

	if (is_resilver) {
		(void) printf(gettext("\t%s resilvered, %.2f%% done"),
		    processed_buf, 100 * fraction_done);
	} else if (is_scrub) {
		(void) printf(gettext("\t%s repaired, %.2f%% done"),
		    processed_buf, 100 * fraction_done);
	}

	if (pause == 0) {
		/*
		 * Only provide an estimate iff:
		 * 1) we haven't yet issued all we expected, and
		 * 2) the issue rate exceeds 10 MB/s, and
		 * 3) it's either:
		 *    a) a resilver which has started repairs, or
		 *    b) a scrub which has entered the issue phase.
		 */
		if (total_i >= issued && issue_rate >= 10 * 1024 * 1024 &&
		    ((is_resilver && ps->pss_processed > 0) ||
		    (is_scrub && issued > 0))) {
			secs_to_dhms((total_i - issued) / issue_rate, time_buf);
			(void) printf(gettext(", %s to go\n"), time_buf);
		} else {
			(void) printf(gettext(", no estimated "
			    "completion time\n"));
		}
	} else {
		(void) printf(gettext("\n"));
	}
}

static void
print_rebuild_status_impl(vdev_rebuild_stat_t *vrs, uint_t c, char *vdev_name)
{
	if (vrs == NULL || vrs->vrs_state == VDEV_REBUILD_NONE)
		return;

	printf("  ");
	printf_color(ANSI_BOLD, gettext("scan:"));
	printf(" ");

	uint64_t bytes_scanned = vrs->vrs_bytes_scanned;
	uint64_t bytes_issued = vrs->vrs_bytes_issued;
	uint64_t bytes_rebuilt = vrs->vrs_bytes_rebuilt;
	uint64_t bytes_est_s = vrs->vrs_bytes_est;
	uint64_t bytes_est_i = vrs->vrs_bytes_est;
	if (c > offsetof(vdev_rebuild_stat_t, vrs_pass_bytes_skipped) / 8)
		bytes_est_i -= vrs->vrs_pass_bytes_skipped;
	uint64_t scan_rate = (vrs->vrs_pass_bytes_scanned /
	    (vrs->vrs_pass_time_ms + 1)) * 1000;
	uint64_t issue_rate = (vrs->vrs_pass_bytes_issued /
	    (vrs->vrs_pass_time_ms + 1)) * 1000;
	double scan_pct = MIN((double)bytes_scanned * 100 /
	    (bytes_est_s + 1), 100);

	/* Format all of the numbers we will be reporting */
	char bytes_scanned_buf[7], bytes_issued_buf[7];
	char bytes_rebuilt_buf[7], bytes_est_s_buf[7], bytes_est_i_buf[7];
	char scan_rate_buf[7], issue_rate_buf[7], time_buf[32];
	zfs_nicebytes(bytes_scanned, bytes_scanned_buf,
	    sizeof (bytes_scanned_buf));
	zfs_nicebytes(bytes_issued, bytes_issued_buf,
	    sizeof (bytes_issued_buf));
	zfs_nicebytes(bytes_rebuilt, bytes_rebuilt_buf,
	    sizeof (bytes_rebuilt_buf));
	zfs_nicebytes(bytes_est_s, bytes_est_s_buf, sizeof (bytes_est_s_buf));
	zfs_nicebytes(bytes_est_i, bytes_est_i_buf, sizeof (bytes_est_i_buf));

	time_t start = vrs->vrs_start_time;
	time_t end = vrs->vrs_end_time;

	/* Rebuild is finished or canceled. */
	if (vrs->vrs_state == VDEV_REBUILD_COMPLETE) {
		secs_to_dhms(vrs->vrs_scan_time_ms / 1000, time_buf);
		(void) printf(gettext("resilvered (%s) %s in %s "
		    "with %llu errors on %s"), vdev_name, bytes_rebuilt_buf,
		    time_buf, (u_longlong_t)vrs->vrs_errors, ctime(&end));
		return;
	} else if (vrs->vrs_state == VDEV_REBUILD_CANCELED) {
		(void) printf(gettext("resilver (%s) canceled on %s"),
		    vdev_name, ctime(&end));
		return;
	} else if (vrs->vrs_state == VDEV_REBUILD_ACTIVE) {
		(void) printf(gettext("resilver (%s) in progress since %s"),
		    vdev_name, ctime(&start));
	}

	assert(vrs->vrs_state == VDEV_REBUILD_ACTIVE);

	(void) printf(gettext("\t%s / %s scanned"), bytes_scanned_buf,
	    bytes_est_s_buf);
	if (scan_rate > 0) {
		zfs_nicebytes(scan_rate, scan_rate_buf, sizeof (scan_rate_buf));
		(void) printf(gettext(" at %s/s"), scan_rate_buf);
	}
	(void) printf(gettext(", %s / %s issued"), bytes_issued_buf,
	    bytes_est_i_buf);
	if (issue_rate > 0) {
		zfs_nicebytes(issue_rate, issue_rate_buf,
		    sizeof (issue_rate_buf));
		(void) printf(gettext(" at %s/s"), issue_rate_buf);
	}
	(void) printf(gettext("\n"));

	(void) printf(gettext("\t%s resilvered, %.2f%% done"),
	    bytes_rebuilt_buf, scan_pct);

	if (vrs->vrs_state == VDEV_REBUILD_ACTIVE) {
		if (bytes_est_s >= bytes_scanned &&
		    scan_rate >= 10 * 1024 * 1024) {
			secs_to_dhms((bytes_est_s - bytes_scanned) / scan_rate,
			    time_buf);
			(void) printf(gettext(", %s to go\n"), time_buf);
		} else {
			(void) printf(gettext(", no estimated "
			    "completion time\n"));
		}
	} else {
		(void) printf(gettext("\n"));
	}
}

/*
 * Print rebuild status for top-level vdevs.
 */
static void
print_rebuild_status(zpool_handle_t *zhp, nvlist_t *nvroot)
{
	nvlist_t **child;
	uint_t children;

	if (nvlist_lookup_nvlist_array(nvroot, ZPOOL_CONFIG_CHILDREN,
	    &child, &children) != 0)
		children = 0;

	for (uint_t c = 0; c < children; c++) {
		vdev_rebuild_stat_t *vrs;
		uint_t i;

		if (nvlist_lookup_uint64_array(child[c],
		    ZPOOL_CONFIG_REBUILD_STATS, (uint64_t **)&vrs, &i) == 0) {
			char *name = zpool_vdev_name(g_zfs, zhp,
			    child[c], VDEV_NAME_TYPE_ID);
			print_rebuild_status_impl(vrs, i, name);
			free(name);
		}
	}
}

/*
 * As we don't scrub checkpointed blocks, we want to warn the user that we
 * skipped scanning some blocks if a checkpoint exists or existed at any
 * time during the scan.  If a sequential instead of healing reconstruction
 * was performed then the blocks were reconstructed.  However, their checksums
 * have not been verified so we still print the warning.
 */
static void
print_checkpoint_scan_warning(pool_scan_stat_t *ps, pool_checkpoint_stat_t *pcs)
{
	if (ps == NULL || pcs == NULL)
		return;

	if (pcs->pcs_state == CS_NONE ||
	    pcs->pcs_state == CS_CHECKPOINT_DISCARDING)
		return;

	assert(pcs->pcs_state == CS_CHECKPOINT_EXISTS);

	if (ps->pss_state == DSS_NONE)
		return;

	if ((ps->pss_state == DSS_FINISHED || ps->pss_state == DSS_CANCELED) &&
	    ps->pss_end_time < pcs->pcs_start_time)
		return;

	if (ps->pss_state == DSS_FINISHED || ps->pss_state == DSS_CANCELED) {
		(void) printf(gettext("    scan warning: skipped blocks "
		    "that are only referenced by the checkpoint.\n"));
	} else {
		assert(ps->pss_state == DSS_SCANNING);
		(void) printf(gettext("    scan warning: skipping blocks "
		    "that are only referenced by the checkpoint.\n"));
	}
}

/*
 * Returns B_TRUE if there is an active rebuild in progress.  Otherwise,
 * B_FALSE is returned and 'rebuild_end_time' is set to the end time for
 * the last completed (or cancelled) rebuild.
 */
static boolean_t
check_rebuilding(nvlist_t *nvroot, uint64_t *rebuild_end_time)
{
	nvlist_t **child;
	uint_t children;
	boolean_t rebuilding = B_FALSE;
	uint64_t end_time = 0;

	if (nvlist_lookup_nvlist_array(nvroot, ZPOOL_CONFIG_CHILDREN,
	    &child, &children) != 0)
		children = 0;

	for (uint_t c = 0; c < children; c++) {
		vdev_rebuild_stat_t *vrs;
		uint_t i;

		if (nvlist_lookup_uint64_array(child[c],
		    ZPOOL_CONFIG_REBUILD_STATS, (uint64_t **)&vrs, &i) == 0) {

			if (vrs->vrs_end_time > end_time)
				end_time = vrs->vrs_end_time;

			if (vrs->vrs_state == VDEV_REBUILD_ACTIVE) {
				rebuilding = B_TRUE;
				end_time = 0;
				break;
			}
		}
	}

	if (rebuild_end_time != NULL)
		*rebuild_end_time = end_time;

	return (rebuilding);
}

static void
vdev_stats_nvlist(zpool_handle_t *zhp, status_cbdata_t *cb, nvlist_t *nv,
    int depth, boolean_t isspare, char *parent, nvlist_t *item)
{
	nvlist_t *vds, **child, *ch = NULL;
	uint_t vsc, children;
	vdev_stat_t *vs;
	char *vname;
	uint64_t notpresent;
	const char *type, *path;

	if (nvlist_lookup_nvlist_array(nv, ZPOOL_CONFIG_CHILDREN,
	    &child, &children) != 0)
		children = 0;
	verify(nvlist_lookup_uint64_array(nv, ZPOOL_CONFIG_VDEV_STATS,
	    (uint64_t **)&vs, &vsc) == 0);
	verify(nvlist_lookup_string(nv, ZPOOL_CONFIG_TYPE, &type) == 0);
	if (strcmp(type, VDEV_TYPE_INDIRECT) == 0)
		return;

	if (cb->cb_print_unhealthy && depth > 0 &&
	    for_each_vdev_in_nvlist(nv, vdev_health_check_cb, cb) == 0) {
		return;
	}
	vname = zpool_vdev_name(g_zfs, zhp, nv,
	    cb->cb_name_flags | VDEV_NAME_TYPE_ID);
	vds = fnvlist_alloc();
	fill_vdev_info(vds, zhp, vname, B_FALSE, cb->cb_json_as_int);
	if (cb->cb_flat_vdevs && parent != NULL) {
		fnvlist_add_string(vds, "parent", parent);
	}

	if (isspare) {
		if (vs->vs_aux == VDEV_AUX_SPARED) {
			fnvlist_add_string(vds, "state", "INUSE");
			used_by_other(zhp, nv, vds);
		} else if (vs->vs_state == VDEV_STATE_HEALTHY)
			fnvlist_add_string(vds, "state", "AVAIL");
	} else {
		if (vs->vs_alloc) {
			nice_num_str_nvlist(vds, "alloc_space", vs->vs_alloc,
			    cb->cb_literal, cb->cb_json_as_int,
			    ZFS_NICENUM_BYTES);
		}
		if (vs->vs_space) {
			nice_num_str_nvlist(vds, "total_space", vs->vs_space,
			    cb->cb_literal, cb->cb_json_as_int,
			    ZFS_NICENUM_BYTES);
		}
		if (vs->vs_dspace) {
			nice_num_str_nvlist(vds, "def_space", vs->vs_dspace,
			    cb->cb_literal, cb->cb_json_as_int,
			    ZFS_NICENUM_BYTES);
		}
		if (vs->vs_rsize) {
			nice_num_str_nvlist(vds, "rep_dev_size", vs->vs_rsize,
			    cb->cb_literal, cb->cb_json_as_int,
			    ZFS_NICENUM_BYTES);
		}
		if (vs->vs_esize) {
			nice_num_str_nvlist(vds, "ex_dev_size", vs->vs_esize,
			    cb->cb_literal, cb->cb_json_as_int,
			    ZFS_NICENUM_BYTES);
		}
		if (vs->vs_self_healed) {
			nice_num_str_nvlist(vds, "self_healed",
			    vs->vs_self_healed, cb->cb_literal,
			    cb->cb_json_as_int, ZFS_NICENUM_BYTES);
		}
		if (vs->vs_pspace) {
			nice_num_str_nvlist(vds, "phys_space", vs->vs_pspace,
			    cb->cb_literal, cb->cb_json_as_int,
			    ZFS_NICENUM_BYTES);
		}
		nice_num_str_nvlist(vds, "read_errors", vs->vs_read_errors,
		    cb->cb_literal, cb->cb_json_as_int, ZFS_NICENUM_1024);
		nice_num_str_nvlist(vds, "write_errors", vs->vs_write_errors,
		    cb->cb_literal, cb->cb_json_as_int, ZFS_NICENUM_1024);
		nice_num_str_nvlist(vds, "checksum_errors",
		    vs->vs_checksum_errors, cb->cb_literal,
		    cb->cb_json_as_int, ZFS_NICENUM_1024);
		if (vs->vs_scan_processed) {
			nice_num_str_nvlist(vds, "scan_processed",
			    vs->vs_scan_processed, cb->cb_literal,
			    cb->cb_json_as_int, ZFS_NICENUM_BYTES);
		}
		if (vs->vs_checkpoint_space) {
			nice_num_str_nvlist(vds, "checkpoint_space",
			    vs->vs_checkpoint_space, cb->cb_literal,
			    cb->cb_json_as_int, ZFS_NICENUM_BYTES);
		}
		if (vs->vs_resilver_deferred) {
			nice_num_str_nvlist(vds, "resilver_deferred",
			    vs->vs_resilver_deferred, B_TRUE,
			    cb->cb_json_as_int, ZFS_NICENUM_1024);
		}
		if (children == 0) {
			nice_num_str_nvlist(vds, "slow_ios", vs->vs_slow_ios,
			    cb->cb_literal, cb->cb_json_as_int,
			    ZFS_NICENUM_1024);
		}
		if (cb->cb_print_power) {
			if (children == 0)  {
				/* Only leaf vdevs have physical slots */
				switch (zpool_power_current_state(zhp, (char *)
				    fnvlist_lookup_string(nv,
				    ZPOOL_CONFIG_PATH))) {
				case 0:
					fnvlist_add_string(vds, "power_state",
					    "off");
					break;
				case 1:
					fnvlist_add_string(vds, "power_state",
					    "on");
					break;
				default:
					fnvlist_add_string(vds, "power_state",
					    "-");
				}
			} else {
				fnvlist_add_string(vds, "power_state", "-");
			}
		}
	}

	if (cb->cb_print_dio_verify) {
		nice_num_str_nvlist(vds, "dio_verify_errors",
		    vs->vs_dio_verify_errors, cb->cb_literal,
		    cb->cb_json_as_int, ZFS_NICENUM_1024);
	}

	if (nvlist_lookup_uint64(nv, ZPOOL_CONFIG_NOT_PRESENT,
	    &notpresent) == 0) {
		nice_num_str_nvlist(vds, ZPOOL_CONFIG_NOT_PRESENT,
		    1, B_TRUE, cb->cb_json_as_int, ZFS_NICENUM_BYTES);
		fnvlist_add_string(vds, "was",
		    fnvlist_lookup_string(nv, ZPOOL_CONFIG_PATH));
	} else if (vs->vs_aux != VDEV_AUX_NONE) {
		fnvlist_add_string(vds, "aux", vdev_aux_str[vs->vs_aux]);
	} else if (children == 0 && !isspare &&
	    getenv("ZPOOL_STATUS_NON_NATIVE_ASHIFT_IGNORE") == NULL &&
	    VDEV_STAT_VALID(vs_physical_ashift, vsc) &&
	    vs->vs_configured_ashift < vs->vs_physical_ashift) {
		nice_num_str_nvlist(vds, "configured_ashift",
		    vs->vs_configured_ashift, B_TRUE, cb->cb_json_as_int,
		    ZFS_NICENUM_1024);
		nice_num_str_nvlist(vds, "physical_ashift",
		    vs->vs_physical_ashift, B_TRUE, cb->cb_json_as_int,
		    ZFS_NICENUM_1024);
	}
	if (vs->vs_scan_removing != 0) {
		nice_num_str_nvlist(vds, "removing", vs->vs_scan_removing,
		    B_TRUE, cb->cb_json_as_int, ZFS_NICENUM_1024);
	} else if (VDEV_STAT_VALID(vs_noalloc, vsc) && vs->vs_noalloc != 0) {
		nice_num_str_nvlist(vds, "noalloc", vs->vs_noalloc,
		    B_TRUE, cb->cb_json_as_int, ZFS_NICENUM_1024);
	}

	if (cb->vcdl != NULL) {
		if (nvlist_lookup_string(nv, ZPOOL_CONFIG_PATH, &path) == 0) {
			zpool_nvlist_cmd(cb->vcdl, zpool_get_name(zhp),
			    path, vds);
		}
	}

	if (children == 0) {
		if (cb->cb_print_vdev_init) {
			if (vs->vs_initialize_state != 0) {
				uint64_t st = vs->vs_initialize_state;
				fnvlist_add_string(vds, "init_state",
				    vdev_init_state_str[st]);
				nice_num_str_nvlist(vds, "initialized",
				    vs->vs_initialize_bytes_done,
				    cb->cb_literal, cb->cb_json_as_int,
				    ZFS_NICENUM_BYTES);
				nice_num_str_nvlist(vds, "to_initialize",
				    vs->vs_initialize_bytes_est,
				    cb->cb_literal, cb->cb_json_as_int,
				    ZFS_NICENUM_BYTES);
				nice_num_str_nvlist(vds, "init_time",
				    vs->vs_initialize_action_time,
				    cb->cb_literal, cb->cb_json_as_int,
				    ZFS_NICE_TIMESTAMP);
				nice_num_str_nvlist(vds, "init_errors",
				    vs->vs_initialize_errors,
				    cb->cb_literal, cb->cb_json_as_int,
				    ZFS_NICENUM_1024);
			} else {
				fnvlist_add_string(vds, "init_state",
				    "UNINITIALIZED");
			}
		}
		if (cb->cb_print_vdev_trim) {
			if (vs->vs_trim_notsup == 0) {
					if (vs->vs_trim_state != 0) {
					uint64_t st = vs->vs_trim_state;
					fnvlist_add_string(vds, "trim_state",
					    vdev_trim_state_str[st]);
					nice_num_str_nvlist(vds, "trimmed",
					    vs->vs_trim_bytes_done,
					    cb->cb_literal, cb->cb_json_as_int,
					    ZFS_NICENUM_BYTES);
					nice_num_str_nvlist(vds, "to_trim",
					    vs->vs_trim_bytes_est,
					    cb->cb_literal, cb->cb_json_as_int,
					    ZFS_NICENUM_BYTES);
					nice_num_str_nvlist(vds, "trim_time",
					    vs->vs_trim_action_time,
					    cb->cb_literal, cb->cb_json_as_int,
					    ZFS_NICE_TIMESTAMP);
					nice_num_str_nvlist(vds, "trim_errors",
					    vs->vs_trim_errors,
					    cb->cb_literal, cb->cb_json_as_int,
					    ZFS_NICENUM_1024);
				} else
					fnvlist_add_string(vds, "trim_state",
					    "UNTRIMMED");
			}
			nice_num_str_nvlist(vds, "trim_notsup",
			    vs->vs_trim_notsup, B_TRUE,
			    cb->cb_json_as_int, ZFS_NICENUM_1024);
		}
	} else {
		ch = fnvlist_alloc();
	}

	if (cb->cb_flat_vdevs && children == 0) {
		fnvlist_add_nvlist(item, vname, vds);
	}

	for (int c = 0; c < children; c++) {
		uint64_t islog = B_FALSE, ishole = B_FALSE;
		(void) nvlist_lookup_uint64(child[c], ZPOOL_CONFIG_IS_LOG,
		    &islog);
		(void) nvlist_lookup_uint64(child[c], ZPOOL_CONFIG_IS_HOLE,
		    &ishole);
		if (islog || ishole)
			continue;
		if (nvlist_exists(child[c], ZPOOL_CONFIG_ALLOCATION_BIAS))
			continue;
		if (cb->cb_flat_vdevs) {
			vdev_stats_nvlist(zhp, cb, child[c], depth + 2, isspare,
			    vname, item);
		}
		vdev_stats_nvlist(zhp, cb, child[c], depth + 2, isspare,
		    vname, ch);
	}

	if (ch != NULL) {
		if (!nvlist_empty(ch))
			fnvlist_add_nvlist(vds, "vdevs", ch);
		fnvlist_free(ch);
	}
	fnvlist_add_nvlist(item, vname, vds);
	fnvlist_free(vds);
	free(vname);
}

static void
class_vdevs_nvlist(zpool_handle_t *zhp, status_cbdata_t *cb, nvlist_t *nv,
    const char *class, nvlist_t *item)
{
	uint_t c, children;
	nvlist_t **child;
	nvlist_t *class_obj = NULL;

	if (!cb->cb_flat_vdevs)
		class_obj = fnvlist_alloc();

	assert(zhp != NULL || !cb->cb_verbose);

	if (nvlist_lookup_nvlist_array(nv, ZPOOL_CONFIG_CHILDREN, &child,
	    &children) != 0)
		return;

	for (c = 0; c < children; c++) {
		uint64_t is_log = B_FALSE;
		const char *bias = NULL;
		const char *type = NULL;
		char *name = zpool_vdev_name(g_zfs, zhp, child[c],
		    cb->cb_name_flags | VDEV_NAME_TYPE_ID);

		(void) nvlist_lookup_uint64(child[c], ZPOOL_CONFIG_IS_LOG,
		    &is_log);

		if (is_log) {
			bias = (char *)VDEV_ALLOC_CLASS_LOGS;
		} else {
			(void) nvlist_lookup_string(child[c],
			    ZPOOL_CONFIG_ALLOCATION_BIAS, &bias);
			(void) nvlist_lookup_string(child[c],
			    ZPOOL_CONFIG_TYPE, &type);
		}

		if (bias == NULL || strcmp(bias, class) != 0)
			continue;
		if (!is_log && strcmp(type, VDEV_TYPE_INDIRECT) == 0)
			continue;

		if (cb->cb_flat_vdevs) {
			vdev_stats_nvlist(zhp, cb, child[c], 2, B_FALSE,
			    NULL, item);
		} else {
			vdev_stats_nvlist(zhp, cb, child[c], 2, B_FALSE,
			    NULL, class_obj);
		}
		free(name);
	}
	if (!cb->cb_flat_vdevs) {
		if (!nvlist_empty(class_obj))
			fnvlist_add_nvlist(item, class, class_obj);
		fnvlist_free(class_obj);
	}
}

static void
l2cache_nvlist(zpool_handle_t *zhp, status_cbdata_t *cb, nvlist_t *nv,
    nvlist_t *item)
{
	nvlist_t *l2c = NULL, **l2cache;
	uint_t nl2cache;
	if (nvlist_lookup_nvlist_array(nv, ZPOOL_CONFIG_L2CACHE,
	    &l2cache, &nl2cache) == 0) {
		if (nl2cache == 0)
			return;
		if (!cb->cb_flat_vdevs)
			l2c = fnvlist_alloc();
		for (int i = 0; i < nl2cache; i++) {
			if (cb->cb_flat_vdevs) {
				vdev_stats_nvlist(zhp, cb, l2cache[i], 2,
				    B_FALSE, NULL, item);
			} else {
				vdev_stats_nvlist(zhp, cb, l2cache[i], 2,
				    B_FALSE, NULL, l2c);
			}
		}
	}
	if (!cb->cb_flat_vdevs) {
		if (!nvlist_empty(l2c))
			fnvlist_add_nvlist(item, "l2cache", l2c);
		fnvlist_free(l2c);
	}
}

static void
spares_nvlist(zpool_handle_t *zhp, status_cbdata_t *cb, nvlist_t *nv,
    nvlist_t *item)
{
	nvlist_t *sp = NULL, **spares;
	uint_t nspares;
	if (nvlist_lookup_nvlist_array(nv, ZPOOL_CONFIG_SPARES,
	    &spares, &nspares) == 0) {
		if (nspares == 0)
			return;
		if (!cb->cb_flat_vdevs)
			sp = fnvlist_alloc();
		for (int i = 0; i < nspares; i++) {
			if (cb->cb_flat_vdevs) {
				vdev_stats_nvlist(zhp, cb, spares[i], 2, B_TRUE,
				    NULL, item);
			} else {
				vdev_stats_nvlist(zhp, cb, spares[i], 2, B_TRUE,
				    NULL, sp);
			}
		}
	}
	if (!cb->cb_flat_vdevs) {
		if (!nvlist_empty(sp))
			fnvlist_add_nvlist(item, "spares", sp);
		fnvlist_free(sp);
	}
}

static void
errors_nvlist(zpool_handle_t *zhp, status_cbdata_t *cb, nvlist_t *item)
{
	uint64_t nerr;
	nvlist_t *config = zpool_get_config(zhp, NULL);
	if (nvlist_lookup_uint64(config, ZPOOL_CONFIG_ERRCOUNT,
	    &nerr) == 0) {
		nice_num_str_nvlist(item, ZPOOL_CONFIG_ERRCOUNT, nerr,
		    cb->cb_literal, cb->cb_json_as_int, ZFS_NICENUM_1024);
		if (nerr != 0 && cb->cb_verbose) {
			nvlist_t *nverrlist = NULL;
			if (zpool_get_errlog(zhp, &nverrlist) == 0) {
				int i = 0;
				int count = 0;
				size_t len = MAXPATHLEN * 2;
				nvpair_t *elem = NULL;

				for (nvpair_t *pair =
				    nvlist_next_nvpair(nverrlist, NULL);
				    pair != NULL;
				    pair = nvlist_next_nvpair(nverrlist, pair))
					count++;
				char **errl = (char **)malloc(
				    count * sizeof (char *));

				while ((elem = nvlist_next_nvpair(nverrlist,
				    elem)) != NULL) {
					nvlist_t *nv;
					uint64_t dsobj, obj;

					verify(nvpair_value_nvlist(elem,
					    &nv) == 0);
					verify(nvlist_lookup_uint64(nv,
					    ZPOOL_ERR_DATASET, &dsobj) == 0);
					verify(nvlist_lookup_uint64(nv,
					    ZPOOL_ERR_OBJECT, &obj) == 0);
					errl[i] = safe_malloc(len);
					zpool_obj_to_path(zhp, dsobj, obj,
					    errl[i++], len);
				}
				nvlist_free(nverrlist);
				fnvlist_add_string_array(item, "errlist",
				    (const char **)errl, count);
				for (int i = 0; i < count; ++i)
					free(errl[i]);
				free(errl);
			} else
				fnvlist_add_string(item, "errlist",
				    strerror(errno));
		}
	}
}

static void
ddt_stats_nvlist(ddt_stat_t *dds, status_cbdata_t *cb, nvlist_t *item)
{
	nice_num_str_nvlist(item, "blocks", dds->dds_blocks,
	    cb->cb_literal, cb->cb_json_as_int, ZFS_NICENUM_1024);
	nice_num_str_nvlist(item, "logical_size", dds->dds_lsize,
	    cb->cb_literal, cb->cb_json_as_int, ZFS_NICENUM_BYTES);
	nice_num_str_nvlist(item, "physical_size", dds->dds_psize,
	    cb->cb_literal, cb->cb_json_as_int, ZFS_NICENUM_BYTES);
	nice_num_str_nvlist(item, "deflated_size", dds->dds_dsize,
	    cb->cb_literal, cb->cb_json_as_int, ZFS_NICENUM_BYTES);
	nice_num_str_nvlist(item, "ref_blocks", dds->dds_ref_blocks,
	    cb->cb_literal, cb->cb_json_as_int, ZFS_NICENUM_1024);
	nice_num_str_nvlist(item, "ref_lsize", dds->dds_ref_lsize,
	    cb->cb_literal, cb->cb_json_as_int, ZFS_NICENUM_BYTES);
	nice_num_str_nvlist(item, "ref_psize", dds->dds_ref_psize,
	    cb->cb_literal, cb->cb_json_as_int, ZFS_NICENUM_BYTES);
	nice_num_str_nvlist(item, "ref_dsize", dds->dds_ref_dsize,
	    cb->cb_literal, cb->cb_json_as_int, ZFS_NICENUM_BYTES);
}

static void
dedup_stats_nvlist(zpool_handle_t *zhp, status_cbdata_t *cb, nvlist_t *item)
{
	nvlist_t *config;
	if (cb->cb_dedup_stats) {
		ddt_histogram_t *ddh;
		ddt_stat_t *dds;
		ddt_object_t *ddo;
		nvlist_t *ddt_stat, *ddt_obj, *dedup;
		uint_t c;
		uint64_t cspace_prop;

		config = zpool_get_config(zhp, NULL);
		if (nvlist_lookup_uint64_array(config,
		    ZPOOL_CONFIG_DDT_OBJ_STATS, (uint64_t **)&ddo, &c) != 0)
			return;

		dedup = fnvlist_alloc();
		ddt_obj = fnvlist_alloc();
		nice_num_str_nvlist(dedup, "obj_count", ddo->ddo_count,
		    cb->cb_literal, cb->cb_json_as_int, ZFS_NICENUM_1024);
		if (ddo->ddo_count == 0) {
			fnvlist_add_nvlist(dedup, ZPOOL_CONFIG_DDT_OBJ_STATS,
			    ddt_obj);
			fnvlist_add_nvlist(item, "dedup_stats", dedup);
			fnvlist_free(ddt_obj);
			fnvlist_free(dedup);
			return;
		} else {
			nice_num_str_nvlist(dedup, "dspace", ddo->ddo_dspace,
			    cb->cb_literal, cb->cb_json_as_int,
			    ZFS_NICENUM_1024);
			nice_num_str_nvlist(dedup, "mspace", ddo->ddo_mspace,
			    cb->cb_literal, cb->cb_json_as_int,
			    ZFS_NICENUM_1024);
			/*
			 * Squash cached size into in-core size to handle race.
			 * Only include cached size if it is available.
			 */
			cspace_prop = zpool_get_prop_int(zhp,
			    ZPOOL_PROP_DEDUPCACHED, NULL);
			cspace_prop = MIN(cspace_prop, ddo->ddo_mspace);
			nice_num_str_nvlist(dedup, "cspace", cspace_prop,
			    cb->cb_literal, cb->cb_json_as_int,
			    ZFS_NICENUM_1024);
		}

		ddt_stat = fnvlist_alloc();
		if (nvlist_lookup_uint64_array(config, ZPOOL_CONFIG_DDT_STATS,
		    (uint64_t **)&dds, &c) == 0) {
			nvlist_t *total = fnvlist_alloc();
			if (dds->dds_blocks == 0)
				fnvlist_add_string(total, "blocks", "0");
			else
				ddt_stats_nvlist(dds, cb, total);
			fnvlist_add_nvlist(ddt_stat, "total", total);
			fnvlist_free(total);
		}
		if (nvlist_lookup_uint64_array(config,
		    ZPOOL_CONFIG_DDT_HISTOGRAM, (uint64_t **)&ddh, &c) == 0) {
			nvlist_t *hist = fnvlist_alloc();
			nvlist_t *entry = NULL;
			char buf[16];
			for (int h = 0; h < 64; h++) {
				if (ddh->ddh_stat[h].dds_blocks != 0) {
					entry = fnvlist_alloc();
					ddt_stats_nvlist(&ddh->ddh_stat[h], cb,
					    entry);
					snprintf(buf, 16, "%d", h);
					fnvlist_add_nvlist(hist, buf, entry);
					fnvlist_free(entry);
				}
			}
			if (!nvlist_empty(hist))
				fnvlist_add_nvlist(ddt_stat, "histogram", hist);
			fnvlist_free(hist);
		}

		if (!nvlist_empty(ddt_obj)) {
			fnvlist_add_nvlist(dedup, ZPOOL_CONFIG_DDT_OBJ_STATS,
			    ddt_obj);
		}
		fnvlist_free(ddt_obj);
		if (!nvlist_empty(ddt_stat)) {
			fnvlist_add_nvlist(dedup, ZPOOL_CONFIG_DDT_STATS,
			    ddt_stat);
		}
		fnvlist_free(ddt_stat);
		if (!nvlist_empty(dedup))
			fnvlist_add_nvlist(item, "dedup_stats", dedup);
		fnvlist_free(dedup);
	}
}

static void
raidz_expand_status_nvlist(zpool_handle_t *zhp, status_cbdata_t *cb,
    nvlist_t *nvroot, nvlist_t *item)
{
	uint_t c;
	pool_raidz_expand_stat_t *pres = NULL;
	if (nvlist_lookup_uint64_array(nvroot,
	    ZPOOL_CONFIG_RAIDZ_EXPAND_STATS, (uint64_t **)&pres, &c) == 0) {
		nvlist_t **child;
		uint_t children;
		nvlist_t *nv = fnvlist_alloc();
		verify(nvlist_lookup_nvlist_array(nvroot, ZPOOL_CONFIG_CHILDREN,
		    &child, &children) == 0);
		assert(pres->pres_expanding_vdev < children);
		char *name =
		    zpool_vdev_name(g_zfs, zhp,
		    child[pres->pres_expanding_vdev], 0);
		fill_vdev_info(nv, zhp, name, B_FALSE, cb->cb_json_as_int);
		fnvlist_add_string(nv, "state",
		    pool_scan_state_str[pres->pres_state]);
		nice_num_str_nvlist(nv, "expanding_vdev",
		    pres->pres_expanding_vdev, B_TRUE, cb->cb_json_as_int,
		    ZFS_NICENUM_1024);
		nice_num_str_nvlist(nv, "start_time", pres->pres_start_time,
		    cb->cb_literal, cb->cb_json_as_int, ZFS_NICE_TIMESTAMP);
		nice_num_str_nvlist(nv, "end_time", pres->pres_end_time,
		    cb->cb_literal, cb->cb_json_as_int, ZFS_NICE_TIMESTAMP);
		nice_num_str_nvlist(nv, "to_reflow", pres->pres_to_reflow,
		    cb->cb_literal, cb->cb_json_as_int, ZFS_NICENUM_BYTES);
		nice_num_str_nvlist(nv, "reflowed", pres->pres_reflowed,
		    cb->cb_literal, cb->cb_json_as_int, ZFS_NICENUM_BYTES);
		nice_num_str_nvlist(nv, "waiting_for_resilver",
		    pres->pres_waiting_for_resilver, B_TRUE,
		    cb->cb_json_as_int, ZFS_NICENUM_1024);
		fnvlist_add_nvlist(item, ZPOOL_CONFIG_RAIDZ_EXPAND_STATS, nv);
		fnvlist_free(nv);
		free(name);
	}
}

static void
checkpoint_status_nvlist(nvlist_t *nvroot, status_cbdata_t *cb,
    nvlist_t *item)
{
	uint_t c;
	pool_checkpoint_stat_t *pcs = NULL;
	if (nvlist_lookup_uint64_array(nvroot,
	    ZPOOL_CONFIG_CHECKPOINT_STATS, (uint64_t **)&pcs, &c) == 0) {
		nvlist_t *nv = fnvlist_alloc();
		fnvlist_add_string(nv, "state",
		    checkpoint_state_str[pcs->pcs_state]);
		nice_num_str_nvlist(nv, "start_time",
		    pcs->pcs_start_time, cb->cb_literal, cb->cb_json_as_int,
		    ZFS_NICE_TIMESTAMP);
		nice_num_str_nvlist(nv, "space",
		    pcs->pcs_space, cb->cb_literal, cb->cb_json_as_int,
		    ZFS_NICENUM_BYTES);
		fnvlist_add_nvlist(item, ZPOOL_CONFIG_CHECKPOINT_STATS, nv);
		fnvlist_free(nv);
	}
}

static void
removal_status_nvlist(zpool_handle_t *zhp, status_cbdata_t *cb,
    nvlist_t *nvroot, nvlist_t *item)
{
	uint_t c;
	pool_removal_stat_t *prs = NULL;
	if (nvlist_lookup_uint64_array(nvroot, ZPOOL_CONFIG_REMOVAL_STATS,
	    (uint64_t **)&prs, &c) == 0) {
		if (prs->prs_state != DSS_NONE) {
			nvlist_t **child;
			uint_t children;
			verify(nvlist_lookup_nvlist_array(nvroot,
			    ZPOOL_CONFIG_CHILDREN, &child, &children) == 0);
			assert(prs->prs_removing_vdev < children);
			char *vdev_name = zpool_vdev_name(g_zfs, zhp,
			    child[prs->prs_removing_vdev], B_TRUE);
			nvlist_t *nv = fnvlist_alloc();
			fill_vdev_info(nv, zhp, vdev_name, B_FALSE,
			    cb->cb_json_as_int);
			fnvlist_add_string(nv, "state",
			    pool_scan_state_str[prs->prs_state]);
			nice_num_str_nvlist(nv, "removing_vdev",
			    prs->prs_removing_vdev, B_TRUE, cb->cb_json_as_int,
			    ZFS_NICENUM_1024);
			nice_num_str_nvlist(nv, "start_time",
			    prs->prs_start_time, cb->cb_literal,
			    cb->cb_json_as_int, ZFS_NICE_TIMESTAMP);
			nice_num_str_nvlist(nv, "end_time", prs->prs_end_time,
			    cb->cb_literal, cb->cb_json_as_int,
			    ZFS_NICE_TIMESTAMP);
			nice_num_str_nvlist(nv, "to_copy", prs->prs_to_copy,
			    cb->cb_literal, cb->cb_json_as_int,
			    ZFS_NICENUM_BYTES);
			nice_num_str_nvlist(nv, "copied", prs->prs_copied,
			    cb->cb_literal, cb->cb_json_as_int,
			    ZFS_NICENUM_BYTES);
			nice_num_str_nvlist(nv, "mapping_memory",
			    prs->prs_mapping_memory, cb->cb_literal,
			    cb->cb_json_as_int, ZFS_NICENUM_BYTES);
			fnvlist_add_nvlist(item,
			    ZPOOL_CONFIG_REMOVAL_STATS, nv);
			fnvlist_free(nv);
			free(vdev_name);
		}
	}
}

static void
scan_status_nvlist(zpool_handle_t *zhp, status_cbdata_t *cb,
    nvlist_t *nvroot, nvlist_t *item)
{
	pool_scan_stat_t *ps = NULL;
	uint_t c;
	nvlist_t *scan = fnvlist_alloc();
	nvlist_t **child;
	uint_t children;

	if (nvlist_lookup_uint64_array(nvroot, ZPOOL_CONFIG_SCAN_STATS,
	    (uint64_t **)&ps, &c) == 0) {
		fnvlist_add_string(scan, "function",
		    pool_scan_func_str[ps->pss_func]);
		fnvlist_add_string(scan, "state",
		    pool_scan_state_str[ps->pss_state]);
		nice_num_str_nvlist(scan, "start_time", ps->pss_start_time,
		    cb->cb_literal, cb->cb_json_as_int, ZFS_NICE_TIMESTAMP);
		nice_num_str_nvlist(scan, "end_time", ps->pss_end_time,
		    cb->cb_literal, cb->cb_json_as_int, ZFS_NICE_TIMESTAMP);
		nice_num_str_nvlist(scan, "to_examine", ps->pss_to_examine,
		    cb->cb_literal, cb->cb_json_as_int, ZFS_NICENUM_BYTES);
		nice_num_str_nvlist(scan, "examined", ps->pss_examined,
		    cb->cb_literal, cb->cb_json_as_int, ZFS_NICENUM_BYTES);
		nice_num_str_nvlist(scan, "skipped", ps->pss_skipped,
		    cb->cb_literal, cb->cb_json_as_int, ZFS_NICENUM_BYTES);
		nice_num_str_nvlist(scan, "processed", ps->pss_processed,
		    cb->cb_literal, cb->cb_json_as_int, ZFS_NICENUM_BYTES);
		nice_num_str_nvlist(scan, "errors", ps->pss_errors,
		    cb->cb_literal, cb->cb_json_as_int, ZFS_NICENUM_1024);
		nice_num_str_nvlist(scan, "bytes_per_scan", ps->pss_pass_exam,
		    cb->cb_literal, cb->cb_json_as_int, ZFS_NICENUM_BYTES);
		nice_num_str_nvlist(scan, "pass_start", ps->pss_pass_start,
		    B_TRUE, cb->cb_json_as_int, ZFS_NICENUM_1024);
		nice_num_str_nvlist(scan, "scrub_pause",
		    ps->pss_pass_scrub_pause, cb->cb_literal,
		    cb->cb_json_as_int, ZFS_NICE_TIMESTAMP);
		nice_num_str_nvlist(scan, "scrub_spent_paused",
		    ps->pss_pass_scrub_spent_paused,
		    B_TRUE, cb->cb_json_as_int, ZFS_NICENUM_1024);
		nice_num_str_nvlist(scan, "issued_bytes_per_scan",
		    ps->pss_pass_issued, cb->cb_literal,
		    cb->cb_json_as_int, ZFS_NICENUM_BYTES);
		nice_num_str_nvlist(scan, "issued", ps->pss_issued,
		    cb->cb_literal, cb->cb_json_as_int, ZFS_NICENUM_BYTES);
		if (ps->pss_error_scrub_func == POOL_SCAN_ERRORSCRUB &&
		    ps->pss_error_scrub_start > ps->pss_start_time) {
			fnvlist_add_string(scan, "err_scrub_func",
			    pool_scan_func_str[ps->pss_error_scrub_func]);
			fnvlist_add_string(scan, "err_scrub_state",
			    pool_scan_state_str[ps->pss_error_scrub_state]);
			nice_num_str_nvlist(scan, "err_scrub_start_time",
			    ps->pss_error_scrub_start,
			    cb->cb_literal, cb->cb_json_as_int,
			    ZFS_NICE_TIMESTAMP);
			nice_num_str_nvlist(scan, "err_scrub_end_time",
			    ps->pss_error_scrub_end,
			    cb->cb_literal, cb->cb_json_as_int,
			    ZFS_NICE_TIMESTAMP);
			nice_num_str_nvlist(scan, "err_scrub_examined",
			    ps->pss_error_scrub_examined,
			    cb->cb_literal, cb->cb_json_as_int,
			    ZFS_NICENUM_1024);
			nice_num_str_nvlist(scan, "err_scrub_to_examine",
			    ps->pss_error_scrub_to_be_examined,
			    cb->cb_literal, cb->cb_json_as_int,
			    ZFS_NICENUM_1024);
			nice_num_str_nvlist(scan, "err_scrub_pause",
			    ps->pss_pass_error_scrub_pause,
			    B_TRUE, cb->cb_json_as_int, ZFS_NICENUM_1024);
		}
	}

	if (nvlist_lookup_nvlist_array(nvroot, ZPOOL_CONFIG_CHILDREN,
	    &child, &children) == 0) {
		vdev_rebuild_stat_t *vrs;
		uint_t i;
		char *name;
		nvlist_t *nv;
		nvlist_t *rebuild = fnvlist_alloc();
		uint64_t st;
		for (uint_t c = 0; c < children; c++) {
			if (nvlist_lookup_uint64_array(child[c],
			    ZPOOL_CONFIG_REBUILD_STATS, (uint64_t **)&vrs,
			    &i) == 0) {
				if (vrs->vrs_state != VDEV_REBUILD_NONE) {
					nv = fnvlist_alloc();
					name = zpool_vdev_name(g_zfs, zhp,
					    child[c], VDEV_NAME_TYPE_ID);
					fill_vdev_info(nv, zhp, name, B_FALSE,
					    cb->cb_json_as_int);
					st = vrs->vrs_state;
					fnvlist_add_string(nv, "state",
					    vdev_rebuild_state_str[st]);
					nice_num_str_nvlist(nv, "start_time",
					    vrs->vrs_start_time, cb->cb_literal,
					    cb->cb_json_as_int,
					    ZFS_NICE_TIMESTAMP);
					nice_num_str_nvlist(nv, "end_time",
					    vrs->vrs_end_time, cb->cb_literal,
					    cb->cb_json_as_int,
					    ZFS_NICE_TIMESTAMP);
					nice_num_str_nvlist(nv, "scan_time",
					    vrs->vrs_scan_time_ms * 1000000,
					    cb->cb_literal, cb->cb_json_as_int,
					    ZFS_NICENUM_TIME);
					nice_num_str_nvlist(nv, "scanned",
					    vrs->vrs_bytes_scanned,
					    cb->cb_literal, cb->cb_json_as_int,
					    ZFS_NICENUM_BYTES);
					nice_num_str_nvlist(nv, "issued",
					    vrs->vrs_bytes_issued,
					    cb->cb_literal, cb->cb_json_as_int,
					    ZFS_NICENUM_BYTES);
					nice_num_str_nvlist(nv, "rebuilt",
					    vrs->vrs_bytes_rebuilt,
					    cb->cb_literal, cb->cb_json_as_int,
					    ZFS_NICENUM_BYTES);
					nice_num_str_nvlist(nv, "to_scan",
					    vrs->vrs_bytes_est, cb->cb_literal,
					    cb->cb_json_as_int,
					    ZFS_NICENUM_BYTES);
					nice_num_str_nvlist(nv, "errors",
					    vrs->vrs_errors, cb->cb_literal,
					    cb->cb_json_as_int,
					    ZFS_NICENUM_1024);
					nice_num_str_nvlist(nv, "pass_time",
					    vrs->vrs_pass_time_ms * 1000000,
					    cb->cb_literal, cb->cb_json_as_int,
					    ZFS_NICENUM_TIME);
					nice_num_str_nvlist(nv, "pass_scanned",
					    vrs->vrs_pass_bytes_scanned,
					    cb->cb_literal, cb->cb_json_as_int,
					    ZFS_NICENUM_BYTES);
					nice_num_str_nvlist(nv, "pass_issued",
					    vrs->vrs_pass_bytes_issued,
					    cb->cb_literal, cb->cb_json_as_int,
					    ZFS_NICENUM_BYTES);
					nice_num_str_nvlist(nv, "pass_skipped",
					    vrs->vrs_pass_bytes_skipped,
					    cb->cb_literal, cb->cb_json_as_int,
					    ZFS_NICENUM_BYTES);
					fnvlist_add_nvlist(rebuild, name, nv);
					free(name);
				}
			}
		}
		if (!nvlist_empty(rebuild))
			fnvlist_add_nvlist(scan, "rebuild_stats", rebuild);
		fnvlist_free(rebuild);
	}

	if (!nvlist_empty(scan))
		fnvlist_add_nvlist(item, ZPOOL_CONFIG_SCAN_STATS, scan);
	fnvlist_free(scan);
}

/*
 * Print the scan status.
 */
static void
print_scan_status(zpool_handle_t *zhp, nvlist_t *nvroot)
{
	uint64_t rebuild_end_time = 0, resilver_end_time = 0;
	boolean_t have_resilver = B_FALSE, have_scrub = B_FALSE;
	boolean_t have_errorscrub = B_FALSE;
	boolean_t active_resilver = B_FALSE;
	pool_checkpoint_stat_t *pcs = NULL;
	pool_scan_stat_t *ps = NULL;
	uint_t c;
	time_t scrub_start = 0, errorscrub_start = 0;

	if (nvlist_lookup_uint64_array(nvroot, ZPOOL_CONFIG_SCAN_STATS,
	    (uint64_t **)&ps, &c) == 0) {
		if (ps->pss_func == POOL_SCAN_RESILVER) {
			resilver_end_time = ps->pss_end_time;
			active_resilver = (ps->pss_state == DSS_SCANNING);
		}

		have_resilver = (ps->pss_func == POOL_SCAN_RESILVER);
		have_scrub = (ps->pss_func == POOL_SCAN_SCRUB);
		scrub_start = ps->pss_start_time;
		if (c > offsetof(pool_scan_stat_t,
		    pss_pass_error_scrub_pause) / 8) {
			have_errorscrub = (ps->pss_error_scrub_func ==
			    POOL_SCAN_ERRORSCRUB);
			errorscrub_start = ps->pss_error_scrub_start;
		}
	}

	boolean_t active_rebuild = check_rebuilding(nvroot, &rebuild_end_time);
	boolean_t have_rebuild = (active_rebuild || (rebuild_end_time > 0));

	/* Always print the scrub status when available. */
	if (have_scrub && scrub_start > errorscrub_start)
		print_scan_scrub_resilver_status(ps);
	else if (have_errorscrub && errorscrub_start >= scrub_start)
		print_err_scrub_status(ps);

	/*
	 * When there is an active resilver or rebuild print its status.
	 * Otherwise print the status of the last resilver or rebuild.
	 */
	if (active_resilver || (!active_rebuild && have_resilver &&
	    resilver_end_time && resilver_end_time > rebuild_end_time)) {
		print_scan_scrub_resilver_status(ps);
	} else if (active_rebuild || (!active_resilver && have_rebuild &&
	    rebuild_end_time && rebuild_end_time > resilver_end_time)) {
		print_rebuild_status(zhp, nvroot);
	}

	(void) nvlist_lookup_uint64_array(nvroot,
	    ZPOOL_CONFIG_CHECKPOINT_STATS, (uint64_t **)&pcs, &c);
	print_checkpoint_scan_warning(ps, pcs);
}

/*
 * Print out detailed removal status.
 */
static void
print_removal_status(zpool_handle_t *zhp, pool_removal_stat_t *prs)
{
	char copied_buf[7], examined_buf[7], total_buf[7], rate_buf[7];
	time_t start, end;
	nvlist_t *config, *nvroot;
	nvlist_t **child;
	uint_t children;
	char *vdev_name;

	if (prs == NULL || prs->prs_state == DSS_NONE)
		return;

	/*
	 * Determine name of vdev.
	 */
	config = zpool_get_config(zhp, NULL);
	nvroot = fnvlist_lookup_nvlist(config,
	    ZPOOL_CONFIG_VDEV_TREE);
	verify(nvlist_lookup_nvlist_array(nvroot, ZPOOL_CONFIG_CHILDREN,
	    &child, &children) == 0);
	assert(prs->prs_removing_vdev < children);
	vdev_name = zpool_vdev_name(g_zfs, zhp,
	    child[prs->prs_removing_vdev], B_TRUE);

	printf_color(ANSI_BOLD, gettext("remove: "));

	start = prs->prs_start_time;
	end = prs->prs_end_time;
	zfs_nicenum(prs->prs_copied, copied_buf, sizeof (copied_buf));

	/*
	 * Removal is finished or canceled.
	 */
	if (prs->prs_state == DSS_FINISHED) {
		uint64_t minutes_taken = (end - start) / 60;

		(void) printf(gettext("Removal of vdev %llu copied %s "
		    "in %lluh%um, completed on %s"),
		    (longlong_t)prs->prs_removing_vdev,
		    copied_buf,
		    (u_longlong_t)(minutes_taken / 60),
		    (uint_t)(minutes_taken % 60),
		    ctime((time_t *)&end));
	} else if (prs->prs_state == DSS_CANCELED) {
		(void) printf(gettext("Removal of %s canceled on %s"),
		    vdev_name, ctime(&end));
	} else {
		uint64_t copied, total, elapsed, rate, mins_left, hours_left;
		double fraction_done;

		assert(prs->prs_state == DSS_SCANNING);

		/*
		 * Removal is in progress.
		 */
		(void) printf(gettext(
		    "Evacuation of %s in progress since %s"),
		    vdev_name, ctime(&start));

		copied = prs->prs_copied > 0 ? prs->prs_copied : 1;
		total = prs->prs_to_copy;
		fraction_done = (double)copied / total;

		/* elapsed time for this pass */
		elapsed = time(NULL) - prs->prs_start_time;
		elapsed = elapsed > 0 ? elapsed : 1;
		rate = copied / elapsed;
		rate = rate > 0 ? rate : 1;
		mins_left = ((total - copied) / rate) / 60;
		hours_left = mins_left / 60;

		zfs_nicenum(copied, examined_buf, sizeof (examined_buf));
		zfs_nicenum(total, total_buf, sizeof (total_buf));
		zfs_nicenum(rate, rate_buf, sizeof (rate_buf));

		/*
		 * do not print estimated time if hours_left is more than
		 * 30 days
		 */
		(void) printf(gettext(
		    "\t%s copied out of %s at %s/s, %.2f%% done"),
		    examined_buf, total_buf, rate_buf, 100 * fraction_done);
		if (hours_left < (30 * 24)) {
			(void) printf(gettext(", %lluh%um to go\n"),
			    (u_longlong_t)hours_left, (uint_t)(mins_left % 60));
		} else {
			(void) printf(gettext(
			    ", (copy is slow, no estimated time)\n"));
		}
	}
	free(vdev_name);

	if (prs->prs_mapping_memory > 0) {
		char mem_buf[7];
		zfs_nicenum(prs->prs_mapping_memory, mem_buf, sizeof (mem_buf));
		(void) printf(gettext(
		    "\t%s memory used for removed device mappings\n"),
		    mem_buf);
	}
}

/*
 * Print out detailed raidz expansion status.
 */
static void
print_raidz_expand_status(zpool_handle_t *zhp, pool_raidz_expand_stat_t *pres)
{
	char copied_buf[7];

	if (pres == NULL || pres->pres_state == DSS_NONE)
		return;

	/*
	 * Determine name of vdev.
	 */
	nvlist_t *config = zpool_get_config(zhp, NULL);
	nvlist_t *nvroot = fnvlist_lookup_nvlist(config,
	    ZPOOL_CONFIG_VDEV_TREE);
	nvlist_t **child;
	uint_t children;
	verify(nvlist_lookup_nvlist_array(nvroot, ZPOOL_CONFIG_CHILDREN,
	    &child, &children) == 0);
	assert(pres->pres_expanding_vdev < children);

	printf_color(ANSI_BOLD, gettext("expand: "));

	time_t start = pres->pres_start_time;
	time_t end = pres->pres_end_time;
	char *vname =
	    zpool_vdev_name(g_zfs, zhp, child[pres->pres_expanding_vdev], 0);
	zfs_nicenum(pres->pres_reflowed, copied_buf, sizeof (copied_buf));

	/*
	 * Expansion is finished or canceled.
	 */
	if (pres->pres_state == DSS_FINISHED) {
		char time_buf[32];
		secs_to_dhms(end - start, time_buf);

		(void) printf(gettext("expanded %s-%u copied %s in %s, "
		    "on %s"), vname, (int)pres->pres_expanding_vdev,
		    copied_buf, time_buf, ctime((time_t *)&end));
	} else {
		char examined_buf[7], total_buf[7], rate_buf[7];
		uint64_t copied, total, elapsed, rate, secs_left;
		double fraction_done;

		assert(pres->pres_state == DSS_SCANNING);

		/*
		 * Expansion is in progress.
		 */
		(void) printf(gettext(
		    "expansion of %s-%u in progress since %s"),
		    vname, (int)pres->pres_expanding_vdev, ctime(&start));

		copied = pres->pres_reflowed > 0 ? pres->pres_reflowed : 1;
		total = pres->pres_to_reflow;
		fraction_done = (double)copied / total;

		/* elapsed time for this pass */
		elapsed = time(NULL) - pres->pres_start_time;
		elapsed = elapsed > 0 ? elapsed : 1;
		rate = copied / elapsed;
		rate = rate > 0 ? rate : 1;
		secs_left = (total - copied) / rate;

		zfs_nicenum(copied, examined_buf, sizeof (examined_buf));
		zfs_nicenum(total, total_buf, sizeof (total_buf));
		zfs_nicenum(rate, rate_buf, sizeof (rate_buf));

		/*
		 * do not print estimated time if hours_left is more than
		 * 30 days
		 */
		(void) printf(gettext("\t%s / %s copied at %s/s, %.2f%% done"),
		    examined_buf, total_buf, rate_buf, 100 * fraction_done);
		if (pres->pres_waiting_for_resilver) {
			(void) printf(gettext(", paused for resilver or "
			    "clear\n"));
		} else if (secs_left < (30 * 24 * 3600)) {
			char time_buf[32];
			secs_to_dhms(secs_left, time_buf);
			(void) printf(gettext(", %s to go\n"), time_buf);
		} else {
			(void) printf(gettext(
			    ", (copy is slow, no estimated time)\n"));
		}
	}
	free(vname);
}
static void
print_checkpoint_status(pool_checkpoint_stat_t *pcs)
{
	time_t start;
	char space_buf[7];

	if (pcs == NULL || pcs->pcs_state == CS_NONE)
		return;

	(void) printf(gettext("checkpoint: "));

	start = pcs->pcs_start_time;
	zfs_nicenum(pcs->pcs_space, space_buf, sizeof (space_buf));

	if (pcs->pcs_state == CS_CHECKPOINT_EXISTS) {
		char *date = ctime(&start);

		/*
		 * ctime() adds a newline at the end of the generated
		 * string, thus the weird format specifier and the
		 * strlen() call used to chop it off from the output.
		 */
		(void) printf(gettext("created %.*s, consumes %s\n"),
		    (int)(strlen(date) - 1), date, space_buf);
		return;
	}

	assert(pcs->pcs_state == CS_CHECKPOINT_DISCARDING);

	(void) printf(gettext("discarding, %s remaining.\n"),
	    space_buf);
}

static void
print_error_log(zpool_handle_t *zhp)
{
	nvlist_t *nverrlist = NULL;
	nvpair_t *elem;
	char *pathname;
	size_t len = MAXPATHLEN * 2;

	if (zpool_get_errlog(zhp, &nverrlist) != 0)
		return;

	(void) printf("errors: Permanent errors have been "
	    "detected in the following files:\n\n");

	pathname = safe_malloc(len);
	elem = NULL;
	while ((elem = nvlist_next_nvpair(nverrlist, elem)) != NULL) {
		nvlist_t *nv;
		uint64_t dsobj, obj;

		verify(nvpair_value_nvlist(elem, &nv) == 0);
		verify(nvlist_lookup_uint64(nv, ZPOOL_ERR_DATASET,
		    &dsobj) == 0);
		verify(nvlist_lookup_uint64(nv, ZPOOL_ERR_OBJECT,
		    &obj) == 0);
		zpool_obj_to_path(zhp, dsobj, obj, pathname, len);
		(void) printf("%7s %s\n", "", pathname);
	}
	free(pathname);
	nvlist_free(nverrlist);
}

static void
print_spares(zpool_handle_t *zhp, status_cbdata_t *cb, nvlist_t **spares,
    uint_t nspares)
{
	uint_t i;
	char *name;

	if (nspares == 0)
		return;

	(void) printf(gettext("\tspares\n"));

	for (i = 0; i < nspares; i++) {
		name = zpool_vdev_name(g_zfs, zhp, spares[i],
		    cb->cb_name_flags);
		print_status_config(zhp, cb, name, spares[i], 2, B_TRUE, NULL);
		free(name);
	}
}

static void
print_l2cache(zpool_handle_t *zhp, status_cbdata_t *cb, nvlist_t **l2cache,
    uint_t nl2cache)
{
	uint_t i;
	char *name;

	if (nl2cache == 0)
		return;

	(void) printf(gettext("\tcache\n"));

	for (i = 0; i < nl2cache; i++) {
		name = zpool_vdev_name(g_zfs, zhp, l2cache[i],
		    cb->cb_name_flags);
		print_status_config(zhp, cb, name, l2cache[i], 2,
		    B_FALSE, NULL);
		free(name);
	}
}

static void
print_dedup_stats(zpool_handle_t *zhp, nvlist_t *config, boolean_t literal)
{
	ddt_histogram_t *ddh;
	ddt_stat_t *dds;
	ddt_object_t *ddo;
	uint_t c;
	/* Extra space provided for literal display */
	char dspace[32], mspace[32], cspace[32];
	uint64_t cspace_prop;
	enum zfs_nicenum_format format;
	zprop_source_t src;

	/*
	 * If the pool was faulted then we may not have been able to
	 * obtain the config. Otherwise, if we have anything in the dedup
	 * table continue processing the stats.
	 */
	if (nvlist_lookup_uint64_array(config, ZPOOL_CONFIG_DDT_OBJ_STATS,
	    (uint64_t **)&ddo, &c) != 0)
		return;

	(void) printf("\n");
	(void) printf(gettext(" dedup: "));
	if (ddo->ddo_count == 0) {
		(void) printf(gettext("no DDT entries\n"));
		return;
	}

	/*
	 * Squash cached size into in-core size to handle race.
	 * Only include cached size if it is available.
	 */
	cspace_prop = zpool_get_prop_int(zhp, ZPOOL_PROP_DEDUPCACHED, &src);
	cspace_prop = MIN(cspace_prop, ddo->ddo_mspace);
	format = literal ? ZFS_NICENUM_RAW : ZFS_NICENUM_1024;
	zfs_nicenum_format(cspace_prop, cspace, sizeof (cspace), format);
	zfs_nicenum_format(ddo->ddo_dspace, dspace, sizeof (dspace), format);
	zfs_nicenum_format(ddo->ddo_mspace, mspace, sizeof (mspace), format);
	(void) printf("DDT entries %llu, size %s on disk, %s in core",
	    (u_longlong_t)ddo->ddo_count,
	    dspace,
	    mspace);
	if (src != ZPROP_SRC_DEFAULT) {
		(void) printf(", %s cached (%.02f%%)",
		    cspace,
		    (double)cspace_prop / (double)ddo->ddo_mspace * 100.0);
	}
	(void) printf("\n");

	verify(nvlist_lookup_uint64_array(config, ZPOOL_CONFIG_DDT_STATS,
	    (uint64_t **)&dds, &c) == 0);
	verify(nvlist_lookup_uint64_array(config, ZPOOL_CONFIG_DDT_HISTOGRAM,
	    (uint64_t **)&ddh, &c) == 0);
	zpool_dump_ddt(dds, ddh);
}

#define	ST_SIZE	4096
#define	AC_SIZE	2048

static void
print_status_reason(zpool_handle_t *zhp, status_cbdata_t *cbp,
    zpool_status_t reason, zpool_errata_t errata, nvlist_t *item)
{
	char status[ST_SIZE];
	char action[AC_SIZE];
	memset(status, 0, ST_SIZE);
	memset(action, 0, AC_SIZE);

	switch (reason) {
	case ZPOOL_STATUS_MISSING_DEV_R:
		snprintf(status, ST_SIZE, gettext("One or more devices could "
		    "not be opened.  Sufficient replicas exist for\n\tthe pool "
		    "to continue functioning in a degraded state.\n"));
		snprintf(action, AC_SIZE, gettext("Attach the missing device "
		    "and online it using 'zpool online'.\n"));
		break;

	case ZPOOL_STATUS_MISSING_DEV_NR:
		snprintf(status, ST_SIZE, gettext("One or more devices could "
		    "not be opened.  There are insufficient\n\treplicas for the"
		    " pool to continue functioning.\n"));
		snprintf(action, AC_SIZE, gettext("Attach the missing device "
		    "and online it using 'zpool online'.\n"));
		break;

	case ZPOOL_STATUS_CORRUPT_LABEL_R:
		snprintf(status, ST_SIZE, gettext("One or more devices could "
		    "not be used because the label is missing or\n\tinvalid.  "
		    "Sufficient replicas exist for the pool to continue\n\t"
		    "functioning in a degraded state.\n"));
		snprintf(action, AC_SIZE, gettext("Replace the device using "
		    "'zpool replace'.\n"));
		break;

	case ZPOOL_STATUS_CORRUPT_LABEL_NR:
		snprintf(status, ST_SIZE, gettext("One or more devices could "
		    "not be used because the label is missing \n\tor invalid.  "
		    "There are insufficient replicas for the pool to "
		    "continue\n\tfunctioning.\n"));
		zpool_explain_recover(zpool_get_handle(zhp),
		    zpool_get_name(zhp), reason, zpool_get_config(zhp, NULL),
		    action, AC_SIZE);
		break;

	case ZPOOL_STATUS_FAILING_DEV:
		snprintf(status, ST_SIZE, gettext("One or more devices has "
		    "experienced an unrecoverable error.  An\n\tattempt was "
		    "made to correct the error.  Applications are "
		    "unaffected.\n"));
		snprintf(action, AC_SIZE, gettext("Determine if the "
		    "device needs to be replaced, and clear the errors\n\tusing"
		    " 'zpool clear' or replace the device with 'zpool "
		    "replace'.\n"));
		break;

	case ZPOOL_STATUS_OFFLINE_DEV:
		snprintf(status, ST_SIZE, gettext("One or more devices has "
		    "been taken offline by the administrator.\n\tSufficient "
		    "replicas exist for the pool to continue functioning in "
		    "a\n\tdegraded state.\n"));
		snprintf(action, AC_SIZE, gettext("Online the device "
		    "using 'zpool online' or replace the device with\n\t'zpool "
		    "replace'.\n"));
		break;

	case ZPOOL_STATUS_REMOVED_DEV:
		snprintf(status, ST_SIZE, gettext("One or more devices have "
		    "been removed.\n\tSufficient replicas exist for the pool "
		    "to continue functioning in a\n\tdegraded state.\n"));
		snprintf(action, AC_SIZE, gettext("Online the device "
		    "using zpool online' or replace the device with\n\t'zpool "
		    "replace'.\n"));
		break;

	case ZPOOL_STATUS_RESILVERING:
	case ZPOOL_STATUS_REBUILDING:
		snprintf(status, ST_SIZE, gettext("One or more devices is "
		    "currently being resilvered.  The pool will\n\tcontinue "
		    "to function, possibly in a degraded state.\n"));
		snprintf(action, AC_SIZE, gettext("Wait for the resilver to "
		    "complete.\n"));
		break;

	case ZPOOL_STATUS_REBUILD_SCRUB:
		snprintf(status, ST_SIZE, gettext("One or more devices have "
		    "been sequentially resilvered, scrubbing\n\tthe pool "
		    "is recommended.\n"));
		snprintf(action, AC_SIZE, gettext("Use 'zpool scrub' to "
		    "verify all data checksums.\n"));
		break;

	case ZPOOL_STATUS_CORRUPT_DATA:
		snprintf(status, ST_SIZE, gettext("One or more devices has "
		    "experienced an error resulting in data\n\tcorruption.  "
		    "Applications may be affected.\n"));
		snprintf(action, AC_SIZE, gettext("Restore the file in question"
		    " if possible.  Otherwise restore the\n\tentire pool from "
		    "backup.\n"));
		break;

	case ZPOOL_STATUS_CORRUPT_POOL:
		snprintf(status, ST_SIZE, gettext("The pool metadata is "
		    "corrupted and the pool cannot be opened.\n"));
		zpool_explain_recover(zpool_get_handle(zhp),
		    zpool_get_name(zhp), reason, zpool_get_config(zhp, NULL),
		    action, AC_SIZE);
		break;

	case ZPOOL_STATUS_VERSION_OLDER:
		snprintf(status, ST_SIZE, gettext("The pool is formatted using "
		    "a legacy on-disk format.  The pool can\n\tstill be used, "
		    "but some features are unavailable.\n"));
		snprintf(action, AC_SIZE, gettext("Upgrade the pool using "
		    "'zpool upgrade'.  Once this is done, the\n\tpool will no "
		    "longer be accessible on software that does not support\n\t"
		    "feature flags.\n"));
		break;

	case ZPOOL_STATUS_VERSION_NEWER:
		snprintf(status, ST_SIZE, gettext("The pool has been upgraded "
		    "to a newer, incompatible on-disk version.\n\tThe pool "
		    "cannot be accessed on this system.\n"));
		snprintf(action, AC_SIZE, gettext("Access the pool from a "
		    "system running more recent software, or\n\trestore the "
		    "pool from backup.\n"));
		break;

	case ZPOOL_STATUS_FEAT_DISABLED:
		snprintf(status, ST_SIZE, gettext("Some supported and "
		    "requested features are not enabled on the pool.\n\t"
		    "The pool can still be used, but some features are "
		    "unavailable.\n"));
		snprintf(action, AC_SIZE, gettext("Enable all features using "
		    "'zpool upgrade'. Once this is done,\n\tthe pool may no "
		    "longer be accessible by software that does not support\n\t"
		    "the features. See zpool-features(7) for details.\n"));
		break;

	case ZPOOL_STATUS_COMPATIBILITY_ERR:
		snprintf(status, ST_SIZE, gettext("This pool has a "
		    "compatibility list specified, but it could not be\n\t"
		    "read/parsed at this time. The pool can still be used, "
		    "but this\n\tshould be investigated.\n"));
		snprintf(action, AC_SIZE, gettext("Check the value of the "
		    "'compatibility' property against the\n\t"
		    "appropriate file in " ZPOOL_SYSCONF_COMPAT_D " or "
		    ZPOOL_DATA_COMPAT_D ".\n"));
		break;

	case ZPOOL_STATUS_INCOMPATIBLE_FEAT:
		snprintf(status, ST_SIZE, gettext("One or more features "
		    "are enabled on the pool despite not being\n\t"
		    "requested by the 'compatibility' property.\n"));
		snprintf(action, AC_SIZE, gettext("Consider setting "
		    "'compatibility' to an appropriate value, or\n\t"
		    "adding needed features to the relevant file in\n\t"
		    ZPOOL_SYSCONF_COMPAT_D " or " ZPOOL_DATA_COMPAT_D ".\n"));
		break;

	case ZPOOL_STATUS_UNSUP_FEAT_READ:
		snprintf(status, ST_SIZE, gettext("The pool cannot be accessed "
		    "on this system because it uses the\n\tfollowing feature(s)"
		    " not supported on this system:\n"));
		zpool_collect_unsup_feat(zpool_get_config(zhp, NULL), status,
		    1024);
		snprintf(action, AC_SIZE, gettext("Access the pool from a "
		    "system that supports the required feature(s),\n\tor "
		    "restore the pool from backup.\n"));
		break;

	case ZPOOL_STATUS_UNSUP_FEAT_WRITE:
		snprintf(status, ST_SIZE, gettext("The pool can only be "
		    "accessed in read-only mode on this system. It\n\tcannot be"
		    " accessed in read-write mode because it uses the "
		    "following\n\tfeature(s) not supported on this system:\n"));
		zpool_collect_unsup_feat(zpool_get_config(zhp, NULL), status,
		    1024);
		snprintf(action, AC_SIZE, gettext("The pool cannot be accessed "
		    "in read-write mode. Import the pool with\n"
		    "\t\"-o readonly=on\", access the pool from a system that "
		    "supports the\n\trequired feature(s), or restore the "
		    "pool from backup.\n"));
		break;

	case ZPOOL_STATUS_FAULTED_DEV_R:
		snprintf(status, ST_SIZE, gettext("One or more devices are "
		    "faulted in response to persistent errors.\n\tSufficient "
		    "replicas exist for the pool to continue functioning "
		    "in a\n\tdegraded state.\n"));
		snprintf(action, AC_SIZE, gettext("Replace the faulted device, "
		    "or use 'zpool clear' to mark the device\n\trepaired.\n"));
		break;

	case ZPOOL_STATUS_FAULTED_DEV_NR:
		snprintf(status, ST_SIZE, gettext("One or more devices are "
		    "faulted in response to persistent errors.  There are "
		    "insufficient replicas for the pool to\n\tcontinue "
		    "functioning.\n"));
		snprintf(action, AC_SIZE, gettext("Destroy and re-create the "
		    "pool from a backup source.  Manually marking the device\n"
		    "\trepaired using 'zpool clear' may allow some data "
		    "to be recovered.\n"));
		break;

	case ZPOOL_STATUS_IO_FAILURE_MMP:
		snprintf(status, ST_SIZE, gettext("The pool is suspended "
		    "because multihost writes failed or were delayed;\n\t"
		    "another system could import the pool undetected.\n"));
		snprintf(action, AC_SIZE, gettext("Make sure the pool's devices"
		    " are connected, then reboot your system and\n\timport the "
		    "pool or run 'zpool clear' to resume the pool.\n"));
		break;

	case ZPOOL_STATUS_IO_FAILURE_WAIT:
	case ZPOOL_STATUS_IO_FAILURE_CONTINUE:
		snprintf(status, ST_SIZE, gettext("One or more devices are "
		    "faulted in response to IO failures.\n"));
		snprintf(action, AC_SIZE, gettext("Make sure the affected "
		    "devices are connected, then run 'zpool clear'.\n"));
		break;

	case ZPOOL_STATUS_BAD_LOG:
		snprintf(status, ST_SIZE, gettext("An intent log record "
		    "could not be read.\n"
		    "\tWaiting for administrator intervention to fix the "
		    "faulted pool.\n"));
		snprintf(action, AC_SIZE, gettext("Either restore the affected "
		    "device(s) and run 'zpool online',\n"
		    "\tor ignore the intent log records by running "
		    "'zpool clear'.\n"));
		break;

	case ZPOOL_STATUS_NON_NATIVE_ASHIFT:
		snprintf(status, ST_SIZE, gettext("One or more devices are "
		    "configured to use a non-native block size.\n"
		    "\tExpect reduced performance.\n"));
		snprintf(action, AC_SIZE, gettext("Replace affected devices "
		    "with devices that support the\n\tconfigured block size, "
		    "or migrate data to a properly configured\n\tpool.\n"));
		break;

	case ZPOOL_STATUS_HOSTID_MISMATCH:
		snprintf(status, ST_SIZE, gettext("Mismatch between pool hostid"
		    " and system hostid on imported pool.\n\tThis pool was "
		    "previously imported into a system with a different "
		    "hostid,\n\tand then was verbatim imported into this "
		    "system.\n"));
		snprintf(action, AC_SIZE, gettext("Export this pool on all "
		    "systems on which it is imported.\n"
		    "\tThen import it to correct the mismatch.\n"));
		break;

	case ZPOOL_STATUS_ERRATA:
		snprintf(status, ST_SIZE, gettext("Errata #%d detected.\n"),
		    errata);
		switch (errata) {
		case ZPOOL_ERRATA_NONE:
			break;

		case ZPOOL_ERRATA_ZOL_2094_SCRUB:
			snprintf(action, AC_SIZE, gettext("To correct the issue"
			    " run 'zpool scrub'.\n"));
			break;

		case ZPOOL_ERRATA_ZOL_6845_ENCRYPTION:
			(void) strlcat(status, gettext("\tExisting encrypted "
			    "datasets contain an on-disk incompatibility\n\t "
			    "which needs to be corrected.\n"), ST_SIZE);
			snprintf(action, AC_SIZE, gettext("To correct the issue"
			    " backup existing encrypted datasets to new\n\t"
			    "encrypted datasets and destroy the old ones. "
			    "'zfs mount -o ro' can\n\tbe used to temporarily "
			    "mount existing encrypted datasets readonly.\n"));
			break;

		case ZPOOL_ERRATA_ZOL_8308_ENCRYPTION:
			(void) strlcat(status, gettext("\tExisting encrypted "
			    "snapshots and bookmarks contain an on-disk\n\t"
			    "incompatibility. This may cause on-disk "
			    "corruption if they are used\n\twith "
			    "'zfs recv'.\n"), ST_SIZE);
			snprintf(action, AC_SIZE, gettext("To correct the"
			    "issue, enable the bookmark_v2 feature. No "
			    "additional\n\taction is needed if there are no "
			    "encrypted snapshots or bookmarks.\n\tIf preserving"
			    "the encrypted snapshots and bookmarks is required,"
			    " use\n\ta non-raw send to backup and restore them."
			    " Alternately, they may be\n\tremoved to resolve "
			    "the incompatibility.\n"));
			break;

		default:
			/*
			 * All errata which allow the pool to be imported
			 * must contain an action message.
			 */
			assert(0);
		}
		break;

	default:
		/*
		 * The remaining errors can't actually be generated, yet.
		 */
		assert(reason == ZPOOL_STATUS_OK);
	}

	if (status[0] != 0) {
		if (cbp->cb_json)
			fnvlist_add_string(item, "status", status);
		else {
			printf_color(ANSI_BOLD, gettext("status: "));
			printf_color(ANSI_YELLOW, status);
		}
	}

	if (action[0] != 0) {
		if (cbp->cb_json)
			fnvlist_add_string(item, "action", action);
		else {
			printf_color(ANSI_BOLD, gettext("action: "));
			printf_color(ANSI_YELLOW, action);
		}
	}
}

static int
status_callback_json(zpool_handle_t *zhp, void *data)
{
	status_cbdata_t *cbp = data;
	nvlist_t *config, *nvroot;
	const char *msgid;
	char pool_guid[256];
	char msgbuf[256];
	uint64_t guid;
	zpool_status_t reason;
	zpool_errata_t errata;
	uint_t c;
	vdev_stat_t *vs;
	nvlist_t *item, *d, *load_info, *vds;

	/* If dedup stats were requested, also fetch dedupcached. */
	if (cbp->cb_dedup_stats > 1)
		zpool_add_propname(zhp, ZPOOL_DEDUPCACHED_PROP_NAME);
	reason = zpool_get_status(zhp, &msgid, &errata);
	/*
	 * If we were given 'zpool status -x', only report those pools with
	 * problems.
	 */
	if (cbp->cb_explain &&
	    (reason == ZPOOL_STATUS_OK ||
	    reason == ZPOOL_STATUS_VERSION_OLDER ||
	    reason == ZPOOL_STATUS_FEAT_DISABLED ||
	    reason == ZPOOL_STATUS_COMPATIBILITY_ERR ||
	    reason == ZPOOL_STATUS_INCOMPATIBLE_FEAT)) {
		return (0);
	}

	d = fnvlist_lookup_nvlist(cbp->cb_jsobj, "pools");
	item = fnvlist_alloc();
	vds = fnvlist_alloc();
	fill_pool_info(item, zhp, B_FALSE, cbp->cb_json_as_int);
	config = zpool_get_config(zhp, NULL);

	if (config != NULL) {
		nvroot = fnvlist_lookup_nvlist(config, ZPOOL_CONFIG_VDEV_TREE);
		verify(nvlist_lookup_uint64_array(nvroot,
		    ZPOOL_CONFIG_VDEV_STATS, (uint64_t **)&vs, &c) == 0);
		if (cbp->cb_json_pool_key_guid) {
			guid = fnvlist_lookup_uint64(config,
			    ZPOOL_CONFIG_POOL_GUID);
			snprintf(pool_guid, 256, "%llu", (u_longlong_t)guid);
		}
		cbp->cb_count++;

		print_status_reason(zhp, cbp, reason, errata, item);
		if (msgid != NULL) {
			snprintf(msgbuf, 256,
			    "https://openzfs.github.io/openzfs-docs/msg/%s",
			    msgid);
			fnvlist_add_string(item, "msgid", msgid);
			fnvlist_add_string(item, "moreinfo", msgbuf);
		}

		if (nvlist_lookup_nvlist(config, ZPOOL_CONFIG_LOAD_INFO,
		    &load_info) == 0) {
			fnvlist_add_nvlist(item, ZPOOL_CONFIG_LOAD_INFO,
			    load_info);
		}

		scan_status_nvlist(zhp, cbp, nvroot, item);
		removal_status_nvlist(zhp, cbp, nvroot, item);
		checkpoint_status_nvlist(nvroot, cbp, item);
		raidz_expand_status_nvlist(zhp, cbp, nvroot, item);
		vdev_stats_nvlist(zhp, cbp, nvroot, 0, B_FALSE, NULL, vds);
		if (cbp->cb_flat_vdevs) {
			class_vdevs_nvlist(zhp, cbp, nvroot,
			    VDEV_ALLOC_BIAS_DEDUP, vds);
			class_vdevs_nvlist(zhp, cbp, nvroot,
			    VDEV_ALLOC_BIAS_SPECIAL, vds);
			class_vdevs_nvlist(zhp, cbp, nvroot,
			    VDEV_ALLOC_CLASS_LOGS, vds);
			l2cache_nvlist(zhp, cbp, nvroot, vds);
			spares_nvlist(zhp, cbp, nvroot, vds);

			fnvlist_add_nvlist(item, "vdevs", vds);
			fnvlist_free(vds);
		} else {
			fnvlist_add_nvlist(item, "vdevs", vds);
			fnvlist_free(vds);

			class_vdevs_nvlist(zhp, cbp, nvroot,
			    VDEV_ALLOC_BIAS_DEDUP, item);
			class_vdevs_nvlist(zhp, cbp, nvroot,
			    VDEV_ALLOC_BIAS_SPECIAL, item);
			class_vdevs_nvlist(zhp, cbp, nvroot,
			    VDEV_ALLOC_CLASS_LOGS, item);
			l2cache_nvlist(zhp, cbp, nvroot, item);
			spares_nvlist(zhp, cbp, nvroot, item);
		}
		dedup_stats_nvlist(zhp, cbp, item);
		errors_nvlist(zhp, cbp, item);
	}
	if (cbp->cb_json_pool_key_guid) {
		fnvlist_add_nvlist(d, pool_guid, item);
	} else {
		fnvlist_add_nvlist(d, zpool_get_name(zhp),
		    item);
	}
	fnvlist_free(item);
	return (0);
}

/*
 * Display a summary of pool status.  Displays a summary such as:
 *
 *        pool: tank
 *	status: DEGRADED
 *	reason: One or more devices ...
 *         see: https://openzfs.github.io/openzfs-docs/msg/ZFS-xxxx-01
 *	config:
 *		mirror		DEGRADED
 *                c1t0d0	OK
 *                c2t0d0	UNAVAIL
 *
 * When given the '-v' option, we print out the complete config.  If the '-e'
 * option is specified, then we print out error rate information as well.
 */
static int
status_callback(zpool_handle_t *zhp, void *data)
{
	status_cbdata_t *cbp = data;
	nvlist_t *config, *nvroot;
	const char *msgid;
	zpool_status_t reason;
	zpool_errata_t errata;
	const char *health;
	uint_t c;
	vdev_stat_t *vs;

	/* If dedup stats were requested, also fetch dedupcached. */
	if (cbp->cb_dedup_stats > 1)
		zpool_add_propname(zhp, ZPOOL_DEDUPCACHED_PROP_NAME);

	config = zpool_get_config(zhp, NULL);
	reason = zpool_get_status(zhp, &msgid, &errata);

	cbp->cb_count++;

	/*
	 * If we were given 'zpool status -x', only report those pools with
	 * problems.
	 */
	if (cbp->cb_explain &&
	    (reason == ZPOOL_STATUS_OK ||
	    reason == ZPOOL_STATUS_VERSION_OLDER ||
	    reason == ZPOOL_STATUS_FEAT_DISABLED ||
	    reason == ZPOOL_STATUS_COMPATIBILITY_ERR ||
	    reason == ZPOOL_STATUS_INCOMPATIBLE_FEAT)) {
		if (!cbp->cb_allpools) {
			(void) printf(gettext("pool '%s' is healthy\n"),
			    zpool_get_name(zhp));
			if (cbp->cb_first)
				cbp->cb_first = B_FALSE;
		}
		return (0);
	}

	if (cbp->cb_first)
		cbp->cb_first = B_FALSE;
	else
		(void) printf("\n");

	nvroot = fnvlist_lookup_nvlist(config, ZPOOL_CONFIG_VDEV_TREE);
	verify(nvlist_lookup_uint64_array(nvroot, ZPOOL_CONFIG_VDEV_STATS,
	    (uint64_t **)&vs, &c) == 0);

	health = zpool_get_state_str(zhp);

	printf("  ");
	printf_color(ANSI_BOLD, gettext("pool:"));
	printf(" %s\n", zpool_get_name(zhp));
	fputc(' ', stdout);
	printf_color(ANSI_BOLD, gettext("state: "));

	printf_color(health_str_to_color(health), "%s", health);

	fputc('\n', stdout);
	print_status_reason(zhp, cbp, reason, errata, NULL);

	if (msgid != NULL) {
		printf("   ");
		printf_color(ANSI_BOLD, gettext("see:"));
		printf(gettext(
		    " https://openzfs.github.io/openzfs-docs/msg/%s\n"),
		    msgid);
	}

	if (config != NULL) {
		uint64_t nerr;
		nvlist_t **spares, **l2cache;
		uint_t nspares, nl2cache;

		print_scan_status(zhp, nvroot);

		pool_removal_stat_t *prs = NULL;
		(void) nvlist_lookup_uint64_array(nvroot,
		    ZPOOL_CONFIG_REMOVAL_STATS, (uint64_t **)&prs, &c);
		print_removal_status(zhp, prs);

		pool_checkpoint_stat_t *pcs = NULL;
		(void) nvlist_lookup_uint64_array(nvroot,
		    ZPOOL_CONFIG_CHECKPOINT_STATS, (uint64_t **)&pcs, &c);
		print_checkpoint_status(pcs);

		pool_raidz_expand_stat_t *pres = NULL;
		(void) nvlist_lookup_uint64_array(nvroot,
		    ZPOOL_CONFIG_RAIDZ_EXPAND_STATS, (uint64_t **)&pres, &c);
		print_raidz_expand_status(zhp, pres);

		cbp->cb_namewidth = max_width(zhp, nvroot, 0, 0,
		    cbp->cb_name_flags | VDEV_NAME_TYPE_ID);
		if (cbp->cb_namewidth < 10)
			cbp->cb_namewidth = 10;

		color_start(ANSI_BOLD);
		(void) printf(gettext("config:\n\n"));
		(void) printf(gettext("\t%-*s  %-8s %5s %5s %5s"),
		    cbp->cb_namewidth, "NAME", "STATE", "READ", "WRITE",
		    "CKSUM");
		color_end();

		if (cbp->cb_print_slow_ios) {
			printf_color(ANSI_BOLD, " %5s", gettext("SLOW"));
		}

		if (cbp->cb_print_power) {
			printf_color(ANSI_BOLD, " %5s", gettext("POWER"));
		}

		if (cbp->cb_print_dio_verify) {
			printf_color(ANSI_BOLD, " %5s", gettext("DIO"));
		}

		if (cbp->vcdl != NULL)
			print_cmd_columns(cbp->vcdl, 0);

		printf("\n");

		print_status_config(zhp, cbp, zpool_get_name(zhp), nvroot, 0,
		    B_FALSE, NULL);

		print_class_vdevs(zhp, cbp, nvroot, VDEV_ALLOC_BIAS_DEDUP);
		print_class_vdevs(zhp, cbp, nvroot, VDEV_ALLOC_BIAS_SPECIAL);
		print_class_vdevs(zhp, cbp, nvroot, VDEV_ALLOC_CLASS_LOGS);

		if (nvlist_lookup_nvlist_array(nvroot, ZPOOL_CONFIG_L2CACHE,
		    &l2cache, &nl2cache) == 0)
			print_l2cache(zhp, cbp, l2cache, nl2cache);

		if (nvlist_lookup_nvlist_array(nvroot, ZPOOL_CONFIG_SPARES,
		    &spares, &nspares) == 0)
			print_spares(zhp, cbp, spares, nspares);

		if (nvlist_lookup_uint64(config, ZPOOL_CONFIG_ERRCOUNT,
		    &nerr) == 0) {
			(void) printf("\n");
			if (nerr == 0) {
				(void) printf(gettext(
				    "errors: No known data errors\n"));
			} else if (!cbp->cb_verbose) {
				color_start(ANSI_RED);
				(void) printf(gettext("errors: %llu data "
				    "errors, use '-v' for a list\n"),
				    (u_longlong_t)nerr);
				color_end();
			} else {
				print_error_log(zhp);
			}
		}

		if (cbp->cb_dedup_stats)
			print_dedup_stats(zhp, config, cbp->cb_literal);
	} else {
		(void) printf(gettext("config: The configuration cannot be "
		    "determined.\n"));
	}

	return (0);
}

/*
 * zpool status [-dDegiLpPstvx] [-c [script1,script2,...]] ...
 * 				[-j|--json [--json-flat-vdevs] [--json-int] ...
 * 				[--json-pool-key-guid]] [--power] [-T d|u] ...
 * 				[pool] [interval [count]]
 *
 *	-c CMD	For each vdev, run command CMD
 *	-D	Display dedup status (undocumented)
 *	-d	Display Direct I/O write verify errors
 *	-e	Display only unhealthy vdevs
 *	-g	Display guid for individual vdev name.
 *	-i	Display vdev initialization status.
 *	-j [...]	Display output in JSON format
 *	   --json-flat-vdevs Display vdevs in flat hierarchy
 *	   --json-int Display numbers in integer format instead of string
 *	   --json-pool-key-guid Use pool GUID as key for pool objects
 *	-L	Follow links when resolving vdev path name.
 *	-P	Display full path for vdev name.
 *	-p	Display values in parsable (exact) format.
 *	--power	Display vdev enclosure slot power status
 *	-s	Display slow IOs column.
 *	-T	Display a timestamp in date(1) or Unix format
 *	-t	Display vdev TRIM status.
 *	-v	Display complete error logs
 *	-x	Display only pools with potential problems
 *
 * Describes the health status of all pools or some subset.
 */
int
zpool_do_status(int argc, char **argv)
{
	int c;
	int ret;
	float interval = 0;
	unsigned long count = 0;
	status_cbdata_t cb = { 0 };
	nvlist_t *data;
	char *cmd = NULL;

	struct option long_options[] = {
		{"power", no_argument, NULL, ZPOOL_OPTION_POWER},
		{"json", no_argument, NULL, 'j'},
		{"json-int", no_argument, NULL, ZPOOL_OPTION_JSON_NUMS_AS_INT},
		{"json-flat-vdevs", no_argument, NULL,
		    ZPOOL_OPTION_JSON_FLAT_VDEVS},
		{"json-pool-key-guid", no_argument, NULL,
		    ZPOOL_OPTION_POOL_KEY_GUID},
		{0, 0, 0, 0}
	};

	/* check options */
	while ((c = getopt_long(argc, argv, "c:jdDegiLpPstT:vx", long_options,
	    NULL)) != -1) {
		switch (c) {
		case 'c':
			if (cmd != NULL) {
				fprintf(stderr,
				    gettext("Can't set -c flag twice\n"));
				exit(1);
			}

			if (getenv("ZPOOL_SCRIPTS_ENABLED") != NULL &&
			    !libzfs_envvar_is_set("ZPOOL_SCRIPTS_ENABLED")) {
				fprintf(stderr, gettext(
				    "Can't run -c, disabled by "
				    "ZPOOL_SCRIPTS_ENABLED.\n"));
				exit(1);
			}

			if ((getuid() <= 0 || geteuid() <= 0) &&
			    !libzfs_envvar_is_set("ZPOOL_SCRIPTS_AS_ROOT")) {
				fprintf(stderr, gettext(
				    "Can't run -c with root privileges "
				    "unless ZPOOL_SCRIPTS_AS_ROOT is set.\n"));
				exit(1);
			}
			cmd = optarg;
			break;
		case 'd':
			cb.cb_print_dio_verify = B_TRUE;
			break;
		case 'D':
			if (++cb.cb_dedup_stats  > 2)
				cb.cb_dedup_stats = 2;
			break;
		case 'e':
			cb.cb_print_unhealthy = B_TRUE;
			break;
		case 'g':
			cb.cb_name_flags |= VDEV_NAME_GUID;
			break;
		case 'i':
			cb.cb_print_vdev_init = B_TRUE;
			break;
		case 'L':
			cb.cb_name_flags |= VDEV_NAME_FOLLOW_LINKS;
			break;
		case 'p':
			cb.cb_literal = B_TRUE;
			break;
		case 'P':
			cb.cb_name_flags |= VDEV_NAME_PATH;
			break;
		case 's':
			cb.cb_print_slow_ios = B_TRUE;
			break;
		case 't':
			cb.cb_print_vdev_trim = B_TRUE;
			break;
		case 'T':
			get_timestamp_arg(*optarg);
			break;
		case 'v':
			cb.cb_verbose = B_TRUE;
			break;
		case 'j':
			cb.cb_json = B_TRUE;
			break;
		case 'x':
			cb.cb_explain = B_TRUE;
			break;
		case ZPOOL_OPTION_POWER:
			cb.cb_print_power = B_TRUE;
			break;
		case ZPOOL_OPTION_JSON_FLAT_VDEVS:
			cb.cb_flat_vdevs = B_TRUE;
			break;
		case ZPOOL_OPTION_JSON_NUMS_AS_INT:
			cb.cb_json_as_int = B_TRUE;
			cb.cb_literal = B_TRUE;
			break;
		case ZPOOL_OPTION_POOL_KEY_GUID:
			cb.cb_json_pool_key_guid = B_TRUE;
			break;
		case '?':
			if (optopt == 'c') {
				print_zpool_script_list("status");
				exit(0);
			} else {
				fprintf(stderr,
				    gettext("invalid option '%c'\n"), optopt);
			}
			usage(B_FALSE);
		}
	}

	argc -= optind;
	argv += optind;

	get_interval_count(&argc, argv, &interval, &count);

	if (argc == 0)
		cb.cb_allpools = B_TRUE;

	cb.cb_first = B_TRUE;
	cb.cb_print_status = B_TRUE;

	if (cb.cb_flat_vdevs && !cb.cb_json) {
		fprintf(stderr, gettext("'--json-flat-vdevs' only works with"
		    " '-j' option\n"));
		usage(B_FALSE);
	}

	if (cb.cb_json_as_int && !cb.cb_json) {
		(void) fprintf(stderr, gettext("'--json-int' only works with"
		    " '-j' option\n"));
		usage(B_FALSE);
	}

	if (!cb.cb_json && cb.cb_json_pool_key_guid) {
		(void) fprintf(stderr, gettext("'json-pool-key-guid' only"
		    " works with '-j' option\n"));
		usage(B_FALSE);
	}

	for (;;) {
		if (cb.cb_json) {
			cb.cb_jsobj = zpool_json_schema(0, 1);
			data = fnvlist_alloc();
			fnvlist_add_nvlist(cb.cb_jsobj, "pools", data);
			fnvlist_free(data);
		}

		if (timestamp_fmt != NODATE) {
			if (cb.cb_json) {
				if (cb.cb_json_as_int) {
					fnvlist_add_uint64(cb.cb_jsobj, "time",
					    time(NULL));
				} else {
					char ts[128];
					get_timestamp(timestamp_fmt, ts, 128);
					fnvlist_add_string(cb.cb_jsobj, "time",
					    ts);
				}
			} else
				print_timestamp(timestamp_fmt);
		}

		if (cmd != NULL)
			cb.vcdl = all_pools_for_each_vdev_run(argc, argv, cmd,
			    NULL, NULL, 0, 0);

		if (cb.cb_json) {
			ret = for_each_pool(argc, argv, B_TRUE, NULL,
			    ZFS_TYPE_POOL, cb.cb_literal,
			    status_callback_json, &cb);
		} else {
			ret = for_each_pool(argc, argv, B_TRUE, NULL,
			    ZFS_TYPE_POOL, cb.cb_literal,
			    status_callback, &cb);
		}

		if (cb.vcdl != NULL)
			free_vdev_cmd_data_list(cb.vcdl);

		if (cb.cb_json) {
			if (ret == 0)
				zcmd_print_json(cb.cb_jsobj);
			else
				nvlist_free(cb.cb_jsobj);
		} else {
			if (argc == 0 && cb.cb_count == 0) {
				(void) fprintf(stderr, "%s",
				    gettext("no pools available\n"));
			} else if (cb.cb_explain && cb.cb_first &&
			    cb.cb_allpools) {
				(void) printf("%s",
				    gettext("all pools are healthy\n"));
			}
		}

		if (ret != 0)
			return (ret);

		if (interval == 0)
			break;

		if (count != 0 && --count == 0)
			break;

		(void) fflush(stdout);
		(void) fsleep(interval);
	}

	return (0);
}

typedef struct upgrade_cbdata {
	int	cb_first;
	int	cb_argc;
	uint64_t cb_version;
	char	**cb_argv;
} upgrade_cbdata_t;

static int
check_unsupp_fs(zfs_handle_t *zhp, void *unsupp_fs)
{
	int zfs_version = (int)zfs_prop_get_int(zhp, ZFS_PROP_VERSION);
	int *count = (int *)unsupp_fs;

	if (zfs_version > ZPL_VERSION) {
		(void) printf(gettext("%s (v%d) is not supported by this "
		    "implementation of ZFS.\n"),
		    zfs_get_name(zhp), zfs_version);
		(*count)++;
	}

	zfs_iter_filesystems_v2(zhp, 0, check_unsupp_fs, unsupp_fs);

	zfs_close(zhp);

	return (0);
}

static int
upgrade_version(zpool_handle_t *zhp, uint64_t version)
{
	int ret;
	nvlist_t *config;
	uint64_t oldversion;
	int unsupp_fs = 0;

	config = zpool_get_config(zhp, NULL);
	verify(nvlist_lookup_uint64(config, ZPOOL_CONFIG_VERSION,
	    &oldversion) == 0);

	char compat[ZFS_MAXPROPLEN];
	if (zpool_get_prop(zhp, ZPOOL_PROP_COMPATIBILITY, compat,
	    ZFS_MAXPROPLEN, NULL, B_FALSE) != 0)
		compat[0] = '\0';

	assert(SPA_VERSION_IS_SUPPORTED(oldversion));
	assert(oldversion < version);

	ret = zfs_iter_root(zpool_get_handle(zhp), check_unsupp_fs, &unsupp_fs);
	if (ret != 0)
		return (ret);

	if (unsupp_fs) {
		(void) fprintf(stderr, gettext("Upgrade not performed due "
		    "to %d unsupported filesystems (max v%d).\n"),
		    unsupp_fs, (int)ZPL_VERSION);
		return (1);
	}

	if (strcmp(compat, ZPOOL_COMPAT_LEGACY) == 0) {
		(void) fprintf(stderr, gettext("Upgrade not performed because "
		    "'compatibility' property set to '"
		    ZPOOL_COMPAT_LEGACY "'.\n"));
		return (1);
	}

	ret = zpool_upgrade(zhp, version);
	if (ret != 0)
		return (ret);

	if (version >= SPA_VERSION_FEATURES) {
		(void) printf(gettext("Successfully upgraded "
		    "'%s' from version %llu to feature flags.\n"),
		    zpool_get_name(zhp), (u_longlong_t)oldversion);
	} else {
		(void) printf(gettext("Successfully upgraded "
		    "'%s' from version %llu to version %llu.\n"),
		    zpool_get_name(zhp), (u_longlong_t)oldversion,
		    (u_longlong_t)version);
	}

	return (0);
}

static int
upgrade_enable_all(zpool_handle_t *zhp, int *countp)
{
	int i, ret, count;
	boolean_t firstff = B_TRUE;
	nvlist_t *enabled = zpool_get_features(zhp);

	char compat[ZFS_MAXPROPLEN];
	if (zpool_get_prop(zhp, ZPOOL_PROP_COMPATIBILITY, compat,
	    ZFS_MAXPROPLEN, NULL, B_FALSE) != 0)
		compat[0] = '\0';

	boolean_t requested_features[SPA_FEATURES];
	if (zpool_do_load_compat(compat, requested_features) !=
	    ZPOOL_COMPATIBILITY_OK)
		return (-1);

	count = 0;
	for (i = 0; i < SPA_FEATURES; i++) {
		const char *fname = spa_feature_table[i].fi_uname;
		const char *fguid = spa_feature_table[i].fi_guid;

		if (!spa_feature_table[i].fi_zfs_mod_supported ||
		    (spa_feature_table[i].fi_flags & ZFEATURE_FLAG_NO_UPGRADE))
			continue;

		if (!nvlist_exists(enabled, fguid) && requested_features[i]) {
			char *propname;
			verify(-1 != asprintf(&propname, "feature@%s", fname));
			ret = zpool_set_prop(zhp, propname,
			    ZFS_FEATURE_ENABLED);
			if (ret != 0) {
				free(propname);
				return (ret);
			}
			count++;

			if (firstff) {
				(void) printf(gettext("Enabled the "
				    "following features on '%s':\n"),
				    zpool_get_name(zhp));
				firstff = B_FALSE;
			}
			(void) printf(gettext("  %s\n"), fname);
			free(propname);
		}
	}

	if (countp != NULL)
		*countp = count;
	return (0);
}

static int
upgrade_cb(zpool_handle_t *zhp, void *arg)
{
	upgrade_cbdata_t *cbp = arg;
	nvlist_t *config;
	uint64_t version;
	boolean_t modified_pool = B_FALSE;
	int ret;

	config = zpool_get_config(zhp, NULL);
	verify(nvlist_lookup_uint64(config, ZPOOL_CONFIG_VERSION,
	    &version) == 0);

	assert(SPA_VERSION_IS_SUPPORTED(version));

	if (version < cbp->cb_version) {
		cbp->cb_first = B_FALSE;
		ret = upgrade_version(zhp, cbp->cb_version);
		if (ret != 0)
			return (ret);
		modified_pool = B_TRUE;

		/*
		 * If they did "zpool upgrade -a", then we could
		 * be doing ioctls to different pools.  We need
		 * to log this history once to each pool, and bypass
		 * the normal history logging that happens in main().
		 */
		(void) zpool_log_history(g_zfs, history_str);
		log_history = B_FALSE;
	}

	if (cbp->cb_version >= SPA_VERSION_FEATURES) {
		int count;
		ret = upgrade_enable_all(zhp, &count);
		if (ret != 0)
			return (ret);

		if (count > 0) {
			cbp->cb_first = B_FALSE;
			modified_pool = B_TRUE;
		}
	}

	if (modified_pool) {
		(void) printf("\n");
		(void) after_zpool_upgrade(zhp);
	}

	return (0);
}

static int
upgrade_list_older_cb(zpool_handle_t *zhp, void *arg)
{
	upgrade_cbdata_t *cbp = arg;
	nvlist_t *config;
	uint64_t version;

	config = zpool_get_config(zhp, NULL);
	verify(nvlist_lookup_uint64(config, ZPOOL_CONFIG_VERSION,
	    &version) == 0);

	assert(SPA_VERSION_IS_SUPPORTED(version));

	if (version < SPA_VERSION_FEATURES) {
		if (cbp->cb_first) {
			(void) printf(gettext("The following pools are "
			    "formatted with legacy version numbers and can\n"
			    "be upgraded to use feature flags.  After "
			    "being upgraded, these pools\nwill no "
			    "longer be accessible by software that does not "
			    "support feature\nflags.\n\n"
			    "Note that setting a pool's 'compatibility' "
			    "feature to '" ZPOOL_COMPAT_LEGACY "' will\n"
			    "inhibit upgrades.\n\n"));
			(void) printf(gettext("VER  POOL\n"));
			(void) printf(gettext("---  ------------\n"));
			cbp->cb_first = B_FALSE;
		}

		(void) printf("%2llu   %s\n", (u_longlong_t)version,
		    zpool_get_name(zhp));
	}

	return (0);
}

static int
upgrade_list_disabled_cb(zpool_handle_t *zhp, void *arg)
{
	upgrade_cbdata_t *cbp = arg;
	nvlist_t *config;
	uint64_t version;

	config = zpool_get_config(zhp, NULL);
	verify(nvlist_lookup_uint64(config, ZPOOL_CONFIG_VERSION,
	    &version) == 0);

	if (version >= SPA_VERSION_FEATURES) {
		int i;
		boolean_t poolfirst = B_TRUE;
		nvlist_t *enabled = zpool_get_features(zhp);

		for (i = 0; i < SPA_FEATURES; i++) {
			const char *fguid = spa_feature_table[i].fi_guid;
			const char *fname = spa_feature_table[i].fi_uname;

			if (!spa_feature_table[i].fi_zfs_mod_supported)
				continue;

			if (!nvlist_exists(enabled, fguid)) {
				if (cbp->cb_first) {
					(void) printf(gettext("\nSome "
					    "supported features are not "
					    "enabled on the following pools. "
					    "Once a\nfeature is enabled the "
					    "pool may become incompatible with "
					    "software\nthat does not support "
					    "the feature. See "
					    "zpool-features(7) for "
					    "details.\n\n"
					    "Note that the pool "
					    "'compatibility' feature can be "
					    "used to inhibit\nfeature "
					    "upgrades.\n\n"
					    "Features marked with (*) are not "
					    "applied automatically on upgrade, "
					    "and\nmust be applied explicitly "
					    "with zpool-set(7).\n\n"));
					(void) printf(gettext("POOL  "
					    "FEATURE\n"));
					(void) printf(gettext("------"
					    "---------\n"));
					cbp->cb_first = B_FALSE;
				}

				if (poolfirst) {
					(void) printf(gettext("%s\n"),
					    zpool_get_name(zhp));
					poolfirst = B_FALSE;
				}

				(void) printf(gettext("      %s%s\n"), fname,
				    spa_feature_table[i].fi_flags &
				    ZFEATURE_FLAG_NO_UPGRADE ? "(*)" : "");
			}
			/*
			 * If they did "zpool upgrade -a", then we could
			 * be doing ioctls to different pools.  We need
			 * to log this history once to each pool, and bypass
			 * the normal history logging that happens in main().
			 */
			(void) zpool_log_history(g_zfs, history_str);
			log_history = B_FALSE;
		}
	}

	return (0);
}

static int
upgrade_one(zpool_handle_t *zhp, void *data)
{
	boolean_t modified_pool = B_FALSE;
	upgrade_cbdata_t *cbp = data;
	uint64_t cur_version;
	int ret;

	if (strcmp("log", zpool_get_name(zhp)) == 0) {
		(void) fprintf(stderr, gettext("'log' is now a reserved word\n"
		    "Pool 'log' must be renamed using export and import"
		    " to upgrade.\n"));
		return (1);
	}

	cur_version = zpool_get_prop_int(zhp, ZPOOL_PROP_VERSION, NULL);
	if (cur_version > cbp->cb_version) {
		(void) printf(gettext("Pool '%s' is already formatted "
		    "using more current version '%llu'.\n\n"),
		    zpool_get_name(zhp), (u_longlong_t)cur_version);
		return (0);
	}

	if (cbp->cb_version != SPA_VERSION && cur_version == cbp->cb_version) {
		(void) printf(gettext("Pool '%s' is already formatted "
		    "using version %llu.\n\n"), zpool_get_name(zhp),
		    (u_longlong_t)cbp->cb_version);
		return (0);
	}

	if (cur_version != cbp->cb_version) {
		modified_pool = B_TRUE;
		ret = upgrade_version(zhp, cbp->cb_version);
		if (ret != 0)
			return (ret);
	}

	if (cbp->cb_version >= SPA_VERSION_FEATURES) {
		int count = 0;
		ret = upgrade_enable_all(zhp, &count);
		if (ret != 0)
			return (ret);

		if (count != 0) {
			modified_pool = B_TRUE;
		} else if (cur_version == SPA_VERSION) {
			(void) printf(gettext("Pool '%s' already has all "
			    "supported and requested features enabled.\n"),
			    zpool_get_name(zhp));
		}
	}

	if (modified_pool) {
		(void) printf("\n");
		(void) after_zpool_upgrade(zhp);
	}

	return (0);
}

/*
 * zpool upgrade
 * zpool upgrade -v
 * zpool upgrade [-V version] <-a | pool ...>
 *
 * With no arguments, display downrev'd ZFS pool available for upgrade.
 * Individual pools can be upgraded by specifying the pool, and '-a' will
 * upgrade all pools.
 */
int
zpool_do_upgrade(int argc, char **argv)
{
	int c;
	upgrade_cbdata_t cb = { 0 };
	int ret = 0;
	boolean_t showversions = B_FALSE;
	boolean_t upgradeall = B_FALSE;
	char *end;


	/* check options */
	while ((c = getopt(argc, argv, ":avV:")) != -1) {
		switch (c) {
		case 'a':
			upgradeall = B_TRUE;
			break;
		case 'v':
			showversions = B_TRUE;
			break;
		case 'V':
			cb.cb_version = strtoll(optarg, &end, 10);
			if (*end != '\0' ||
			    !SPA_VERSION_IS_SUPPORTED(cb.cb_version)) {
				(void) fprintf(stderr,
				    gettext("invalid version '%s'\n"), optarg);
				usage(B_FALSE);
			}
			break;
		case ':':
			(void) fprintf(stderr, gettext("missing argument for "
			    "'%c' option\n"), optopt);
			usage(B_FALSE);
			break;
		case '?':
			(void) fprintf(stderr, gettext("invalid option '%c'\n"),
			    optopt);
			usage(B_FALSE);
		}
	}

	cb.cb_argc = argc;
	cb.cb_argv = argv;
	argc -= optind;
	argv += optind;

	if (cb.cb_version == 0) {
		cb.cb_version = SPA_VERSION;
	} else if (!upgradeall && argc == 0) {
		(void) fprintf(stderr, gettext("-V option is "
		    "incompatible with other arguments\n"));
		usage(B_FALSE);
	}

	if (showversions) {
		if (upgradeall || argc != 0) {
			(void) fprintf(stderr, gettext("-v option is "
			    "incompatible with other arguments\n"));
			usage(B_FALSE);
		}
	} else if (upgradeall) {
		if (argc != 0) {
			(void) fprintf(stderr, gettext("-a option should not "
			    "be used along with a pool name\n"));
			usage(B_FALSE);
		}
	}

	(void) printf("%s", gettext("This system supports ZFS pool feature "
	    "flags.\n\n"));
	if (showversions) {
		int i;

		(void) printf(gettext("The following features are "
		    "supported:\n\n"));
		(void) printf(gettext("FEAT DESCRIPTION\n"));
		(void) printf("----------------------------------------------"
		    "---------------\n");
		for (i = 0; i < SPA_FEATURES; i++) {
			zfeature_info_t *fi = &spa_feature_table[i];
			if (!fi->fi_zfs_mod_supported)
				continue;
			const char *ro =
			    (fi->fi_flags & ZFEATURE_FLAG_READONLY_COMPAT) ?
			    " (read-only compatible)" : "";

			(void) printf("%-37s%s\n", fi->fi_uname, ro);
			(void) printf("     %s\n", fi->fi_desc);
		}
		(void) printf("\n");

		(void) printf(gettext("The following legacy versions are also "
		    "supported:\n\n"));
		(void) printf(gettext("VER  DESCRIPTION\n"));
		(void) printf("---  -----------------------------------------"
		    "---------------\n");
		(void) printf(gettext(" 1   Initial ZFS version\n"));
		(void) printf(gettext(" 2   Ditto blocks "
		    "(replicated metadata)\n"));
		(void) printf(gettext(" 3   Hot spares and double parity "
		    "RAID-Z\n"));
		(void) printf(gettext(" 4   zpool history\n"));
		(void) printf(gettext(" 5   Compression using the gzip "
		    "algorithm\n"));
		(void) printf(gettext(" 6   bootfs pool property\n"));
		(void) printf(gettext(" 7   Separate intent log devices\n"));
		(void) printf(gettext(" 8   Delegated administration\n"));
		(void) printf(gettext(" 9   refquota and refreservation "
		    "properties\n"));
		(void) printf(gettext(" 10  Cache devices\n"));
		(void) printf(gettext(" 11  Improved scrub performance\n"));
		(void) printf(gettext(" 12  Snapshot properties\n"));
		(void) printf(gettext(" 13  snapused property\n"));
		(void) printf(gettext(" 14  passthrough-x aclinherit\n"));
		(void) printf(gettext(" 15  user/group space accounting\n"));
		(void) printf(gettext(" 16  stmf property support\n"));
		(void) printf(gettext(" 17  Triple-parity RAID-Z\n"));
		(void) printf(gettext(" 18  Snapshot user holds\n"));
		(void) printf(gettext(" 19  Log device removal\n"));
		(void) printf(gettext(" 20  Compression using zle "
		    "(zero-length encoding)\n"));
		(void) printf(gettext(" 21  Deduplication\n"));
		(void) printf(gettext(" 22  Received properties\n"));
		(void) printf(gettext(" 23  Slim ZIL\n"));
		(void) printf(gettext(" 24  System attributes\n"));
		(void) printf(gettext(" 25  Improved scrub stats\n"));
		(void) printf(gettext(" 26  Improved snapshot deletion "
		    "performance\n"));
		(void) printf(gettext(" 27  Improved snapshot creation "
		    "performance\n"));
		(void) printf(gettext(" 28  Multiple vdev replacements\n"));
		(void) printf(gettext("\nFor more information on a particular "
		    "version, including supported releases,\n"));
		(void) printf(gettext("see the ZFS Administration Guide.\n\n"));
	} else if (argc == 0 && upgradeall) {
		cb.cb_first = B_TRUE;
		ret = zpool_iter(g_zfs, upgrade_cb, &cb);
		if (ret == 0 && cb.cb_first) {
			if (cb.cb_version == SPA_VERSION) {
				(void) printf(gettext("All pools are already "
				    "formatted using feature flags.\n\n"));
				(void) printf(gettext("Every feature flags "
				    "pool already has all supported and "
				    "requested features enabled.\n"));
			} else {
				(void) printf(gettext("All pools are already "
				    "formatted with version %llu or higher.\n"),
				    (u_longlong_t)cb.cb_version);
			}
		}
	} else if (argc == 0) {
		cb.cb_first = B_TRUE;
		ret = zpool_iter(g_zfs, upgrade_list_older_cb, &cb);
		assert(ret == 0);

		if (cb.cb_first) {
			(void) printf(gettext("All pools are formatted "
			    "using feature flags.\n\n"));
		} else {
			(void) printf(gettext("\nUse 'zpool upgrade -v' "
			    "for a list of available legacy versions.\n"));
		}

		cb.cb_first = B_TRUE;
		ret = zpool_iter(g_zfs, upgrade_list_disabled_cb, &cb);
		assert(ret == 0);

		if (cb.cb_first) {
			(void) printf(gettext("Every feature flags pool has "
			    "all supported and requested features enabled.\n"));
		} else {
			(void) printf(gettext("\n"));
		}
	} else {
		ret = for_each_pool(argc, argv, B_FALSE, NULL, ZFS_TYPE_POOL,
		    B_FALSE, upgrade_one, &cb);
	}

	return (ret);
}

typedef struct hist_cbdata {
	boolean_t first;
	boolean_t longfmt;
	boolean_t internal;
} hist_cbdata_t;

static void
print_history_records(nvlist_t *nvhis, hist_cbdata_t *cb)
{
	nvlist_t **records;
	uint_t numrecords;
	int i;

	verify(nvlist_lookup_nvlist_array(nvhis, ZPOOL_HIST_RECORD,
	    &records, &numrecords) == 0);
	for (i = 0; i < numrecords; i++) {
		nvlist_t *rec = records[i];
		char tbuf[64] = "";

		if (nvlist_exists(rec, ZPOOL_HIST_TIME)) {
			time_t tsec;
			struct tm t;

			tsec = fnvlist_lookup_uint64(records[i],
			    ZPOOL_HIST_TIME);
			(void) localtime_r(&tsec, &t);
			(void) strftime(tbuf, sizeof (tbuf), "%F.%T", &t);
		}

		if (nvlist_exists(rec, ZPOOL_HIST_ELAPSED_NS)) {
			uint64_t elapsed_ns = fnvlist_lookup_int64(records[i],
			    ZPOOL_HIST_ELAPSED_NS);
			(void) snprintf(tbuf + strlen(tbuf),
			    sizeof (tbuf) - strlen(tbuf),
			    " (%lldms)", (long long)elapsed_ns / 1000 / 1000);
		}

		if (nvlist_exists(rec, ZPOOL_HIST_CMD)) {
			(void) printf("%s %s", tbuf,
			    fnvlist_lookup_string(rec, ZPOOL_HIST_CMD));
		} else if (nvlist_exists(rec, ZPOOL_HIST_INT_EVENT)) {
			int ievent =
			    fnvlist_lookup_uint64(rec, ZPOOL_HIST_INT_EVENT);
			if (!cb->internal)
				continue;
			if (ievent >= ZFS_NUM_LEGACY_HISTORY_EVENTS) {
				(void) printf("%s unrecognized record:\n",
				    tbuf);
				dump_nvlist(rec, 4);
				continue;
			}
			(void) printf("%s [internal %s txg:%lld] %s", tbuf,
			    zfs_history_event_names[ievent],
			    (longlong_t)fnvlist_lookup_uint64(
			    rec, ZPOOL_HIST_TXG),
			    fnvlist_lookup_string(rec, ZPOOL_HIST_INT_STR));
		} else if (nvlist_exists(rec, ZPOOL_HIST_INT_NAME)) {
			if (!cb->internal)
				continue;
			(void) printf("%s [txg:%lld] %s", tbuf,
			    (longlong_t)fnvlist_lookup_uint64(
			    rec, ZPOOL_HIST_TXG),
			    fnvlist_lookup_string(rec, ZPOOL_HIST_INT_NAME));
			if (nvlist_exists(rec, ZPOOL_HIST_DSNAME)) {
				(void) printf(" %s (%llu)",
				    fnvlist_lookup_string(rec,
				    ZPOOL_HIST_DSNAME),
				    (u_longlong_t)fnvlist_lookup_uint64(rec,
				    ZPOOL_HIST_DSID));
			}
			(void) printf(" %s", fnvlist_lookup_string(rec,
			    ZPOOL_HIST_INT_STR));
		} else if (nvlist_exists(rec, ZPOOL_HIST_IOCTL)) {
			if (!cb->internal)
				continue;
			(void) printf("%s ioctl %s\n", tbuf,
			    fnvlist_lookup_string(rec, ZPOOL_HIST_IOCTL));
			if (nvlist_exists(rec, ZPOOL_HIST_INPUT_NVL)) {
				(void) printf("    input:\n");
				dump_nvlist(fnvlist_lookup_nvlist(rec,
				    ZPOOL_HIST_INPUT_NVL), 8);
			}
			if (nvlist_exists(rec, ZPOOL_HIST_OUTPUT_NVL)) {
				(void) printf("    output:\n");
				dump_nvlist(fnvlist_lookup_nvlist(rec,
				    ZPOOL_HIST_OUTPUT_NVL), 8);
			}
			if (nvlist_exists(rec, ZPOOL_HIST_OUTPUT_SIZE)) {
				(void) printf("    output nvlist omitted; "
				    "original size: %lldKB\n",
				    (longlong_t)fnvlist_lookup_int64(rec,
				    ZPOOL_HIST_OUTPUT_SIZE) / 1024);
			}
			if (nvlist_exists(rec, ZPOOL_HIST_ERRNO)) {
				(void) printf("    errno: %lld\n",
				    (longlong_t)fnvlist_lookup_int64(rec,
				    ZPOOL_HIST_ERRNO));
			}
		} else {
			if (!cb->internal)
				continue;
			(void) printf("%s unrecognized record:\n", tbuf);
			dump_nvlist(rec, 4);
		}

		if (!cb->longfmt) {
			(void) printf("\n");
			continue;
		}
		(void) printf(" [");
		if (nvlist_exists(rec, ZPOOL_HIST_WHO)) {
			uid_t who = fnvlist_lookup_uint64(rec, ZPOOL_HIST_WHO);
			struct passwd *pwd = getpwuid(who);
			(void) printf("user %d ", (int)who);
			if (pwd != NULL)
				(void) printf("(%s) ", pwd->pw_name);
		}
		if (nvlist_exists(rec, ZPOOL_HIST_HOST)) {
			(void) printf("on %s",
			    fnvlist_lookup_string(rec, ZPOOL_HIST_HOST));
		}
		if (nvlist_exists(rec, ZPOOL_HIST_ZONE)) {
			(void) printf(":%s",
			    fnvlist_lookup_string(rec, ZPOOL_HIST_ZONE));
		}

		(void) printf("]");
		(void) printf("\n");
	}
}

/*
 * Print out the command history for a specific pool.
 */
static int
get_history_one(zpool_handle_t *zhp, void *data)
{
	nvlist_t *nvhis;
	int ret;
	hist_cbdata_t *cb = (hist_cbdata_t *)data;
	uint64_t off = 0;
	boolean_t eof = B_FALSE;

	cb->first = B_FALSE;

	(void) printf(gettext("History for '%s':\n"), zpool_get_name(zhp));

	while (!eof) {
		if ((ret = zpool_get_history(zhp, &nvhis, &off, &eof)) != 0)
			return (ret);

		print_history_records(nvhis, cb);
		nvlist_free(nvhis);
	}
	(void) printf("\n");

	return (ret);
}

/*
 * zpool history <pool>
 *
 * Displays the history of commands that modified pools.
 */
int
zpool_do_history(int argc, char **argv)
{
	hist_cbdata_t cbdata = { 0 };
	int ret;
	int c;

	cbdata.first = B_TRUE;
	/* check options */
	while ((c = getopt(argc, argv, "li")) != -1) {
		switch (c) {
		case 'l':
			cbdata.longfmt = B_TRUE;
			break;
		case 'i':
			cbdata.internal = B_TRUE;
			break;
		case '?':
			(void) fprintf(stderr, gettext("invalid option '%c'\n"),
			    optopt);
			usage(B_FALSE);
		}
	}
	argc -= optind;
	argv += optind;

	ret = for_each_pool(argc, argv, B_FALSE, NULL, ZFS_TYPE_POOL,
	    B_FALSE, get_history_one, &cbdata);

	if (argc == 0 && cbdata.first == B_TRUE) {
		(void) fprintf(stderr, gettext("no pools available\n"));
		return (0);
	}

	return (ret);
}

typedef struct ev_opts {
	int verbose;
	int scripted;
	int follow;
	int clear;
	char poolname[ZFS_MAX_DATASET_NAME_LEN];
} ev_opts_t;

static void
zpool_do_events_short(nvlist_t *nvl, ev_opts_t *opts)
{
	char ctime_str[26], str[32];
	const char *ptr;
	int64_t *tv;
	uint_t n;

	verify(nvlist_lookup_int64_array(nvl, FM_EREPORT_TIME, &tv, &n) == 0);
	memset(str, ' ', 32);
	(void) ctime_r((const time_t *)&tv[0], ctime_str);
	(void) memcpy(str, ctime_str+4,  6);		/* 'Jun 30' */
	(void) memcpy(str+7, ctime_str+20, 4);		/* '1993' */
	(void) memcpy(str+12, ctime_str+11, 8);		/* '21:49:08' */
	(void) sprintf(str+20, ".%09lld", (longlong_t)tv[1]); /* '.123456789' */
	if (opts->scripted)
		(void) printf(gettext("%s\t"), str);
	else
		(void) printf(gettext("%s "), str);

	verify(nvlist_lookup_string(nvl, FM_CLASS, &ptr) == 0);
	(void) printf(gettext("%s\n"), ptr);
}

static void
zpool_do_events_nvprint(nvlist_t *nvl, int depth)
{
	nvpair_t *nvp;
	static char flagstr[256];

	for (nvp = nvlist_next_nvpair(nvl, NULL);
	    nvp != NULL; nvp = nvlist_next_nvpair(nvl, nvp)) {

		data_type_t type = nvpair_type(nvp);
		const char *name = nvpair_name(nvp);

		boolean_t b;
		uint8_t i8;
		uint16_t i16;
		uint32_t i32;
		uint64_t i64;
		const char *str;
		nvlist_t *cnv;

		printf(gettext("%*s%s = "), depth, "", name);

		switch (type) {
		case DATA_TYPE_BOOLEAN:
			printf(gettext("%s"), "1");
			break;

		case DATA_TYPE_BOOLEAN_VALUE:
			(void) nvpair_value_boolean_value(nvp, &b);
			printf(gettext("%s"), b ? "1" : "0");
			break;

		case DATA_TYPE_BYTE:
			(void) nvpair_value_byte(nvp, &i8);
			printf(gettext("0x%x"), i8);
			break;

		case DATA_TYPE_INT8:
			(void) nvpair_value_int8(nvp, (void *)&i8);
			printf(gettext("0x%x"), i8);
			break;

		case DATA_TYPE_UINT8:
			(void) nvpair_value_uint8(nvp, &i8);
			printf(gettext("0x%x"), i8);
			break;

		case DATA_TYPE_INT16:
			(void) nvpair_value_int16(nvp, (void *)&i16);
			printf(gettext("0x%x"), i16);
			break;

		case DATA_TYPE_UINT16:
			(void) nvpair_value_uint16(nvp, &i16);
			printf(gettext("0x%x"), i16);
			break;

		case DATA_TYPE_INT32:
			(void) nvpair_value_int32(nvp, (void *)&i32);
			printf(gettext("0x%x"), i32);
			break;

		case DATA_TYPE_UINT32:
			(void) nvpair_value_uint32(nvp, &i32);
			if (strcmp(name,
			    FM_EREPORT_PAYLOAD_ZFS_ZIO_STAGE) == 0 ||
			    strcmp(name,
			    FM_EREPORT_PAYLOAD_ZFS_ZIO_PIPELINE) == 0) {
				zfs_valstr_zio_stage(i32, flagstr,
				    sizeof (flagstr));
				printf(gettext("0x%x [%s]"), i32, flagstr);
			} else if (strcmp(name,
			    FM_EREPORT_PAYLOAD_ZFS_ZIO_TYPE) == 0) {
				zfs_valstr_zio_type(i32, flagstr,
				    sizeof (flagstr));
				printf(gettext("0x%x [%s]"), i32, flagstr);
			} else if (strcmp(name,
			    FM_EREPORT_PAYLOAD_ZFS_ZIO_PRIORITY) == 0) {
				zfs_valstr_zio_priority(i32, flagstr,
				    sizeof (flagstr));
				printf(gettext("0x%x [%s]"), i32, flagstr);
			} else {
				printf(gettext("0x%x"), i32);
			}
			break;

		case DATA_TYPE_INT64:
			(void) nvpair_value_int64(nvp, (void *)&i64);
			printf(gettext("0x%llx"), (u_longlong_t)i64);
			break;

		case DATA_TYPE_UINT64:
			(void) nvpair_value_uint64(nvp, &i64);
			/*
			 * translate vdev state values to readable
			 * strings to aide zpool events consumers
			 */
			if (strcmp(name,
			    FM_EREPORT_PAYLOAD_ZFS_VDEV_STATE) == 0 ||
			    strcmp(name,
			    FM_EREPORT_PAYLOAD_ZFS_VDEV_LASTSTATE) == 0) {
				printf(gettext("\"%s\" (0x%llx)"),
				    zpool_state_to_name(i64, VDEV_AUX_NONE),
				    (u_longlong_t)i64);
			} else if (strcmp(name,
			    FM_EREPORT_PAYLOAD_ZFS_ZIO_FLAGS) == 0) {
				zfs_valstr_zio_flag(i64, flagstr,
				    sizeof (flagstr));
				printf(gettext("0x%llx [%s]"),
				    (u_longlong_t)i64, flagstr);
			} else {
				printf(gettext("0x%llx"), (u_longlong_t)i64);
			}
			break;

		case DATA_TYPE_HRTIME:
			(void) nvpair_value_hrtime(nvp, (void *)&i64);
			printf(gettext("0x%llx"), (u_longlong_t)i64);
			break;

		case DATA_TYPE_STRING:
			(void) nvpair_value_string(nvp, &str);
			printf(gettext("\"%s\""), str ? str : "<NULL>");
			break;

		case DATA_TYPE_NVLIST:
			printf(gettext("(embedded nvlist)\n"));
			(void) nvpair_value_nvlist(nvp, &cnv);
			zpool_do_events_nvprint(cnv, depth + 8);
			printf(gettext("%*s(end %s)"), depth, "", name);
			break;

		case DATA_TYPE_NVLIST_ARRAY: {
			nvlist_t **val;
			uint_t i, nelem;

			(void) nvpair_value_nvlist_array(nvp, &val, &nelem);
			printf(gettext("(%d embedded nvlists)\n"), nelem);
			for (i = 0; i < nelem; i++) {
				printf(gettext("%*s%s[%d] = %s\n"),
				    depth, "", name, i, "(embedded nvlist)");
				zpool_do_events_nvprint(val[i], depth + 8);
				printf(gettext("%*s(end %s[%i])\n"),
				    depth, "", name, i);
			}
			printf(gettext("%*s(end %s)\n"), depth, "", name);
			}
			break;

		case DATA_TYPE_INT8_ARRAY: {
			int8_t *val;
			uint_t i, nelem;

			(void) nvpair_value_int8_array(nvp, &val, &nelem);
			for (i = 0; i < nelem; i++)
				printf(gettext("0x%x "), val[i]);

			break;
			}

		case DATA_TYPE_UINT8_ARRAY: {
			uint8_t *val;
			uint_t i, nelem;

			(void) nvpair_value_uint8_array(nvp, &val, &nelem);
			for (i = 0; i < nelem; i++)
				printf(gettext("0x%x "), val[i]);

			break;
			}

		case DATA_TYPE_INT16_ARRAY: {
			int16_t *val;
			uint_t i, nelem;

			(void) nvpair_value_int16_array(nvp, &val, &nelem);
			for (i = 0; i < nelem; i++)
				printf(gettext("0x%x "), val[i]);

			break;
			}

		case DATA_TYPE_UINT16_ARRAY: {
			uint16_t *val;
			uint_t i, nelem;

			(void) nvpair_value_uint16_array(nvp, &val, &nelem);
			for (i = 0; i < nelem; i++)
				printf(gettext("0x%x "), val[i]);

			break;
			}

		case DATA_TYPE_INT32_ARRAY: {
			int32_t *val;
			uint_t i, nelem;

			(void) nvpair_value_int32_array(nvp, &val, &nelem);
			for (i = 0; i < nelem; i++)
				printf(gettext("0x%x "), val[i]);

			break;
			}

		case DATA_TYPE_UINT32_ARRAY: {
			uint32_t *val;
			uint_t i, nelem;

			(void) nvpair_value_uint32_array(nvp, &val, &nelem);
			for (i = 0; i < nelem; i++)
				printf(gettext("0x%x "), val[i]);

			break;
			}

		case DATA_TYPE_INT64_ARRAY: {
			int64_t *val;
			uint_t i, nelem;

			(void) nvpair_value_int64_array(nvp, &val, &nelem);
			for (i = 0; i < nelem; i++)
				printf(gettext("0x%llx "),
				    (u_longlong_t)val[i]);

			break;
			}

		case DATA_TYPE_UINT64_ARRAY: {
			uint64_t *val;
			uint_t i, nelem;

			(void) nvpair_value_uint64_array(nvp, &val, &nelem);
			for (i = 0; i < nelem; i++)
				printf(gettext("0x%llx "),
				    (u_longlong_t)val[i]);

			break;
			}

		case DATA_TYPE_STRING_ARRAY: {
			const char **str;
			uint_t i, nelem;

			(void) nvpair_value_string_array(nvp, &str, &nelem);
			for (i = 0; i < nelem; i++)
				printf(gettext("\"%s\" "),
				    str[i] ? str[i] : "<NULL>");

			break;
			}

		case DATA_TYPE_BOOLEAN_ARRAY:
		case DATA_TYPE_BYTE_ARRAY:
		case DATA_TYPE_DOUBLE:
		case DATA_TYPE_DONTCARE:
		case DATA_TYPE_UNKNOWN:
			printf(gettext("<unknown>"));
			break;
		}

		printf(gettext("\n"));
	}
}

static int
zpool_do_events_next(ev_opts_t *opts)
{
	nvlist_t *nvl;
	int zevent_fd, ret, dropped;
	const char *pool;

	zevent_fd = open(ZFS_DEV, O_RDWR);
	VERIFY(zevent_fd >= 0);

	if (!opts->scripted)
		(void) printf(gettext("%-30s %s\n"), "TIME", "CLASS");

	while (1) {
		ret = zpool_events_next(g_zfs, &nvl, &dropped,
		    (opts->follow ? ZEVENT_NONE : ZEVENT_NONBLOCK), zevent_fd);
		if (ret || nvl == NULL)
			break;

		if (dropped > 0)
			(void) printf(gettext("dropped %d events\n"), dropped);

		if (strlen(opts->poolname) > 0 &&
		    nvlist_lookup_string(nvl, FM_FMRI_ZFS_POOL, &pool) == 0 &&
		    strcmp(opts->poolname, pool) != 0)
			continue;

		zpool_do_events_short(nvl, opts);

		if (opts->verbose) {
			zpool_do_events_nvprint(nvl, 8);
			printf(gettext("\n"));
		}
		(void) fflush(stdout);

		nvlist_free(nvl);
	}

	VERIFY(0 == close(zevent_fd));

	return (ret);
}

static int
zpool_do_events_clear(void)
{
	int count, ret;

	ret = zpool_events_clear(g_zfs, &count);
	if (!ret)
		(void) printf(gettext("cleared %d events\n"), count);

	return (ret);
}

/*
 * zpool events [-vHf [pool] | -c]
 *
 * Displays events logs by ZFS.
 */
int
zpool_do_events(int argc, char **argv)
{
	ev_opts_t opts = { 0 };
	int ret;
	int c;

	/* check options */
	while ((c = getopt(argc, argv, "vHfc")) != -1) {
		switch (c) {
		case 'v':
			opts.verbose = 1;
			break;
		case 'H':
			opts.scripted = 1;
			break;
		case 'f':
			opts.follow = 1;
			break;
		case 'c':
			opts.clear = 1;
			break;
		case '?':
			(void) fprintf(stderr, gettext("invalid option '%c'\n"),
			    optopt);
			usage(B_FALSE);
		}
	}
	argc -= optind;
	argv += optind;

	if (argc > 1) {
		(void) fprintf(stderr, gettext("too many arguments\n"));
		usage(B_FALSE);
	} else if (argc == 1) {
		(void) strlcpy(opts.poolname, argv[0], sizeof (opts.poolname));
		if (!zfs_name_valid(opts.poolname, ZFS_TYPE_POOL)) {
			(void) fprintf(stderr,
			    gettext("invalid pool name '%s'\n"), opts.poolname);
			usage(B_FALSE);
		}
	}

	if ((argc == 1 || opts.verbose || opts.scripted || opts.follow) &&
	    opts.clear) {
		(void) fprintf(stderr,
		    gettext("invalid options combined with -c\n"));
		usage(B_FALSE);
	}

	if (opts.clear)
		ret = zpool_do_events_clear();
	else
		ret = zpool_do_events_next(&opts);

	return (ret);
}

static int
get_callback_vdev(zpool_handle_t *zhp, char *vdevname, void *data)
{
	zprop_get_cbdata_t *cbp = (zprop_get_cbdata_t *)data;
	char value[ZFS_MAXPROPLEN];
	zprop_source_t srctype;
	nvlist_t *props, *item, *d;
	props = item = d = NULL;

	if (cbp->cb_json) {
		d = fnvlist_lookup_nvlist(cbp->cb_jsobj, "vdevs");
		if (d == NULL) {
			fprintf(stderr, "vdevs obj not found.\n");
			exit(1);
		}
		props = fnvlist_alloc();
	}

	for (zprop_list_t *pl = cbp->cb_proplist; pl != NULL;
	    pl = pl->pl_next) {
		char *prop_name;
		/*
		 * If the first property is pool name, it is a special
		 * placeholder that we can skip. This will also skip
		 * over the name property when 'all' is specified.
		 */
		if (pl->pl_prop == ZPOOL_PROP_NAME &&
		    pl == cbp->cb_proplist)
			continue;

		if (pl->pl_prop == ZPROP_INVAL) {
			prop_name = pl->pl_user_prop;
		} else {
			prop_name = (char *)vdev_prop_to_name(pl->pl_prop);
		}
		if (zpool_get_vdev_prop(zhp, vdevname, pl->pl_prop,
		    prop_name, value, sizeof (value), &srctype,
		    cbp->cb_literal) == 0) {
			zprop_collect_property(vdevname, cbp, prop_name,
			    value, srctype, NULL, NULL, props);
		}
	}

	if (cbp->cb_json) {
		if (!nvlist_empty(props)) {
			item = fnvlist_alloc();
			fill_vdev_info(item, zhp, vdevname, B_TRUE,
			    cbp->cb_json_as_int);
			fnvlist_add_nvlist(item, "properties", props);
			fnvlist_add_nvlist(d, vdevname, item);
			fnvlist_add_nvlist(cbp->cb_jsobj, "vdevs", d);
			fnvlist_free(item);
		}
		fnvlist_free(props);
	}

	return (0);
}

static int
get_callback_vdev_cb(void *zhp_data, nvlist_t *nv, void *data)
{
	zpool_handle_t *zhp = zhp_data;
	zprop_get_cbdata_t *cbp = (zprop_get_cbdata_t *)data;
	char *vdevname;
	const char *type;
	int ret;

	/*
	 * zpool_vdev_name() transforms the root vdev name (i.e., root-0) to the
	 * pool name for display purposes, which is not desired. Fallback to
	 * zpool_vdev_name() when not dealing with the root vdev.
	 */
	type = fnvlist_lookup_string(nv, ZPOOL_CONFIG_TYPE);
	if (zhp != NULL && strcmp(type, "root") == 0)
		vdevname = strdup("root-0");
	else
		vdevname = zpool_vdev_name(g_zfs, zhp, nv,
		    cbp->cb_vdevs.cb_name_flags);

	(void) vdev_expand_proplist(zhp, vdevname, &cbp->cb_proplist);

	ret = get_callback_vdev(zhp, vdevname, data);

	free(vdevname);

	return (ret);
}

static int
get_callback(zpool_handle_t *zhp, void *data)
{
	zprop_get_cbdata_t *cbp = (zprop_get_cbdata_t *)data;
	char value[ZFS_MAXPROPLEN];
	zprop_source_t srctype;
	zprop_list_t *pl;
	int vid;
	int err = 0;
	nvlist_t *props, *item, *d;
	props = item = d = NULL;

	if (cbp->cb_type == ZFS_TYPE_VDEV) {
		if (cbp->cb_json) {
			nvlist_t *pool = fnvlist_alloc();
			fill_pool_info(pool, zhp, B_FALSE, cbp->cb_json_as_int);
			fnvlist_add_nvlist(cbp->cb_jsobj, "pool", pool);
			fnvlist_free(pool);
		}

		if (strcmp(cbp->cb_vdevs.cb_names[0], "all-vdevs") == 0) {
			for_each_vdev(zhp, get_callback_vdev_cb, data);
		} else {
			/* Adjust column widths for vdev properties */
			for (vid = 0; vid < cbp->cb_vdevs.cb_names_count;
			    vid++) {
				vdev_expand_proplist(zhp,
				    cbp->cb_vdevs.cb_names[vid],
				    &cbp->cb_proplist);
			}
			/* Display the properties */
			for (vid = 0; vid < cbp->cb_vdevs.cb_names_count;
			    vid++) {
				get_callback_vdev(zhp,
				    cbp->cb_vdevs.cb_names[vid], data);
			}
		}
	} else {
		assert(cbp->cb_type == ZFS_TYPE_POOL);
		if (cbp->cb_json) {
			d = fnvlist_lookup_nvlist(cbp->cb_jsobj, "pools");
			if (d == NULL) {
				fprintf(stderr, "pools obj not found.\n");
				exit(1);
			}
			props = fnvlist_alloc();
		}
		for (pl = cbp->cb_proplist; pl != NULL; pl = pl->pl_next) {
			/*
			 * Skip the special fake placeholder. This will also
			 * skip over the name property when 'all' is specified.
			 */
			if (pl->pl_prop == ZPOOL_PROP_NAME &&
			    pl == cbp->cb_proplist)
				continue;

			if (pl->pl_prop == ZPROP_INVAL &&
			    zfs_prop_user(pl->pl_user_prop)) {
				srctype = ZPROP_SRC_LOCAL;

				if (zpool_get_userprop(zhp, pl->pl_user_prop,
				    value, sizeof (value), &srctype) != 0)
					continue;

				err = zprop_collect_property(
				    zpool_get_name(zhp), cbp, pl->pl_user_prop,
				    value, srctype, NULL, NULL, props);
			} else if (pl->pl_prop == ZPROP_INVAL &&
			    (zpool_prop_feature(pl->pl_user_prop) ||
			    zpool_prop_unsupported(pl->pl_user_prop))) {
				srctype = ZPROP_SRC_LOCAL;

				if (zpool_prop_get_feature(zhp,
				    pl->pl_user_prop, value,
				    sizeof (value)) == 0) {
					err = zprop_collect_property(
					    zpool_get_name(zhp), cbp,
					    pl->pl_user_prop, value, srctype,
					    NULL, NULL, props);
				}
			} else {
				if (zpool_get_prop(zhp, pl->pl_prop, value,
				    sizeof (value), &srctype,
				    cbp->cb_literal) != 0)
					continue;

				err = zprop_collect_property(
				    zpool_get_name(zhp), cbp,
				    zpool_prop_to_name(pl->pl_prop),
				    value, srctype, NULL, NULL, props);
			}
			if (err != 0)
				return (err);
		}

		if (cbp->cb_json) {
			if (!nvlist_empty(props)) {
				item = fnvlist_alloc();
				fill_pool_info(item, zhp, B_TRUE,
				    cbp->cb_json_as_int);
				fnvlist_add_nvlist(item, "properties", props);
				if (cbp->cb_json_pool_key_guid) {
					char buf[256];
					uint64_t guid = fnvlist_lookup_uint64(
					    zpool_get_config(zhp, NULL),
					    ZPOOL_CONFIG_POOL_GUID);
					snprintf(buf, 256, "%llu",
					    (u_longlong_t)guid);
					fnvlist_add_nvlist(d, buf, item);
				} else {
					const char *name = zpool_get_name(zhp);
					fnvlist_add_nvlist(d, name, item);
				}
				fnvlist_add_nvlist(cbp->cb_jsobj, "pools", d);
				fnvlist_free(item);
			}
			fnvlist_free(props);
		}
	}

	return (0);
}

/*
 * zpool get [-Hp] [-o "all" | field[,...]] <"all" | property[,...]> <pool> ...
 *
 *	-H	Scripted mode.  Don't display headers, and separate properties
 *		by a single tab.
 *	-o	List of columns to display.  Defaults to
 *		"name,property,value,source".
 * 	-p	Display values in parsable (exact) format.
 * 	-j	Display output in JSON format.
 * 	--json-int	Display numbers as integers instead of strings.
 * 	--json-pool-key-guid	Set pool GUID as key for pool objects.
 *
 * Get properties of pools in the system. Output space statistics
 * for each one as well as other attributes.
 */
int
zpool_do_get(int argc, char **argv)
{
	zprop_get_cbdata_t cb = { 0 };
	zprop_list_t fake_name = { 0 };
	int ret;
	int c, i;
	char *propstr = NULL;
	char *vdev = NULL;
	nvlist_t *data = NULL;

	cb.cb_first = B_TRUE;

	/*
	 * Set up default columns and sources.
	 */
	cb.cb_sources = ZPROP_SRC_ALL;
	cb.cb_columns[0] = GET_COL_NAME;
	cb.cb_columns[1] = GET_COL_PROPERTY;
	cb.cb_columns[2] = GET_COL_VALUE;
	cb.cb_columns[3] = GET_COL_SOURCE;
	cb.cb_type = ZFS_TYPE_POOL;
	cb.cb_vdevs.cb_name_flags |= VDEV_NAME_TYPE_ID;
	current_prop_type = cb.cb_type;

	struct option long_options[] = {
		{"json", no_argument, NULL, 'j'},
		{"json-int", no_argument, NULL, ZPOOL_OPTION_JSON_NUMS_AS_INT},
		{"json-pool-key-guid", no_argument, NULL,
		    ZPOOL_OPTION_POOL_KEY_GUID},
		{0, 0, 0, 0}
	};

	/* check options */
	while ((c = getopt_long(argc, argv, ":jHpo:", long_options,
	    NULL)) != -1) {
		switch (c) {
		case 'p':
			cb.cb_literal = B_TRUE;
			break;
		case 'H':
			cb.cb_scripted = B_TRUE;
			break;
		case 'j':
			cb.cb_json = B_TRUE;
			cb.cb_jsobj = zpool_json_schema(0, 1);
			data = fnvlist_alloc();
			break;
		case ZPOOL_OPTION_POOL_KEY_GUID:
			cb.cb_json_pool_key_guid = B_TRUE;
			break;
		case ZPOOL_OPTION_JSON_NUMS_AS_INT:
			cb.cb_json_as_int = B_TRUE;
			cb.cb_literal = B_TRUE;
			break;
		case 'o':
			memset(&cb.cb_columns, 0, sizeof (cb.cb_columns));
			i = 0;

			for (char *tok; (tok = strsep(&optarg, ",")); ) {
				static const char *const col_opts[] =
				{ "name", "property", "value", "source",
				    "all" };
				static const zfs_get_column_t col_cols[] =
				{ GET_COL_NAME, GET_COL_PROPERTY, GET_COL_VALUE,
				    GET_COL_SOURCE };

				if (i == ZFS_GET_NCOLS - 1) {
					(void) fprintf(stderr, gettext("too "
					"many fields given to -o "
					"option\n"));
					usage(B_FALSE);
				}

				for (c = 0; c < ARRAY_SIZE(col_opts); ++c)
					if (strcmp(tok, col_opts[c]) == 0)
						goto found;

				(void) fprintf(stderr,
				    gettext("invalid column name '%s'\n"), tok);
				usage(B_FALSE);

found:
				if (c >= 4) {
					if (i > 0) {
						(void) fprintf(stderr,
						    gettext("\"all\" conflicts "
						    "with specific fields "
						    "given to -o option\n"));
						usage(B_FALSE);
					}

					memcpy(cb.cb_columns, col_cols,
					    sizeof (col_cols));
					i = ZFS_GET_NCOLS - 1;
				} else
					cb.cb_columns[i++] = col_cols[c];
			}
			break;
		case '?':
			(void) fprintf(stderr, gettext("invalid option '%c'\n"),
			    optopt);
			usage(B_FALSE);
		}
	}

	argc -= optind;
	argv += optind;

	if (!cb.cb_json && cb.cb_json_as_int) {
		(void) fprintf(stderr, gettext("'--json-int' only works with"
		    " '-j' option\n"));
		usage(B_FALSE);
	}

	if (!cb.cb_json && cb.cb_json_pool_key_guid) {
		(void) fprintf(stderr, gettext("'json-pool-key-guid' only"
		    " works with '-j' option\n"));
		usage(B_FALSE);
	}

	if (argc < 1) {
		(void) fprintf(stderr, gettext("missing property "
		    "argument\n"));
		usage(B_FALSE);
	}

	/* Properties list is needed later by zprop_get_list() */
	propstr = argv[0];

	argc--;
	argv++;

	if (argc == 0) {
		/* No args, so just print the defaults. */
	} else if (are_all_pools(argc, argv)) {
		/* All the args are pool names */
	} else if (are_all_pools(1, argv)) {
		/* The first arg is a pool name */
		if ((argc == 2 && strcmp(argv[1], "all-vdevs") == 0) ||
		    (argc == 2 && strcmp(argv[1], "root") == 0) ||
		    are_vdevs_in_pool(argc - 1, argv + 1, argv[0],
		    &cb.cb_vdevs)) {

			if (strcmp(argv[1], "root") == 0)
				vdev = strdup("root-0");

			/* ... and the rest are vdev names */
			if (vdev == NULL)
				cb.cb_vdevs.cb_names = argv + 1;
			else
				cb.cb_vdevs.cb_names = &vdev;

			cb.cb_vdevs.cb_names_count = argc - 1;
			cb.cb_type = ZFS_TYPE_VDEV;
			argc = 1; /* One pool to process */
		} else {
			if (cb.cb_json) {
				nvlist_free(cb.cb_jsobj);
				nvlist_free(data);
			}
			fprintf(stderr, gettext("Expected a list of vdevs in"
			    " \"%s\", but got:\n"), argv[0]);
			error_list_unresolved_vdevs(argc - 1, argv + 1,
			    argv[0], &cb.cb_vdevs);
			fprintf(stderr, "\n");
			usage(B_FALSE);
			return (1);
		}
	} else {
		if (cb.cb_json) {
			nvlist_free(cb.cb_jsobj);
			nvlist_free(data);
		}
		/*
		 * The first arg isn't the name of a valid pool.
		 */
		fprintf(stderr, gettext("Cannot get properties of %s: "
		    "no such pool available.\n"), argv[0]);
		return (1);
	}

	if (zprop_get_list(g_zfs, propstr, &cb.cb_proplist,
	    cb.cb_type) != 0) {
		/* Use correct list of valid properties (pool or vdev) */
		current_prop_type = cb.cb_type;
		usage(B_FALSE);
	}

	if (cb.cb_proplist != NULL) {
		fake_name.pl_prop = ZPOOL_PROP_NAME;
		fake_name.pl_width = strlen(gettext("NAME"));
		fake_name.pl_next = cb.cb_proplist;
		cb.cb_proplist = &fake_name;
	}

	if (cb.cb_json) {
		if (cb.cb_type == ZFS_TYPE_VDEV)
			fnvlist_add_nvlist(cb.cb_jsobj, "vdevs", data);
		else
			fnvlist_add_nvlist(cb.cb_jsobj, "pools", data);
		fnvlist_free(data);
	}

	ret = for_each_pool(argc, argv, B_TRUE, &cb.cb_proplist, cb.cb_type,
	    cb.cb_literal, get_callback, &cb);

	if (ret == 0 && cb.cb_json)
		zcmd_print_json(cb.cb_jsobj);
	else if (ret != 0 && cb.cb_json)
		nvlist_free(cb.cb_jsobj);

	if (cb.cb_proplist == &fake_name)
		zprop_free_list(fake_name.pl_next);
	else
		zprop_free_list(cb.cb_proplist);

	if (vdev != NULL)
		free(vdev);

	return (ret);
}

typedef struct set_cbdata {
	char *cb_propname;
	char *cb_value;
	zfs_type_t cb_type;
	vdev_cbdata_t cb_vdevs;
	boolean_t cb_any_successful;
} set_cbdata_t;

static int
set_pool_callback(zpool_handle_t *zhp, set_cbdata_t *cb)
{
	int error;

	/* Check if we have out-of-bounds features */
	if (strcmp(cb->cb_propname, ZPOOL_CONFIG_COMPATIBILITY) == 0) {
		boolean_t features[SPA_FEATURES];
		if (zpool_do_load_compat(cb->cb_value, features) !=
		    ZPOOL_COMPATIBILITY_OK)
			return (-1);

		nvlist_t *enabled = zpool_get_features(zhp);
		spa_feature_t i;
		for (i = 0; i < SPA_FEATURES; i++) {
			const char *fguid = spa_feature_table[i].fi_guid;
			if (nvlist_exists(enabled, fguid) && !features[i])
				break;
		}
		if (i < SPA_FEATURES)
			(void) fprintf(stderr, gettext("Warning: one or "
			    "more features already enabled on pool '%s'\n"
			    "are not present in this compatibility set.\n"),
			    zpool_get_name(zhp));
	}

	/* if we're setting a feature, check it's in compatibility set */
	if (zpool_prop_feature(cb->cb_propname) &&
	    strcmp(cb->cb_value, ZFS_FEATURE_ENABLED) == 0) {
		char *fname = strchr(cb->cb_propname, '@') + 1;
		spa_feature_t f;

		if (zfeature_lookup_name(fname, &f) == 0) {
			char compat[ZFS_MAXPROPLEN];
			if (zpool_get_prop(zhp, ZPOOL_PROP_COMPATIBILITY,
			    compat, ZFS_MAXPROPLEN, NULL, B_FALSE) != 0)
				compat[0] = '\0';

			boolean_t features[SPA_FEATURES];
			if (zpool_do_load_compat(compat, features) !=
			    ZPOOL_COMPATIBILITY_OK) {
				(void) fprintf(stderr, gettext("Error: "
				    "cannot enable feature '%s' on pool '%s'\n"
				    "because the pool's 'compatibility' "
				    "property cannot be parsed.\n"),
				    fname, zpool_get_name(zhp));
				return (-1);
			}

			if (!features[f]) {
				(void) fprintf(stderr, gettext("Error: "
				    "cannot enable feature '%s' on pool '%s'\n"
				    "as it is not specified in this pool's "
				    "current compatibility set.\n"
				    "Consider setting 'compatibility' to a "
				    "less restrictive set, or to 'off'.\n"),
				    fname, zpool_get_name(zhp));
				return (-1);
			}
		}
	}

	error = zpool_set_prop(zhp, cb->cb_propname, cb->cb_value);

	return (error);
}

static int
set_callback(zpool_handle_t *zhp, void *data)
{
	int error;
	set_cbdata_t *cb = (set_cbdata_t *)data;

	if (cb->cb_type == ZFS_TYPE_VDEV) {
		error = zpool_set_vdev_prop(zhp, *cb->cb_vdevs.cb_names,
		    cb->cb_propname, cb->cb_value);
	} else {
		assert(cb->cb_type == ZFS_TYPE_POOL);
		error = set_pool_callback(zhp, cb);
	}

	cb->cb_any_successful = !error;
	return (error);
}

int
zpool_do_set(int argc, char **argv)
{
	set_cbdata_t cb = { 0 };
	int error;
	char *vdev = NULL;

	current_prop_type = ZFS_TYPE_POOL;
	if (argc > 1 && argv[1][0] == '-') {
		(void) fprintf(stderr, gettext("invalid option '%c'\n"),
		    argv[1][1]);
		usage(B_FALSE);
	}

	if (argc < 2) {
		(void) fprintf(stderr, gettext("missing property=value "
		    "argument\n"));
		usage(B_FALSE);
	}

	if (argc < 3) {
		(void) fprintf(stderr, gettext("missing pool name\n"));
		usage(B_FALSE);
	}

	if (argc > 4) {
		(void) fprintf(stderr, gettext("too many pool names\n"));
		usage(B_FALSE);
	}

	cb.cb_propname = argv[1];
	cb.cb_type = ZFS_TYPE_POOL;
	cb.cb_vdevs.cb_name_flags |= VDEV_NAME_TYPE_ID;
	cb.cb_value = strchr(cb.cb_propname, '=');
	if (cb.cb_value == NULL) {
		(void) fprintf(stderr, gettext("missing value in "
		    "property=value argument\n"));
		usage(B_FALSE);
	}

	*(cb.cb_value) = '\0';
	cb.cb_value++;
	argc -= 2;
	argv += 2;

	/* argv[0] is pool name */
	if (!is_pool(argv[0])) {
		(void) fprintf(stderr,
		    gettext("cannot open '%s': is not a pool\n"), argv[0]);
		return (EINVAL);
	}

	/* argv[1], when supplied, is vdev name */
	if (argc == 2) {

		if (strcmp(argv[1], "root") == 0)
			vdev = strdup("root-0");
		else
			vdev = strdup(argv[1]);

		if (!are_vdevs_in_pool(1, &vdev, argv[0], &cb.cb_vdevs)) {
			(void) fprintf(stderr, gettext(
			    "cannot find '%s' in '%s': device not in pool\n"),
			    vdev, argv[0]);
			free(vdev);
			return (EINVAL);
		}
		cb.cb_vdevs.cb_names = &vdev;
		cb.cb_vdevs.cb_names_count = 1;
		cb.cb_type = ZFS_TYPE_VDEV;
	}

	error = for_each_pool(1, argv, B_TRUE, NULL, ZFS_TYPE_POOL,
	    B_FALSE, set_callback, &cb);

	if (vdev != NULL)
		free(vdev);

	return (error);
}

/* Add up the total number of bytes left to initialize/trim across all vdevs */
static uint64_t
vdev_activity_remaining(nvlist_t *nv, zpool_wait_activity_t activity)
{
	uint64_t bytes_remaining;
	nvlist_t **child;
	uint_t c, children;
	vdev_stat_t *vs;

	assert(activity == ZPOOL_WAIT_INITIALIZE ||
	    activity == ZPOOL_WAIT_TRIM);

	verify(nvlist_lookup_uint64_array(nv, ZPOOL_CONFIG_VDEV_STATS,
	    (uint64_t **)&vs, &c) == 0);

	if (activity == ZPOOL_WAIT_INITIALIZE &&
	    vs->vs_initialize_state == VDEV_INITIALIZE_ACTIVE)
		bytes_remaining = vs->vs_initialize_bytes_est -
		    vs->vs_initialize_bytes_done;
	else if (activity == ZPOOL_WAIT_TRIM &&
	    vs->vs_trim_state == VDEV_TRIM_ACTIVE)
		bytes_remaining = vs->vs_trim_bytes_est -
		    vs->vs_trim_bytes_done;
	else
		bytes_remaining = 0;

	if (nvlist_lookup_nvlist_array(nv, ZPOOL_CONFIG_CHILDREN,
	    &child, &children) != 0)
		children = 0;

	for (c = 0; c < children; c++)
		bytes_remaining += vdev_activity_remaining(child[c], activity);

	return (bytes_remaining);
}

/* Add up the total number of bytes left to rebuild across top-level vdevs */
static uint64_t
vdev_activity_top_remaining(nvlist_t *nv)
{
	uint64_t bytes_remaining = 0;
	nvlist_t **child;
	uint_t children;
	int error;

	if (nvlist_lookup_nvlist_array(nv, ZPOOL_CONFIG_CHILDREN,
	    &child, &children) != 0)
		children = 0;

	for (uint_t c = 0; c < children; c++) {
		vdev_rebuild_stat_t *vrs;
		uint_t i;

		error = nvlist_lookup_uint64_array(child[c],
		    ZPOOL_CONFIG_REBUILD_STATS, (uint64_t **)&vrs, &i);
		if (error == 0) {
			if (vrs->vrs_state == VDEV_REBUILD_ACTIVE) {
				bytes_remaining += (vrs->vrs_bytes_est -
				    vrs->vrs_bytes_rebuilt);
			}
		}
	}

	return (bytes_remaining);
}

/* Whether any vdevs are 'spare' or 'replacing' vdevs */
static boolean_t
vdev_any_spare_replacing(nvlist_t *nv)
{
	nvlist_t **child;
	uint_t c, children;
	const char *vdev_type;

	(void) nvlist_lookup_string(nv, ZPOOL_CONFIG_TYPE, &vdev_type);

	if (strcmp(vdev_type, VDEV_TYPE_REPLACING) == 0 ||
	    strcmp(vdev_type, VDEV_TYPE_SPARE) == 0 ||
	    strcmp(vdev_type, VDEV_TYPE_DRAID_SPARE) == 0) {
		return (B_TRUE);
	}

	if (nvlist_lookup_nvlist_array(nv, ZPOOL_CONFIG_CHILDREN,
	    &child, &children) != 0)
		children = 0;

	for (c = 0; c < children; c++) {
		if (vdev_any_spare_replacing(child[c]))
			return (B_TRUE);
	}

	return (B_FALSE);
}

typedef struct wait_data {
	char *wd_poolname;
	boolean_t wd_scripted;
	boolean_t wd_exact;
	boolean_t wd_headers_once;
	boolean_t wd_should_exit;
	/* Which activities to wait for */
	boolean_t wd_enabled[ZPOOL_WAIT_NUM_ACTIVITIES];
	float wd_interval;
	pthread_cond_t wd_cv;
	pthread_mutex_t wd_mutex;
} wait_data_t;

/*
 * Print to stdout a single line, containing one column for each activity that
 * we are waiting for specifying how many bytes of work are left for that
 * activity.
 */
static void
print_wait_status_row(wait_data_t *wd, zpool_handle_t *zhp, int row)
{
	nvlist_t *config, *nvroot;
	uint_t c;
	int i;
	pool_checkpoint_stat_t *pcs = NULL;
	pool_scan_stat_t *pss = NULL;
	pool_removal_stat_t *prs = NULL;
	pool_raidz_expand_stat_t *pres = NULL;
	const char *const headers[] = {"DISCARD", "FREE", "INITIALIZE",
	    "REPLACE", "REMOVE", "RESILVER", "SCRUB", "TRIM", "RAIDZ_EXPAND"};
	int col_widths[ZPOOL_WAIT_NUM_ACTIVITIES];

	/* Calculate the width of each column */
	for (i = 0; i < ZPOOL_WAIT_NUM_ACTIVITIES; i++) {
		/*
		 * Make sure we have enough space in the col for pretty-printed
		 * numbers and for the column header, and then leave a couple
		 * spaces between cols for readability.
		 */
		col_widths[i] = MAX(strlen(headers[i]), 6) + 2;
	}

	if (timestamp_fmt != NODATE)
		print_timestamp(timestamp_fmt);

	/* Print header if appropriate */
	int term_height = terminal_height();
	boolean_t reprint_header = (!wd->wd_headers_once && term_height > 0 &&
	    row % (term_height-1) == 0);
	if (!wd->wd_scripted && (row == 0 || reprint_header)) {
		for (i = 0; i < ZPOOL_WAIT_NUM_ACTIVITIES; i++) {
			if (wd->wd_enabled[i])
				(void) printf("%*s", col_widths[i], headers[i]);
		}
		(void) fputc('\n', stdout);
	}

	/* Bytes of work remaining in each activity */
	int64_t bytes_rem[ZPOOL_WAIT_NUM_ACTIVITIES] = {0};

	bytes_rem[ZPOOL_WAIT_FREE] =
	    zpool_get_prop_int(zhp, ZPOOL_PROP_FREEING, NULL);

	config = zpool_get_config(zhp, NULL);
	nvroot = fnvlist_lookup_nvlist(config, ZPOOL_CONFIG_VDEV_TREE);

	(void) nvlist_lookup_uint64_array(nvroot,
	    ZPOOL_CONFIG_CHECKPOINT_STATS, (uint64_t **)&pcs, &c);
	if (pcs != NULL && pcs->pcs_state == CS_CHECKPOINT_DISCARDING)
		bytes_rem[ZPOOL_WAIT_CKPT_DISCARD] = pcs->pcs_space;

	(void) nvlist_lookup_uint64_array(nvroot,
	    ZPOOL_CONFIG_REMOVAL_STATS, (uint64_t **)&prs, &c);
	if (prs != NULL && prs->prs_state == DSS_SCANNING)
		bytes_rem[ZPOOL_WAIT_REMOVE] = prs->prs_to_copy -
		    prs->prs_copied;

	(void) nvlist_lookup_uint64_array(nvroot,
	    ZPOOL_CONFIG_SCAN_STATS, (uint64_t **)&pss, &c);
	if (pss != NULL && pss->pss_state == DSS_SCANNING &&
	    pss->pss_pass_scrub_pause == 0) {
		int64_t rem = pss->pss_to_examine - pss->pss_issued;
		if (pss->pss_func == POOL_SCAN_SCRUB)
			bytes_rem[ZPOOL_WAIT_SCRUB] = rem;
		else
			bytes_rem[ZPOOL_WAIT_RESILVER] = rem;
	} else if (check_rebuilding(nvroot, NULL)) {
		bytes_rem[ZPOOL_WAIT_RESILVER] =
		    vdev_activity_top_remaining(nvroot);
	}

	(void) nvlist_lookup_uint64_array(nvroot,
	    ZPOOL_CONFIG_RAIDZ_EXPAND_STATS, (uint64_t **)&pres, &c);
	if (pres != NULL && pres->pres_state == DSS_SCANNING) {
		int64_t rem = pres->pres_to_reflow - pres->pres_reflowed;
		bytes_rem[ZPOOL_WAIT_RAIDZ_EXPAND] = rem;
	}

	bytes_rem[ZPOOL_WAIT_INITIALIZE] =
	    vdev_activity_remaining(nvroot, ZPOOL_WAIT_INITIALIZE);
	bytes_rem[ZPOOL_WAIT_TRIM] =
	    vdev_activity_remaining(nvroot, ZPOOL_WAIT_TRIM);

	/*
	 * A replace finishes after resilvering finishes, so the amount of work
	 * left for a replace is the same as for resilvering.
	 *
	 * It isn't quite correct to say that if we have any 'spare' or
	 * 'replacing' vdevs and a resilver is happening, then a replace is in
	 * progress, like we do here. When a hot spare is used, the faulted vdev
	 * is not removed after the hot spare is resilvered, so parent 'spare'
	 * vdev is not removed either. So we could have a 'spare' vdev, but be
	 * resilvering for a different reason. However, we use it as a heuristic
	 * because we don't have access to the DTLs, which could tell us whether
	 * or not we have really finished resilvering a hot spare.
	 */
	if (vdev_any_spare_replacing(nvroot))
		bytes_rem[ZPOOL_WAIT_REPLACE] =  bytes_rem[ZPOOL_WAIT_RESILVER];

	for (i = 0; i < ZPOOL_WAIT_NUM_ACTIVITIES; i++) {
		char buf[64];
		if (!wd->wd_enabled[i])
			continue;

		if (wd->wd_exact) {
			(void) snprintf(buf, sizeof (buf), "%" PRIi64,
			    bytes_rem[i]);
		} else {
			zfs_nicenum(bytes_rem[i], buf, sizeof (buf));
		}

		if (wd->wd_scripted)
			(void) printf(i == 0 ? "%s" : "\t%s", buf);
		else
			(void) printf(" %*s", col_widths[i] - 1, buf);
	}
	(void) printf("\n");
	(void) fflush(stdout);
}

static void *
wait_status_thread(void *arg)
{
	wait_data_t *wd = (wait_data_t *)arg;
	zpool_handle_t *zhp;

	if ((zhp = zpool_open(g_zfs, wd->wd_poolname)) == NULL)
		return (void *)(1);

	for (int row = 0; ; row++) {
		boolean_t missing;
		struct timespec timeout;
		int ret = 0;
		(void) clock_gettime(CLOCK_REALTIME, &timeout);

		if (zpool_refresh_stats(zhp, &missing) != 0 || missing ||
		    zpool_props_refresh(zhp) != 0) {
			zpool_close(zhp);
			return (void *)(uintptr_t)(missing ? 0 : 1);
		}

		print_wait_status_row(wd, zhp, row);

		timeout.tv_sec += floor(wd->wd_interval);
		long nanos = timeout.tv_nsec +
		    (wd->wd_interval - floor(wd->wd_interval)) * NANOSEC;
		if (nanos >= NANOSEC) {
			timeout.tv_sec++;
			timeout.tv_nsec = nanos - NANOSEC;
		} else {
			timeout.tv_nsec = nanos;
		}
		pthread_mutex_lock(&wd->wd_mutex);
		if (!wd->wd_should_exit)
			ret = pthread_cond_timedwait(&wd->wd_cv, &wd->wd_mutex,
			    &timeout);
		pthread_mutex_unlock(&wd->wd_mutex);
		if (ret == 0) {
			break; /* signaled by main thread */
		} else if (ret != ETIMEDOUT) {
			(void) fprintf(stderr, gettext("pthread_cond_timedwait "
			    "failed: %s\n"), strerror(ret));
			zpool_close(zhp);
			return (void *)(uintptr_t)(1);
		}
	}

	zpool_close(zhp);
	return (void *)(0);
}

int
zpool_do_wait(int argc, char **argv)
{
	boolean_t verbose = B_FALSE;
	int c, i;
	unsigned long count;
	pthread_t status_thr;
	int error = 0;
	zpool_handle_t *zhp;

	wait_data_t wd;
	wd.wd_scripted = B_FALSE;
	wd.wd_exact = B_FALSE;
	wd.wd_headers_once = B_FALSE;
	wd.wd_should_exit = B_FALSE;

	pthread_mutex_init(&wd.wd_mutex, NULL);
	pthread_cond_init(&wd.wd_cv, NULL);

	/* By default, wait for all types of activity. */
	for (i = 0; i < ZPOOL_WAIT_NUM_ACTIVITIES; i++)
		wd.wd_enabled[i] = B_TRUE;

	while ((c = getopt(argc, argv, "HpT:t:")) != -1) {
		switch (c) {
		case 'H':
			wd.wd_scripted = B_TRUE;
			break;
		case 'n':
			wd.wd_headers_once = B_TRUE;
			break;
		case 'p':
			wd.wd_exact = B_TRUE;
			break;
		case 'T':
			get_timestamp_arg(*optarg);
			break;
		case 't':
			/* Reset activities array */
			memset(&wd.wd_enabled, 0, sizeof (wd.wd_enabled));

			for (char *tok; (tok = strsep(&optarg, ",")); ) {
				static const char *const col_opts[] = {
				    "discard", "free", "initialize", "replace",
				    "remove", "resilver", "scrub", "trim",
				    "raidz_expand" };

				for (i = 0; i < ARRAY_SIZE(col_opts); ++i)
					if (strcmp(tok, col_opts[i]) == 0) {
						wd.wd_enabled[i] = B_TRUE;
						goto found;
					}

				(void) fprintf(stderr,
				    gettext("invalid activity '%s'\n"), tok);
				usage(B_FALSE);
found:;
			}
			break;
		case '?':
			(void) fprintf(stderr, gettext("invalid option '%c'\n"),
			    optopt);
			usage(B_FALSE);
		}
	}

	argc -= optind;
	argv += optind;

	get_interval_count(&argc, argv, &wd.wd_interval, &count);
	if (count != 0) {
		/* This subcmd only accepts an interval, not a count */
		(void) fprintf(stderr, gettext("too many arguments\n"));
		usage(B_FALSE);
	}

	if (wd.wd_interval != 0)
		verbose = B_TRUE;

	if (argc < 1) {
		(void) fprintf(stderr, gettext("missing 'pool' argument\n"));
		usage(B_FALSE);
	}
	if (argc > 1) {
		(void) fprintf(stderr, gettext("too many arguments\n"));
		usage(B_FALSE);
	}

	wd.wd_poolname = argv[0];

	if ((zhp = zpool_open(g_zfs, wd.wd_poolname)) == NULL)
		return (1);

	if (verbose) {
		/*
		 * We use a separate thread for printing status updates because
		 * the main thread will call lzc_wait(), which blocks as long
		 * as an activity is in progress, which can be a long time.
		 */
		if (pthread_create(&status_thr, NULL, wait_status_thread, &wd)
		    != 0) {
			(void) fprintf(stderr, gettext("failed to create status"
			    "thread: %s\n"), strerror(errno));
			zpool_close(zhp);
			return (1);
		}
	}

	/*
	 * Loop over all activities that we are supposed to wait for until none
	 * of them are in progress. Note that this means we can end up waiting
	 * for more activities to complete than just those that were in progress
	 * when we began waiting; if an activity we are interested in begins
	 * while we are waiting for another activity, we will wait for both to
	 * complete before exiting.
	 */
	for (;;) {
		boolean_t missing = B_FALSE;
		boolean_t any_waited = B_FALSE;

		for (i = 0; i < ZPOOL_WAIT_NUM_ACTIVITIES; i++) {
			boolean_t waited;

			if (!wd.wd_enabled[i])
				continue;

			error = zpool_wait_status(zhp, i, &missing, &waited);
			if (error != 0 || missing)
				break;

			any_waited = (any_waited || waited);
		}

		if (error != 0 || missing || !any_waited)
			break;
	}

	zpool_close(zhp);

	if (verbose) {
		uintptr_t status;
		pthread_mutex_lock(&wd.wd_mutex);
		wd.wd_should_exit = B_TRUE;
		pthread_cond_signal(&wd.wd_cv);
		pthread_mutex_unlock(&wd.wd_mutex);
		(void) pthread_join(status_thr, (void *)&status);
		if (status != 0)
			error = status;
	}

	pthread_mutex_destroy(&wd.wd_mutex);
	pthread_cond_destroy(&wd.wd_cv);
	return (error);
}

/*
 * zpool ddtprune -d|-p <amount> <pool>
 *
 *       -d <days>	Prune entries <days> old and older
 *       -p <percent>	Prune <percent> amount of entries
 *
 * Prune single reference entries from DDT to satisfy the amount specified.
 */
int
zpool_do_ddt_prune(int argc, char **argv)
{
	zpool_ddt_prune_unit_t unit = ZPOOL_DDT_PRUNE_NONE;
	uint64_t amount = 0;
	zpool_handle_t *zhp;
	char *endptr;
	int c;

	while ((c = getopt(argc, argv, "d:p:")) != -1) {
		switch (c) {
		case 'd':
			if (unit == ZPOOL_DDT_PRUNE_PERCENTAGE) {
				(void) fprintf(stderr, gettext("-d cannot be "
				    "combined with -p option\n"));
				usage(B_FALSE);
			}
			errno = 0;
			amount = strtoull(optarg, &endptr, 0);
			if (errno != 0 || *endptr != '\0' || amount == 0) {
				(void) fprintf(stderr,
				    gettext("invalid days value\n"));
				usage(B_FALSE);
			}
			amount *= 86400;	/* convert days to seconds */
			unit = ZPOOL_DDT_PRUNE_AGE;
			break;
		case 'p':
			if (unit == ZPOOL_DDT_PRUNE_AGE) {
				(void) fprintf(stderr, gettext("-p cannot be "
				    "combined with -d option\n"));
				usage(B_FALSE);
			}
			errno = 0;
			amount = strtoull(optarg, &endptr, 0);
			if (errno != 0 || *endptr != '\0' ||
			    amount == 0 || amount > 100) {
				(void) fprintf(stderr,
				    gettext("invalid percentage value\n"));
				usage(B_FALSE);
			}
			unit = ZPOOL_DDT_PRUNE_PERCENTAGE;
			break;
		case '?':
			(void) fprintf(stderr, gettext("invalid option '%c'\n"),
			    optopt);
			usage(B_FALSE);
		}
	}
	argc -= optind;
	argv += optind;

	if (unit == ZPOOL_DDT_PRUNE_NONE) {
		(void) fprintf(stderr,
		    gettext("missing amount option (-d|-p <value>)\n"));
		usage(B_FALSE);
	} else if (argc < 1) {
		(void) fprintf(stderr, gettext("missing pool argument\n"));
		usage(B_FALSE);
	} else if (argc > 1) {
		(void) fprintf(stderr, gettext("too many arguments\n"));
		usage(B_FALSE);
	}
	zhp = zpool_open(g_zfs, argv[0]);
	if (zhp == NULL)
		return (-1);

	int error = zpool_ddt_prune(zhp, unit, amount);

	zpool_close(zhp);

	return (error);
}

static int
find_command_idx(const char *command, int *idx)
{
	for (int i = 0; i < NCOMMAND; ++i) {
		if (command_table[i].name == NULL)
			continue;

		if (strcmp(command, command_table[i].name) == 0) {
			*idx = i;
			return (0);
		}
	}
	return (1);
}

/*
 * Display version message
 */
static int
zpool_do_version(int argc, char **argv)
{
	int c;
	nvlist_t *jsobj = NULL, *zfs_ver = NULL;
	boolean_t json = B_FALSE;

	struct option long_options[] = {
		{"json", no_argument, NULL, 'j'},
	};

	while ((c = getopt_long(argc, argv, "j", long_options, NULL)) != -1) {
		switch (c) {
		case 'j':
			json = B_TRUE;
			jsobj = zpool_json_schema(0, 1);
			break;
		case '?':
			(void) fprintf(stderr, gettext("invalid option '%c'\n"),
			    optopt);
			usage(B_FALSE);
		}
	}

	argc -= optind;
	if (argc != 0) {
		(void) fprintf(stderr, "too many arguments\n");
		usage(B_FALSE);
	}

	if (json) {
		zfs_ver = zfs_version_nvlist();
		if (zfs_ver) {
			fnvlist_add_nvlist(jsobj, "zfs_version", zfs_ver);
			zcmd_print_json(jsobj);
			fnvlist_free(zfs_ver);
			return (0);
		} else
			return (-1);
	} else
		return (zfs_version_print() != 0);
}

/* Display documentation */
static int
zpool_do_help(int argc, char **argv)
{
	char page[MAXNAMELEN];
	if (argc < 3 || strcmp(argv[2], "zpool") == 0)
		strcpy(page, "zpool");
	else if (strcmp(argv[2], "concepts") == 0 ||
	    strcmp(argv[2], "props") == 0)
		snprintf(page, sizeof (page), "zpool%s", argv[2]);
	else
		snprintf(page, sizeof (page), "zpool-%s", argv[2]);

	execlp("man", "man", page, NULL);

	fprintf(stderr, "couldn't run man program: %s", strerror(errno));
	return (-1);
}

/*
 * Do zpool_load_compat() and print error message on failure
 */
static zpool_compat_status_t
zpool_do_load_compat(const char *compat, boolean_t *list)
{
	char report[1024];

	zpool_compat_status_t ret;

	ret = zpool_load_compat(compat, list, report, 1024);
	switch (ret) {

	case ZPOOL_COMPATIBILITY_OK:
		break;

	case ZPOOL_COMPATIBILITY_NOFILES:
	case ZPOOL_COMPATIBILITY_BADFILE:
	case ZPOOL_COMPATIBILITY_BADTOKEN:
		(void) fprintf(stderr, "Error: %s\n", report);
		break;

	case ZPOOL_COMPATIBILITY_WARNTOKEN:
		(void) fprintf(stderr, "Warning: %s\n", report);
		ret = ZPOOL_COMPATIBILITY_OK;
		break;
	}
	return (ret);
}

int
main(int argc, char **argv)
{
	int ret = 0;
	int i = 0;
	char *cmdname;
	char **newargv;

	(void) setlocale(LC_ALL, "");
	(void) setlocale(LC_NUMERIC, "C");
	(void) textdomain(TEXT_DOMAIN);
	srand(time(NULL));

	opterr = 0;

	/*
	 * Make sure the user has specified some command.
	 */
	if (argc < 2) {
		(void) fprintf(stderr, gettext("missing command\n"));
		usage(B_FALSE);
	}

	cmdname = argv[1];

	/*
	 * Special case '-?'
	 */
	if ((strcmp(cmdname, "-?") == 0) || strcmp(cmdname, "--help") == 0)
		usage(B_TRUE);

	/*
	 * Special case '-V|--version'
	 */
	if ((strcmp(cmdname, "-V") == 0) || (strcmp(cmdname, "--version") == 0))
		return (zfs_version_print() != 0);

	/*
	 * Special case 'help'
	 */
	if (strcmp(cmdname, "help") == 0)
		return (zpool_do_help(argc, argv));

	if ((g_zfs = libzfs_init()) == NULL) {
		(void) fprintf(stderr, "%s\n", libzfs_error_init(errno));
		return (1);
	}

	libzfs_print_on_error(g_zfs, B_TRUE);

	zfs_save_arguments(argc, argv, history_str, sizeof (history_str));

	/*
	 * Many commands modify input strings for string parsing reasons.
	 * We create a copy to protect the original argv.
	 */
	newargv = safe_malloc((argc + 1) * sizeof (newargv[0]));
	for (i = 0; i < argc; i++)
		newargv[i] = strdup(argv[i]);
	newargv[argc] = NULL;

	/*
	 * Run the appropriate command.
	 */
	if (find_command_idx(cmdname, &i) == 0) {
		current_command = &command_table[i];
		ret = command_table[i].func(argc - 1, newargv + 1);
	} else if (strchr(cmdname, '=')) {
		verify(find_command_idx("set", &i) == 0);
		current_command = &command_table[i];
		ret = command_table[i].func(argc, newargv);
	} else if (strcmp(cmdname, "freeze") == 0 && argc == 3) {
		/*
		 * 'freeze' is a vile debugging abomination, so we treat
		 * it as such.
		 */
		zfs_cmd_t zc = {"\0"};

		(void) strlcpy(zc.zc_name, argv[2], sizeof (zc.zc_name));
		ret = zfs_ioctl(g_zfs, ZFS_IOC_POOL_FREEZE, &zc);
		if (ret != 0) {
			(void) fprintf(stderr,
			gettext("failed to freeze pool: %d\n"), errno);
			ret = 1;
		}

		log_history = 0;
	} else {
		(void) fprintf(stderr, gettext("unrecognized "
		    "command '%s'\n"), cmdname);
		usage(B_FALSE);
		ret = 1;
	}

	for (i = 0; i < argc; i++)
		free(newargv[i]);
	free(newargv);

	if (ret == 0 && log_history)
		(void) zpool_log_history(g_zfs, history_str);

	libzfs_fini(g_zfs);

	/*
	 * The 'ZFS_ABORT' environment variable causes us to dump core on exit
	 * for the purposes of running ::findleaks.
	 */
	if (getenv("ZFS_ABORT") != NULL) {
		(void) printf("dumping core by request\n");
		abort();
	}

	return (ret);
}
