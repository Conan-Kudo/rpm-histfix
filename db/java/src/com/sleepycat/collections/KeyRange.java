/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2002-2004
 *      Sleepycat Software.  All rights reserved.
 *
 * $Id: KeyRange.java,v 1.2 2004/05/05 15:43:48 mark Exp $
 */

package com.sleepycat.collections;

import java.util.Comparator;

import com.sleepycat.db.DatabaseEntry;

/**
 * Encapsulates a key range for use with a RangeCursor.
 */
class KeyRange {

    Comparator comparator;
    DatabaseEntry beginKey;
    DatabaseEntry endKey;
    boolean singleKey;
    boolean beginInclusive;
    boolean endInclusive;

    /**
     * Creates an unconstrained key range.
     */
    KeyRange(Comparator comparator) {
        this.comparator = comparator;
    }

    /**
     * Creates a range for a single key.
     */
    KeyRange subRange(DatabaseEntry key)
        throws KeyRangeException {

        if (!check(key)) {
            throw new KeyRangeException("singleKey out of range");
        }
        KeyRange range = new KeyRange(comparator);
        range.beginKey = key;
        range.endKey = key;
        range.beginInclusive = true;
        range.endInclusive = true;
        range.singleKey = true;
        return range;
    }

    /**
     * Creates a range that is the intersection of this range and the given
     * range parameters.
     */
    KeyRange subRange(DatabaseEntry beginKey, boolean beginInclusive,
                      DatabaseEntry endKey, boolean endInclusive)
        throws KeyRangeException {

        if (beginKey == null) {
            beginKey = this.beginKey;
            beginInclusive = this.beginInclusive;
        } else if (!check(beginKey, beginInclusive)) {
            throw new KeyRangeException("beginKey out of range");
        }
        if (endKey == null) {
            endKey = this.endKey;
            endInclusive = this.endInclusive;
        } else if (!check(endKey, endInclusive)) {
            throw new KeyRangeException("endKey out of range");
        }
        KeyRange range = new KeyRange(comparator);
        range.beginKey = beginKey;
        range.endKey = endKey;
        range.beginInclusive = beginInclusive;
        range.endInclusive = endInclusive;
        return range;
    }

    /**
     * Returns the key of a single-key range, or null if not a single-key
     * range.
     */
    final DatabaseEntry getSingleKey() {

        return singleKey ? beginKey : null;
    }

    /**
     * Returns whether this range has a begin or end bound.
     */
    final boolean hasBound() {

        return endKey != null || beginKey != null;
    }

    /**
     * Formats this range as a string for debugging.
     */
    public String toString() {

        return "[KeyRange " + beginKey + ' ' + beginInclusive +
                              endKey + ' ' + endInclusive +
                              (singleKey ? " single" : "");
    }

    /**
     * Returns whether a given key is within range.
     */
    boolean check(DatabaseEntry key) {

        if (singleKey) {
            return (compare(key, beginKey) == 0);
        } else {
            return checkBegin(key, true) && checkEnd(key, true);
        }
    }

    /**
     * Returns whether a given key is within range.
     */
    boolean check(DatabaseEntry key, boolean inclusive) {

        if (singleKey) {
            return (compare(key, beginKey) == 0);
        } else {
            return checkBegin(key, inclusive) && checkEnd(key, inclusive);
        }
    }

    /**
     * Returns whether the given key is within range with respect to the
     * beginning of the range.
     *
     * <p>The inclusive parameter should be true for checking a key read from
     * the database; this will require that the key is within range.  When
     * inclusive=false the key is allowed to be equal to the beginKey for the
     * range; this is used for checking a new exclusive bound of a
     * sub-range.</p>
     *
     * <p>Note that when inclusive=false and beginInclusive=true our check is
     * not exactly correct because in theory we should allow the key to be "one
     * less" than the existing bound; however, checking for "one less"  is
     * impossible so we do the best we can and test the bounds
     * conservatively.</p>
     */
    boolean checkBegin(DatabaseEntry key, boolean inclusive) {

        if (beginKey == null) {
            return true;
        } else if (!beginInclusive && inclusive) {
            return compare(key, beginKey) > 0;
        } else {
            return compare(key, beginKey) >= 0;
        }
    }

    /**
     * Returns whether the given key is within range with respect to the
     * end of the range.  See checkBegin for details.
     */
    boolean checkEnd(DatabaseEntry key, boolean inclusive) {

        if (endKey == null) {
            return true;
        } else if (!endInclusive && inclusive) {
            return compare(key, endKey) < 0;
        } else {
            return compare(key, endKey) <= 0;
        }
    }

    /**
     * Compares two keys, using the user comparator if there is one.
     */
    int compare(DatabaseEntry key1, DatabaseEntry key2) {

        if (comparator != null) {
            return comparator.compare(getByteArray(key1), getByteArray(key2));
        } else {
            return compareBytes
                    (key1.getData(), key1.getOffset(), key1.getSize(),
                     key2.getData(), key2.getOffset(), key2.getSize());

        }
    }

    /**
     * Compares two keys as unsigned byte arrays, which is the default
     * comparison used by JE/DB.
     */
    static int compareBytes(byte[] data1, int offset1, int size1,
                            byte[] data2, int offset2, int size2) {

        for (int i = 0; i < size1 && i < size2; i++) {

            int b1 = 0xFF & data1[offset1 + i];
            int b2 = 0xFF & data2[offset2 + i];
            if (b1 < b2)
                return -1;
            else if (b1 > b2)
                return 1;
        }

        if (size1 < size2)
            return -1;
        else if (size1 > size2)
            return 1;
        else
            return 0;
    }

    /**
     * Returns a copy of an entry.
     */
    static DatabaseEntry copy(DatabaseEntry from) {
        return new DatabaseEntry(getByteArray(from));
    }

    /**
     * Copies one entry to another.
     */
    static void copy(DatabaseEntry from, DatabaseEntry to) {
        to.setData(getByteArray(from));
        to.setOffset(0);
    }

    /**
     * Returns an entry's byte array, copying it if the entry offset is
     * non-zero.
     */
    static byte[] getByteArray(DatabaseEntry entry) {

        byte[] bytes = entry.getData();
        if (bytes == null) return null;
        int size = entry.getSize();
        byte[] data = new byte[size];
        System.arraycopy(bytes, entry.getOffset(), data, 0, size);
        return data;
    }

    /**
     * Returns the two DatabaseEntry objects have the same data value.
     */
    static boolean equalBytes(DatabaseEntry e1, DatabaseEntry e2) {

        if (e1 == null && e2 == null) {
            return true;
        }
        if (e1 == null || e2 == null) {
            return false;
        }

        byte[] d1 = e1.getData();
        byte[] d2 = e2.getData();
        int s1 = e1.getSize();
        int s2 = e2.getSize();
        int o1 = e1.getOffset();
        int o2 = e2.getOffset();

        if (d1 == null && d2 == null) {
            return true;
        }
        if (d1 == null || d2 == null) {
            return false;
        }
        if (s1 != s2) {
            return false;
        }
        for (int i = 0; i < s1; i += 1) {
            if (d1[o1 + i] != d2[o2 + i]) {
                return false;
            }
        }
        return true;
    }

    /**
     * Converts the byte array of this thang to space-separated integers,
     * and suffixed by the record number if applicable.
     *
     * @param dbt the thang to convert.
     *
     * @param the resulting string.
     */
    static String toString(DatabaseEntry dbt) {

        int len = dbt.getOffset() + dbt.getSize();
        StringBuffer buf = new StringBuffer(len * 2);
        byte[] data = dbt.getData();
        for (int i = dbt.getOffset(); i < len; i++) {
            String num = Integer.toHexString(data[i]);
            if (num.length() < 2) buf.append('0');
            buf.append(num);
        }
        return buf.toString();
    }
}
