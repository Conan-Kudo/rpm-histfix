/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000-2004
 *      Sleepycat Software.  All rights reserved.
 *
 * $Id: BooleanBinding.java,v 1.5 2004/08/13 15:16:44 mjc Exp $
 */

package com.sleepycat.bind.tuple;

import com.sleepycat.db.DatabaseEntry;

/**
 * A concrete <code>TupleBinding</code> for a <code>Boolean</code> primitive
 * wrapper or a <code>boolean</code> primitive.
 *
 * <p>There are two ways to use this class:</p>
 * <ol>
 * <li>When using the {@link com.sleepycat.db} package directly, the static
 * methods in this class can be used to convert between primitive values and
 * {@link DatabaseEntry} objects.</li>
 * <li>When using the {@link com.sleepycat.collections} package, an instance of
 * this class can be used with any stored collection.  The easiest way to
 * obtain a binding instance is with the {@link
 * TupleBinding#getPrimitiveBinding} method.</li>
 * </ol>
 */
public class BooleanBinding extends TupleBinding {

    private static final int BOOLEAN_SIZE = 1;

    // javadoc is inherited
    public Object entryToObject(TupleInput input) {

	return input.readBoolean() ? Boolean.TRUE : Boolean.FALSE;
    }

    // javadoc is inherited
    public void objectToEntry(Object object, TupleOutput output) {

        /* Do nothing.  Not called by objectToEntry(Object,DatabaseEntry). */
    }

    // javadoc is inherited
    public void objectToEntry(Object object, DatabaseEntry entry) {

        booleanToEntry(((Boolean) object).booleanValue(), entry);
    }

    /**
     * Converts an entry buffer into a simple <code>boolean</code> value.
     *
     * @param entry is the source entry buffer.
     *
     * @return the resulting value.
     */
    public static boolean entryToBoolean(DatabaseEntry entry) {

        return entryToInput(entry).readBoolean();
    }

    /**
     * Converts a simple <code>boolean</code> value into an entry buffer.
     *
     * @param val is the source value.
     *
     * @param entry is the destination entry buffer.
     */
    public static void booleanToEntry(boolean val, DatabaseEntry entry) {

        outputToEntry(newOutput(new byte[BOOLEAN_SIZE]).writeBoolean(val),
		      entry);
    }
}
