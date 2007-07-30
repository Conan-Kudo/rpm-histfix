# See the file LICENSE for redistribution information.
#
# Copyright (c) 2000,2007 Oracle.  All rights reserved.
#
# $Id: test091.tcl,v 12.5 2007/05/17 15:15:56 bostic Exp $
#
# TEST	test091
# TEST	Test of DB_CONSUME_WAIT.
proc test091 { method {nconsumers 4} \
    {nproducers 2} {nitems 1000} {start 0 } {tnum "091"} args} {
	if { [is_queue $method ] == 0 } {
		puts "Skipping test0$tnum for $method."
		return;
	}
	eval {test070 $method \
	    $nconsumers $nproducers $nitems WAIT $start -txn $tnum } $args
	eval {test070 $method \
	    $nconsumers $nproducers $nitems WAIT $start -cdb $tnum } $args
}
