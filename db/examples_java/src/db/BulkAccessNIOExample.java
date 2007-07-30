/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997,2007 Oracle.  All rights reserved.
 *
 * $Id: BulkAccessNIOExample.java,v 1.6 2007/05/17 15:15:36 bostic Exp $
 */

package db;

import com.sleepycat.db.*;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.InputStreamReader;
import java.io.IOException;
import java.io.PrintStream;
import java.nio.ByteBuffer;

class BulkAccessNIOExample {
    private static final String FileName = "access.db";

    public BulkAccessNIOExample() {
    }

    public static void main(String[] argv) {
        try {
            BulkAccessNIOExample app = new BulkAccessNIOExample();
            app.run();
        } catch (DatabaseException dbe) {
            System.err.println("BulkAccessNIOExample: " + dbe.toString());
            System.exit(1);
        } catch (FileNotFoundException fnfe) {
            System.err.println("BulkAccessNIOExample: " + fnfe.toString());
            System.exit(1);
        }
        System.exit(0);
    }

    // Prompts for a line, and keeps prompting until a non blank
    // line is returned.  Returns null on error.
    //
    public static String askForLine(InputStreamReader reader,
                                    PrintStream out, String prompt) {
        String result = "";
        while (result != null && result.length() == 0) {
            out.print(prompt);
            out.flush();
            result = getLine(reader);
        }
        return result;
    }

    // Not terribly efficient, but does the job.
    // Works for reading a line from stdin or a file.
    // Returns null on EOF.  If EOF appears in the middle
    // of a line, returns that line, then null on next call.
    //
    public static String getLine(InputStreamReader reader) {
        StringBuffer b = new StringBuffer();
        int c;
        try {
            while ((c = reader.read()) != -1 && c != '\n') {
                if (c != '\r')
                    b.append((char)c);
            }
        } catch (IOException ioe) {
            c = -1;
        }

        if (c == -1 && b.length() == 0)
            return null;
        else
            return b.toString();
    }

    public void run() throws DatabaseException, FileNotFoundException {
        // Remove the previous database.
        new File(FileName).delete();

        // Create the database object.
        // There is no environment for this simple example.
        DatabaseConfig config = new DatabaseConfig();
        config.setErrorStream(System.err);
        config.setErrorPrefix("BulkAccessNIOExample");
        config.setType(DatabaseType.BTREE);
        config.setAllowCreate(true);
        config.setMode(0644);
        Database table = new Database(FileName, null, config);

        //
        // Insert records into the database, where the key is the user
        // input and the data is the user input in reverse order.
        //
        InputStreamReader reader = new InputStreamReader(System.in);

        for (;;) {
            String line = askForLine(reader, System.out, "input> ");
            if (line == null || (line.compareToIgnoreCase("end") == 0))
                break;

            String reversed = (new StringBuffer(line)).reverse().toString();

            // See definition of StringEntry below
            //
            StringEntry key = new StringEntry(line, true);
            StringEntry data = new StringEntry(reversed, true);

            try {
                if (table.putNoOverwrite(null, key, data) == OperationStatus.KEYEXIST)
                    System.out.println("Key " + line + " already exists.");
            } catch (DatabaseException dbe) {
                System.out.println(dbe.toString());
            }
        }

        // Acquire a cursor for the table.
        Cursor cursor = table.openCursor(null, null);
        DatabaseEntry foo = new DatabaseEntry();

        MultipleKeyNIODataEntry bulk_data = new MultipleKeyNIODataEntry();

        ByteBuffer rawData = ByteBuffer.allocateDirect(1024*1024);
        bulk_data.setDataNIO(rawData);
        bulk_data.setUserBuffer(1024 * 1024, true);

        // Walk through the table, printing the key/data pairs.
        //
        while (cursor.getNext(foo, bulk_data, null) == OperationStatus.SUCCESS) {
            StringEntry key, data;
            key = new StringEntry();
            data = new StringEntry();

            while (bulk_data.next(key, data))
                System.out.println(key.getString() + " : " + data.getString());
        }
        cursor.close();
        table.close();
    }

    // Here's an example of how you can extend DatabaseEntry in a
    // straightforward way to allow easy storage/retrieval of strings, or
    // whatever kind of data you wish.  We've declared it as a static inner
    // class, but it need not be.
    //
    static class StringEntry extends DatabaseEntry {
        StringEntry() {
        }

        StringEntry(String value, boolean nioData) {
            setString(value, nioData);
        }

        void setString(String value, boolean nioData) {
            byte[] data = value.getBytes();
            if(nioData) {
                ByteBuffer newBuf = ByteBuffer.allocateDirect(data.length);
                newBuf.position(0);
                newBuf.put(data, 0, data.length);
                setDataNIO(newBuf);
                setSize(data.length);
            } else {
                setData(data);
                setSize(data.length);
            }
        }

        String getString() {
            String ret;
            if(getData() == null) {
                ByteBuffer tmp = getDataNIO();
                tmp.position(getOffset());
                byte[] data = new byte[getSize()];
                tmp.get(data, 0, getSize());
                ret = new String(data, 0, getSize());
            } else {
                ret = new String(getData(), getOffset(), getSize());
            }
            return ret;
        }
    }
}
