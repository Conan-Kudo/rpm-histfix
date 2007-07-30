# See the file LICENSE for redistribution information.
#
# Copyright (c) 2004,2007 Oracle.  All rights reserved.
#
# $Id: rep034.tcl,v 12.22 2007/06/21 16:32:24 alanb Exp $
#
# TEST	rep034
# TEST	Test of STARTUPDONE notification.
# TEST
# TEST	STARTUPDONE can now be recognized without the need for new "live" log
# TEST  records from the master (under favorable conditions).  The response to
# TEST  the ALL_REQ at the end of synchronization includes an end-of-log marker
# TEST  that now triggers it.  However, the message containing that end marker
# TEST  could get lost, so live log records still serve as a back-up mechanism.
# TEST  The end marker may also be set under c2c sync, but only if the serving
# TEST  client has itself achieved STARTUPDONE.
#
proc rep034 { method { niter 2 } { tnum "034" } args } {

	source ./include.tcl
	if { $is_windows9x_test == 1 } {
		puts "Skipping replication test on Win 9x platform."
		return
	}

	# Valid for all access methods.
	if { $checking_valid_methods } {
		return "ALL"
	}

	set args [convert_args $method $args]
	set logsets [create_logsets 3]
	foreach l $logsets {
		puts "Rep$tnum ($method $args):\
		    Test of startup synchronization detection."
		puts "Rep$tnum: Master logs are [lindex $l 0]"
		puts "Rep$tnum: Client 0 logs are [lindex $l 1]"
		puts "Rep$tnum: Client 1 logs are [lindex $l 2]"
		rep034_sub $method $niter $tnum $l $args
	}
}

# This test manages on its own the decision of whether or not to open an
# environment with recovery.  (It varies throughout the test.)  Therefore there
# is no need to run it twice (as we often do with a loop in the main proc).
# 
proc rep034_sub { method niter tnum logset largs } {
	global anywhere
	global testdir
	global startup_done
	global rep_verbose
	global rep034_got_allreq

	set verbargs ""
	if { $rep_verbose == 1 } {
		set verbargs " -verbose {rep on} "
	}

	env_cleanup $testdir

	replsetup $testdir/MSGQUEUEDIR

	set masterdir $testdir/MASTERDIR
	set clientdir $testdir/CLIENTDIR
	set clientdir2 $testdir/CLIENTDIR2

	file mkdir $masterdir
	file mkdir $clientdir
	file mkdir $clientdir2

	set m_logtype [lindex $logset 0]
	set c_logtype [lindex $logset 1]
	set c2_logtype [lindex $logset 2]

	# In-memory logs require a large log buffer, and cannot
	# be used with -txn nosync.
	set m_logargs [adjust_logargs $m_logtype]
	set c_logargs [adjust_logargs $c_logtype]
	set c2_logargs [adjust_logargs $c2_logtype]
	set m_txnargs [adjust_txnargs $m_logtype]
	set c_txnargs [adjust_txnargs $c_logtype]
	set c2_txnargs [adjust_txnargs $c2_logtype]

	# In first part of test master serves requests.
	# 
	set anywhere 0

	# Create a master; add some data.
	# 
	repladd 1
	set ma_envcmd "berkdb_env_noerr -create $m_txnargs $m_logargs \
	    -event rep_event $verbargs -errpfx MASTER \
	    -home $masterdir -rep_master -rep_transport \[list 1 replsend\]"
	set masterenv [eval $ma_envcmd]
	puts "\tRep$tnum.a: Create master; add some data."
	eval rep_test $method $masterenv NULL $niter 0 0 0 0 $largs

	# Bring up a new client, and see that it can get STARTUPDONE with no new
	# live transactions at the master.
	# 
	puts "\tRep$tnum.b: Bring up client; check STARTUPDONE."
	repladd 2
	set cl_envcmd "berkdb_env_noerr -create $c_txnargs $c_logargs \
	    -event rep_event $verbargs -errpfx CLIENT \
	    -home $clientdir -rep_client -rep_transport \[list 2 replsend\]"
	set clientenv [eval $cl_envcmd]
	set envlist "{$masterenv 1} {$clientenv 2}"
	set startup_done 0
	process_msgs $envlist

	error_check_good done_without_live_txns \
	    [stat_field $clientenv rep_stat "Startup complete"] 1

	# Test that the event got fired as well.  In the rest of the test things
	# get a little complex (what with having two clients), so only check the
	# event part here.  The important point is the various ways that
	# STARTUPDONE can be computed, so testing the event firing mechanism
	# just this once is enough.
	#
	error_check_good done_event_too $startup_done 1

	#
	# Bring up another client.  Do additional new txns at master, ensure
	# that STARTUPDONE is not triggered at NEWMASTER LSN.
	# 
	puts "\tRep$tnum.c: Another client; no STARTUPDONE at NEWMASTER LSN."
	set newmaster_lsn [stat_field $masterenv rep_stat "Next LSN expected"]
	repladd 3
	#
	# !!! Please note that we're giving client2 a special customized version
	# of the replication transport call-back function.
	#
	set cl2_envcmd "berkdb_env_noerr -create $c2_txnargs $c2_logargs \
	    -event rep_event $verbargs -errpfx CLIENT2 \
	    -home $clientdir2 -rep_client -rep_transport \[list 3 rep034_send\]"
	set client2env [eval $cl2_envcmd]

	set envlist "{$masterenv 1} {$clientenv 2} {$client2env 3}"
	set verified false
	for {set i 0} {$i < 10} {incr i} {
		proc_msgs_once $envlist
		set client2lsn \
		    [stat_field $client2env rep_stat "Next LSN expected"]

		# Get to the point where we've gone past where the master's LSN
		# was at NEWMASTER time, and make sure we haven't yet gotten
		# STARTUPDONE.  Ten loop iterations should be plenty.
		# 
		if {[$client2env log_compare $client2lsn $newmaster_lsn] > 0} {
			if {![stat_field \
			    $client2env rep_stat "Startup complete"]} {
				set verified true
			}
			break;
		}
		eval rep_test $method $masterenv NULL $niter 0 0 0 0 $largs
	}
	error_check_good no_newmaster_trigger $verified true

	process_msgs $envlist
	error_check_good done_during_live_txns \
	    [stat_field $client2env rep_stat "Startup complete"] 1

	#
	# From here on out we use client-to-client sync.
	# 
	set anywhere 1

	# Here we rely on recovery at client 1.  If that client is running with
	# in-memory logs, forgo the remainder of the test.
	#
	if {$c_logtype eq "in-mem"} {
		puts "\tRep$tnum.d: Skip rest of test for in-memory logging."
		$masterenv close
		$clientenv close
		$client2env close
		replclose $testdir/MSGQUEUEDIR
		return
	}

	# Shut down client 1.  Bring it back, with recovery.  Verify that it can
	# get STARTUPDONE by syncing to other client, even with no new master
	# txns.
	# 
	puts "\tRep$tnum.d: Verify STARTUPDONE using c2c sync."
	$clientenv close
	set clientenv [eval $cl_envcmd -recover]
	set envlist "{$masterenv 1} {$clientenv 2} {$client2env 3}"

	# Clear counters at client2, so that we can check "Client service
	# requests" in a moment.
	# 
	$client2env rep_stat -clear
	process_msgs $envlist
	error_check_good done_via_c2c \
	    [stat_field $clientenv rep_stat "Startup complete"] 1
	#
	# Make sure our request was served by client2.  This isn't a test of c2c
	# sync per se, but if this fails it indicates that we're not really
	# testing what we thought we were testing.
	# 
	error_check_bad c2c_served_by_master \
	    [stat_field $client2env rep_stat "Client service requests"] 0

	# Verify that we don't get STARTUPDONE if we are using c2c sync to
	# another client, and the serving client has not itself reached
	# STARTUPDONE, because that suggests that the serving client could be
	# way far behind.   But that we can still eventually get STARTUPDONE, as
	# a fall-back, once the master starts generating new txns again.
	#
	# To do so, we'll need to restart both clients.  Start with the client
	# that will serve the request.  Turn off "anywhere" process for a moment
	# so that we can get this client set up without having the other one
	# running.
	#
	# Now it's client 2 that needs recovery.  Forgo the rest of the test if
	# it is logging in memory.  (We could get this far in mixed mode, with
	# client 1 logging on disk.)
	# 
	if {$c2_logtype eq "in-mem"} {
		puts "\tRep$tnum.e: Skip rest of test for in-memory logging."
		$masterenv close
		$clientenv close
		$client2env close
		replclose $testdir/MSGQUEUEDIR
		return
	}
	puts "\tRep$tnum.e: Check no STARTUPDONE when c2c server is behind."
	$clientenv close
	$client2env close
	
	set anywhere 0
	set client2env [eval $cl2_envcmd -recover]
	set envlist "{$masterenv 1} {$client2env 3}"
	
	# We want client2 to get partway through initialization, but once it
	# sends the ALL_REQ to the master, we want to cut things off there.
	# Recall that we gave client2 a special "wrapper" version of the
	# replication transport call-back function: that function will set a
	# flag when it sees an ALL_REQ message go by.
	# 
	set rep034_got_allreq false
	while { !$rep034_got_allreq } {
		proc_msgs_once $envlist
	}

	#
	# To make sure we're doing a valid test, verify that we really did
	# succeed in getting the serving client into the state we intended.
	# 
	error_check_good serve_from_notstarted \
	    [stat_field $client2env rep_stat "Startup complete"] 0

	# Start up the client to be tested.  Make sure it doesn't get
	# STARTUPDONE (yet).  Again, the checking of service request stats is
	# just for test debugging, to make sure we have a valid test.
	#
	# To add insult to injury, not only do we not get STARTUPDONE from the
	# "behind" client, we also don't even get all the log records we need
	# (because we didn't allow client2's ALL_REQ to get to the master).
	# And no mechanism to let us know that.  The only resolution is to wait
	# for gap detection to rerequest (which would then go to the master).
	# So, set a small rep_request upper bound, so that it doesn't take a ton
	# of new live txns to reach the trigger.
	# 
	set anywhere 1
	$client2env rep_stat -clear
	replclear 2
	set clientenv [eval $cl_envcmd -recover]
	$clientenv rep_request 4 4
	set envlist "{$masterenv 1} {$clientenv 2} {$client2env 3}"

	while {[rep034_proc_msgs_once $masterenv $clientenv $client2env] > 0} {}

	error_check_good not_from_undone_c2c_client \
	    [stat_field $clientenv rep_stat "Startup complete"] 0

	error_check_bad c2c_served_by_master \
	    [stat_field $client2env rep_stat "Client service requests"] 0

	# Verify that we nevertheless *do* get STARTUPDONE after the master
	# starts generating new txns again.
	# 
	puts "\tRep$tnum.f: Check STARTUPDONE via fall-back to live txns."
	eval rep_test $method $masterenv NULL $niter 0 0 0 0 $largs
	process_msgs $envlist
	error_check_good fallback_live_txns \
	    [stat_field $clientenv rep_stat "Startup complete"] 1

	$masterenv close
	$clientenv close
	$client2env close
	replclose $testdir/MSGQUEUEDIR
	set anywhere 0
}

# Do a round of message processing, but juggle things such that client2 can
# never receive a message from the master.
#
# Assumes the usual "{$masterenv 1} {$clientenv 2} {$client2env 3}" structure.
# 
proc rep034_proc_msgs_once { masterenv clientenv client2env } {
	set nproced [proc_msgs_once "{$masterenv 1}" NONE err]
	error_check_good pmonce_1 $err 0
	replclear 3
	
	incr nproced [proc_msgs_once "{$clientenv 2} {$client2env 3}" NONE err]
	error_check_good pmonce_2 $err 0

	return $nproced
}

# Wrapper for replsend.  Mostly just a pass-through to the real replsend, except
# we watch for an ALL_REQ, and just set a flag when we see it.
# 
proc rep034_send { control rec fromid toid flags lsn } {
	global rep034_got_allreq

	if {[berkdb msgtype $control] eq "all_req"} {
		set rep034_got_allreq true
	}
	return [replsend $control $rec $fromid $toid $flags $lsn]
}
