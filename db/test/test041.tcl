# See the file LICENSE for redistribution information.
#
# Copyright (c) 1996-2003
#	Sleepycat Software.  All rights reserved.
#
# $Id: test041.tcl,v 11.8 2003/01/08 05:53:54 bostic Exp $
#
# TEST	test041
# TEST	Test039 with off-page duplicates
# TEST	DB_GET_BOTH functionality with off-page duplicates.
proc test041 { method {nentries 10000} args} {
	# Test with off-page duplicates
	eval {test039 $method $nentries 20 "041" -pagesize 512} $args

	# Test with multiple pages of off-page duplicates
	eval {test039 $method [expr $nentries / 10] 100 "041" -pagesize 512} \
	    $args
}
