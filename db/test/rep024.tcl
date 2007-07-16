# See the file LICENSE for redistribution information.
#
# Copyright (c) 2004-2006
#	Oracle Corporation.  All rights reserved.
#
# $Id: rep024.tcl,v 12.9 2006/08/24 14:46:37 bostic Exp $
#
# TEST  	rep024
# TEST	Replication page allocation / verify test
# TEST
# TEST	Start a master (site 1) and a client (site 2).  Master
# TEST	closes (simulating a crash).  Site 2 becomes the master
# TEST	and site 1 comes back up as a client.  Verify database.

proc rep024 { method { niter 1000 } { tnum "024" } args } {

	source ./include.tcl
	if { $is_windows9x_test == 1 } {
		puts "Skipping replication test on Win 9x platform."
		return
	}

	# Run for all access methods.
	if { $checking_valid_methods } {
		return "ALL"
	}

	global fixed_len
	set orig_fixed_len $fixed_len
	set fixed_len 448
	set args [convert_args $method $args]
	set logsets [create_logsets 2]

	# Run all tests with and without recovery.
	set envargs ""
	foreach r $test_recopts {
		foreach l $logsets {
			set logindex [lsearch -exact $l "in-memory"]
			if { $r == "-recover" && $logindex != -1 } {
				puts "Rep$tnum: Skipping\
				    for in-memory logs with -recover."
				continue
			}
			puts "Rep$tnum ($method $r): \
			    Replication page allocation/verify test."
			puts "Rep$tnum: Master logs are [lindex $l 0]"
			puts "Rep$tnum: Client logs are [lindex $l 1]"
			rep024_sub $method $niter $tnum $envargs $l $r $args
		}
	}
	set fixed_len $orig_fixed_len
	return
}

proc rep024_sub { method niter tnum envargs logset recargs largs } {
	source ./include.tcl
	global testdir
	env_cleanup $testdir

	replsetup $testdir/MSGQUEUEDIR

	set masterdir $testdir/MASTERDIR
	set clientdir $testdir/CLIENTDIR

	file mkdir $masterdir
	file mkdir $clientdir
	set m_logtype [lindex $logset 0]
	set c_logtype [lindex $logset 1]

	# In-memory logs require a large log buffer, and cannot
	# be used with -txn nosync.  This test requires -txn, so
	# we only have to adjust the logargs.
	set m_logargs [adjust_logargs $m_logtype]
	set c_logargs [adjust_logargs $c_logtype]

	if { [is_record_based $method] == 1 } {
		set checkfunc test024_recno.check
	} else {
		set checkfunc test024.check
	}

	# Open a master.
	repladd 1
	set env_cmd(1) "berkdb_env_noerr -create \
	    -log_max 1000000 $envargs $recargs -home $masterdir \
	    -errpfx MASTER -txn $m_logargs \
	    -rep_transport \[list 1 replsend\]"
#	set env_cmd(1) "berkdb_env_noerr -create \
#	    -log_max 1000000 $envargs $recargs -home $masterdir \
#	    -verbose {rep on} -errfile /dev/stderr \
#	    -errpfx MASTER -txn $m_logargs \
#	    -rep_transport \[list 1 replsend\]"
	set masterenv [eval $env_cmd(1) -rep_master]
	error_check_good master_env [is_valid_env $masterenv] TRUE

	# Open a client
	repladd 2
	set env_cmd(2) "berkdb_env_noerr -create \
	    -log_max 1000000 $envargs $recargs -home $clientdir \
	    -errpfx CLIENT -txn $c_logargs \
	    -rep_transport \[list 2 replsend\]"
#	set env_cmd(2) "berkdb_env_noerr -create \
#	    -log_max 1000000 $envargs $recargs -home $clientdir \
#	    -verbose {rep on} -errfile /dev/stderr \
#	    -errpfx CLIENT -txn $c_logargs \
#	    -rep_transport \[list 2 replsend\]"
	set clientenv [eval $env_cmd(2) -rep_client]
	error_check_good client_env [is_valid_env $clientenv] TRUE

	# Bring the client online by processing the startup messages.
	set envlist "{$masterenv 1} {$clientenv 2}"
	process_msgs $envlist

	puts "\tRep$tnum.a: Add data to master, update client."
	#
	# This test uses a small page size and a large fixed_len
	# so it is easy to force a page allocation.
	set key [expr $niter + 1]
	set data A
	set pagesize 512
	if { [is_fixed_length $method] == 1 } {
		set bigdata [repeat $data [expr $pagesize / 2]]
	} else {
		set bigdata [repeat $data [expr 2 * $pagesize]]
	}

	set omethod [convert_method $method]
	set testfile "test$tnum.db"
	set db [eval "berkdb_open -create $omethod -auto_commit \
	    -pagesize $pagesize -env $masterenv $largs $testfile"]
	eval rep_test $method $masterenv $db $niter 0 0 0 0 $largs
	process_msgs $envlist

	# Close client.  Force a page allocation on the master.
	# An overflow page (or big page, for hash) will do the job.
	#
	puts "\tRep$tnum.b: Close client, force page allocation on master."
	error_check_good client_close [$clientenv close] 0
#	error_check_good client_verify \
#	    [verify_dir $clientdir "\tRep$tnum.b: " 0 0 1] 0
	set pages1 [r24_check_pages $db $method]
	set txn [$masterenv txn]
	error_check_good put_bigdata [eval {$db put} \
	    -txn $txn {$key [chop_data $method $bigdata]}] 0
	error_check_good txn_commit [$txn commit] 0

	# Verify that we have allocated new pages.
	set pages2 [r24_check_pages $db $method]
	set newpages [expr $pages2 - $pages1]

	# Close master and discard messages for site 2.  Now everybody
	# is closed and sites 1 and 2 have different contents.
	puts "\tRep$tnum.c: Close master."
	error_check_good db_close [$db close] 0
	error_check_good master_close [$masterenv close] 0
	if { $newpages <= 0 } {
		puts "FAIL: no new pages allocated."
		return
	}
	error_check_good master_verify \
	    [verify_dir $masterdir "\tRep$tnum.c: " 0 0 1] 0

	# Run a loop, opening the original client as master and the
	# original master as client.  Test db_verify.
	foreach option { "no new data" "add new data" } {
		puts "\tRep$tnum.d: Swap master and client ($option)."
		set newmasterenv [eval $env_cmd(2) -rep_master]
		set newclientenv [eval $env_cmd(1) -rep_client]
		set envlist "{$newmasterenv 2} {$newclientenv 1}"
		process_msgs $envlist
		if { $option == "add new data" } {
			set key [expr $niter + 2]
			set db [eval "berkdb_open -create $omethod \
			    -auto_commit -pagesize $pagesize \
			    -env $newmasterenv $largs $testfile"]
			set pages1 [r24_check_pages $db $method]
			set txn [$newmasterenv txn]
			error_check_good put_bigdata [eval {$db put} \
			    -txn $txn {$key [chop_data $method $bigdata]}] 0
			error_check_good txn_commit [$txn commit] 0
			set pages2 [r24_check_pages $db $method]
			set newpages [expr $pages2 - $pages1]
			error_check_good db_close [$db close] 0
			process_msgs $envlist
		}
		puts "\tRep$tnum.e: Close master and client, run verify."
		error_check_good newmasterenv_close [$newmasterenv close] 0
		error_check_good newclientenv_close [$newclientenv close] 0
		if { $newpages <= 0 } {
			puts "FAIL: no new pages allocated."
			return
		}
		# This test can leave unreferenced pages on systems without
		# FTRUNCATE and that's OK, so set unref to 0.
		error_check_good verify \
		    [verify_dir $masterdir "\tRep$tnum.f: " 0 0 1 0 0] 0
		error_check_good verify \
		    [verify_dir $clientdir "\tRep$tnum.g: " 0 0 1 0 0] 0
	}
	replclose $testdir/MSGQUEUEDIR
}

proc r24_check_pages { db method } {
	if { [is_hash $method] == 1 } {
		set pages [stat_field $db stat "Number of big pages"]
	} elseif { [is_queue $method] == 1 } {
		set pages [stat_field $db stat "Number of pages"]
	} else {
		set pages [stat_field $db stat "Overflow pages"]
	}
	return $pages
}
