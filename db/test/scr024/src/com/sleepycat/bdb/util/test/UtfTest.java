/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2002-2003
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: UtfTest.java,v 1.1 2003/12/15 21:44:54 jbj Exp $
 */

package com.sleepycat.bdb.util.test;

import com.sleepycat.bdb.util.FastOutputStream;
import com.sleepycat.bdb.util.UtfOps;
import java.io.DataOutputStream;
import java.io.IOException;
import java.util.Arrays;
import junit.framework.Test;
import junit.framework.TestCase;
import junit.framework.TestSuite;

/**
 * @author Mark Hayes
 */
public class UtfTest extends TestCase {

    public static void main(String[] args)
        throws Exception {

        junit.textui.TestRunner.run(suite());
    }

    public static Test suite()
        throws Exception {

        TestSuite suite = new TestSuite(UtfTest.class);
        return suite;
    }

    public UtfTest(String name) {

        super(name);
    }

    public void setUp() {

        System.out.println("UtfTest." + getName());
    }

    /**
     * Compares the UtfOps implementation to the java.util.DataOutputStream
     * (and by implication DataInputStream) implementation, character for
     * character in the full Unicode set.
     */
    public void testMultibyte()
        throws Exception {

        char c = 0;
        byte[] buf = new byte[10];
        byte[] javaBuf = new byte[10];
        char[] cArray = new char[1];
        FastOutputStream javaBufStream = new FastOutputStream(javaBuf);
        DataOutputStream javaOutStream = new DataOutputStream(javaBufStream);

        try {
            for (int cInt = Character.MIN_VALUE; cInt <= Character.MAX_VALUE;
                 cInt += 1) {
                c = (char) cInt;
                cArray[0] = c;
                int byteLen = UtfOps.getByteLength(cArray);

                javaBufStream.reset();
                javaOutStream.writeUTF(new String(cArray));
                int javaByteLen = javaBufStream.size() - 2;

                if (byteLen != javaByteLen) {
                    fail("Character 0x" + Integer.toHexString(c) +
                         " UtfOps size " + byteLen +
                         " != JavaIO size " + javaByteLen);
                }

                Arrays.fill(buf, (byte) 0);
                UtfOps.charsToBytes(cArray, 0, buf, 0, 1);

                for (int i = 0; i < byteLen; i += 1) {
                    if (buf[i] != javaBuf[i + 2]) {
                        fail("Character 0x" + Integer.toHexString(c) +
                             " byte offset " + i +
                             " UtfOps byte " + Integer.toHexString(buf[i]) +
                             " != JavaIO byte " +
                             Integer.toHexString(javaBuf[i + 2]));
                    }
                }

                int charLen = UtfOps.getCharLength(buf, 0, byteLen);
                if (charLen != 1) {
                    fail("Character 0x" + Integer.toHexString(c) +
                         " UtfOps char len " + charLen +
                         " but should be one");
                }

                cArray[0] = (char) 0;
                int len = UtfOps.bytesToChars(buf, 0, cArray, 0, byteLen,
                                              true);
                if (len != byteLen) {
                    fail("Character 0x" + Integer.toHexString(c) +
                         " UtfOps bytesToChars(w/byteLen) len " + len +
                         " but should be " + byteLen);
                }

                if (cArray[0] != c) {
                    fail("Character 0x" + Integer.toHexString(c) +
                         " UtfOps bytesToChars(w/byteLen) char " +
                         Integer.toHexString(cArray[0]));
                }

                cArray[0] = (char) 0;
                len = UtfOps.bytesToChars(buf, 0, cArray, 0, 1, false);
                if (len != byteLen) {
                    fail("Character 0x" + Integer.toHexString(c) +
                         " UtfOps bytesToChars(w/charLen) len " + len +
                         " but should be " + byteLen);
                }

                if (cArray[0] != c) {
                    fail("Character 0x" + Integer.toHexString(c) +
                         " UtfOps bytesToChars(w/charLen) char " +
                         Integer.toHexString(cArray[0]));
                }

                String s = new String(cArray, 0, 1);
                byte[] sBytes = UtfOps.stringToBytes(s);
                if (sBytes.length != byteLen) {
                    fail("Character 0x" + Integer.toHexString(c) +
                         " UtfOps stringToBytes() len " + sBytes.length +
                         " but should be " + byteLen);
                }

                for (int i = 0; i < byteLen; i += 1) {
                    if (sBytes[i] != javaBuf[i + 2]) {
                        fail("Character 0x" + Integer.toHexString(c) +
                             " byte offset " + i +
                             " UtfOps byte " + Integer.toHexString(sBytes[i]) +
                             " != JavaIO byte " +
                             Integer.toHexString(javaBuf[i + 2]));
                    }
                }
            }
        } catch (Exception e) {
            System.out.println("Character 0x" + Integer.toHexString(c) +
                               " exception occurred");
            throw e;
        }
    }
}

