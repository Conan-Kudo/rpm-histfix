/*
 * Automatically generated by jrpcgen 0.95.1 on 12/18/01 7:23 PM
 * jrpcgen is part of the "Remote Tea" ONC/RPC package for Java
 * See http://acplt.org/ks/remotetea.html for details
 */
package com.sleepycat.db.rpcserver;
import org.acplt.oncrpc.*;
import java.io.IOException;

public class __db_pget_msg implements XdrAble {
    public int dbpcl_id;
    public int txnpcl_id;
    public int skeydlen;
    public int skeydoff;
    public int skeyulen;
    public int skeyflags;
    public byte [] skeydata;
    public int pkeydlen;
    public int pkeydoff;
    public int pkeyulen;
    public int pkeyflags;
    public byte [] pkeydata;
    public int datadlen;
    public int datadoff;
    public int dataulen;
    public int dataflags;
    public byte [] datadata;
    public int flags;

    public __db_pget_msg() {
    }

    public __db_pget_msg(XdrDecodingStream xdr)
           throws OncRpcException, IOException {
        xdrDecode(xdr);
    }

    public void xdrEncode(XdrEncodingStream xdr)
           throws OncRpcException, IOException {
        xdr.xdrEncodeInt(dbpcl_id);
        xdr.xdrEncodeInt(txnpcl_id);
        xdr.xdrEncodeInt(skeydlen);
        xdr.xdrEncodeInt(skeydoff);
        xdr.xdrEncodeInt(skeyulen);
        xdr.xdrEncodeInt(skeyflags);
        xdr.xdrEncodeDynamicOpaque(skeydata);
        xdr.xdrEncodeInt(pkeydlen);
        xdr.xdrEncodeInt(pkeydoff);
        xdr.xdrEncodeInt(pkeyulen);
        xdr.xdrEncodeInt(pkeyflags);
        xdr.xdrEncodeDynamicOpaque(pkeydata);
        xdr.xdrEncodeInt(datadlen);
        xdr.xdrEncodeInt(datadoff);
        xdr.xdrEncodeInt(dataulen);
        xdr.xdrEncodeInt(dataflags);
        xdr.xdrEncodeDynamicOpaque(datadata);
        xdr.xdrEncodeInt(flags);
    }

    public void xdrDecode(XdrDecodingStream xdr)
           throws OncRpcException, IOException {
        dbpcl_id = xdr.xdrDecodeInt();
        txnpcl_id = xdr.xdrDecodeInt();
        skeydlen = xdr.xdrDecodeInt();
        skeydoff = xdr.xdrDecodeInt();
        skeyulen = xdr.xdrDecodeInt();
        skeyflags = xdr.xdrDecodeInt();
        skeydata = xdr.xdrDecodeDynamicOpaque();
        pkeydlen = xdr.xdrDecodeInt();
        pkeydoff = xdr.xdrDecodeInt();
        pkeyulen = xdr.xdrDecodeInt();
        pkeyflags = xdr.xdrDecodeInt();
        pkeydata = xdr.xdrDecodeDynamicOpaque();
        datadlen = xdr.xdrDecodeInt();
        datadoff = xdr.xdrDecodeInt();
        dataulen = xdr.xdrDecodeInt();
        dataflags = xdr.xdrDecodeInt();
        datadata = xdr.xdrDecodeDynamicOpaque();
        flags = xdr.xdrDecodeInt();
    }

}
// End of __db_pget_msg.java
