/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2002-2003
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: Sample.java,v 1.1 2003/12/15 21:44:10 jbj Exp $
 */

package com.sleepycat.examples.bdb.shipment.sentity;

import com.sleepycat.bdb.TransactionRunner;
import com.sleepycat.bdb.TransactionWorker;
import com.sleepycat.bdb.collection.StoredIterator;
import com.sleepycat.db.DbException;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.util.Collection;
import java.util.Iterator;
import java.util.Set;

/**
 * Sample is the main entry point for the sample program and may be run as
 * follows:
 *
 * <pre>
 * java com.sleepycat.examples.bdb.shipment.sentity.Sample
 *      [-h <home-directory> ] [-multiprocess]
 * </pre>
 *
 * <p> The default for the home directory is ./tmp -- the tmp subdirectory of
 * the current directory where the sample is run. To specify a different home
 * directory, use the -home option. The home directory must exist before
 * running the sample.  To recreate the sample database from scratch, delete
 * all files in the home directory before running the sample. </p>
 *
 * <p> The default processing model is single-process, in which Berkeley DB
 * recovery is always run when the environment is opened.  If a multi-process
 * model is desired, use the -multiprocess option and implement a monitor
 * process which handles recovery.  If -multiprocess is specified, Berkeley DB
 * recovery will not be run automatically when opening the environment. </p>
 *
 * @author Mark Hayes
 */
public class Sample {

    private SampleDatabase db;
    private SampleViews views;

    /**
     * Run the sample program.
     */
    public static void main(String[] args) {

        System.out.println("\nRunning sample: " + Sample.class);

        // Parse the command line arguments.
        //
        boolean runRecovery = true;
        String homeDir = "./tmp";
        for (int i = 0; i < args.length; i += 1) {
            String arg = args[i];
            if (args[i].equals("-h") && i < args.length - 1) {
                i += 1;
                homeDir = args[i];
            } else if (args[i].equals("-multiprocess")) {
                runRecovery = false;
            } else {
                System.err.println("Usage:\n java " + Sample.class.getName() +
                                  "\n  [-h <home-directory>] [-multiprocess]");
                System.exit(2);
            }
        }

        // Run the sample.
        //
        Sample sample = null;
        try {
            sample = new Sample(homeDir, runRecovery);
            sample.run();
        } catch (Exception e) {
            // If an exception reaches this point, the last transaction did not
            // complete.  If the exception is DbRunRecoveryException, follow
            // the Berkeley DB recovery procedures before running again.
            e.printStackTrace();
        } finally {
            if (sample != null) {
                try {
                    // Always attempt to close the database cleanly.
                    sample.close();
                } catch (Exception e) {
                    System.err.println("Exception during database close:");
                    e.printStackTrace();
                }
            }
        }
    }

    /**
     * Open the database and views.
     */
    private Sample(String homeDir, boolean runRecovery)
        throws DbException, FileNotFoundException {

        db = new SampleDatabase(homeDir, runRecovery);
        views = new SampleViews(db);
    }

    /**
     * Close the database cleanly.
     */
    private void close()
        throws DbException, IOException {

        db.close();
    }

    /**
     * Run two transactions to populate and print the database.  A
     * TransactionRunner is used to ensure consistent handling of transactions,
     * including deadlock retries.  But the best transaction handling mechanism
     * to use depends on the application.
     */
    private void run()
        throws Exception {

        TransactionRunner runner = new TransactionRunner(db.getEnvironment());
        runner.run(new PopulateDatabase());
        runner.run(new PrintDatabase());
    }

    /**
     * Populate the database in a single transaction.
     */
    private class PopulateDatabase implements TransactionWorker {

        public void doWork()
            throws Exception {
            addSuppliers();
            addParts();
            addShipments();
        }
    }

    /**
     * Print the database in a single transaction.  All entities are printed
     * and the indices are used to print the entities for certain keys.
     *
     * <p> Note the use of special iterator() methods.  These are used here
     * with indices to find the shipments for certain keys.  For details on
     * database iterators see {@link StoredIterator}. </p>
     */
    private class PrintDatabase implements TransactionWorker {

        public void doWork()
            throws Exception {
            printValues("Parts",
                         views.getPartSet().iterator());
            printValues("Suppliers",
                         views.getSupplierSet().iterator());
            printValues("Suppliers for City Paris",
                         views.getSupplierByCityMap().duplicates(
                                            "Paris").iterator());
            printValues("Shipments",
                         views.getShipmentSet().iterator());
            printValues("Shipments for Part P1",
                         views.getShipmentByPartMap().duplicates(
                                            new PartKey("P1")).iterator());
            printValues("Shipments for Supplier S1",
                         views.getShipmentBySupplierMap().duplicates(
                                            new SupplierKey("S1")).iterator());
        }
    }

    /**
     * Populate the part entities in the database.  If the part set is not
     * empty, assume that this has already been done.
     */
    private void addParts() {

        Set parts = views.getPartSet();
        if (parts.isEmpty()) {
            System.out.println("Adding Parts");
            parts.add(new Part("P1", "Nut", "Red",
                      new Weight(12.0, Weight.GRAMS), "London"));
            parts.add(new Part("P2", "Bolt", "Green",
                      new Weight(17.0, Weight.GRAMS), "Paris"));
            parts.add(new Part("P3", "Screw", "Blue",
                      new Weight(17.0, Weight.GRAMS), "Rome"));
            parts.add(new Part("P4", "Screw", "Red",
                      new Weight(14.0, Weight.GRAMS), "London"));
            parts.add(new Part("P5", "Cam", "Blue",
                      new Weight(12.0, Weight.GRAMS), "Paris"));
            parts.add(new Part("P6", "Cog", "Red",
                      new Weight(19.0, Weight.GRAMS), "London"));
        }
    }

    /**
     * Populate the supplier entities in the database.  If the supplier set is
     * not empty, assume that this has already been done.
     */
    private void addSuppliers() {

        Set suppliers = views.getSupplierSet();
        if (suppliers.isEmpty()) {
            System.out.println("Adding Suppliers");
            suppliers.add(new Supplier("S1", "Smith", 20, "London"));
            suppliers.add(new Supplier("S2", "Jones", 10, "Paris"));
            suppliers.add(new Supplier("S3", "Blake", 30, "Paris"));
            suppliers.add(new Supplier("S4", "Clark", 20, "London"));
            suppliers.add(new Supplier("S5", "Adams", 30, "Athens"));
        }
    }

    /**
     * Populate the shipment entities in the database.  If the shipment set
     * is not empty, assume that this has already been done.
     */
    private void addShipments() {

        Set shipments = views.getShipmentSet();
        if (shipments.isEmpty()) {
            System.out.println("Adding Shipments");
            shipments.add(new Shipment("P1", "S1", 300));
            shipments.add(new Shipment("P2", "S1", 200));
            shipments.add(new Shipment("P3", "S1", 400));
            shipments.add(new Shipment("P4", "S1", 200));
            shipments.add(new Shipment("P5", "S1", 100));
            shipments.add(new Shipment("P6", "S1", 100));
            shipments.add(new Shipment("P1", "S2", 300));
            shipments.add(new Shipment("P2", "S2", 400));
            shipments.add(new Shipment("P2", "S3", 200));
            shipments.add(new Shipment("P2", "S4", 200));
            shipments.add(new Shipment("P4", "S4", 300));
            shipments.add(new Shipment("P5", "S4", 400));
        }
    }

    /**
     * Print the objects returned by an iterator of entity value objects.
     *
     * <p><b> IMPORTANT: All database iterators must be closed to avoid
     * serious database problems.  If the iterator is not closed, the
     * underlying Berkeley DB cursor is not closed either. </b></p>
     */
    private void printValues(String label, Iterator iterator) {

        System.out.println("\n--- " + label + " ---");
        try {
            while (iterator.hasNext()) {
                System.out.println(iterator.next().toString());
            }
        } finally {
            // IMPORTANT: Use StoredIterator to close all database
            // iterators.  If java.util.Iterator is in hand, you can safely
            // close it by calling StoredIterator.close(Iterator).
            StoredIterator.close(iterator);
        }
    }
}
