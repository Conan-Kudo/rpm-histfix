/*-
* See the file LICENSE for redistribution information.
*
* Copyright (c) 2002-2004
*	Sleepycat Software.  All rights reserved.
*
* $Id: JoinCursor.java,v 1.2 2004/04/09 15:08:38 mjc Exp $
*/

package com.sleepycat.db;

import com.sleepycat.db.internal.DbConstants;
import com.sleepycat.db.internal.Dbc;

public class JoinCursor {
    private Database database;
    private Dbc dbc;
    private JoinConfig config;

    JoinCursor(final Database database,
               final Dbc dbc,
               final JoinConfig config) {
        this.database = database;
        this.dbc = dbc;
        this.config = config;
    }

    public void close()
        throws DatabaseException {

        dbc.close();
    }

    public Database getDatabase() {
        return database;
    }

    public JoinConfig getConfig() {
        return config;
    }

    public OperationStatus getNext(final DatabaseEntry key, LockMode lockMode)
        throws DatabaseException {

        return OperationStatus.fromInt(
            dbc.get(key, DatabaseEntry.IGNORE,
                DbConstants.DB_JOIN_ITEM |
                LockMode.getFlag(lockMode)));
    }

    public OperationStatus getNext(final DatabaseEntry key,
                                   final DatabaseEntry data,
                                   LockMode lockMode)
        throws DatabaseException {

        return OperationStatus.fromInt(
            dbc.get(key, data, LockMode.getFlag(lockMode) |
                ((data == null) ? 0 : data.getMultiFlag())));
    }
}
