/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2001
 * 	Sleepycat Software.  All rights reserved.
 *
 * Id: DbMultipleDataIterator.java,v 1.2 2001/05/01 01:44:52 krinsky Exp 
 */

package com.sleepycat.db;

/**
 *
 * @author David M. Krinsky
 */
public class DbMultipleDataIterator extends DbMultipleIterator
{
    // public methods
    public DbMultipleDataIterator(Dbt data)
    {
        super(data);
    }

    public boolean next(Dbt data)
    {
        int dataoff = DbUtil.array2int(buf, pos);
        
        // crack out the data offset and length.
        if (dataoff < 0) {
            return (false);
        }

        pos -= int32sz;
        int datasz = DbUtil.array2int(buf, pos);

        pos -= int32sz;

        data.set_data(buf);
        data.set_size(datasz);
        data.set_offset(dataoff);

        return (true);
    }
}


// end of DbMultipleDataIterator.java
