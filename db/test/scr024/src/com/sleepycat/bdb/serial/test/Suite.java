/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000-2003
 *      Sleepycat Software.  All rights reserved.
 *
 * $Id: Suite.java,v 1.1 2003/12/15 21:44:54 jbj Exp $
 */
package com.sleepycat.bdb.serial.test;

import junit.framework.Test;
import junit.framework.TestSuite;

/**
 * @author Mark Hayes
 */
public class Suite {

    public static void main(String[] args)
        throws Exception {

        junit.textui.TestRunner.run(suite());
    }

    public static Test suite()
        throws Exception {

        TestSuite suite = new TestSuite();
        suite.addTest(StoredClassCatalogTest.suite());
        suite.addTest(TupleSerialDbFactoryTest.suite());
        return suite;
    }
}
