# See the file LICENSE for redistribution information.
#
# Copyright (c) 1999-2003
#	Sleepycat Software.  All rights reserved.
#
# $Id: env007.tcl,v 11.31 2003/11/20 14:32:51 sandstro Exp $
#
# TEST	env007
# TEST	Test DB_CONFIG config file options for berkdb env.
# TEST	1) Make sure command line option is respected
# TEST	2) Make sure that config file option is respected
# TEST	3) Make sure that if -both- DB_CONFIG and the set_<whatever>
# TEST		method is used, only the file is respected.
# TEST	Then test all known config options.
# TEST	Also test config options on berkdb open.  This isn't
# TEST	really env testing, but there's no better place to put it.
proc env007 { } {
	global errorInfo
	global passwd
	global has_crypto
	source ./include.tcl

	puts "Env007: DB_CONFIG and getters test."
	puts "Env007.a: Test berkdb env options using getters and stat."

	# Set up options we can check via stat or getters.  Structure
	# of the list is:
	# 	0.  Arg used in berkdb env command
	# 	1.  Arg used in DB_CONFIG file
	#	2.  Value assigned in berkdb env command
	#	3.  Value assigned in DB_CONFIG file
	# 	4.  Message output during test
	#	5.  Stat command to run (empty if we can't get the info
	#		from stat).
	# 	6.  String to search for in stat output
	# 	7.  Arg used in getter
	#
	set rlist {
	{ " -txn_max " "set_tx_max" "19" "31" "Env007.a1: Txn Max"
	    "txn_stat" "Max Txns" "get_tx_max" }
	{ " -lock_max_locks " "set_lk_max_locks" "17" "29" "Env007.a2: Lock Max"
	    "lock_stat" "Maximum locks" "get_lk_max_locks" }
	{ " -lock_max_lockers " "set_lk_max_lockers" "1500" "2000"
	    "Env007.a3: Max Lockers" "lock_stat" "Maximum lockers"
	    "get_lk_max_lockers" }
	{ " -lock_max_objects " "set_lk_max_objects" "1500" "2000"
	    "Env007.a4: Max Objects" "lock_stat" "Maximum objects"
	    "get_lk_max_objects" }
	{ " -log_buffer " "set_lg_bsize" "65536" "131072" "Env007.a5: Log Bsize"
	    "log_stat" "Log record cache size" "get_lg_bsize" }
	{ " -log_max " "set_lg_max" "8388608" "9437184" "Env007.a6: Log Max"
	    "log_stat" "Current log file size" "get_lg_max" }
	{ " -cachesize " "set_cachesize" "0 536870912 1" "1 0 1"
	    "Env007.a7: Cachesize" "" "" "get_cachesize" }
	{ " -lock_timeout " "set_lock_timeout" "100" "120"
	    "Env007.a8: Lock Timeout" "" "" "get_timeout lock" }
	{ " -log_regionmax " "set_lg_regionmax" "8388608" "4194304"
	    "Env007.a9: Log Regionmax" "" "" "get_lg_regionmax" }
	{ " -mmapsize " "set_mp_mmapsize" "12582912" "8388608"
	    "Env007.a10: Mmapsize" "" "" "get_mp_mmapsize" }
	{ " -shm_key " "set_shm_key" "15" "35" "Env007.a11: Shm Key"
	    "" "" "get_shm_key" }
	{ " -tmp_dir " "set_tmp_dir" "." "./TEMPDIR" "Env007.a12: Temp dir"
	    "" "" "get_tmp_dir" }
	{ " -txn_timeout " "set_txn_timeout" "100" "120"
	    "Env007.a13: Txn timeout" "" "" "get_timeout txn" }
	}

	set e "berkdb_env -create -mode 0644 -home $testdir -txn "
	set qnxexclude {set_cachesize}
	foreach item $rlist {
		set envarg [lindex $item 0]
		set configarg [lindex $item 1]
		set envval [lindex $item 2]
		set configval [lindex $item 3]
		set msg [lindex $item 4]
		set statcmd [lindex $item 5]
		set statstr [lindex $item 6]
		set getter [lindex $item 7]

		if { $is_qnx_test &&
		    [lsearch $qnxexclude $configarg] != -1 } {
			puts "\tSkip $configarg for QNX"
			continue
		}
		env_cleanup $testdir
		# First verify using just env args
		puts "\t$msg Environment argument only"
		set env [eval $e $envarg {$envval}]
		error_check_good envopen:0 [is_valid_env $env] TRUE
		error_check_good get_envval [eval $env $getter] $envval
		if { $statcmd != "" } {
			env007_check $env $statcmd $statstr $envval
		}
		error_check_good envclose:0 [$env close] 0

		env_cleanup $testdir
		env007_make_config $configarg $configval

		# Verify using just config file
		puts "\t$msg Config file only"
		set env [eval $e]
		error_check_good envopen:1 [is_valid_env $env] TRUE
		error_check_good get_configval1 [eval $env $getter] $configval
		if { $statcmd != "" } {
			env007_check $env $statcmd $statstr $configval
		}
		error_check_good envclose:1 [$env close] 0

		# Now verify using env args and config args
		puts "\t$msg Environment arg and config file"
		set env [eval $e $envarg {$envval}]
		error_check_good envopen:2 [is_valid_env $env] TRUE
		# Getter should retrieve config val, not envval.
		error_check_good get_configval2 [eval $env $getter] $configval
		if { $statcmd != "" } {
			env007_check $env $statcmd $statstr $configval
		}
		error_check_good envclose:2 [$env close] 0
	}

	#
	# Test all options that can be set in DB_CONFIG.  Write it out
	# to the file and make sure we can open the env.  This execs
	# the config file code.  Also check with a getter that the
	# expected value is returned.
	#
	puts "\tEnv007.b: Test berkdb env config options using getters\
	    and env open."

	# The cfglist variable contains options that can be set in DB_CONFIG.
	set cfglist {
	{ "set_data_dir" "." "get_data_dirs" "." }
	{ "set_flags" "db_auto_commit" "get_flags" "-auto_commit" }
	{ "set_flags" "db_cdb_alldb" "get_flags" "-cdb_alldb" }
	{ "set_flags" "db_direct_db" "get_flags" "-direct_db" }
	{ "set_flags" "db_direct_log" "get_flags" "-direct_log" }
	{ "set_flags" "db_log_autoremove" "get_flags" "-log_remove" }
	{ "set_flags" "db_nolocking" "get_flags" "-nolock" }
	{ "set_flags" "db_nommap" "get_flags" "-nommap" }
	{ "set_flags" "db_nopanic" "get_flags" "-nopanic" }
	{ "set_flags" "db_overwrite" "get_flags" "-overwrite" }
	{ "set_flags" "db_region_init" "get_flags" "-region_init" }
	{ "set_flags" "db_txn_nosync" "get_flags" "-nosync" }
	{ "set_flags" "db_txn_write_nosync" "get_flags" "-wrnosync" }
	{ "set_flags" "db_yieldcpu" "get_flags" "-yield" }
	{ "set_lg_bsize" "65536" "get_lg_bsize" "65536" }
	{ "set_lg_dir" "." "get_lg_dir" "." }
	{ "set_lg_max" "8388608" "get_lg_max" "8388608" }
	{ "set_lg_regionmax" "65536" "get_lg_regionmax" "65536" }
	{ "set_lk_detect" "db_lock_default" "get_lk_detect" "default" }
	{ "set_lk_detect" "db_lock_expire" "get_lk_detect" "expire" }
	{ "set_lk_detect" "db_lock_maxlocks" "get_lk_detect" "maxlocks" }
	{ "set_lk_detect" "db_lock_minlocks" "get_lk_detect" "minlocks" }
	{ "set_lk_detect" "db_lock_minwrite" "get_lk_detect" "minwrite" }
	{ "set_lk_detect" "db_lock_oldest" "get_lk_detect" "oldest" }
	{ "set_lk_detect" "db_lock_random" "get_lk_detect" "random" }
	{ "set_lk_detect" "db_lock_youngest" "get_lk_detect" "youngest" }
	{ "set_lk_max_lockers" "1500" "get_lk_max_lockers" "1500" }
	{ "set_lk_max_locks" "29" "get_lk_max_locks" "29" }
	{ "set_lk_max_objects" "1500" "get_lk_max_objects" "1500" }
	{ "set_lock_timeout" "100" "get_timeout lock" "100" }
	{ "set_mp_mmapsize" "12582912" "get_mp_mmapsize" "12582912" }
	{ "set_region_init" "1" "get_flags" "-region_init" }
	{ "set_shm_key" "15" "get_shm_key" "15" }
	{ "set_tas_spins" "15" "get_tas_spins" "15" }
	{ "set_tmp_dir" "." "get_tmp_dir" "." }
	{ "set_tx_max" "31" "get_tx_max" "31" }
	{ "set_txn_timeout" "50" "get_timeout txn" "50" }
	{ "set_verbose" "db_verb_chkpoint" "get_verbose chkpt" "on" }
	{ "set_verbose" "db_verb_deadlock" "get_verbose deadlock" "on" }
	{ "set_verbose" "db_verb_recovery" "get_verbose recovery" "on" }
	{ "set_verbose" "db_verb_replication" "get_verbose rep" "on" }
	{ "set_verbose" "db_verb_waitsfor" "get_verbose wait" "on" }
	}

	env_cleanup $testdir
	set e "berkdb_env -create -mode 0644 -home $testdir -txn"
	set qnxexclude {db_direct_db db_direct_log}
	foreach item $cfglist {
		env_cleanup $testdir
		set configarg [lindex $item 0]
		set configval [lindex $item 1]
		set getter [lindex $item 2]
		set getval [lindex $item 3]

		if { $is_qnx_test &&
		    [lsearch $qnxexclude $configval] != -1} {
			puts "\t\t Skip $configarg $configval for QNX"
			continue
		}
		env007_make_config $configarg $configval

		# Verify using config file
		puts "\t\t $configarg $configval"
		if {[catch { set env [eval $e]} res] != 0} {
			puts "FAIL: $res"
			continue
		}
		error_check_good envvalid:1 [is_valid_env $env] TRUE
		error_check_good getter:1 [eval $env $getter] $getval
		error_check_good envclose:1 [$env close] 0
	}

	puts "\tEnv007.c: Test berkdb env options using getters and env open."
	# The envopenlist variable contains options that can be set using
	# berkdb env.  We always set -mpool.
	set envopenlist {
	{ "-cdb" "" "-cdb" "get_open_flags" }
	{ "-errpfx" "FOO" "FOO" "get_errpfx" }
	{ "-lock" "" "-lock" "get_open_flags" }
	{ "-log" "" "-log" "get_open_flags" }
	{ "" "" "-mpool" "get_open_flags" }
	{ "-system_mem" "-shm_key 1" "-system_mem" "get_open_flags" }
	{ "-txn" "" "-txn" "get_open_flags" }
	{ "-recover" "-txn" "-recover" "get_open_flags" }
	{ "-recover_fatal" "-txn" "-recover_fatal" "get_open_flags" }
	{ "-use_environ" "" "-use_environ" "get_open_flags" }
	{ "-use_environ_root" "" "-use_environ_root" "get_open_flags" }
	{ "" "" "-create" "get_open_flags" }
	{ "-private" ""  "-private" "get_open_flags" }
	{ "-thread" "" "-thread" "get_open_flags" }
	{ "-txn_timestamp" "100000000" "100000000" "get_tx_timestamp" }
	}

	if { $has_crypto == 1 } {
		lappend envopenlist \
		    { "-encryptaes" "$passwd" "-encryptaes" "get_encrypt_flags" }
	}

	set e "berkdb_env -create -mode 0644 -home $testdir"
	set qnxexclude {-system_mem}
	foreach item $envopenlist {
		set envarg [lindex $item 0]
		set envval [lindex $item 1]
		set retval [lindex $item 2]
		set getter [lindex $item 3]

		if { $is_qnx_test &&
		    [lsearch $qnxexclude $envarg] != -1} {
			puts "\t\t Skip $envarg for QNX"
			continue
		}
		env_cleanup $testdir
		# Set up env
		set env [eval $e $envarg $envval]
		error_check_good envopen [is_valid_env $env] TRUE

		# Check that getter retrieves expected retval.
		set get_retval [eval $env $getter]
		if { [is_substr $get_retval $retval] != 1 } {
			puts "FAIL: $retval\
			    should be a substring of $get_retval"
			continue
		}
		error_check_good envclose [$env close] 0

		# The -encryptany flag can only be tested on an existing
		# environment that supports encryption, so do it here.
		if { $has_crypto == 1 } {
			if { $envarg == "-encryptaes" } {
				set env [eval berkdb_env -home $testdir\
				    -encryptany $passwd]
				error_check_good get_encryptany \
				    [eval $env get_encrypt_flags] "-encryptaes"
				error_check_good env_close [$env close] 0
			}
		}
	}

	puts "\tEnv007.d: Test berkdb env options using set_flags and getters."

	# The flaglist variable contains options that can be set using
	# $env set_flags.
	set flaglist {
	{ "-direct_db" }
	{ "-direct_log" }
	{ "-log_remove" }
	{ "-nolock" }
	{ "-nommap" }
	{ "-nopanic" }
	{ "-nosync" }
	{ "-overwrite" }
	{ "-panic" }
	{ "-wrnosync" }
	}
	set e "berkdb_env -create -mode 0644 -home $testdir"
	set qnxexclude {-direct_db -direct_log}
	foreach item $flaglist {
		set flag [lindex $item 0]
		if { $is_qnx_test &&
		    [lsearch $qnxexclude $flag] != -1} {
			puts "\t\t Skip $flag for QNX"
			continue
		}
		env_cleanup $testdir

		# Set up env
		set env [eval $e]
		error_check_good envopen [is_valid_env $env] TRUE

		# Use set_flags to turn on new env characteristics.
		error_check_good "flag $flag on" [$env set_flags $flag on] 0
		# Check that getter retrieves expected retval.
		set get_retval [eval $env get_flags]
		if { [is_substr $get_retval $flag] != 1 } {
			puts "FAIL: $flag should be a substring of $get_retval"
			continue
		}
		# Use set_flags to turn off env characteristics, make sure
		# they are gone.
		error_check_good "flag $flag off" [$env set_flags $flag off] 0
		set get_retval [eval $env get_flags]
		if { [is_substr $get_retval $flag] == 1 } {
			puts "FAIL: $flag should not be in $get_retval"
			continue
		}

		error_check_good envclose [$env close] 0
	}

	puts "\tEnv007.e: Test env get_home."
	env_cleanup $testdir
	# Set up env
	set env [eval $e]
	error_check_good env_open [is_valid_env $env] TRUE
	# Test for correct value.
	set get_retval [eval $env get_home]
	error_check_good get_home $get_retval $testdir
	error_check_good envclose [$env close] 0

	puts "\tEnv007.f: Test that bad config values are rejected."
	set cfglist {
	{ "set_cachesize" "1048576" }
	{ "set_flags" "db_xxx" }
	{ "set_flags" "1" }
	{ "set_flags" "db_txn_nosync x" }
	{ "set_lg_bsize" "db_xxx" }
	{ "set_lg_max" "db_xxx" }
	{ "set_lg_regionmax" "db_xxx" }
	{ "set_lk_detect" "db_xxx" }
	{ "set_lk_detect" "1" }
	{ "set_lk_detect" "db_lock_youngest x" }
	{ "set_lk_max" "db_xxx" }
	{ "set_lk_max_locks" "db_xxx" }
	{ "set_lk_max_lockers" "db_xxx" }
	{ "set_lk_max_objects" "db_xxx" }
	{ "set_mp_mmapsize" "db_xxx" }
	{ "set_region_init" "db_xxx" }
	{ "set_shm_key" "db_xxx" }
	{ "set_tas_spins" "db_xxx" }
	{ "set_tx_max" "db_xxx" }
	{ "set_verbose" "db_xxx" }
	{ "set_verbose" "1" }
	{ "set_verbose" "db_verb_recovery x" }
	}

	set e "berkdb_env_noerr -create -mode 0644 \
	    -home $testdir -log -lock -txn "
	foreach item $cfglist {
		set configarg [lindex $item 0]
		set configval [lindex $item 1]

		env007_make_config $configarg $configval

		#  verify using just config file
		set stat [catch {eval $e} ret]
		error_check_good envopen $stat 1
		error_check_good error [is_substr $errorInfo \
		    "incorrect arguments for name-value pair"] 1
	}

	puts "\tEnv007.g: Config name error set_xxx"
	set e "berkdb_env_noerr -create -mode 0644 \
	    -home $testdir -log -lock -txn "
	env007_make_config "set_xxx" 1
	set stat [catch {eval $e} ret]
	error_check_good envopen $stat 1
	error_check_good error [is_substr $errorInfo \
		    "unrecognized name-value pair"] 1

	puts "\tEnv007.h: Test berkdb open flags and getters."
	# Check options that we configure with berkdb open and
	# query via getters.  Structure of the list is:
	# 	0.  Flag used in berkdb open command
	#	1.  Value specified to flag
	#	2.  Specific method, if needed
	# 	3.  Arg used in getter

	set olist {
	{ "-minkey" "4" " -btree " "get_bt_minkey" }
	{ "-cachesize" "0 1048576 1" "" "get_cachesize" }
	{ "" "FILENAME DBNAME" "" "get_dbname" }
	{ "" "" "" "get_env" }
	{ "-errpfx" "ERROR:" "" "get_errpfx" }
	{ "" "-chksum" "" "get_flags" }
	{ "-delim" "58" "-recno" "get_re_delim" }
	{ "" "-dup" "" "get_flags" }
	{ "" "-dup -dupsort" "" "get_flags" }
	{ "" "-recnum" "" "get_flags" }
	{ "" "-revsplitoff" "" "get_flags" }
	{ "" "-renumber" "-recno" "get_flags" }
	{ "" "-snapshot" "-recno" "get_flags" }
	{ "" "-create" "" "get_open_flags" }
	{ "" "-create -dirty" "" "get_open_flags" }
	{ "" "-create -excl" "" "get_open_flags" }
	{ "" "-create -nommap" "" "get_open_flags" }
	{ "" "-create -thread" "" "get_open_flags" }
	{ "" "-create -truncate" "" "get_open_flags" }
	{ "-ffactor" "40" " -hash " "get_h_ffactor" }
	{ "-lorder" "4321" "" "get_lorder" }
	{ "-nelem" "10000" " -hash " "get_h_nelem" }
	{ "-pagesize" "4096" "" "get_pagesize" }
	{ "-extent" "4" "-queue" "get_q_extentsize" }
	{ "-len" "20" "-recno" "get_re_len" }
	{ "-pad" "0" "-recno" "get_re_pad" }
	{ "-source" "include.tcl" "-recno" "get_re_source" }
	}

	set o "berkdb_open -create -mode 0644"
	foreach item $olist {
		cleanup $testdir NULL
		set flag [lindex $item 0]
		set flagval [lindex $item 1]
		set method [lindex $item 2]
		if { $method == "" } {
			set method " -btree "
		}
		set getter [lindex $item 3]

		# Check that open is successful with the flag.
		# The option -cachesize requires grouping for $flagval.
		if { $flag == "-cachesize" } {
			set db [eval $o $method $flag {$flagval}\
			    $testdir/a.db]
		} else {
			set db [eval $o $method $flag $flagval\
			    $testdir/a.db]
		}
		error_check_good dbopen:0 [is_valid_db $db] TRUE

		# Check that getter retrieves the correct value.
		# Cachesizes under 500MB are adjusted upward to
		# about 25% so just make sure we're in the right
		# ballpark, between 1.2 and 1.3 of the original value.
		if { $flag == "-cachesize" } {
			set retval [eval $db $getter]
			set retbytes [lindex $retval 1]
			set setbytes [lindex $flagval 1]
			error_check_good cachesize_low\
			    [expr $retbytes > [expr $setbytes * 6 / 5]] 1
			error_check_good cachesize_high\
			    [expr $retbytes < [expr $setbytes * 13 / 10]] 1
		} else {
			error_check_good get_flagval \
			    [eval $db $getter] $flagval
		}
		error_check_good dbclose:0 [$db close] 0
	}

	puts "\tEnv007.i: Test berkdb_open -rdonly."
	# This test is done separately because -rdonly can only be specified
	# on an already existing database.
	set flag "-rdonly"
	set db [eval berkdb_open $flag $testdir/a.db]
	error_check_good open_rdonly [is_valid_db $db] TRUE

	error_check_good get_rdonly [eval $db get_open_flags] $flag
	error_check_good dbclose:0 [$db close] 0

	puts "\tEnv007.j: Test berkdb open flags and getters\
	    requiring environments."
	# Check options that we configure with berkdb open and
	# query via getters.  Structure of the list is:
	# 	0.  Flag used in berkdb open command
	#	1.  Value specified to flag
	#	2.  Specific method, if needed
	# 	3.  Arg used in getter
	# 	4.  Additional flags needed in setting up env

	set elist {
	{ "" "-auto_commit" "" "get_open_flags" "" }
	}

	if { $has_crypto == 1 } {
		lappend elist \
		    { "" "-encrypt" "" "get_flags" "-encryptaes $passwd" }
	}

	set e "berkdb_env -create -home $testdir -txn "
	set o "berkdb_open -create -btree -mode 0644 "
	foreach item $elist {
		env_cleanup $testdir
		set flag [lindex $item 0]
		set flagval [lindex $item 1]
		set method [lindex $item 2]
		if { $method == "" } {
			set method " -btree "
		}
		set getter [lindex $item 3]
		set envflag [lindex $item 4]

		# Check that open is successful with the flag.
		set env [eval $e $envflag]
		set db [eval $o -env $env $flag $flagval a.db]
		error_check_good dbopen:0 [is_valid_db $db] TRUE

		# Check that getter retrieves the correct value
		set get_flagval [eval $db $getter]
		error_check_good get_flagval [is_substr $get_flagval $flagval] 1
		error_check_good dbclose [$db close] 0
		error_check_good envclose [$env close] 0
	}
}

proc env007_check { env statcmd statstr testval } {
	set stat [$env $statcmd]
	set checked 0
	foreach statpair $stat {
		if {$checked == 1} {
			break
		}
		set statmsg [lindex $statpair 0]
		set statval [lindex $statpair 1]
		if {[is_substr $statmsg $statstr] != 0} {
			set checked 1
			error_check_good $statstr:ck $statval $testval
		}
	}
	error_check_good $statstr:test $checked 1
}

proc env007_make_config { carg cval } {
	global testdir

	set cid [open $testdir/DB_CONFIG w]
	puts $cid "$carg $cval"
	close $cid
}
