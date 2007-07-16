/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2002-2006
 *	Oracle Corporation.  All rights reserved.
 *
 * $Id: Shipment.java,v 12.4 2006/08/24 14:45:56 bostic Exp $
 */

package collections.ship.factory;

import java.io.Serializable;

import com.sleepycat.bind.tuple.MarshalledTupleKeyEntity;
import com.sleepycat.bind.tuple.TupleInput;
import com.sleepycat.bind.tuple.TupleOutput;

/**
 * A Shipment represents the combined key/data pair for a shipment entity.
 *
 * <p> In this sample, Shipment is bound to the stored key/data entry by
 * implementing the MarshalledTupleKeyEntity interface. </p>
 *
 * <p> The binding is "tricky" in that it uses this class for both the stored
 * data entry and the combined entity object.  To do this, the key field(s)
 * are transient and are set by the binding after the data object has been
 * deserialized. This avoids the use of a ShipmentData class completely. </p>
 *
 * <p> Since this class is used directly for data storage, it must be
 * Serializable. </p>
 *
 * @author Mark Hayes
 */
public class Shipment implements Serializable, MarshalledTupleKeyEntity {

    static final String PART_KEY = "part";
    static final String SUPPLIER_KEY = "supplier";

    private transient String partNumber;
    private transient String supplierNumber;
    private int quantity;

    public Shipment(String partNumber, String supplierNumber, int quantity) {

        this.partNumber = partNumber;
        this.supplierNumber = supplierNumber;
        this.quantity = quantity;
    }

    public final String getPartNumber() {

        return partNumber;
    }

    public final String getSupplierNumber() {

        return supplierNumber;
    }

    public final int getQuantity() {

        return quantity;
    }

    public String toString() {

        return "[Shipment: part=" + partNumber +
	    " supplier=" + supplierNumber +
	    " quantity=" + quantity + ']';
    }

    // --- MarshalledTupleKeyEntity implementation ---

    public void marshalPrimaryKey(TupleOutput keyOutput) {

        keyOutput.writeString(this.partNumber);
        keyOutput.writeString(this.supplierNumber);
    }

    public void unmarshalPrimaryKey(TupleInput keyInput) {

        this.partNumber = keyInput.readString();
        this.supplierNumber = keyInput.readString();
    }

    public boolean marshalSecondaryKey(String keyName, TupleOutput keyOutput) {

        if (keyName.equals(PART_KEY)) {
            keyOutput.writeString(this.partNumber);
            return true;
        } else if (keyName.equals(SUPPLIER_KEY)) {
            keyOutput.writeString(this.supplierNumber);
            return true;
        } else {
            throw new UnsupportedOperationException(keyName);
        }
    }

    public boolean nullifyForeignKey(String keyName) {

        throw new UnsupportedOperationException(keyName);
    }
}
