/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2002-2004
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: MarshalledKey.java,v 1.3 2004/09/22 16:17:13 mark Exp $
 */

package com.sleepycat.examples.collections.ship.marshal;

import com.sleepycat.bind.tuple.TupleInput;
import com.sleepycat.bind.tuple.TupleOutput;

/**
 * MarshalledKey is implemented by key objects and called by {@link
 * SampleViews.MarshalledKeyBinding}.  In this sample, MarshalledKey is
 * implemented by {@link PartKey}, {@link SupplierKey}, and {@link
 * ShipmentKey}.  This interface is package-protected rather than public to
 * hide the marshalling interface from other users of the data objects.  Note
 * that a MarshalledKey must also have a no arguments constructor so
 * that it can be instantiated by the binding.
 *
 * @author Mark Hayes
 */
interface MarshalledKey {

    /**
     * Construct the key tuple entry from the key object.
     */
    void marshalKey(TupleOutput keyOutput);

    /**
     * Construct the key object from the key tuple entry.
     */
    void unmarshalKey(TupleInput keyInput);
}
