#!/bin/ksh -p
# SPDX-License-Identifier: CDDL-1.0

#
# This file and its contents are supplied under the terms of the
# Common Development and Distribution License ("CDDL"), version 1.0.
# You may only use this file in accordance with the terms of version
# 1.0 of the CDDL.
#
# A full copy of the text of the CDDL should have accompanied this
# source.  A copy of the CDDL is also available via the Internet at
# http://www.illumos.org/license/CDDL.
#

#
# Copyright (c) 2017 by Delphix. All rights reserved.
#

. $STF_SUITE/tests/functional/pool_checkpoint/pool_checkpoint.kshlib

#
# DESCRIPTION:
#	Ensure that checkpoint verification within zdb works as
#	we expect.
#
# STRATEGY:
#	1. Create pool
#	2. Populate it
#	3. Take checkpoint
#	4. Modify data (include at least one destructive change) 
#	5. Verify zdb finds checkpoint when run on current state
#	6. Verify zdb finds old dataset when run on checkpointed
#	   state
#	7. Export pool, and verify the same things with zdb to
#	   test the -e option.
#	8. Import pool and discard checkpoint
#	9. Verify zdb does not find the checkpoint anymore in the
#	   current state.
#	10.Verify that zdb cannot find the checkpointed state
#	   anymore when trying to open it for verification.
#

verify_runnable "global"

#
# zdb does this thing where it imports the checkpointed state of the
# pool under a new pool with a different name, alongside the pool
# with the current state. The name of this temporary pool is the
# name of the actual pool with the suffix below appended to it.
#
BOGUS_SUFFIX="_CHECKPOINTED_UNIVERSE"
CHECKPOINTED_FS1=$TESTPOOL$BOGUS_SUFFIX/$TESTFS1

setup_test_pool
log_onexit cleanup_test_pool

populate_test_pool
log_must zpool checkpoint $TESTPOOL

test_change_state_after_checkpoint

log_must eval "zdb $TESTPOOL | grep -q \"Checkpointed uberblock found\""
log_mustnot eval "zdb -k $TESTPOOL | grep -q \"Checkpointed uberblock found\""
log_mustnot eval "zdb $TESTPOOL | grep \"Dataset $FS1\""
log_must eval "zdb -k $TESTPOOL | grep \"Dataset $CHECKPOINTED_FS1\""
log_must eval "zdb -k $TESTPOOL/ | grep \"$TESTPOOL$BOGUS_SUFFIX\""

log_must zpool export $TESTPOOL

log_must eval "zdb -e $TESTPOOL | grep \"Checkpointed uberblock found\""
log_mustnot eval "zdb -k -e $TESTPOOL | grep \"Checkpointed uberblock found\""
log_mustnot eval "zdb -e $TESTPOOL | grep \"Dataset $FS1\""
log_must eval "zdb -k -e $TESTPOOL | grep \"Dataset $CHECKPOINTED_FS1\""
log_must eval "zdb -k -e $TESTPOOL/ | grep \"$TESTPOOL$BOGUS_SUFFIX\""

log_must zpool import $TESTPOOL

log_must zpool checkpoint -d $TESTPOOL

log_mustnot eval "zdb $TESTPOOL | grep \"Checkpointed uberblock found\""
log_mustnot eval "zdb -k $TESTPOOL"

log_pass "zdb can analyze checkpointed pools."
