/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000-2003
 *      Sleepycat Software.  All rights reserved.
 *
 * $Id: StoredMap.java,v 1.1 2003/12/15 21:44:12 jbj Exp $
 */

package com.sleepycat.bdb.collection;

import com.sleepycat.bdb.bind.DataBinding;
import com.sleepycat.bdb.bind.EntityBinding;
import com.sleepycat.bdb.DataCursor;
import com.sleepycat.bdb.DataIndex;
import com.sleepycat.bdb.DataStore;
import com.sleepycat.bdb.DataView;
import com.sleepycat.bdb.KeyRangeException;
import com.sleepycat.db.Db;
import com.sleepycat.db.DbException;
import java.io.IOException;
import java.util.Collection;
import java.util.Collections;
import java.util.Iterator;
import java.util.Map;
import java.util.Set;
import java.util.SortedMap;
import java.util.SortedSet;

/**
 * A Map view of a {@link DataStore} or {@link DataIndex}.
 *
 * <p>In addition to the standard Map methods, this class provides the
 * following methods for stored maps only.  Note that the use of these methods
 * is not compatible with the standard Java collections interface.</p>
 * <ul>
 * <li>{@link #duplicates(Object)}</li>
 * <li>{@link #append(Object)}</li>
 * </ul>
 *
 * @author Mark Hayes
 */
public class StoredMap extends StoredContainer implements Map {

    private StoredKeySet keySet;
    private StoredEntrySet entrySet;
    private StoredValueSet valueSet;

    /**
     * Creates a map view of a {@link DataStore}.
     *
     * @param store is the DataStore underlying the new collection.
     *
     * @param keyBinding is the binding used to translate between key buffers
     * and key objects.
     *
     * @param valueBinding is the binding used to translate between value
     * buffers and value objects.
     *
     * @param writeAllowed is true to create a read-write collection or false
     * to create a read-only collection.
     *
     * @throws IllegalArgumentException if formats are not consistently
     * defined or a parameter is invalid.
     *
     * @throws RuntimeExceptionWrapper if a {@link DbException} is thrown.
     */
    public StoredMap(DataStore store, DataBinding keyBinding,
                     DataBinding valueBinding, boolean writeAllowed) {

        super(new DataView(store, null, keyBinding, valueBinding,
                           null, writeAllowed));
    }

    protected Object clone()
        throws CloneNotSupportedException {

        // cached collections must be cleared and recreated with the new view
        // of the map to inherit the new view's properties
        StoredMap other = (StoredMap) super.clone();
        other.keySet = null;
        other.entrySet = null;
        other.valueSet = null;
        return other;
    }

    /**
     * Creates a map entity view of a {@link DataStore}.
     *
     * @param store is the DataStore underlying the new collection.
     *
     * @param keyBinding is the binding used to translate between key buffers
     * and key objects.
     *
     * @param valueEntityBinding is the binding used to translate between
     * key/value buffers and entity value objects.
     *
     * @param writeAllowed is true to create a read-write collection or false
     * to create a read-only collection.
     *
     * @throws IllegalArgumentException if formats are not consistently
     * defined or a parameter is invalid.
     *
     * @throws RuntimeExceptionWrapper if a {@link DbException} is thrown.
     */
    public StoredMap(DataStore store, DataBinding keyBinding,
                     EntityBinding valueEntityBinding, boolean writeAllowed) {

        super(new DataView(store, null, keyBinding, null,
                           valueEntityBinding, writeAllowed));
    }

    /**
     * Creates a map view of a {@link DataIndex}.
     *
     * @param index is the DataIndex underlying the new collection.
     *
     * @param keyBinding is the binding used to translate between key buffers
     * and key objects.
     *
     * @param valueBinding is the binding used to translate between value
     * buffers and value objects.
     *
     * @param writeAllowed is true to create a read-write collection or false
     * to create a read-only collection.
     *
     * @throws IllegalArgumentException if formats are not consistently
     * defined or a parameter is invalid.
     *
     * @throws RuntimeExceptionWrapper if a {@link DbException} is thrown.
     */
    public StoredMap(DataIndex index, DataBinding keyBinding,
                     DataBinding valueBinding, boolean writeAllowed) {

        super(new DataView(null, index, keyBinding, valueBinding,
                           null, writeAllowed));
    }

    /**
     * Creates a map entity view of a {@link DataIndex}.
     *
     * @param index is the DataIndex underlying the new collection.
     *
     * @param keyBinding is the binding used to translate between key buffers
     * and key objects.
     *
     * @param valueEntityBinding is the binding used to translate between
     * key/value buffers and entity value objects.
     *
     * @param writeAllowed is true to create a read-write collection or false
     * to create a read-only collection.
     *
     * @throws IllegalArgumentException if formats are not consistently
     * defined or a parameter is invalid.
     *
     * @throws RuntimeExceptionWrapper if a {@link DbException} is thrown.
     */
    public StoredMap(DataIndex index, DataBinding keyBinding,
                     EntityBinding valueEntityBinding, boolean writeAllowed) {

        super(new DataView(null, index, keyBinding, null,
                           valueEntityBinding, writeAllowed));
    }

    StoredMap(DataView view) {

        super(view);
    }

    /**
     * Returns the value to which this map maps the specified key.
     * If duplicates are allowed, this method returns the first duplicate, in
     * the order in which duplicates are configured, that maps to the specified
     * key.
     * This method conforms to the {@link Map#get} interface.
     *
     * @throws RuntimeExceptionWrapper if a {@link DbException} is thrown.
     */
    public Object get(Object key) {

        return super.get(key);
    }

    /**
     * Associates the specified value with the specified key in this map
     * (optional operation).
     * If duplicates are allowed and the specified key is already mapped to a
     * value, this method appends the new duplicate after the existing
     * duplicates.
     * This method conforms to the {@link Map#put} interface.
     *
     * <p>The key parameter may be null if an entity binding is used and the
     * key will be derived from the value (entity) parameter.  If an entity
     * binding is used and the key parameter is non-null, then the key
     * parameter must be equal to the key derived from the value parameter.</p>
     *
     * @return the previous value associated with specified key, or null if
     * there was no mapping for the key or if duplicates are allowed.
     *
     * @throws UnsupportedOperationException if the collection is indexed, or
     * if the collection is read-only.
     *
     * @throws IllegalArgumentException if an entity value binding is used and
     * the primary key of the value given is different than the existing stored
     * primary key.
     *
     * @throws RuntimeExceptionWrapper if a {@link DbException} is thrown.
     */
    public Object put(Object key, Object value) {

        return super.put(key, value);
    }

    /**
     * Appends a given value returning the newly assigned key.
     * If a {@link com.sleepycat.bdb.PrimaryKeyAssigner} is associated with
     * Store for this map, it will be used to assigned the returned key.
     * Otherwise the Store must be a QUEUE or RECNO database and the next
     * available record number is assigned as the key.
     * This method does not exist in the standard {@link Map} interface.
     *
     * @param value the value to be appended.
     *
     * @return the assigned key.
     *
     * @throws UnsupportedOperationException if the collection is indexed, or
     * if the collection is read-only, or if the Store has no {@link
     * com.sleepycat.bdb.PrimaryKeyAssigner} and is not a QUEUE or RECNO
     * database.
     *
     * @throws RuntimeExceptionWrapper if a {@link DbException} is thrown.
     */
    public Object append(Object value) {

        boolean doAutoCommit = beginAutoCommit();
        try {
            Object[] key = new Object[1];
            view.append(value, key, null);
            commitAutoCommit(doAutoCommit);
            return key[0];
        } catch (Exception e) {
            throw handleException(e, doAutoCommit);
        }
    }

    /**
     * Removes the mapping for this key from this map if present (optional
     * operation).
     * If duplicates are allowed, this method removes all duplicates for the
     * given key.
     * This method conforms to the {@link Map#remove} interface.
     *
     * @throws UnsupportedOperationException if the collection is read-only.
     *
     * @throws RuntimeExceptionWrapper if a {@link DbException} is thrown.
     */
    public Object remove(Object key) {

        Object[] oldVal = new Object[1];
        removeKey(key, oldVal);
        return oldVal[0];
    }

    /**
     * Returns true if this map contains the specified key.
     * This method conforms to the {@link Map#containsKey} interface.
     *
     * @throws RuntimeExceptionWrapper if a {@link DbException} is thrown.
     */
    public boolean containsKey(Object key) {

        return super.containsKey(key);
    }

    /**
     * Returns true if this map contains the specified value.
     * When an entity binding is used, this method returns whether the map
     * contains the primary key and value mapping of the entity.
     * This method conforms to the {@link Map#containsValue} interface.
     *
     * @throws RuntimeExceptionWrapper if a {@link DbException} is thrown.
     */
    public boolean containsValue(Object value) {

        return super.containsValue(value);
    }

    /**
     * Copies all of the mappings from the specified map to this map (optional
     * operation).
     * When duplicates are allowed, the mappings in the specified map are
     * effectively appended to the existing mappings in this map, that is no
     * previously existing mappings in this map are replaced.
     * This method conforms to the {@link Map#putAll} interface.
     *
     * @throws UnsupportedOperationException if the collection is read-only, or
     * if the collection is indexed.
     *
     * @throws RuntimeExceptionWrapper if a {@link DbException} is thrown.
     */
    public void putAll(Map map) {

        boolean doAutoCommit = beginAutoCommit();
        Iterator entries = null;
        try {
            entries = map.entrySet().iterator();
            while (entries.hasNext()) {
                Map.Entry entry = (Map.Entry) entries.next();
                put(entry.getKey(), entry.getValue());
            }
            StoredIterator.close(entries);
            commitAutoCommit(doAutoCommit);
        } catch (Exception e) {
            StoredIterator.close(entries);
            throw handleException(e, doAutoCommit);
        }
    }

    /**
     * Returns a set view of the keys contained in this map.
     * A {@link SortedSet} is returned if the map is ordered.
     * The returned collection will be read-only if the map is read-only.
     * This method conforms to the {@link Map#keySet()} interface.
     *
     * @return a {@link StoredKeySet} or a {@link StoredSortedKeySet} for this
     * map.
     *
     * @throws RuntimeExceptionWrapper if a {@link DbException} is thrown.
     *
     * @see #isOrdered
     * @see #isWriteAllowed
     */
    public Set keySet() {

        if (keySet == null) {
            synchronized (this) {
                if (keySet == null) {
                    DataView newView = view.keySetView();
                    if (isOrdered()) {
                        keySet = new StoredSortedKeySet(newView);
                    } else {
                        keySet = new StoredKeySet(newView);
                    }
                }
            }
        }
        return keySet;
    }

    /**
     * Returns a set view of the mappings contained in this map.
     * A {@link SortedSet} is returned if the map is ordered.
     * The returned collection will be read-only if the map is read-only.
     * This method conforms to the {@link Map#entrySet()} interface.
     *
     * @return a {@link StoredEntrySet} or a {@link StoredSortedEntrySet} for
     * this map.
     *
     * @throws RuntimeExceptionWrapper if a {@link DbException} is thrown.
     *
     * @see #isOrdered
     * @see #isWriteAllowed
     */
    public Set entrySet() {

        if (entrySet == null) {
            synchronized (this) {
                if (entrySet == null) {
                    if (isOrdered()) {
                        entrySet = new StoredSortedEntrySet(view);
                    } else {
                        entrySet = new StoredEntrySet(view);
                    }
                }
            }
        }
        return entrySet;
    }

    /**
     * Returns a collection view of the values contained in this map.
     * A {@link SortedSet} is returned if the map is ordered and the
     * value/entity binding can be used to derive the map's key from its
     * value/entity object.
     * The returned collection will be read-only if the map is read-only.
     * This method conforms to the {@link Map#entrySet()} interface.
     *
     * @return a {@link StoredEntrySet} or a {@link StoredSortedEntrySet} for
     * this map.
     *
     * @throws RuntimeExceptionWrapper if a {@link DbException} is thrown.
     *
     * @see #isOrdered
     * @see #isWriteAllowed
     */
    public Collection values() {

        if (valueSet == null) {
            synchronized (this) {
                if (valueSet == null) {
                    DataView newView = view.valueSetView();
                    if (isOrdered() && newView.canDeriveKeyFromValue()) {
                        valueSet = new StoredSortedValueSet(newView);
                    } else {
                        valueSet = new StoredValueSet(newView);
                    }
                }
            }
        }
        return valueSet;
    }

    /**
     * Returns a new collection containing the values mapped to the given
     * key in this map.  This collection's iterator() method is particularly
     * useful for iterating over the duplicates for a given key, since this
     * is not supported by the standard Map interface.
     * This method does not exist in the standard {@link Map} interface.
     *
     * <p>If no mapping for the given key is present, an empty collection is
     * returned.  If duplicates are not allowed, at most a single value will be
     * in the collection returned.  If duplicates are allowed, the returned
     * collection's add() method may be used to add values for the given
     * key.</p>
     *
     * @param key is the key for which values are to be returned.
     *
     * @throws RuntimeExceptionWrapper if a {@link DbException} is thrown.
     */
    public Collection duplicates(Object key) {

        try {
            DataView newView = view.valueSetView(key);
            return new StoredValueSet(newView, true);
        } catch (KeyRangeException e) {
            return Collections.EMPTY_SET;
        } catch (Exception e) {
            throw StoredContainer.convertException(e);
        }
    }

    /**
     * Compares the specified object with this map for equality.
     * A value comparison is performed by this method and the stored values
     * are compared rather than calling the equals() method of each element.
     * This method conforms to the {@link Map#equals} interface.
     *
     * @throws RuntimeExceptionWrapper if a {@link DbException} is thrown.
     */
    public boolean equals(Object other) {

        if (other instanceof Map) {
            return entrySet().equals(((Map) other).entrySet());
        } else {
            return false;
        }
    }

    /**
     * Converts the map to a string representation for debugging.
     * WARNING: All mappings will be converted to strings and returned and
     * therefore the returned string may be very large.
     *
     * @return the string representation.
     *
     * @throws RuntimeExceptionWrapper if a {@link DbException} is thrown.
     */
    public String toString() {

        return entrySet().toString();
    }
}

