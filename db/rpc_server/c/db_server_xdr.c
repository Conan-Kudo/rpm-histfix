#include "db_config.h"

#ifdef HAVE_RPC
/*
 * Please do not edit this file.
 * It was generated using rpcgen.
 */

#ifndef NO_SYSTEM_INCLUDES
#include <rpc/rpc.h>
#endif

#include "db_int.h"
#include "dbinc_auto/db_server.h"

bool_t
xdr___env_cachesize_msg(xdrs, objp)
	register XDR *xdrs;
	__env_cachesize_msg *objp;
{

	if (!xdr_u_int(xdrs, &objp->dbenvcl_id))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->gbytes))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->bytes))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->ncache))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr___env_cachesize_reply(xdrs, objp)
	register XDR *xdrs;
	__env_cachesize_reply *objp;
{

	if (!xdr_int(xdrs, &objp->status))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr___env_close_msg(xdrs, objp)
	register XDR *xdrs;
	__env_close_msg *objp;
{

	if (!xdr_u_int(xdrs, &objp->dbenvcl_id))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->flags))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr___env_close_reply(xdrs, objp)
	register XDR *xdrs;
	__env_close_reply *objp;
{

	if (!xdr_int(xdrs, &objp->status))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr___env_create_msg(xdrs, objp)
	register XDR *xdrs;
	__env_create_msg *objp;
{

	if (!xdr_u_int(xdrs, &objp->timeout))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr___env_create_reply(xdrs, objp)
	register XDR *xdrs;
	__env_create_reply *objp;
{

	if (!xdr_int(xdrs, &objp->status))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->envcl_id))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr___env_dbremove_msg(xdrs, objp)
	register XDR *xdrs;
	__env_dbremove_msg *objp;
{

	if (!xdr_u_int(xdrs, &objp->dbenvcl_id))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->txnpcl_id))
		return (FALSE);
	if (!xdr_string(xdrs, &objp->name, ~0))
		return (FALSE);
	if (!xdr_string(xdrs, &objp->subdb, ~0))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->flags))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr___env_dbremove_reply(xdrs, objp)
	register XDR *xdrs;
	__env_dbremove_reply *objp;
{

	if (!xdr_int(xdrs, &objp->status))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr___env_dbrename_msg(xdrs, objp)
	register XDR *xdrs;
	__env_dbrename_msg *objp;
{

	if (!xdr_u_int(xdrs, &objp->dbenvcl_id))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->txnpcl_id))
		return (FALSE);
	if (!xdr_string(xdrs, &objp->name, ~0))
		return (FALSE);
	if (!xdr_string(xdrs, &objp->subdb, ~0))
		return (FALSE);
	if (!xdr_string(xdrs, &objp->newname, ~0))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->flags))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr___env_dbrename_reply(xdrs, objp)
	register XDR *xdrs;
	__env_dbrename_reply *objp;
{

	if (!xdr_int(xdrs, &objp->status))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr___env_encrypt_msg(xdrs, objp)
	register XDR *xdrs;
	__env_encrypt_msg *objp;
{

	if (!xdr_u_int(xdrs, &objp->dbenvcl_id))
		return (FALSE);
	if (!xdr_string(xdrs, &objp->passwd, ~0))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->flags))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr___env_encrypt_reply(xdrs, objp)
	register XDR *xdrs;
	__env_encrypt_reply *objp;
{

	if (!xdr_int(xdrs, &objp->status))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr___env_flags_msg(xdrs, objp)
	register XDR *xdrs;
	__env_flags_msg *objp;
{

	if (!xdr_u_int(xdrs, &objp->dbenvcl_id))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->flags))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->onoff))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr___env_flags_reply(xdrs, objp)
	register XDR *xdrs;
	__env_flags_reply *objp;
{

	if (!xdr_int(xdrs, &objp->status))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr___env_open_msg(xdrs, objp)
	register XDR *xdrs;
	__env_open_msg *objp;
{

	if (!xdr_u_int(xdrs, &objp->dbenvcl_id))
		return (FALSE);
	if (!xdr_string(xdrs, &objp->home, ~0))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->flags))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->mode))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr___env_open_reply(xdrs, objp)
	register XDR *xdrs;
	__env_open_reply *objp;
{

	if (!xdr_int(xdrs, &objp->status))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->envcl_id))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr___env_remove_msg(xdrs, objp)
	register XDR *xdrs;
	__env_remove_msg *objp;
{

	if (!xdr_u_int(xdrs, &objp->dbenvcl_id))
		return (FALSE);
	if (!xdr_string(xdrs, &objp->home, ~0))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->flags))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr___env_remove_reply(xdrs, objp)
	register XDR *xdrs;
	__env_remove_reply *objp;
{

	if (!xdr_int(xdrs, &objp->status))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr___txn_abort_msg(xdrs, objp)
	register XDR *xdrs;
	__txn_abort_msg *objp;
{

	if (!xdr_u_int(xdrs, &objp->txnpcl_id))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr___txn_abort_reply(xdrs, objp)
	register XDR *xdrs;
	__txn_abort_reply *objp;
{

	if (!xdr_int(xdrs, &objp->status))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr___txn_begin_msg(xdrs, objp)
	register XDR *xdrs;
	__txn_begin_msg *objp;
{

	if (!xdr_u_int(xdrs, &objp->dbenvcl_id))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->parentcl_id))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->flags))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr___txn_begin_reply(xdrs, objp)
	register XDR *xdrs;
	__txn_begin_reply *objp;
{

	if (!xdr_int(xdrs, &objp->status))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->txnidcl_id))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr___txn_commit_msg(xdrs, objp)
	register XDR *xdrs;
	__txn_commit_msg *objp;
{

	if (!xdr_u_int(xdrs, &objp->txnpcl_id))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->flags))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr___txn_commit_reply(xdrs, objp)
	register XDR *xdrs;
	__txn_commit_reply *objp;
{

	if (!xdr_int(xdrs, &objp->status))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr___txn_discard_msg(xdrs, objp)
	register XDR *xdrs;
	__txn_discard_msg *objp;
{

	if (!xdr_u_int(xdrs, &objp->txnpcl_id))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->flags))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr___txn_discard_reply(xdrs, objp)
	register XDR *xdrs;
	__txn_discard_reply *objp;
{

	if (!xdr_int(xdrs, &objp->status))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr___txn_prepare_msg(xdrs, objp)
	register XDR *xdrs;
	__txn_prepare_msg *objp;
{

	if (!xdr_u_int(xdrs, &objp->txnpcl_id))
		return (FALSE);
	if (!xdr_opaque(xdrs, objp->gid, 128))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr___txn_prepare_reply(xdrs, objp)
	register XDR *xdrs;
	__txn_prepare_reply *objp;
{

	if (!xdr_int(xdrs, &objp->status))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr___txn_recover_msg(xdrs, objp)
	register XDR *xdrs;
	__txn_recover_msg *objp;
{

	if (!xdr_u_int(xdrs, &objp->dbenvcl_id))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->count))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->flags))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr___txn_recover_reply(xdrs, objp)
	register XDR *xdrs;
	__txn_recover_reply *objp;
{

	if (!xdr_int(xdrs, &objp->status))
		return (FALSE);
	if (!xdr_array(xdrs, (char **)&objp->txn.txn_val, (u_int *) &objp->txn.txn_len, ~0,
		sizeof (u_int), (xdrproc_t) xdr_u_int))
		return (FALSE);
	if (!xdr_bytes(xdrs, (char **)&objp->gid.gid_val, (u_int *) &objp->gid.gid_len, ~0))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->retcount))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr___db_associate_msg(xdrs, objp)
	register XDR *xdrs;
	__db_associate_msg *objp;
{

	if (!xdr_u_int(xdrs, &objp->dbpcl_id))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->txnpcl_id))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->sdbpcl_id))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->flags))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr___db_associate_reply(xdrs, objp)
	register XDR *xdrs;
	__db_associate_reply *objp;
{

	if (!xdr_int(xdrs, &objp->status))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr___db_bt_maxkey_msg(xdrs, objp)
	register XDR *xdrs;
	__db_bt_maxkey_msg *objp;
{

	if (!xdr_u_int(xdrs, &objp->dbpcl_id))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->maxkey))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr___db_bt_maxkey_reply(xdrs, objp)
	register XDR *xdrs;
	__db_bt_maxkey_reply *objp;
{

	if (!xdr_int(xdrs, &objp->status))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr___db_bt_minkey_msg(xdrs, objp)
	register XDR *xdrs;
	__db_bt_minkey_msg *objp;
{

	if (!xdr_u_int(xdrs, &objp->dbpcl_id))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->minkey))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr___db_bt_minkey_reply(xdrs, objp)
	register XDR *xdrs;
	__db_bt_minkey_reply *objp;
{

	if (!xdr_int(xdrs, &objp->status))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr___db_close_msg(xdrs, objp)
	register XDR *xdrs;
	__db_close_msg *objp;
{

	if (!xdr_u_int(xdrs, &objp->dbpcl_id))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->flags))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr___db_close_reply(xdrs, objp)
	register XDR *xdrs;
	__db_close_reply *objp;
{

	if (!xdr_int(xdrs, &objp->status))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr___db_create_msg(xdrs, objp)
	register XDR *xdrs;
	__db_create_msg *objp;
{

	if (!xdr_u_int(xdrs, &objp->dbenvcl_id))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->flags))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr___db_create_reply(xdrs, objp)
	register XDR *xdrs;
	__db_create_reply *objp;
{

	if (!xdr_int(xdrs, &objp->status))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->dbcl_id))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr___db_del_msg(xdrs, objp)
	register XDR *xdrs;
	__db_del_msg *objp;
{

	if (!xdr_u_int(xdrs, &objp->dbpcl_id))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->txnpcl_id))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->keydlen))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->keydoff))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->keyulen))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->keyflags))
		return (FALSE);
	if (!xdr_bytes(xdrs, (char **)&objp->keydata.keydata_val, (u_int *) &objp->keydata.keydata_len, ~0))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->flags))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr___db_del_reply(xdrs, objp)
	register XDR *xdrs;
	__db_del_reply *objp;
{

	if (!xdr_int(xdrs, &objp->status))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr___db_encrypt_msg(xdrs, objp)
	register XDR *xdrs;
	__db_encrypt_msg *objp;
{

	if (!xdr_u_int(xdrs, &objp->dbpcl_id))
		return (FALSE);
	if (!xdr_string(xdrs, &objp->passwd, ~0))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->flags))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr___db_encrypt_reply(xdrs, objp)
	register XDR *xdrs;
	__db_encrypt_reply *objp;
{

	if (!xdr_int(xdrs, &objp->status))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr___db_extentsize_msg(xdrs, objp)
	register XDR *xdrs;
	__db_extentsize_msg *objp;
{

	if (!xdr_u_int(xdrs, &objp->dbpcl_id))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->extentsize))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr___db_extentsize_reply(xdrs, objp)
	register XDR *xdrs;
	__db_extentsize_reply *objp;
{

	if (!xdr_int(xdrs, &objp->status))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr___db_flags_msg(xdrs, objp)
	register XDR *xdrs;
	__db_flags_msg *objp;
{

	if (!xdr_u_int(xdrs, &objp->dbpcl_id))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->flags))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr___db_flags_reply(xdrs, objp)
	register XDR *xdrs;
	__db_flags_reply *objp;
{

	if (!xdr_int(xdrs, &objp->status))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr___db_get_msg(xdrs, objp)
	register XDR *xdrs;
	__db_get_msg *objp;
{

	if (!xdr_u_int(xdrs, &objp->dbpcl_id))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->txnpcl_id))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->keydlen))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->keydoff))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->keyulen))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->keyflags))
		return (FALSE);
	if (!xdr_bytes(xdrs, (char **)&objp->keydata.keydata_val, (u_int *) &objp->keydata.keydata_len, ~0))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->datadlen))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->datadoff))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->dataulen))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->dataflags))
		return (FALSE);
	if (!xdr_bytes(xdrs, (char **)&objp->datadata.datadata_val, (u_int *) &objp->datadata.datadata_len, ~0))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->flags))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr___db_get_reply(xdrs, objp)
	register XDR *xdrs;
	__db_get_reply *objp;
{

	if (!xdr_int(xdrs, &objp->status))
		return (FALSE);
	if (!xdr_bytes(xdrs, (char **)&objp->keydata.keydata_val, (u_int *) &objp->keydata.keydata_len, ~0))
		return (FALSE);
	if (!xdr_bytes(xdrs, (char **)&objp->datadata.datadata_val, (u_int *) &objp->datadata.datadata_len, ~0))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr___db_h_ffactor_msg(xdrs, objp)
	register XDR *xdrs;
	__db_h_ffactor_msg *objp;
{

	if (!xdr_u_int(xdrs, &objp->dbpcl_id))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->ffactor))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr___db_h_ffactor_reply(xdrs, objp)
	register XDR *xdrs;
	__db_h_ffactor_reply *objp;
{

	if (!xdr_int(xdrs, &objp->status))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr___db_h_nelem_msg(xdrs, objp)
	register XDR *xdrs;
	__db_h_nelem_msg *objp;
{

	if (!xdr_u_int(xdrs, &objp->dbpcl_id))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->nelem))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr___db_h_nelem_reply(xdrs, objp)
	register XDR *xdrs;
	__db_h_nelem_reply *objp;
{

	if (!xdr_int(xdrs, &objp->status))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr___db_key_range_msg(xdrs, objp)
	register XDR *xdrs;
	__db_key_range_msg *objp;
{

	if (!xdr_u_int(xdrs, &objp->dbpcl_id))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->txnpcl_id))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->keydlen))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->keydoff))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->keyulen))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->keyflags))
		return (FALSE);
	if (!xdr_bytes(xdrs, (char **)&objp->keydata.keydata_val, (u_int *) &objp->keydata.keydata_len, ~0))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->flags))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr___db_key_range_reply(xdrs, objp)
	register XDR *xdrs;
	__db_key_range_reply *objp;
{

	if (!xdr_int(xdrs, &objp->status))
		return (FALSE);
	if (!xdr_double(xdrs, &objp->less))
		return (FALSE);
	if (!xdr_double(xdrs, &objp->equal))
		return (FALSE);
	if (!xdr_double(xdrs, &objp->greater))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr___db_lorder_msg(xdrs, objp)
	register XDR *xdrs;
	__db_lorder_msg *objp;
{

	if (!xdr_u_int(xdrs, &objp->dbpcl_id))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->lorder))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr___db_lorder_reply(xdrs, objp)
	register XDR *xdrs;
	__db_lorder_reply *objp;
{

	if (!xdr_int(xdrs, &objp->status))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr___db_open_msg(xdrs, objp)
	register XDR *xdrs;
	__db_open_msg *objp;
{

	if (!xdr_u_int(xdrs, &objp->dbpcl_id))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->txnpcl_id))
		return (FALSE);
	if (!xdr_string(xdrs, &objp->name, ~0))
		return (FALSE);
	if (!xdr_string(xdrs, &objp->subdb, ~0))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->type))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->flags))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->mode))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr___db_open_reply(xdrs, objp)
	register XDR *xdrs;
	__db_open_reply *objp;
{

	if (!xdr_int(xdrs, &objp->status))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->dbcl_id))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->type))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->dbflags))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->lorder))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr___db_pagesize_msg(xdrs, objp)
	register XDR *xdrs;
	__db_pagesize_msg *objp;
{

	if (!xdr_u_int(xdrs, &objp->dbpcl_id))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->pagesize))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr___db_pagesize_reply(xdrs, objp)
	register XDR *xdrs;
	__db_pagesize_reply *objp;
{

	if (!xdr_int(xdrs, &objp->status))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr___db_pget_msg(xdrs, objp)
	register XDR *xdrs;
	__db_pget_msg *objp;
{

	if (!xdr_u_int(xdrs, &objp->dbpcl_id))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->txnpcl_id))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->skeydlen))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->skeydoff))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->skeyulen))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->skeyflags))
		return (FALSE);
	if (!xdr_bytes(xdrs, (char **)&objp->skeydata.skeydata_val, (u_int *) &objp->skeydata.skeydata_len, ~0))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->pkeydlen))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->pkeydoff))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->pkeyulen))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->pkeyflags))
		return (FALSE);
	if (!xdr_bytes(xdrs, (char **)&objp->pkeydata.pkeydata_val, (u_int *) &objp->pkeydata.pkeydata_len, ~0))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->datadlen))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->datadoff))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->dataulen))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->dataflags))
		return (FALSE);
	if (!xdr_bytes(xdrs, (char **)&objp->datadata.datadata_val, (u_int *) &objp->datadata.datadata_len, ~0))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->flags))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr___db_pget_reply(xdrs, objp)
	register XDR *xdrs;
	__db_pget_reply *objp;
{

	if (!xdr_int(xdrs, &objp->status))
		return (FALSE);
	if (!xdr_bytes(xdrs, (char **)&objp->skeydata.skeydata_val, (u_int *) &objp->skeydata.skeydata_len, ~0))
		return (FALSE);
	if (!xdr_bytes(xdrs, (char **)&objp->pkeydata.pkeydata_val, (u_int *) &objp->pkeydata.pkeydata_len, ~0))
		return (FALSE);
	if (!xdr_bytes(xdrs, (char **)&objp->datadata.datadata_val, (u_int *) &objp->datadata.datadata_len, ~0))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr___db_put_msg(xdrs, objp)
	register XDR *xdrs;
	__db_put_msg *objp;
{

	if (!xdr_u_int(xdrs, &objp->dbpcl_id))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->txnpcl_id))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->keydlen))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->keydoff))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->keyulen))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->keyflags))
		return (FALSE);
	if (!xdr_bytes(xdrs, (char **)&objp->keydata.keydata_val, (u_int *) &objp->keydata.keydata_len, ~0))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->datadlen))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->datadoff))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->dataulen))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->dataflags))
		return (FALSE);
	if (!xdr_bytes(xdrs, (char **)&objp->datadata.datadata_val, (u_int *) &objp->datadata.datadata_len, ~0))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->flags))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr___db_put_reply(xdrs, objp)
	register XDR *xdrs;
	__db_put_reply *objp;
{

	if (!xdr_int(xdrs, &objp->status))
		return (FALSE);
	if (!xdr_bytes(xdrs, (char **)&objp->keydata.keydata_val, (u_int *) &objp->keydata.keydata_len, ~0))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr___db_re_delim_msg(xdrs, objp)
	register XDR *xdrs;
	__db_re_delim_msg *objp;
{

	if (!xdr_u_int(xdrs, &objp->dbpcl_id))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->delim))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr___db_re_delim_reply(xdrs, objp)
	register XDR *xdrs;
	__db_re_delim_reply *objp;
{

	if (!xdr_int(xdrs, &objp->status))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr___db_re_len_msg(xdrs, objp)
	register XDR *xdrs;
	__db_re_len_msg *objp;
{

	if (!xdr_u_int(xdrs, &objp->dbpcl_id))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->len))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr___db_re_len_reply(xdrs, objp)
	register XDR *xdrs;
	__db_re_len_reply *objp;
{

	if (!xdr_int(xdrs, &objp->status))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr___db_re_pad_msg(xdrs, objp)
	register XDR *xdrs;
	__db_re_pad_msg *objp;
{

	if (!xdr_u_int(xdrs, &objp->dbpcl_id))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->pad))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr___db_re_pad_reply(xdrs, objp)
	register XDR *xdrs;
	__db_re_pad_reply *objp;
{

	if (!xdr_int(xdrs, &objp->status))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr___db_remove_msg(xdrs, objp)
	register XDR *xdrs;
	__db_remove_msg *objp;
{

	if (!xdr_u_int(xdrs, &objp->dbpcl_id))
		return (FALSE);
	if (!xdr_string(xdrs, &objp->name, ~0))
		return (FALSE);
	if (!xdr_string(xdrs, &objp->subdb, ~0))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->flags))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr___db_remove_reply(xdrs, objp)
	register XDR *xdrs;
	__db_remove_reply *objp;
{

	if (!xdr_int(xdrs, &objp->status))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr___db_rename_msg(xdrs, objp)
	register XDR *xdrs;
	__db_rename_msg *objp;
{

	if (!xdr_u_int(xdrs, &objp->dbpcl_id))
		return (FALSE);
	if (!xdr_string(xdrs, &objp->name, ~0))
		return (FALSE);
	if (!xdr_string(xdrs, &objp->subdb, ~0))
		return (FALSE);
	if (!xdr_string(xdrs, &objp->newname, ~0))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->flags))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr___db_rename_reply(xdrs, objp)
	register XDR *xdrs;
	__db_rename_reply *objp;
{

	if (!xdr_int(xdrs, &objp->status))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr___db_stat_msg(xdrs, objp)
	register XDR *xdrs;
	__db_stat_msg *objp;
{

	if (!xdr_u_int(xdrs, &objp->dbpcl_id))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->flags))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr___db_stat_reply(xdrs, objp)
	register XDR *xdrs;
	__db_stat_reply *objp;
{

	if (!xdr_int(xdrs, &objp->status))
		return (FALSE);
	if (!xdr_array(xdrs, (char **)&objp->stats.stats_val, (u_int *) &objp->stats.stats_len, ~0,
		sizeof (u_int), (xdrproc_t) xdr_u_int))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr___db_sync_msg(xdrs, objp)
	register XDR *xdrs;
	__db_sync_msg *objp;
{

	if (!xdr_u_int(xdrs, &objp->dbpcl_id))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->flags))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr___db_sync_reply(xdrs, objp)
	register XDR *xdrs;
	__db_sync_reply *objp;
{

	if (!xdr_int(xdrs, &objp->status))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr___db_truncate_msg(xdrs, objp)
	register XDR *xdrs;
	__db_truncate_msg *objp;
{

	if (!xdr_u_int(xdrs, &objp->dbpcl_id))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->txnpcl_id))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->flags))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr___db_truncate_reply(xdrs, objp)
	register XDR *xdrs;
	__db_truncate_reply *objp;
{

	if (!xdr_int(xdrs, &objp->status))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->count))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr___db_cursor_msg(xdrs, objp)
	register XDR *xdrs;
	__db_cursor_msg *objp;
{

	if (!xdr_u_int(xdrs, &objp->dbpcl_id))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->txnpcl_id))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->flags))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr___db_cursor_reply(xdrs, objp)
	register XDR *xdrs;
	__db_cursor_reply *objp;
{

	if (!xdr_int(xdrs, &objp->status))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->dbcidcl_id))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr___db_join_msg(xdrs, objp)
	register XDR *xdrs;
	__db_join_msg *objp;
{

	if (!xdr_u_int(xdrs, &objp->dbpcl_id))
		return (FALSE);
	if (!xdr_array(xdrs, (char **)&objp->curs.curs_val, (u_int *) &objp->curs.curs_len, ~0,
		sizeof (u_int), (xdrproc_t) xdr_u_int))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->flags))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr___db_join_reply(xdrs, objp)
	register XDR *xdrs;
	__db_join_reply *objp;
{

	if (!xdr_int(xdrs, &objp->status))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->dbcidcl_id))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr___dbc_close_msg(xdrs, objp)
	register XDR *xdrs;
	__dbc_close_msg *objp;
{

	if (!xdr_u_int(xdrs, &objp->dbccl_id))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr___dbc_close_reply(xdrs, objp)
	register XDR *xdrs;
	__dbc_close_reply *objp;
{

	if (!xdr_int(xdrs, &objp->status))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr___dbc_count_msg(xdrs, objp)
	register XDR *xdrs;
	__dbc_count_msg *objp;
{

	if (!xdr_u_int(xdrs, &objp->dbccl_id))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->flags))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr___dbc_count_reply(xdrs, objp)
	register XDR *xdrs;
	__dbc_count_reply *objp;
{

	if (!xdr_int(xdrs, &objp->status))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->dupcount))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr___dbc_del_msg(xdrs, objp)
	register XDR *xdrs;
	__dbc_del_msg *objp;
{

	if (!xdr_u_int(xdrs, &objp->dbccl_id))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->flags))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr___dbc_del_reply(xdrs, objp)
	register XDR *xdrs;
	__dbc_del_reply *objp;
{

	if (!xdr_int(xdrs, &objp->status))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr___dbc_dup_msg(xdrs, objp)
	register XDR *xdrs;
	__dbc_dup_msg *objp;
{

	if (!xdr_u_int(xdrs, &objp->dbccl_id))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->flags))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr___dbc_dup_reply(xdrs, objp)
	register XDR *xdrs;
	__dbc_dup_reply *objp;
{

	if (!xdr_int(xdrs, &objp->status))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->dbcidcl_id))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr___dbc_get_msg(xdrs, objp)
	register XDR *xdrs;
	__dbc_get_msg *objp;
{

	if (!xdr_u_int(xdrs, &objp->dbccl_id))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->keydlen))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->keydoff))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->keyulen))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->keyflags))
		return (FALSE);
	if (!xdr_bytes(xdrs, (char **)&objp->keydata.keydata_val, (u_int *) &objp->keydata.keydata_len, ~0))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->datadlen))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->datadoff))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->dataulen))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->dataflags))
		return (FALSE);
	if (!xdr_bytes(xdrs, (char **)&objp->datadata.datadata_val, (u_int *) &objp->datadata.datadata_len, ~0))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->flags))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr___dbc_get_reply(xdrs, objp)
	register XDR *xdrs;
	__dbc_get_reply *objp;
{

	if (!xdr_int(xdrs, &objp->status))
		return (FALSE);
	if (!xdr_bytes(xdrs, (char **)&objp->keydata.keydata_val, (u_int *) &objp->keydata.keydata_len, ~0))
		return (FALSE);
	if (!xdr_bytes(xdrs, (char **)&objp->datadata.datadata_val, (u_int *) &objp->datadata.datadata_len, ~0))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr___dbc_pget_msg(xdrs, objp)
	register XDR *xdrs;
	__dbc_pget_msg *objp;
{

	if (!xdr_u_int(xdrs, &objp->dbccl_id))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->skeydlen))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->skeydoff))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->skeyulen))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->skeyflags))
		return (FALSE);
	if (!xdr_bytes(xdrs, (char **)&objp->skeydata.skeydata_val, (u_int *) &objp->skeydata.skeydata_len, ~0))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->pkeydlen))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->pkeydoff))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->pkeyulen))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->pkeyflags))
		return (FALSE);
	if (!xdr_bytes(xdrs, (char **)&objp->pkeydata.pkeydata_val, (u_int *) &objp->pkeydata.pkeydata_len, ~0))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->datadlen))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->datadoff))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->dataulen))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->dataflags))
		return (FALSE);
	if (!xdr_bytes(xdrs, (char **)&objp->datadata.datadata_val, (u_int *) &objp->datadata.datadata_len, ~0))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->flags))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr___dbc_pget_reply(xdrs, objp)
	register XDR *xdrs;
	__dbc_pget_reply *objp;
{

	if (!xdr_int(xdrs, &objp->status))
		return (FALSE);
	if (!xdr_bytes(xdrs, (char **)&objp->skeydata.skeydata_val, (u_int *) &objp->skeydata.skeydata_len, ~0))
		return (FALSE);
	if (!xdr_bytes(xdrs, (char **)&objp->pkeydata.pkeydata_val, (u_int *) &objp->pkeydata.pkeydata_len, ~0))
		return (FALSE);
	if (!xdr_bytes(xdrs, (char **)&objp->datadata.datadata_val, (u_int *) &objp->datadata.datadata_len, ~0))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr___dbc_put_msg(xdrs, objp)
	register XDR *xdrs;
	__dbc_put_msg *objp;
{

	if (!xdr_u_int(xdrs, &objp->dbccl_id))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->keydlen))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->keydoff))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->keyulen))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->keyflags))
		return (FALSE);
	if (!xdr_bytes(xdrs, (char **)&objp->keydata.keydata_val, (u_int *) &objp->keydata.keydata_len, ~0))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->datadlen))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->datadoff))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->dataulen))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->dataflags))
		return (FALSE);
	if (!xdr_bytes(xdrs, (char **)&objp->datadata.datadata_val, (u_int *) &objp->datadata.datadata_len, ~0))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->flags))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr___dbc_put_reply(xdrs, objp)
	register XDR *xdrs;
	__dbc_put_reply *objp;
{

	if (!xdr_int(xdrs, &objp->status))
		return (FALSE);
	if (!xdr_bytes(xdrs, (char **)&objp->keydata.keydata_val, (u_int *) &objp->keydata.keydata_len, ~0))
		return (FALSE);
	return (TRUE);
}
#endif /* HAVE_RPC */
