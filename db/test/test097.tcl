# See the file LICENSE for redistribution information.
#
# Copyright (c) 1996-2002
#	Sleepycat Software.  All rights reserved.
#
# Id: test097.tcl,v 11.7 2002/08/08 15:38:13 bostic Exp 
#
# TEST	test097
# TEST	Open up a large set of database files simultaneously.
# TEST	Adjust for local file descriptor resource limits.
# TEST	Then use the first 1000 entries from the dictionary.
# TEST	Insert each with self as key and a fixed, medium length data string;
# TEST	retrieve each. After all are entered, retrieve all; compare output
# TEST	to original.

proc test097 { method {ndbs 500} {nentries 400} args } {
	global pad_datastr
	source ./include.tcl

	set args [convert_args $method $args]
	set encargs ""
	set args [split_encargs $args encargs]

	puts -nonewline "Test097: $method ($args) "
	puts "$nentries entries in at most $ndbs simultaneous databases"

	# Open an environment, with a 1MB cache.
	set eindex [lsearch -exact $args "-env"]
	if { $eindex != -1 } {
		incr eindex
		set env [lindex $args $eindex]
		puts "Test097: $method: skipping for env $env"
		return
	}
	env_cleanup $testdir
	set env [eval {berkdb_env -create \
	     -cachesize { 0 1048576 1 } -txn} -home $testdir $encargs]
	error_check_good dbenv [is_valid_env $env] TRUE

	# Create the database and open the dictionary
	set testfile test097.db
	set t1 $testdir/t1
	set t2 $testdir/t2
	set t3 $testdir/t3
	puts "\tTest097.a: Simultaneous open"
	set numdb [test097_open tdb $ndbs $method $env $testfile $args]
	if { $numdb == 0 } {
		puts "\tTest097: Insufficient resources available -- skipping."
		error_check_good envclose [$env close] 0
		return
	}

	set did [open $dict]

	set pflags ""
	set gflags ""
	set txn ""
	set count 0

	# Here is the loop where we put and get each key/data pair
	if { [is_record_based $method] == 1 } {
		append gflags "-recno"
	}
	puts "\tTest097.b: put/get on $numdb databases"
	set datastr "abcdefghij"
	set pad_datastr [pad_data $method $datastr]
	while { [gets $did str] != -1 && $count < $nentries } {
		if { [is_record_based $method] == 1 } {
			set key [expr $count + 1]
		} else {
			set key $str
		}
		for { set i 1 } { $i <= $numdb } { incr i } {
			set ret [eval {$tdb($i) put} $txn $pflags \
			    {$key [chop_data $method $datastr]}]
			error_check_good put $ret 0
			set ret [eval {$tdb($i) get} $gflags {$key}]
			error_check_good get $ret [list [list $key \
			    [pad_data $method $datastr]]]
		}
		incr count
	}
	close $did

	# Now we will get each key from the DB and compare the results
	# to the original.
	puts "\tTest097.c: dump and check files"
	for { set j 1 } { $j <= $numdb } { incr j } {
		dump_file $tdb($j) $txn $t1 test097.check
		error_check_good db_close [$tdb($j) close] 0

		# Now compare the keys to see if they match the dictionary
		if { [is_record_based $method] == 1 } {
			set oid [open $t2 w]
			for {set i 1} {$i <= $nentries} {set i [incr i]} {
				puts $oid $i
			}
			close $oid
			filesort $t2 $t3
			file rename -force $t3 $t2
		} else {
			set q q
			filehead $nentries $dict $t3
			filesort $t3 $t2
		}
		filesort $t1 $t3

		error_check_good Test097:diff($t3,$t2) [filecmp $t3 $t2] 0
	}
	error_check_good envclose [$env close] 0
}

# Check function for test097; data should be fixed are identical
proc test097.check { key data } {
	global pad_datastr
	error_check_good "data mismatch for key $key" $data $pad_datastr
}

proc test097_open { tdb ndbs method env testfile largs } {
	global errorCode
	upvar $tdb db

	set j 0
	set numdb $ndbs
	if { [is_queueext $method] } {
		set numdb [expr $ndbs / 4]
	}
	set omethod [convert_method $method]
	for { set i 1 } {$i <= $numdb } { incr i } {
		set stat [catch {eval {berkdb_open -env $env \
		     -pagesize 512 -create -mode 0644} \
		     $largs {$omethod $testfile.$i}} db($i)]
		#
		# Check if we've reached our limit
		#
		if { $stat == 1 } {
			set min 20
			set em [is_substr $errorCode EMFILE]
			set en [is_substr $errorCode ENFILE]
			error_check_good open_ret [expr $em || $en] 1
			puts \
    "\tTest097.a.1 Encountered resource limits opening $i files, adjusting"
			if { [is_queueext $method] } {
				set end [expr $j / 4]
				set min 10
			} else {
				set end [expr $j - 10]
			}
			#
			# If we cannot open even $min files, then this test is
			# not very useful.  Close up shop and go back.
			#
			if { $end < $min } {
				test097_close db 1 $j
				return 0
			}
			test097_close db [expr $end + 1] $j
			return $end
		} else {
			error_check_good dbopen [is_valid_db $db($i)] TRUE
			set j $i
		}
	}
	return $j
}

proc test097_close { tdb start end } {
	upvar $tdb db

	for { set i $start } { $i <= $end } { incr i } {
		error_check_good db($i)close [$db($i) close] 0
	}
}
