/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2002-2004
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: Weight.java,v 1.2 2004/09/22 16:17:13 mark Exp $
 */

package com.sleepycat.examples.collections.ship.marshal;

import java.io.Serializable;

/**
 * Weight represents a weight amount and unit of measure.
 *
 * <p> In this sample, Weight is embedded in part data values which are stored
 * as Java serialized objects; therefore Weight must be Serializable. </p>
 *
 * @author Mark Hayes
 */
public class Weight implements Serializable {

    public final static String GRAMS = "grams";
    public final static String OUNCES = "ounces";

    private double amount;
    private String units;

    public Weight(double amount, String units) {

        this.amount = amount;
        this.units = units;
    }

    public final double getAmount() {

        return amount;
    }

    public final String getUnits() {

        return units;
    }

    public String toString() {

        return "[" + amount + ' ' + units + ']';
    }
}
