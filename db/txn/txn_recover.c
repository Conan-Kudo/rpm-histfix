/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2001
 *	Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "Id: txn_recover.c,v 1.12 2001/04/27 15:48:16 bostic Exp ";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <string.h>
#endif

#ifdef  HAVE_RPC
#include "db_server.h"
#endif

#include "db_int.h"
#include "txn.h"
#include "db_page.h"
#include "log.h"
#include "db_auto.h"
#include "crdel_auto.h"
#include "db_ext.h"

#ifdef  HAVE_RPC
#include "rpc_client_ext.h"
#endif

/*
 * __txn_continue
 *	Fill in the fields of the local transaction structure given
 *	the detail transaction structure.
 *	XXX I'm not sure that we work correctly with nested txns.
 *
 * PUBLIC: void __txn_continue __P((DB_ENV *, DB_TXN *, TXN_DETAIL *, size_t));
 */
void
__txn_continue(env, txnp, td, off)
	DB_ENV *env;
	DB_TXN *txnp;
	TXN_DETAIL *td;
	size_t off;
{
	txnp->mgrp = env->tx_handle;
	txnp->parent = NULL;
	txnp->last_lsn = td->last_lsn;
	txnp->txnid = td->txnid;
	txnp->off = off;
	txnp->flags = 0;
}

/*
 * __txn_map_gid
 *	Return the txn that corresponds to this global ID.
 *
 * PUBLIC: int __txn_map_gid __P((DB_ENV *,
 * PUBLIC:     u_int8_t *, TXN_DETAIL **, size_t *));
 */
int
__txn_map_gid(dbenv, gid, tdp, offp)
	DB_ENV *dbenv;
	u_int8_t *gid;
	TXN_DETAIL **tdp;
	size_t *offp;
{
	DB_TXNMGR *mgr;
	DB_TXNREGION *tmr;

	mgr = dbenv->tx_handle;
	tmr = mgr->reginfo.primary;

	/*
	 * Search the internal active transaction table to find the
	 * matching xid.  If this is a performance hit, then we
	 * can create a hash table, but I doubt it's worth it.
	 */
	R_LOCK(dbenv, &mgr->reginfo);
	for (*tdp = SH_TAILQ_FIRST(&tmr->active_txn, __txn_detail);
	    *tdp != NULL;
	    *tdp = SH_TAILQ_NEXT(*tdp, links, __txn_detail))
		if (memcmp(gid, (*tdp)->xid, sizeof((*tdp)->xid)) == 0)
			break;
	R_UNLOCK(dbenv, &mgr->reginfo);

	if (*tdp == NULL)
		return (EINVAL);

	*offp = R_OFFSET(&mgr->reginfo, *tdp);
	return (0);
}

/*
 * txn_recover --
 *	Public interface to retrieve the list of prepared, but not yet
 * commited transactions.  See __txn_get_prepared for details.  This
 * function and __db_xa_recover both wrap that one.
 *
 * EXTERN: int txn_recover
 * EXTERN:     __P((DB_ENV *, DB_PREPLIST *, long, long *, u_int32_t));
 */
int
txn_recover(dbenv, preplist, count, retp, flags)
	DB_ENV *dbenv;
	DB_PREPLIST *preplist;
	long count, *retp;
	u_int32_t flags;
{
#ifdef HAVE_RPC
	if (F_ISSET(dbenv, DB_ENV_RPCCLIENT))
		return (__dbcl_txn_recover(dbenv,
		    preplist, count, retp, flags));
#endif
	return (__txn_get_prepared(dbenv, NULL, preplist, count, retp, flags));
}

/*
 * __txn_get_prepared --
 *	Returns a list of prepared (and for XA, heuristically completed)
 *	transactions (less than or equal to the count parameter).  One of
 *	xids or txns must be set to point to an array of the appropriate type.
 *	The count parameter indicates the number of entries in the xids and/or
 *	txns array. The retp parameter will be set to indicate the number of
 *	entries	returned in the xids/txns array.  Flags indicates the operation,
 *	one of DB_FIRST or DB_NEXT.
 *
 * PUBLIC: int __txn_get_prepared __P((DB_ENV *,
 * PUBLIC:     XID *, DB_PREPLIST *, long, long *, u_int32_t));
 */
int
__txn_get_prepared(dbenv, xids, txns, count, retp, flags)
	DB_ENV *dbenv;
	XID *xids;
	DB_PREPLIST *txns;
	long count;		/* This is long for XA compatibility. */
	long  *retp;
	u_int32_t flags;
{
	XID *xidp;
	DB_PREPLIST *prepp;
	TXN_DETAIL *td;
	DB_TXNMGR *mgr;
	DB_TXNREGION *tmr;
	DB_LSN min, open_lsn;
	DBT data;
	__txn_ckp_args *ckp_args;
	int nrestores, open_files, ret;
	void *txninfo;

	ret = 0;
	*retp = 0;
	xidp = xids;
	prepp = txns;
	open_files = 1;
	nrestores = 0;
	MAX_LSN(min);

	/*
	 * If we are starting a scan, then we traverse the active transaction
	 * list once making sure that all transactions are marked as not having
	 * been collected.  Then on each pass, we mark the ones we collected
	 * so that if we cannot collect them all at once, we can finish up
	 * next time with a continue.
	 */

	mgr = dbenv->tx_handle;
	tmr = mgr->reginfo.primary;

	/*
	 * During this pass we need to figure out if we are going to need
	 * to open files.  We need to open files if we've never collected
	 * before (in which case, none of the COLLECTED bits will be set)
	 * and the ones that we are collecting are restored (if they aren't
	 * restored, then we never crashed; just the main server did).
	 */
	R_LOCK(dbenv, &mgr->reginfo);
	if (flags == DB_FIRST) {
		for (td = SH_TAILQ_FIRST(&tmr->active_txn, __txn_detail);
		    td != NULL;
		    td = SH_TAILQ_NEXT(td, links, __txn_detail)) {
			if (F_ISSET(td, TXN_RESTORED))
				nrestores++;
			if (F_ISSET(td, TXN_COLLECTED))
				open_files = 0;
			F_CLR(td, TXN_COLLECTED);
		}

	} else
		open_files = 0;

	/* Now begin collecting active transactions. */
	for (td = SH_TAILQ_FIRST(&tmr->active_txn, __txn_detail);
	    td != NULL && *retp < count;
	    td = SH_TAILQ_NEXT(td, links, __txn_detail)) {
		if (td->status != TXN_PREPARED || F_ISSET(td, TXN_COLLECTED))
			continue;

		if (xids != NULL) {
			xidp->formatID = td->format;
			xidp->gtrid_length = td->gtrid;
			xidp->bqual_length = td->bqual;
			memcpy(xidp->data, td->xid, sizeof(td->xid));
			xidp++;
		}

		if (txns != NULL) {
			if ((ret = __os_calloc(dbenv,
			    1, sizeof(DB_TXN), &prepp->txn)) != 0)
				break;
			__txn_continue(dbenv,
			    prepp->txn, td, R_OFFSET(&mgr->reginfo, td));
			memcpy(prepp->gid, td->xid, sizeof(td->xid));
			prepp++;
		}

		if (log_compare(&td->begin_lsn, &min) < 0)
			min = td->begin_lsn;

		(*retp)++;
		F_SET(td, TXN_COLLECTED);
	}
	R_UNLOCK(dbenv, &mgr->reginfo);

	if (open_files && nrestores &&
	    ret == 0 && *retp != 0 && !IS_MAX_LSN(min)) {
		/*
		 * Figure out the last checkpoint before the smallest
		 * start_lsn in the region.
		 */
		F_SET((DB_LOG *)dbenv->lg_handle, DBLOG_RECOVER);
		memset(&data, 0, sizeof(data));
		for (ret = log_get(dbenv, &open_lsn, &data, DB_CHECKPOINT);
		    ret == 0 && log_compare(&min, &open_lsn) < 0;
		    ret = log_get(dbenv, &open_lsn, &data, DB_SET)) {

			/* Format the log record. */
			if ((ret = __txn_ckp_read(dbenv,
			    data.data, &ckp_args)) != 0) {
				__db_err(dbenv,
				    "Invalid checkpoint record at [%ld][%ld]\n",
				    (u_long)open_lsn.file,
				    (u_long)open_lsn.offset);
				goto out;
			}
			open_lsn = ckp_args->last_ckp;
			__os_free(dbenv, ckp_args, sizeof(*ckp_args));
		}

		/*
		 * There are three ways we got here.
		 * We got a DB_NOTFOUND -- we need to read the first log record.
		 * We found a checkpoint before min.  We're done.
		 * We found a checkpoint after min who's last_ckp is 0.  We
		 *	need to start at the beginning of the log.
		 */

		if ((ret == DB_NOTFOUND || IS_ZERO_LSN(open_lsn)) &&
		    (ret = log_get(dbenv, &open_lsn, &data, DB_FIRST)) != 0) {
			__db_err(dbenv, "No log records.");
			goto out;
		}

		if ((ret = __db_txnlist_init(dbenv, &txninfo)) != 0)
			goto out;
		ret = __env_openfiles(dbenv,
		    txninfo, &data, &open_lsn, NULL, 0, 0);
		if (txninfo != NULL)
			__db_txnlist_end(dbenv, txninfo);
		F_CLR((DB_LOG *)dbenv->lg_handle, DBLOG_RECOVER);
	}
	if (0) {
out:		F_CLR((DB_LOG *)dbenv->lg_handle, DBLOG_RECOVER);
	}
	return (ret);

}

