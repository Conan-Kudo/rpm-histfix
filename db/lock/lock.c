/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996-2003
 *	Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: lock.c,v 11.134 2003/11/18 21:30:38 ubell Exp $";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <string.h>
#include <stdlib.h>
#endif

#include "db_int.h"
#include "dbinc/db_shash.h"
#include "dbinc/lock.h"
#include "dbinc/log.h"

static int  __lock_freelock __P((DB_LOCKTAB *,
		struct __db_lock *, u_int32_t, u_int32_t));
static void __lock_expires __P((DB_ENV *, db_timeval_t *, db_timeout_t));
static void __lock_freelocker
		__P((DB_LOCKTAB *, DB_LOCKREGION *, DB_LOCKER *, u_int32_t));
static int  __lock_get_internal __P((DB_LOCKTAB *, u_int32_t, u_int32_t,
		const DBT *, db_lockmode_t, db_timeout_t, DB_LOCK *));
static int  __lock_getobj
		__P((DB_LOCKTAB *, const DBT *, u_int32_t, int, DB_LOCKOBJ **));
static int  __lock_inherit_locks __P ((DB_LOCKTAB *, u_int32_t, u_int32_t));
static int  __lock_is_parent __P((DB_LOCKTAB *, u_int32_t, DB_LOCKER *));
static int  __lock_put_internal __P((DB_LOCKTAB *,
		struct __db_lock *, u_int32_t,  u_int32_t));
static int  __lock_put_nolock __P((DB_ENV *, DB_LOCK *, int *, u_int32_t));
static void __lock_remove_waiter __P((DB_LOCKTAB *,
		DB_LOCKOBJ *, struct __db_lock *, db_status_t));
static int __lock_set_timeout_internal  __P(( DB_ENV *,
		u_int32_t, db_timeout_t, u_int32_t));
static int __lock_trade __P((DB_ENV *, DB_LOCK *, u_int32_t));
static int __lock_sort_cmp __P((const void *, const void *));
static int __lock_fix_list __P((DB_ENV *, DBT *, u_int32_t));

static const char __db_lock_err[] = "Lock table is out of available %s";
static const char __db_lock_invalid[] = "%s: Lock is no longer valid";
static const char __db_locker_invalid[] = "Locker is not valid";

/*
 * __lock_id_pp --
 *	DB_ENV->lock_id pre/post processing.
 *
 * PUBLIC: int __lock_id_pp __P((DB_ENV *, u_int32_t *));
 */
int
__lock_id_pp(dbenv, idp)
	DB_ENV *dbenv;
	u_int32_t *idp;
{
	int rep_check, ret;

	PANIC_CHECK(dbenv);
	ENV_REQUIRES_CONFIG(dbenv,
	    dbenv->lk_handle, "DB_ENV->lock_id", DB_INIT_LOCK);

	rep_check = IS_ENV_REPLICATED(dbenv) ? 1 : 0;
	if (rep_check)
		__env_rep_enter(dbenv);
	ret = __lock_id(dbenv, idp);
	if (rep_check)
		__env_rep_exit(dbenv);
	return (ret);
}

/*
 * __lock_id --
 *	DB_ENV->lock_id.
 *
 * PUBLIC: int  __lock_id __P((DB_ENV *, u_int32_t *));
 */
int
__lock_id(dbenv, idp)
	DB_ENV *dbenv;
	u_int32_t *idp;
{
	DB_LOCKER *lk;
	DB_LOCKTAB *lt;
	DB_LOCKREGION *region;
	u_int32_t *ids, locker_ndx;
	int nids, ret;

	lt = dbenv->lk_handle;
	region = lt->reginfo.primary;
	ret = 0;

	/*
	 * Allocate a new lock id.  If we wrap around then we
	 * find the minimum currently in use and make sure we
	 * can stay below that.  This code is similar to code
	 * in __txn_begin_int for recovering txn ids.
	 */
	LOCKREGION(dbenv, lt);
	/*
	 * Our current valid range can span the maximum valid value, so check
	 * for it and wrap manually.
	 */
	if (region->stat.st_id == DB_LOCK_MAXID &&
	    region->stat.st_cur_maxid != DB_LOCK_MAXID)
		region->stat.st_id = DB_LOCK_INVALIDID;
	if (region->stat.st_id == region->stat.st_cur_maxid) {
		if ((ret = __os_malloc(dbenv,
		    sizeof(u_int32_t) * region->stat.st_nlockers, &ids)) != 0)
			goto err;
		nids = 0;
		for (lk = SH_TAILQ_FIRST(&region->lockers, __db_locker);
		    lk != NULL;
		    lk = SH_TAILQ_NEXT(lk, ulinks, __db_locker))
			ids[nids++] = lk->id;
		region->stat.st_id = DB_LOCK_INVALIDID;
		region->stat.st_cur_maxid = DB_LOCK_MAXID;
		if (nids != 0)
			__db_idspace(ids, nids,
			    &region->stat.st_id, &region->stat.st_cur_maxid);
		__os_free(dbenv, ids);
	}
	*idp = ++region->stat.st_id;

	/* Allocate a locker for this id. */
	LOCKER_LOCK(lt, region, *idp, locker_ndx);
	ret = __lock_getlocker(lt, *idp, locker_ndx, 1, &lk);

err:	UNLOCKREGION(dbenv, lt);

	return (ret);
}

/*
 * __lock_id_free_pp --
 *	DB_ENV->lock_id_free pre/post processing.
 *
 * PUBLIC: int __lock_id_free_pp __P((DB_ENV *, u_int32_t));
 */
int
__lock_id_free_pp(dbenv, id)
	DB_ENV *dbenv;
	u_int32_t id;
{
	int rep_check, ret;

	PANIC_CHECK(dbenv);
	ENV_REQUIRES_CONFIG(dbenv,
	    dbenv->lk_handle, "DB_ENV->lock_id_free", DB_INIT_LOCK);

	rep_check = IS_ENV_REPLICATED(dbenv) ? 1 : 0;
	if (rep_check)
		__env_rep_enter(dbenv);
	ret = __lock_id_free(dbenv, id);
	if (rep_check)
		__env_rep_exit(dbenv);
	return (ret);
}

/*
 * __lock_id_free --
 *	Free a locker id.
 *
 * PUBLIC: int  __lock_id_free __P((DB_ENV *, u_int32_t));
 */
int
__lock_id_free(dbenv, id)
	DB_ENV *dbenv;
	u_int32_t id;
{
	DB_LOCKER *sh_locker;
	DB_LOCKTAB *lt;
	DB_LOCKREGION *region;
	u_int32_t locker_ndx;
	int ret;

	PANIC_CHECK(dbenv);
	ENV_REQUIRES_CONFIG(dbenv,
	    dbenv->lk_handle, "DB_ENV->lock_id_free", DB_INIT_LOCK);

	lt = dbenv->lk_handle;
	region = lt->reginfo.primary;

	LOCKREGION(dbenv, lt);
	LOCKER_LOCK(lt, region, id, locker_ndx);
	if ((ret =
	    __lock_getlocker(lt, id, locker_ndx, 0, &sh_locker)) != 0)
		goto err;
	if (sh_locker == NULL) {
		ret = EINVAL;
		goto err;
	}

	if (sh_locker->nlocks != 0) {
		__db_err(dbenv, "Locker still has locks");
		ret = EINVAL;
		goto err;
	}

	__lock_freelocker(lt, region, sh_locker, locker_ndx);

err:	UNLOCKREGION(dbenv, lt);
	return (ret);
}

/*
 * __lock_vec_pp --
 *	DB_ENV->lock_vec pre/post processing.
 *
 * PUBLIC: int __lock_vec_pp __P((DB_ENV *,
 * PUBLIC:     u_int32_t, u_int32_t, DB_LOCKREQ *, int, DB_LOCKREQ **));
 */
int
__lock_vec_pp(dbenv, locker, flags, list, nlist, elistp)
	DB_ENV *dbenv;
	u_int32_t locker, flags;
	int nlist;
	DB_LOCKREQ *list, **elistp;
{
	int rep_check, ret;

	PANIC_CHECK(dbenv);
	ENV_REQUIRES_CONFIG(dbenv,
	    dbenv->lk_handle, "DB_ENV->lock_vec", DB_INIT_LOCK);

	/* Validate arguments. */
	if ((ret = __db_fchk(dbenv,
	     "DB_ENV->lock_vec", flags, DB_LOCK_NOWAIT)) != 0)
		return (ret);

	rep_check = IS_ENV_REPLICATED(dbenv) ? 1 : 0;
	if (rep_check)
		__env_rep_enter(dbenv);
	ret = __lock_vec(dbenv, locker, flags, list, nlist, elistp);
	if (rep_check)
		__env_rep_exit(dbenv);
	return (ret);
}

/*
 * __lock_vec --
 *	DB_ENV->lock_vec.
 *
 *	Vector lock routine.  This function takes a set of operations
 *	and performs them all at once.  In addition, lock_vec provides
 *	functionality for lock inheritance, releasing all locks for a
 *	given locker (used during transaction commit/abort), releasing
 *	all locks on a given object, and generating debugging information.
 *
 * PUBLIC: int __lock_vec __P((DB_ENV *,
 * PUBLIC:     u_int32_t, u_int32_t, DB_LOCKREQ *, int, DB_LOCKREQ **));
 */
int
__lock_vec(dbenv, locker, flags, list, nlist, elistp)
	DB_ENV *dbenv;
	u_int32_t locker, flags;
	int nlist;
	DB_LOCKREQ *list, **elistp;
{
	struct __db_lock *lp, *next_lock;
	DB_LOCK lock;
	DB_LOCKER *sh_locker;
	DB_LOCKOBJ *sh_obj;
	DB_LOCKREGION *region;
	DB_LOCKTAB *lt;
	DBT *objlist, *np;
	u_int32_t lndx, ndx;
	int did_abort, i, ret, run_dd, upgrade, writes;

	/* Check if locks have been globally turned off. */
	if (F_ISSET(dbenv, DB_ENV_NOLOCKING))
		return (0);

	lt = dbenv->lk_handle;
	region = lt->reginfo.primary;

	run_dd = 0;
	LOCKREGION(dbenv, (DB_LOCKTAB *)dbenv->lk_handle);
	for (i = 0, ret = 0; i < nlist && ret == 0; i++)
		switch (list[i].op) {
		case DB_LOCK_GET_TIMEOUT:
			LF_SET(DB_LOCK_SET_TIMEOUT);
		case DB_LOCK_GET:
			if (IS_RECOVERING(dbenv)) {
				LOCK_INIT(list[i].lock);
				break;
			}
			ret = __lock_get_internal(dbenv->lk_handle,
			    locker, flags, list[i].obj,
			    list[i].mode, list[i].timeout, &list[i].lock);
			break;
		case DB_LOCK_INHERIT:
			ret = __lock_inherit_locks(lt, locker, flags);
			break;
		case DB_LOCK_PUT:
			ret = __lock_put_nolock(dbenv,
			    &list[i].lock, &run_dd, flags);
			break;
		case DB_LOCK_PUT_ALL:
		case DB_LOCK_PUT_READ:
		case DB_LOCK_UPGRADE_WRITE:
			/*
			 * Get the locker and mark it as deleted.  This
			 * allows us to traverse the locker links without
			 * worrying that someone else is deleting locks out
			 * from under us.  Since the locker may hold no
			 * locks (i.e., you could call abort before you've
			 * done any work), it's perfectly reasonable for there
			 * to be no locker; this is not an error.
			 */
			LOCKER_LOCK(lt, region, locker, ndx);
			if ((ret = __lock_getlocker(lt,
			    locker, ndx, 0, &sh_locker)) != 0 ||
			    sh_locker == NULL ||
			    F_ISSET(sh_locker, DB_LOCKER_DELETED))
				/*
				 * If ret is set, then we'll generate an
				 * error.  If it's not set, we have nothing
				 * to do.
				 */
				break;
			upgrade = 0;
			writes = 1;
			if (list[i].op == DB_LOCK_PUT_READ)
				writes = 0;
			else if (list[i].op == DB_LOCK_UPGRADE_WRITE) {
				if (F_ISSET(sh_locker, DB_LOCKER_DIRTY))
					upgrade = 1;
				writes = 0;
			}
			objlist = list[i].obj;
			if (objlist != NULL) {
				/*
				 * We know these should be ilocks,
				 * but they could be something else,
				 * so allocate room for the size too.
				 */
				objlist->size = 
				     sh_locker->nwrites * sizeof(DBT);
				if ((ret = __os_malloc(dbenv,
				     objlist->size, &objlist->data)) != 0)
					goto up_done;
				memset(objlist->data, 0, objlist->size);
				np = (DBT *) objlist->data;
			} else
				np = NULL;

			F_SET(sh_locker, DB_LOCKER_DELETED);

			/* Now traverse the locks, releasing each one. */
			for (lp = SH_LIST_FIRST(&sh_locker->heldby, __db_lock);
			    lp != NULL; lp = next_lock) {
				sh_obj = (DB_LOCKOBJ *)
				    ((u_int8_t *)lp + lp->obj);
				next_lock = SH_LIST_NEXT(lp,
				    locker_links, __db_lock);
				if (writes == 1 ||
				    lp->mode == DB_LOCK_READ ||
				    lp->mode == DB_LOCK_DIRTY) {
					SH_LIST_REMOVE(lp,
					    locker_links, __db_lock);
					sh_obj = (DB_LOCKOBJ *)
					    ((u_int8_t *)lp + lp->obj);
					SHOBJECT_LOCK(lt, region, sh_obj, lndx);
					/*
					 * We are not letting lock_put_internal
					 * unlink the lock, so we'll have to
					 * update counts here.
					 */
					sh_locker->nlocks--;
					if (IS_WRITELOCK(lp->mode))
						sh_locker->nwrites--;
					ret = __lock_put_internal(lt, lp,
					    lndx, DB_LOCK_FREE | DB_LOCK_DOALL);
					if (ret != 0)
						break;
					continue;
				}
				if (objlist != NULL) {
					DB_ASSERT((char *)np <
					     (char *)objlist->data +
					     objlist->size);
					np->data = SH_DBT_PTR(&sh_obj->lockobj);
					np->size =  sh_obj->lockobj.size;
					np++;
				}
			}
			if (ret != 0)
				goto up_done;

			if (objlist != NULL)
				if ((ret = __lock_fix_list(dbenv,
				     objlist, sh_locker->nwrites)) != 0)
					goto up_done;
			switch (list[i].op) {
			case DB_LOCK_UPGRADE_WRITE:
				if (upgrade != 1)
					goto up_done;
				for (lp = SH_LIST_FIRST(
				    &sh_locker->heldby, __db_lock);
				    lp != NULL;
				    lp = SH_LIST_NEXT(lp,
					    locker_links, __db_lock)) {
					if (lp->mode != DB_LOCK_WWRITE)
						continue;
					lock.off = R_OFFSET(&lt->reginfo, lp);
					lock.gen = lp->gen;
					F_SET(sh_locker, DB_LOCKER_INABORT);
					if ((ret = __lock_get_internal(lt,
					    locker, DB_LOCK_UPGRADE,
					    NULL, DB_LOCK_WRITE, 0, &lock)) !=0)
						break;
				}
			up_done:
				/* FALL THROUGH */
			case DB_LOCK_PUT_READ:
			case DB_LOCK_PUT_ALL:
				F_CLR(sh_locker, DB_LOCKER_DELETED);
				break;
			default:
				break;
			}
			break;
		case DB_LOCK_PUT_OBJ:
			/* Remove all the locks associated with an object. */
			OBJECT_LOCK(lt, region, list[i].obj, ndx);
			if ((ret = __lock_getobj(lt, list[i].obj,
			    ndx, 0, &sh_obj)) != 0 || sh_obj == NULL) {
				if (ret == 0)
					ret = EINVAL;
				break;
			}

			/*
			 * Go through both waiters and holders.  Don't bother
			 * to run promotion, because everyone is getting
			 * released.  The processes waiting will still get
			 * awakened as their waiters are released.
			 */
			for (lp = SH_TAILQ_FIRST(&sh_obj->waiters, __db_lock);
			    ret == 0 && lp != NULL;
			    lp = SH_TAILQ_FIRST(&sh_obj->waiters, __db_lock))
				ret = __lock_put_internal(lt, lp, ndx,
				    DB_LOCK_UNLINK |
				    DB_LOCK_NOPROMOTE | DB_LOCK_DOALL);

			/*
			 * On the last time around, the object will get
			 * reclaimed by __lock_put_internal, structure the
			 * loop carefully so we do not get bitten.
			 */
			for (lp = SH_TAILQ_FIRST(&sh_obj->holders, __db_lock);
			    ret == 0 && lp != NULL;
			    lp = next_lock) {
				next_lock = SH_TAILQ_NEXT(lp, links, __db_lock);
				ret = __lock_put_internal(lt, lp, ndx,
				    DB_LOCK_UNLINK |
				    DB_LOCK_NOPROMOTE | DB_LOCK_DOALL);
			}
			break;

		case DB_LOCK_TIMEOUT:
			ret = __lock_set_timeout_internal(dbenv,
			    locker, 0, DB_SET_TXN_NOW);
			break;

		case DB_LOCK_TRADE:
			/*
			 * INTERNAL USE ONLY.
			 * Change the holder of the lock described in
			 * list[i].lock to the locker-id specified by
			 * the locker parameter.
			 */
			/*
			 * You had better know what you're doing here.
			 * We are trading locker-id's on a lock to
			 * facilitate file locking on open DB handles.
			 * We do not do any conflict checking on this,
			 * so heaven help you if you use this flag under
			 * any other circumstances.
			 */
			ret = __lock_trade(dbenv, &list[i].lock, locker);
			break;
#ifdef DEBUG
		case DB_LOCK_DUMP:
			/* Find the locker. */
			LOCKER_LOCK(lt, region, locker, ndx);
			if ((ret = __lock_getlocker(lt,
			    locker, ndx, 0, &sh_locker)) != 0 ||
			    sh_locker == NULL ||
			    F_ISSET(sh_locker, DB_LOCKER_DELETED))
				break;

			for (lp = SH_LIST_FIRST(&sh_locker->heldby, __db_lock);
			    lp != NULL;
			    lp = SH_LIST_NEXT(lp, locker_links, __db_lock)) {
				__lock_printlock(lt, lp, 1, NULL);
			}
			break;
#endif
		default:
			__db_err(dbenv,
			    "Invalid lock operation: %d", list[i].op);
			ret = EINVAL;
			break;
		}

	if (ret == 0 && region->detect != DB_LOCK_NORUN &&
	     (region->need_dd || LOCK_TIME_ISVALID(&region->next_timeout)))
		run_dd = 1;
	UNLOCKREGION(dbenv, (DB_LOCKTAB *)dbenv->lk_handle);

	if (run_dd)
		(void)__lock_detect(dbenv, region->detect, &did_abort);

	if (ret != 0 && elistp != NULL)
		*elistp = &list[i - 1];

	return (ret);
}

/*
 * __lock_get_pp --
 *	DB_ENV->lock_get pre/post processing.
 *
 * PUBLIC: int __lock_get_pp __P((DB_ENV *,
 * PUBLIC:     u_int32_t, u_int32_t, const DBT *, db_lockmode_t, DB_LOCK *));
 */
int
__lock_get_pp(dbenv, locker, flags, obj, lock_mode, lock)
	DB_ENV *dbenv;
	u_int32_t locker, flags;
	const DBT *obj;
	db_lockmode_t lock_mode;
	DB_LOCK *lock;
{
	int rep_check, ret;

	PANIC_CHECK(dbenv);
	ENV_REQUIRES_CONFIG(dbenv,
	    dbenv->lk_handle, "DB_ENV->lock_get", DB_INIT_LOCK);

	/* Validate arguments. */
	if ((ret = __db_fchk(dbenv, "DB_ENV->lock_get", flags,
	    DB_LOCK_NOWAIT | DB_LOCK_UPGRADE | DB_LOCK_SWITCH)) != 0)
		return (ret);

	rep_check = IS_ENV_REPLICATED(dbenv) ? 1 : 0;
	if (rep_check)
		__env_rep_enter(dbenv);
	ret = __lock_get(dbenv, locker, flags, obj, lock_mode, lock);
	if (rep_check)
		__env_rep_exit(dbenv);
	return (ret);
}

/*
 * __lock_get --
 *	DB_ENV->lock_get.
 *
 * PUBLIC: int __lock_get __P((DB_ENV *,
 * PUBLIC:     u_int32_t, u_int32_t, const DBT *, db_lockmode_t, DB_LOCK *));
 */
int
__lock_get(dbenv, locker, flags, obj, lock_mode, lock)
	DB_ENV *dbenv;
	u_int32_t locker, flags;
	const DBT *obj;
	db_lockmode_t lock_mode;
	DB_LOCK *lock;
{
	int ret;

	if (IS_RECOVERING(dbenv)) {
		LOCK_INIT(*lock);
		return (0);
	}

	LOCKREGION(dbenv, (DB_LOCKTAB *)dbenv->lk_handle);
	ret = __lock_get_internal(dbenv->lk_handle,
	    locker, flags, obj, lock_mode, 0, lock);
	UNLOCKREGION(dbenv, (DB_LOCKTAB *)dbenv->lk_handle);
	return (ret);
}

/*
 * __lock_get_internal --
 *
 * All the work for lock_get (and for the GET option of lock_vec) is done
 * inside of lock_get_internal.
 */
static int
__lock_get_internal(lt, locker, flags, obj, lock_mode, timeout, lock)
	DB_LOCKTAB *lt;
	u_int32_t locker, flags;
	const DBT *obj;
	db_lockmode_t lock_mode;
	db_timeout_t timeout;
	DB_LOCK *lock;
{
	struct __db_lock *newl, *lp, *wwrite;
	DB_ENV *dbenv;
	DB_LOCKER *sh_locker;
	DB_LOCKOBJ *sh_obj;
	DB_LOCKREGION *region;
	u_int32_t holder, locker_ndx, obj_ndx;
	int did_abort, ihold, grant_dirty, no_dd, ret, t_ret;
	/*
	 * We decide what action to take based on what
	 * locks are already held and what locks are
	 * in the wait queue.
	 */
	enum {
		GRANT,		/* Grant the lock. */
		UPGRADE,	/* Upgrade the lock. */
		HEAD,		/* Wait at head of wait queue. */
		SECOND,		/* Wait as the second waiter. */
		TAIL		/* Wait at tail of the wait queue. */
	} action;

	dbenv = lt->dbenv;
	region = lt->reginfo.primary;

	/* Check if locks have been globally turned off. */
	if (F_ISSET(dbenv, DB_ENV_NOLOCKING))
		return (0);

	no_dd = ret = 0;
	newl = NULL;

	/*
	 * If we are not going to reuse this lock, invalidate it
	 * so that if we fail it will not look like a valid lock.
	 */
	if (!LF_ISSET(DB_LOCK_UPGRADE | DB_LOCK_SWITCH))
		LOCK_INIT(*lock);

	/* Check that the lock mode is valid.  */
	if ((u_int32_t)lock_mode >= region->stat.st_nmodes) {
		__db_err(dbenv, "DB_ENV->lock_get: invalid lock mode %lu",
		    (u_long)lock_mode);
		return (EINVAL);
	}
	region->stat.st_nrequests++;

	if (obj == NULL) {
		DB_ASSERT(LOCK_ISSET(*lock));
		lp = (struct __db_lock *)R_ADDR(&lt->reginfo, lock->off);
		sh_obj = (DB_LOCKOBJ *) ((u_int8_t *)lp + lp->obj);
	} else {
		/* Allocate a shared memory new object. */
		OBJECT_LOCK(lt, region, obj, lock->ndx);
		if ((ret = __lock_getobj(lt, obj, lock->ndx, 1, &sh_obj)) != 0)
			goto err;
	}

	/* Get the locker, we may need it to find our parent. */
	LOCKER_LOCK(lt, region, locker, locker_ndx);
	if ((ret = __lock_getlocker(lt, locker,
	    locker_ndx, locker > DB_LOCK_MAXID ? 1 : 0, &sh_locker)) != 0) {
		/*
		 * XXX We cannot tell if we created the object or not,
		 * so we don't kow if we should free it or not.
		 */
		goto err;
	}

	if (sh_locker == NULL) {
		__db_err(dbenv, "Locker does not exist");
		ret = EINVAL;
		goto err;
	}

	/*
	 * Figure out if we can grant this lock or if it should wait.
	 * By default, we can grant the new lock if it does not conflict with
	 * anyone on the holders list OR anyone on the waiters list.
	 * The reason that we don't grant if there's a conflict is that
	 * this can lead to starvation (a writer waiting on a popularly
	 * read item will never be granted).  The downside of this is that
	 * a waiting reader can prevent an upgrade from reader to writer,
	 * which is not uncommon.
	 *
	 * There are two exceptions to the no-conflict rule.  First, if
	 * a lock is held by the requesting locker AND the new lock does
	 * not conflict with any other holders, then we grant the lock.
	 * The most common place this happens is when the holder has a
	 * WRITE lock and a READ lock request comes in for the same locker.
	 * If we do not grant the read lock, then we guarantee deadlock.
	 * Second, dirty readers are granted if at all possible while
	 * avoiding starvation, see below.
	 *
	 * In case of conflict, we put the new lock on the end of the waiters
	 * list, unless we are upgrading or this is a dirty reader in which
	 * case the locker goes at or near the front of the list.
	 */
	ihold = 0;
	grant_dirty = 0;
	holder = 0;
	wwrite = NULL;

	/*
	 * SWITCH is a special case, used by the queue access method
	 * when we want to get an entry which is past the end of the queue.
	 * We have a DB_READ_LOCK and need to switch it to DB_LOCK_WAIT and
	 * join the waiters queue.  This must be done as a single operation
	 * so that another locker cannot get in and fail to wake us up.
	 */
	if (LF_ISSET(DB_LOCK_SWITCH))
		lp = NULL;
	else
		lp = SH_TAILQ_FIRST(&sh_obj->holders, __db_lock);
	for (; lp != NULL; lp = SH_TAILQ_NEXT(lp, links, __db_lock)) {
		if (locker == lp->holder) {
			if (lp->mode == lock_mode &&
			    lp->status == DB_LSTAT_HELD) {
				if (LF_ISSET(DB_LOCK_UPGRADE))
					goto upgrade;

				/*
				 * Lock is held, so we can increment the
				 * reference count and return this lock
				 * to the caller.  We do not count reference
				 * increments towards the locks held by
				 * the locker.
				 */
				lp->refcount++;
				lock->off = R_OFFSET(&lt->reginfo, lp);
				lock->gen = lp->gen;
				lock->mode = lp->mode;
				goto done;
			} else {
				ihold = 1;
				if (lock_mode == DB_LOCK_WRITE &&
				    lp->mode == DB_LOCK_WWRITE)
					wwrite = lp;
			}
		} else if (__lock_is_parent(lt, lp->holder, sh_locker))
			ihold = 1;
		else if (CONFLICTS(lt, region, lp->mode, lock_mode))
			break;
		else if (lp->mode == DB_LOCK_READ ||
		     lp->mode == DB_LOCK_WWRITE) {
			grant_dirty = 1;
			holder = lp->holder;
		}
	}

	/*
	 * If there are conflicting holders we will have to wait.
	 * An upgrade or dirty reader goes to the head
	 * of the queue, everone else to the back.
	 */
	if (lp != NULL) {
		if (LF_ISSET(DB_LOCK_UPGRADE) ||
		     wwrite != NULL || lock_mode == DB_LOCK_DIRTY)
			action = HEAD;
		else
			action = TAIL;
	} else {
		if (LF_ISSET(DB_LOCK_SWITCH))
			action = TAIL;
		else if (LF_ISSET(DB_LOCK_UPGRADE) || wwrite != NULL)
			action = UPGRADE;
		else  if (ihold)
			action = GRANT;
		else {
			/*
			 * Look for conflicting waiters.
			 */
			for (lp = SH_TAILQ_FIRST(&sh_obj->waiters, __db_lock);
			    lp != NULL;
			    lp = SH_TAILQ_NEXT(lp, links, __db_lock)) {
				if (CONFLICTS(lt, region, lp->mode,
				     lock_mode) && locker != lp->holder)
					break;
			}
			/*
			 * If there are no conflicting holders or waiters,
			 * then we grant. Normally when we wait, we
			 * wait at the end (TAIL).  However, the goal of
			 * DIRTY_READ locks to allow forward progress in the
			 * face of updating transactions, so we try to allow
			 * all DIRTY_READ requests to proceed as rapidly
			 * as possible, so long as we can prevent starvation.
			 *
			 * When determining how to queue a DIRTY_READ
			 * request:
			 *
			 *	1. If there is a waiting upgrading writer,
			 *	   then we enqueue the dirty reader BEHIND it
			 *	   (second in the queue).
			 *	2. Else, if the current holders are either
			 *	   READ or WWRITE, we grant
			 *	3. Else queue SECOND i.e., behind the first
			 *	   waiter.
			 *
			 * The end result is that dirty_readers get to run
			 * so long as other lockers are blocked.  Once
			 * there is a locker which is only waiting on
			 * dirty readers then they queue up behind that
			 * locker so that it gets to run.  In general
			 * this locker will be a WRITE which will shortly
			 * get downgraded to a WWRITE, permitting the
			 * DIRTY locks to be granted.
			 */
			if (lp == NULL)
				action = GRANT;
			else if (lock_mode == DB_LOCK_DIRTY && grant_dirty) {
				/*
				 * An upgrade will be at the head of the
				 * queue.
				 */
				lp = SH_TAILQ_FIRST(
				     &sh_obj->waiters, __db_lock);
				if (lp->mode == DB_LOCK_WRITE &&
				     lp->holder == holder)
					action = SECOND;
				else
					action = GRANT;
			} else if (lock_mode == DB_LOCK_DIRTY)
				action = SECOND;
			else
				action = TAIL;
		}
	}

	switch (action) {
	case HEAD:
	case TAIL:
	case SECOND:
	case GRANT:
		/* Allocate a new lock. */
		if (++region->stat.st_nlocks > region->stat.st_maxnlocks)
			region->stat.st_maxnlocks = region->stat.st_nlocks;

		if ((newl =
		    SH_TAILQ_FIRST(&region->free_locks, __db_lock)) != NULL)
			SH_TAILQ_REMOVE(
			    &region->free_locks, newl, links, __db_lock);
		if (newl == NULL) {
			__db_err(dbenv, __db_lock_err, "locks");
			return (ENOMEM);
		}
		newl->holder = locker;
		newl->refcount = 1;
		newl->mode = lock_mode;
		newl->obj = SH_PTR_TO_OFF(newl, sh_obj);
		/*
		 * Now, insert the lock onto its locker's list.
		 * If the locker does not currently hold any locks,
		 * there's no reason to run a deadlock
		 * detector, save that information.
		 */
		no_dd = sh_locker->master_locker == INVALID_ROFF &&
		    SH_LIST_FIRST(
		    &sh_locker->child_locker, __db_locker) == NULL &&
		    SH_LIST_FIRST(&sh_locker->heldby, __db_lock) == NULL;

		SH_LIST_INSERT_HEAD(
		    &sh_locker->heldby, newl, locker_links, __db_lock);
		break;

	case UPGRADE:
upgrade:
		if (wwrite != NULL) {
			lp = wwrite;
			lp->refcount++;
			lock->off = R_OFFSET(&lt->reginfo, lp);
			lock->gen = lp->gen;
			lock->mode = lock_mode;
		}
		else
			lp =
			    (struct __db_lock *)R_ADDR(&lt->reginfo, lock->off);
		if (IS_WRITELOCK(lock_mode) && !IS_WRITELOCK(lp->mode))
			sh_locker->nwrites++;
		lp->mode = lock_mode;
		goto done;
	}

	switch (action) {
	case UPGRADE:
		DB_ASSERT(0);
		break;
	case GRANT:
		newl->status = DB_LSTAT_HELD;
		SH_TAILQ_INSERT_TAIL(&sh_obj->holders, newl, links);
		break;
	case HEAD:
	case TAIL:
	case SECOND:
		if (LF_ISSET(DB_LOCK_NOWAIT)) {
			ret = DB_LOCK_NOTGRANTED;
			region->stat.st_nnowaits++;
			goto err;
		}
		if ((lp = SH_TAILQ_FIRST(&sh_obj->waiters, __db_lock)) == NULL)
			SH_TAILQ_INSERT_HEAD(&region->dd_objs,
				    sh_obj, dd_links, __db_lockobj);
		switch (action) {
		case HEAD:
			SH_TAILQ_INSERT_HEAD(
			     &sh_obj->waiters, newl, links, __db_lock);
			break;
		case SECOND:
			SH_TAILQ_INSERT_AFTER(
			     &sh_obj->waiters, lp, newl, links, __db_lock);
			break;
		case TAIL:
			SH_TAILQ_INSERT_TAIL(&sh_obj->waiters, newl, links);
			break;
		default:
			DB_ASSERT(0);
		}

		/* If we are switching drop the lock we had. */
		if (LF_ISSET(DB_LOCK_SWITCH) &&
		    (ret = __lock_put_nolock(dbenv,
		    lock, &ihold, DB_LOCK_NOWAITERS)) != 0) {
			__lock_remove_waiter(lt, sh_obj, newl, DB_LSTAT_FREE);
			goto err;
		}

		/*
		 * This is really a blocker for the thread.  It should be
		 * initialized locked, so that when we try to acquire it, we
		 * block.
		 */
		newl->status = DB_LSTAT_WAITING;
		region->stat.st_nconflicts++;
		region->need_dd = 1;
		/*
		 * First check to see if this txn has expired.
		 * If not then see if the lock timeout is past
		 * the expiration of the txn, if it is, use
		 * the txn expiration time.  lk_expire is passed
		 * to avoid an extra call to get the time.
		 */
		if (__lock_expired(dbenv,
		    &sh_locker->lk_expire, &sh_locker->tx_expire)) {
			newl->status = DB_LSTAT_EXPIRED;
			sh_locker->lk_expire = sh_locker->tx_expire;

			/* We are done. */
			goto expired;
		}

		/*
		 * If a timeout was specified in this call then it
		 * takes priority.  If a lock timeout has been specified
		 * for this transaction then use that, otherwise use
		 * the global timeout value.
		 */
		if (!LF_ISSET(DB_LOCK_SET_TIMEOUT)) {
			if (F_ISSET(sh_locker, DB_LOCKER_TIMEOUT))
				timeout = sh_locker->lk_timeout;
			else
				timeout = region->lk_timeout;
		}
		if (timeout != 0)
			__lock_expires(dbenv, &sh_locker->lk_expire, timeout);
		else
			LOCK_SET_TIME_INVALID(&sh_locker->lk_expire);

		if (LOCK_TIME_ISVALID(&sh_locker->tx_expire) &&
			(timeout == 0 || __lock_expired(dbenv,
			    &sh_locker->lk_expire, &sh_locker->tx_expire)))
				sh_locker->lk_expire = sh_locker->tx_expire;
		if (LOCK_TIME_ISVALID(&sh_locker->lk_expire) &&
		    (!LOCK_TIME_ISVALID(&region->next_timeout) ||
		    LOCK_TIME_GREATER(
		    &region->next_timeout, &sh_locker->lk_expire)))
			region->next_timeout = sh_locker->lk_expire;
		UNLOCKREGION(dbenv, (DB_LOCKTAB *)dbenv->lk_handle);

		/*
		 * We are about to wait; before waiting, see if the deadlock
		 * detector should be run.
		 */
		if (region->detect != DB_LOCK_NORUN && !no_dd)
			(void)__lock_detect(dbenv, region->detect, &did_abort);

		MUTEX_LOCK(dbenv, &newl->mutex);
		LOCKREGION(dbenv, (DB_LOCKTAB *)dbenv->lk_handle);

		/* Turn off lock timeout. */
		if (newl->status != DB_LSTAT_EXPIRED)
			LOCK_SET_TIME_INVALID(&sh_locker->lk_expire);

		if (newl->status != DB_LSTAT_PENDING) {
			switch (newl->status) {
			case DB_LSTAT_ABORTED:
				ret = DB_LOCK_DEADLOCK;
				break;
			case DB_LSTAT_NOTEXIST:
				ret = DB_LOCK_NOTEXIST;
				break;
			case DB_LSTAT_EXPIRED:
expired:			SHOBJECT_LOCK(lt, region, sh_obj, obj_ndx);
				if ((ret = __lock_put_internal(
				    lt, newl, obj_ndx,
				    DB_LOCK_UNLINK | DB_LOCK_FREE) != 0))
					goto err;
				if (LOCK_TIME_EQUAL(
				    &sh_locker->lk_expire,
				    &sh_locker->tx_expire)) {
					region->stat.st_ntxntimeouts++;
					return (DB_LOCK_NOTGRANTED);
				} else {
					region->stat.st_nlocktimeouts++;
					return (DB_LOCK_NOTGRANTED);
				}
			default:
				ret = EINVAL;
				break;
			}
			goto err;
		} else if (LF_ISSET(DB_LOCK_UPGRADE)) {
			/*
			 * The lock that was just granted got put on the
			 * holders list.  Since we're upgrading some other
			 * lock, we've got to remove it here.
			 */
			SH_TAILQ_REMOVE(
			    &sh_obj->holders, newl, links, __db_lock);
			/*
			 * Ensure that the object is not believed to be on
			 * the object's lists, if we're traversing by locker.
			 */
			newl->links.stqe_prev = -1;
			goto upgrade;
		} else
			newl->status = DB_LSTAT_HELD;
	}

	lock->off = R_OFFSET(&lt->reginfo, newl);
	lock->gen = newl->gen;
	lock->mode = newl->mode;
	sh_locker->nlocks++;
	if (IS_WRITELOCK(newl->mode))
		sh_locker->nwrites++;

	return (0);

done:
	ret = 0;
err:
	if (newl != NULL &&
	     (t_ret = __lock_freelock(lt, newl, locker,
	     DB_LOCK_FREE | DB_LOCK_UNLINK)) != 0 && ret == 0)
		ret = t_ret;
	return (ret);
}

/*
 * __lock_put_pp --
 *	DB_ENV->lock_put pre/post processing.
 *
 * PUBLIC: int  __lock_put_pp __P((DB_ENV *, DB_LOCK *));
 */
int
__lock_put_pp(dbenv, lock)
	DB_ENV *dbenv;
	DB_LOCK *lock;
{
	int rep_check, ret;

	PANIC_CHECK(dbenv);
	ENV_REQUIRES_CONFIG(dbenv,
	    dbenv->lk_handle, "DB_LOCK->lock_put", DB_INIT_LOCK);

	rep_check = IS_ENV_REPLICATED(dbenv) ? 1 : 0;
	if (rep_check)
		__env_rep_enter(dbenv);
	ret = __lock_put(dbenv, lock);
	if (rep_check)
		__env_rep_exit(dbenv);
	return (ret);
}

/*
 * __lock_put --
 *	DB_ENV->lock_put.
 *
 * PUBLIC: int  __lock_put __P((DB_ENV *, DB_LOCK *));
 */
int
__lock_put(dbenv, lock)
	DB_ENV *dbenv;
	DB_LOCK *lock;
{
	DB_LOCKTAB *lt;
	int ret, run_dd;

	if (IS_RECOVERING(dbenv))
		return (0);

	lt = dbenv->lk_handle;

	LOCKREGION(dbenv, lt);
	ret = __lock_put_nolock(dbenv, lock, &run_dd, 0);
	UNLOCKREGION(dbenv, lt);

	/*
	 * Only run the lock detector if put told us to AND we are running
	 * in auto-detect mode.  If we are not running in auto-detect, then
	 * a call to lock_detect here will 0 the need_dd bit, but will not
	 * actually abort anything.
	 */
	if (ret == 0 && run_dd)
		(void)__lock_detect(dbenv,
		    ((DB_LOCKREGION *)lt->reginfo.primary)->detect, NULL);
	return (ret);
}

static int
__lock_put_nolock(dbenv, lock, runp, flags)
	DB_ENV *dbenv;
	DB_LOCK *lock;
	int *runp;
	u_int32_t flags;
{
	struct __db_lock *lockp;
	DB_LOCKREGION *region;
	DB_LOCKTAB *lt;
	int ret;

	/* Check if locks have been globally turned off. */
	if (F_ISSET(dbenv, DB_ENV_NOLOCKING))
		return (0);

	lt = dbenv->lk_handle;
	region = lt->reginfo.primary;

	lockp = (struct __db_lock *)R_ADDR(&lt->reginfo, lock->off);
	LOCK_INIT(*lock);
	if (lock->gen != lockp->gen) {
		__db_err(dbenv, __db_lock_invalid, "DB_LOCK->lock_put");
		return (EINVAL);
	}

	ret = __lock_put_internal(lt,
	    lockp, lock->ndx, flags | DB_LOCK_UNLINK | DB_LOCK_FREE);

	*runp = 0;
	if (ret == 0 && region->detect != DB_LOCK_NORUN &&
	     (region->need_dd || LOCK_TIME_ISVALID(&region->next_timeout)))
		*runp = 1;

	return (ret);
}

/*
 * __lock_downgrade --
 *
 * Used to downgrade locks.  Currently this is used in two places: 1) by the
 * Concurrent Data Store product to downgrade write locks back to iwrite locks
 * and 2) to downgrade write-handle locks to read-handle locks at the end of
 * an open/create.
 *
 * PUBLIC: int __lock_downgrade __P((DB_ENV *,
 * PUBLIC:     DB_LOCK *, db_lockmode_t, u_int32_t));
 */
int
__lock_downgrade(dbenv, lock, new_mode, flags)
	DB_ENV *dbenv;
	DB_LOCK *lock;
	db_lockmode_t new_mode;
	u_int32_t flags;
{
	struct __db_lock *lockp;
	DB_LOCKER *sh_locker;
	DB_LOCKOBJ *obj;
	DB_LOCKREGION *region;
	DB_LOCKTAB *lt;
	u_int32_t indx;
	int ret;

	COMPQUIET(flags, 0);

	PANIC_CHECK(dbenv);
	ret = 0;

	/* Check if locks have been globally turned off. */
	if (F_ISSET(dbenv, DB_ENV_NOLOCKING))
		return (0);

	lt = dbenv->lk_handle;
	region = lt->reginfo.primary;

	LOCKREGION(dbenv, lt);

	lockp = (struct __db_lock *)R_ADDR(&lt->reginfo, lock->off);
	if (lock->gen != lockp->gen) {
		__db_err(dbenv, __db_lock_invalid, "lock_downgrade");
		ret = EINVAL;
		goto out;
	}

	LOCKER_LOCK(lt, region, lockp->holder, indx);

	if ((ret = __lock_getlocker(lt, lockp->holder,
	    indx, 0, &sh_locker)) != 0 || sh_locker == NULL) {
		if (ret == 0)
			ret = EINVAL;
		__db_err(dbenv, __db_locker_invalid);
		goto out;
	}
	if (IS_WRITELOCK(lockp->mode) && !IS_WRITELOCK(new_mode))
		sh_locker->nwrites--;

	if (new_mode == DB_LOCK_WWRITE)
		F_SET(sh_locker, DB_LOCKER_DIRTY);

	lockp->mode = new_mode;

	/* Get the object associated with this lock. */
	obj = (DB_LOCKOBJ *)((u_int8_t *)lockp + lockp->obj);
	(void)__lock_promote(lt, obj, LF_ISSET(DB_LOCK_NOWAITERS));

out:	UNLOCKREGION(dbenv, lt);

	return (ret);
}

static int
__lock_put_internal(lt, lockp, obj_ndx, flags)
	DB_LOCKTAB *lt;
	struct __db_lock *lockp;
	u_int32_t obj_ndx, flags;
{
	DB_LOCKOBJ *sh_obj;
	DB_LOCKREGION *region;
	int ret, state_changed;

	region = lt->reginfo.primary;
	ret = state_changed = 0;

	if (!OBJ_LINKS_VALID(lockp)) {
		/*
		 * Someone removed this lock while we were doing a release
		 * by locker id.  We are trying to free this lock, but it's
		 * already been done; all we need to do is return it to the
		 * free list.
		 */
		(void)__lock_freelock(lt, lockp, 0, DB_LOCK_FREE);
		return (0);
	}

	if (LF_ISSET(DB_LOCK_DOALL))
		region->stat.st_nreleases += lockp->refcount;
	else
		region->stat.st_nreleases++;

	if (!LF_ISSET(DB_LOCK_DOALL) && lockp->refcount > 1) {
		lockp->refcount--;
		return (0);
	}

	/* Increment generation number. */
	lockp->gen++;

	/* Get the object associated with this lock. */
	sh_obj = (DB_LOCKOBJ *)((u_int8_t *)lockp + lockp->obj);

	/* Remove this lock from its holders/waitlist. */
	if (lockp->status != DB_LSTAT_HELD && lockp->status != DB_LSTAT_PENDING)
		__lock_remove_waiter(lt, sh_obj, lockp, DB_LSTAT_FREE);
	else {
		SH_TAILQ_REMOVE(&sh_obj->holders, lockp, links, __db_lock);
		lockp->links.stqe_prev = -1;
	}

	if (LF_ISSET(DB_LOCK_NOPROMOTE))
		state_changed = 0;
	else
		state_changed = __lock_promote(lt,
		    sh_obj, LF_ISSET(DB_LOCK_REMOVE | DB_LOCK_NOWAITERS));

	/* Check if object should be reclaimed. */
	if (SH_TAILQ_FIRST(&sh_obj->holders, __db_lock) == NULL &&
	    SH_TAILQ_FIRST(&sh_obj->waiters, __db_lock) == NULL) {
		HASHREMOVE_EL(lt->obj_tab,
		    obj_ndx, __db_lockobj, links, sh_obj);
		if (sh_obj->lockobj.size > sizeof(sh_obj->objdata))
			__db_shalloc_free(lt->reginfo.addr,
			    SH_DBT_PTR(&sh_obj->lockobj));
		SH_TAILQ_INSERT_HEAD(
		    &region->free_objs, sh_obj, links, __db_lockobj);
		region->stat.st_nobjects--;
		state_changed = 1;
	}

	/* Free lock. */
	if (LF_ISSET(DB_LOCK_UNLINK | DB_LOCK_FREE))
		ret = __lock_freelock(lt, lockp, lockp->holder, flags);

	/*
	 * If we did not promote anyone; we need to run the deadlock
	 * detector again.
	 */
	if (state_changed == 0)
		region->need_dd = 1;

	return (ret);
}

/*
 * Utility functions; listed alphabetically.
 */

/*
 * __lock_freelock --
 *	Free a lock.  Unlink it from its locker if necessary.
 *
 */
static int
__lock_freelock(lt, lockp, locker, flags)
	DB_LOCKTAB *lt;
	struct __db_lock *lockp;
	u_int32_t locker, flags;
{
	DB_ENV *dbenv;
	DB_LOCKER *sh_locker;
	DB_LOCKREGION *region;
	u_int32_t indx;
	int ret;

	dbenv = lt->dbenv;
	region = lt->reginfo.primary;
	ret = 0;

	if (LF_ISSET(DB_LOCK_UNLINK)) {
		LOCKER_LOCK(lt, region, locker, indx);
		if ((ret = __lock_getlocker(lt,
		    locker, indx, 0, &sh_locker)) != 0 || sh_locker == NULL) {
			if (ret == 0)
				ret = EINVAL;
			__db_err(dbenv, __db_locker_invalid);
			return (ret);
		}

		SH_LIST_REMOVE(lockp, locker_links, __db_lock);
		if (lockp->status == DB_LSTAT_HELD) {
			sh_locker->nlocks--;
			if (IS_WRITELOCK(lockp->mode))
				sh_locker->nwrites--;
		}
	}

	if (LF_ISSET(DB_LOCK_FREE)) {
		lockp->status = DB_LSTAT_FREE;
		SH_TAILQ_INSERT_HEAD(
		    &region->free_locks, lockp, links, __db_lock);
		region->stat.st_nlocks--;
	}

	return (ret);
}

/*
 * __lock_addfamilylocker
 *	Put a locker entry in for a child transaction.
 *
 * PUBLIC: int __lock_addfamilylocker __P((DB_ENV *, u_int32_t, u_int32_t));
 */
int
__lock_addfamilylocker(dbenv, pid, id)
	DB_ENV *dbenv;
	u_int32_t pid, id;
{
	DB_LOCKER *lockerp, *mlockerp;
	DB_LOCKREGION *region;
	DB_LOCKTAB *lt;
	u_int32_t ndx;
	int ret;

	lt = dbenv->lk_handle;
	region = lt->reginfo.primary;
	LOCKREGION(dbenv, lt);

	/* get/create the  parent locker info */
	LOCKER_LOCK(lt, region, pid, ndx);
	if ((ret = __lock_getlocker(dbenv->lk_handle,
	    pid, ndx, 1, &mlockerp)) != 0)
		goto err;

	/*
	 * We assume that only one thread can manipulate
	 * a single transaction family.
	 * Therefore the master locker cannot go away while
	 * we manipulate it, nor can another child in the
	 * family be created at the same time.
	 */
	LOCKER_LOCK(lt, region, id, ndx);
	if ((ret = __lock_getlocker(dbenv->lk_handle,
	    id, ndx, 1, &lockerp)) != 0)
		goto err;

	/* Point to our parent. */
	lockerp->parent_locker = R_OFFSET(&lt->reginfo, mlockerp);

	/* See if this locker is the family master. */
	if (mlockerp->master_locker == INVALID_ROFF)
		lockerp->master_locker = R_OFFSET(&lt->reginfo, mlockerp);
	else {
		lockerp->master_locker = mlockerp->master_locker;
		mlockerp = R_ADDR(&lt->reginfo, mlockerp->master_locker);
	}

	/*
	 * Link the child at the head of the master's list.
	 * The guess is when looking for deadlock that
	 * the most recent child is the one thats blocked.
	 */
	SH_LIST_INSERT_HEAD(
	    &mlockerp->child_locker, lockerp, child_link, __db_locker);

err:
	UNLOCKREGION(dbenv, lt);

	return (ret);
}

/*
 * __lock_freefamilylocker
 *	Remove a locker from the hash table and its family.
 *
 * This must be called without the locker bucket locked.
 *
 * PUBLIC: int __lock_freefamilylocker  __P((DB_LOCKTAB *, u_int32_t));
 */
int
__lock_freefamilylocker(lt, locker)
	DB_LOCKTAB *lt;
	u_int32_t locker;
{
	DB_ENV *dbenv;
	DB_LOCKER *sh_locker;
	DB_LOCKREGION *region;
	u_int32_t indx;
	int ret;

	dbenv = lt->dbenv;
	region = lt->reginfo.primary;

	LOCKREGION(dbenv, lt);
	LOCKER_LOCK(lt, region, locker, indx);

	if ((ret = __lock_getlocker(lt,
	    locker, indx, 0, &sh_locker)) != 0 || sh_locker == NULL)
		goto err;

	if (SH_LIST_FIRST(&sh_locker->heldby, __db_lock) != NULL) {
		ret = EINVAL;
		__db_err(dbenv, "Freeing locker with locks");
		goto err;
	}

	/* If this is part of a family, we must fix up its links. */
	if (sh_locker->master_locker != INVALID_ROFF)
		SH_LIST_REMOVE(sh_locker, child_link, __db_locker);

	__lock_freelocker(lt, region, sh_locker, indx);

err:
	UNLOCKREGION(dbenv, lt);
	return (ret);
}

/*
 * __lock_freelocker
 *	common code for deleting a locker.
 *
 * This must be called with the locker bucket locked.
 */
static void
__lock_freelocker(lt, region, sh_locker, indx)
	DB_LOCKTAB *lt;
	DB_LOCKREGION *region;
	DB_LOCKER *sh_locker;
	u_int32_t indx;

{
	HASHREMOVE_EL(
	    lt->locker_tab, indx, __db_locker, links, sh_locker);
	SH_TAILQ_INSERT_HEAD(
	    &region->free_lockers, sh_locker, links, __db_locker);
	SH_TAILQ_REMOVE(&region->lockers, sh_locker, ulinks, __db_locker);
	region->stat.st_nlockers--;
}

/*
 * __lock_set_timeout
 *		-- set timeout values in shared memory.
 * This is called from the transaction system.
 * We either set the time that this tranaction expires or the
 * amount of time that a lock for this transaction is permitted
 * to wait.
 *
 * PUBLIC: int __lock_set_timeout __P(( DB_ENV *,
 * PUBLIC:      u_int32_t, db_timeout_t, u_int32_t));
 */
int
__lock_set_timeout(dbenv, locker, timeout, op)
	DB_ENV *dbenv;
	u_int32_t locker;
	db_timeout_t timeout;
	u_int32_t op;
{
	DB_LOCKTAB *lt;
	int ret;

	lt = dbenv->lk_handle;

	LOCKREGION(dbenv, lt);
	ret = __lock_set_timeout_internal(dbenv, locker, timeout, op);
	UNLOCKREGION(dbenv, lt);
	return (ret);
}
/*
 * __lock_set_timeout_internal
 *		-- set timeout values in shared memory.
 * This is the internal version called from the lock system.
 * We either set the time that this tranaction expires or the
 * amount of time that a lock for this transaction is permitted
 * to wait.
 *
 */
static int
__lock_set_timeout_internal(dbenv, locker, timeout, op)
	DB_ENV *dbenv;
	u_int32_t locker;
	db_timeout_t timeout;
	u_int32_t op;
{
	DB_LOCKER *sh_locker;
	DB_LOCKREGION *region;
	DB_LOCKTAB *lt;
	u_int32_t locker_ndx;
	int ret;

	lt = dbenv->lk_handle;
	region = lt->reginfo.primary;

	LOCKER_LOCK(lt, region, locker, locker_ndx);
	ret = __lock_getlocker(lt, locker, locker_ndx, 1, &sh_locker);

	if (ret != 0)
		return (ret);

	if (op == DB_SET_TXN_TIMEOUT) {
		if (timeout == 0)
			LOCK_SET_TIME_INVALID(&sh_locker->tx_expire);
		else
			__lock_expires(dbenv, &sh_locker->tx_expire, timeout);
	} else if (op == DB_SET_LOCK_TIMEOUT) {
		sh_locker->lk_timeout = timeout;
		F_SET(sh_locker, DB_LOCKER_TIMEOUT);
	} else if (op == DB_SET_TXN_NOW) {
		LOCK_SET_TIME_INVALID(&sh_locker->tx_expire);
		__lock_expires(dbenv, &sh_locker->tx_expire, 0);
		sh_locker->lk_expire = sh_locker->tx_expire;
		if (!LOCK_TIME_ISVALID(&region->next_timeout) ||
		    LOCK_TIME_GREATER(
		    &region->next_timeout, &sh_locker->lk_expire))
			region->next_timeout = sh_locker->lk_expire;
	} else
		return (EINVAL);

	return (0);
}

/*
 * __lock_inherit_timeout
 *		-- inherit timeout values from parent locker.
 * This is called from the transaction system.  This will
 * return EINVAL if the parent does not exist or did not
 * have a current txn timeout set.
 *
 * PUBLIC: int __lock_inherit_timeout __P(( DB_ENV *, u_int32_t, u_int32_t));
 */
int
__lock_inherit_timeout(dbenv, parent, locker)
	DB_ENV *dbenv;
	u_int32_t parent, locker;
{
	DB_LOCKER *parent_locker, *sh_locker;
	DB_LOCKREGION *region;
	DB_LOCKTAB *lt;
	u_int32_t locker_ndx;
	int ret;

	lt = dbenv->lk_handle;
	region = lt->reginfo.primary;
	ret = 0;
	LOCKREGION(dbenv, lt);

	/* If the parent does not exist, we are done. */
	LOCKER_LOCK(lt, region, parent, locker_ndx);
	if ((ret = __lock_getlocker(lt,
	    parent, locker_ndx, 0, &parent_locker)) != 0)
		goto err;

	/*
	 * If the parent is not there yet, thats ok.  If it
	 * does not have any timouts set, then avoid creating
	 * the child locker at this point.
	 */
	if (parent_locker == NULL ||
	    (LOCK_TIME_ISVALID(&parent_locker->tx_expire) &&
	    !F_ISSET(parent_locker, DB_LOCKER_TIMEOUT))) {
		ret = EINVAL;
		goto done;
	}

	LOCKER_LOCK(lt, region, locker, locker_ndx);
	if ((ret = __lock_getlocker(lt,
	    locker, locker_ndx, 1, &sh_locker)) != 0)
		goto err;

	sh_locker->tx_expire = parent_locker->tx_expire;

	if (F_ISSET(parent_locker, DB_LOCKER_TIMEOUT)) {
		sh_locker->lk_timeout = parent_locker->lk_timeout;
		F_SET(sh_locker, DB_LOCKER_TIMEOUT);
		if (!LOCK_TIME_ISVALID(&parent_locker->tx_expire))
			ret = EINVAL;
	}

done:
err:
	UNLOCKREGION(dbenv, lt);
	return (ret);
}

/*
 * __lock_getlocker --
 *	Get a locker in the locker hash table.  The create parameter
 * indicates if the locker should be created if it doesn't exist in
 * the table.
 *
 * This must be called with the locker bucket locked.
 *
 * PUBLIC: int __lock_getlocker __P((DB_LOCKTAB *,
 * PUBLIC:     u_int32_t, u_int32_t, int, DB_LOCKER **));
 */
int
__lock_getlocker(lt, locker, indx, create, retp)
	DB_LOCKTAB *lt;
	u_int32_t locker, indx;
	int create;
	DB_LOCKER **retp;
{
	DB_ENV *dbenv;
	DB_LOCKER *sh_locker;
	DB_LOCKREGION *region;

	dbenv = lt->dbenv;
	region = lt->reginfo.primary;

	HASHLOOKUP(lt->locker_tab,
	    indx, __db_locker, links, locker, sh_locker, __lock_locker_cmp);

	/*
	 * If we found the locker, then we can just return it.  If
	 * we didn't find the locker, then we need to create it.
	 */
	if (sh_locker == NULL && create) {
		/* Create new locker and then insert it into hash table. */
		if ((sh_locker = SH_TAILQ_FIRST(
		    &region->free_lockers, __db_locker)) == NULL) {
			__db_err(dbenv, __db_lock_err, "locker entries");
			return (ENOMEM);
		}
		SH_TAILQ_REMOVE(
		    &region->free_lockers, sh_locker, links, __db_locker);
		if (++region->stat.st_nlockers > region->stat.st_maxnlockers)
			region->stat.st_maxnlockers = region->stat.st_nlockers;

		sh_locker->id = locker;
		sh_locker->dd_id = 0;
		sh_locker->master_locker = INVALID_ROFF;
		sh_locker->parent_locker = INVALID_ROFF;
		SH_LIST_INIT(&sh_locker->child_locker);
		sh_locker->flags = 0;
		SH_LIST_INIT(&sh_locker->heldby);
		sh_locker->nlocks = 0;
		sh_locker->nwrites = 0;
		sh_locker->lk_timeout = 0;
		LOCK_SET_TIME_INVALID(&sh_locker->tx_expire);
		LOCK_SET_TIME_INVALID(&sh_locker->lk_expire);

		HASHINSERT(lt->locker_tab, indx, __db_locker, links, sh_locker);
		SH_TAILQ_INSERT_HEAD(&region->lockers,
		    sh_locker, ulinks, __db_locker);
	}

	*retp = sh_locker;
	return (0);
}

/*
 * __lock_getobj --
 *	Get an object in the object hash table.  The create parameter
 * indicates if the object should be created if it doesn't exist in
 * the table.
 *
 * This must be called with the object bucket locked.
 */
static int
__lock_getobj(lt, obj, ndx, create, retp)
	DB_LOCKTAB *lt;
	const DBT *obj;
	u_int32_t ndx;
	int create;
	DB_LOCKOBJ **retp;
{
	DB_ENV *dbenv;
	DB_LOCKOBJ *sh_obj;
	DB_LOCKREGION *region;
	int ret;
	void *p;

	dbenv = lt->dbenv;
	region = lt->reginfo.primary;

	/* Look up the object in the hash table. */
	HASHLOOKUP(lt->obj_tab,
	    ndx, __db_lockobj, links, obj, sh_obj, __lock_cmp);

	/*
	 * If we found the object, then we can just return it.  If
	 * we didn't find the object, then we need to create it.
	 */
	if (sh_obj == NULL && create) {
		/* Create new object and then insert it into hash table. */
		if ((sh_obj =
		    SH_TAILQ_FIRST(&region->free_objs, __db_lockobj)) == NULL) {
			__db_err(lt->dbenv, __db_lock_err, "object entries");
			ret = ENOMEM;
			goto err;
		}

		/*
		 * If we can fit this object in the structure, do so instead
		 * of shalloc-ing space for it.
		 */
		if (obj->size <= sizeof(sh_obj->objdata))
			p = sh_obj->objdata;
		else if ((ret = __db_shalloc(
		    lt->reginfo.addr, obj->size, 0, &p)) != 0) {
			__db_err(dbenv, "No space for lock object storage");
			goto err;
		}

		memcpy(p, obj->data, obj->size);

		SH_TAILQ_REMOVE(
		    &region->free_objs, sh_obj, links, __db_lockobj);
		if (++region->stat.st_nobjects > region->stat.st_maxnobjects)
			region->stat.st_maxnobjects = region->stat.st_nobjects;

		SH_TAILQ_INIT(&sh_obj->waiters);
		SH_TAILQ_INIT(&sh_obj->holders);
		sh_obj->lockobj.size = obj->size;
		sh_obj->lockobj.off = SH_PTR_TO_OFF(&sh_obj->lockobj, p);

		HASHINSERT(lt->obj_tab, ndx, __db_lockobj, links, sh_obj);
	}

	*retp = sh_obj;
	return (0);

err:	return (ret);
}

/*
 * __lock_is_parent --
 *	Given a locker and a transaction, return 1 if the locker is
 * an ancestor of the designcated transaction.  This is used to determine
 * if we should grant locks that appear to conflict, but don't because
 * the lock is already held by an ancestor.
 */
static int
__lock_is_parent(lt, locker, sh_locker)
	DB_LOCKTAB *lt;
	u_int32_t locker;
	DB_LOCKER *sh_locker;
{
	DB_LOCKER *parent;

	parent = sh_locker;
	while (parent->parent_locker != INVALID_ROFF) {
		parent = (DB_LOCKER *)
		    R_ADDR(&lt->reginfo, parent->parent_locker);
		if (parent->id == locker)
			return (1);
	}

	return (0);
}

/*
 * __lock_inherit_locks --
 * Called on child commit to merge child's locks with parent's.
 */

static int
__lock_inherit_locks(lt, locker, flags)
	DB_LOCKTAB *lt;
	u_int32_t locker;
	u_int32_t flags;
{
	DB_ENV *dbenv;
	DB_LOCKER *sh_locker, *sh_parent;
	DB_LOCKOBJ *obj;
	DB_LOCKREGION *region;
	int ret;
	struct __db_lock *hlp, *lp;
	u_int32_t ndx;

	region = lt->reginfo.primary;
	dbenv = lt->dbenv;

	/*
	 * Get the committing locker and mark it as deleted.
	 * This allows us to traverse the locker links without
	 * worrying that someone else is deleting locks out
	 * from under us.  However, if the locker doesn't
	 * exist, that just means that the child holds no
	 * locks, so inheritance is easy!
	 */
	LOCKER_LOCK(lt, region, locker, ndx);
	if ((ret = __lock_getlocker(lt,
	    locker, ndx, 0, &sh_locker)) != 0 ||
	    sh_locker == NULL ||
	    F_ISSET(sh_locker, DB_LOCKER_DELETED)) {
		if (ret == 0 && sh_locker != NULL)
			ret = EINVAL;
		__db_err(dbenv, __db_locker_invalid);
		goto err;
	}

	/* Make sure we are a child transaction. */
	if (sh_locker->parent_locker == INVALID_ROFF) {
		__db_err(dbenv, "Not a child transaction");
		ret = EINVAL;
		goto err;
	}
	sh_parent = (DB_LOCKER *)
	    R_ADDR(&lt->reginfo, sh_locker->parent_locker);
	F_SET(sh_locker, DB_LOCKER_DELETED);

	/*
	 * Now, lock the parent locker; move locks from
	 * the committing list to the parent's list.
	 */
	LOCKER_LOCK(lt, region, locker, ndx);
	if (F_ISSET(sh_parent, DB_LOCKER_DELETED)) {
		if (ret == 0) {
			__db_err(dbenv,
			    "Parent locker is not valid");
			ret = EINVAL;
		}
		goto err;
	}

	/*
	 * In order to make it possible for a parent to have
	 * many, many children who lock the same objects, and
	 * not require an inordinate number of locks, we try
	 * to merge the child's locks with its parent's.
	 */
	for (lp = SH_LIST_FIRST(&sh_locker->heldby, __db_lock);
	    lp != NULL;
	    lp = SH_LIST_FIRST(&sh_locker->heldby, __db_lock)) {
		SH_LIST_REMOVE(lp, locker_links, __db_lock);

		/* See if the parent already has a lock. */
		obj = (DB_LOCKOBJ *)((u_int8_t *)lp + lp->obj);
		for (hlp = SH_TAILQ_FIRST(&obj->holders, __db_lock);
		    hlp != NULL;
		    hlp = SH_TAILQ_NEXT(hlp, links, __db_lock))
			if (hlp->holder == sh_parent->id &&
			    lp->mode == hlp->mode)
				break;

		if (hlp != NULL) {
			/* Parent already holds lock. */
			hlp->refcount += lp->refcount;

			/* Remove lock from object list and free it. */
			DB_ASSERT(lp->status == DB_LSTAT_HELD);
			SH_TAILQ_REMOVE(&obj->holders, lp, links, __db_lock);
			(void)__lock_freelock(lt, lp, locker, DB_LOCK_FREE);
		} else {
			/* Just move lock to parent chains. */
			SH_LIST_INSERT_HEAD(&sh_parent->heldby,
			    lp, locker_links, __db_lock);
			lp->holder = sh_parent->id;
		}

		/*
		 * We may need to promote regardless of whether we simply
		 * moved the lock to the parent or changed the parent's
		 * reference count, because there might be a sibling waiting,
		 * who will now be allowed to make forward progress.
		 */
		(void)__lock_promote(lt, obj,
		    LF_ISSET(DB_LOCK_NOWAITERS));
	}

	/* Transfer child counts to parent. */
	sh_parent->nlocks += sh_locker->nlocks;
	sh_parent->nwrites += sh_locker->nwrites;

err:	return (ret);
}

/*
 * __lock_promote --
 *
 * Look through the waiters and holders lists and decide which (if any)
 * locks can be promoted.   Promote any that are eligible.
 *
 * PUBLIC: int __lock_promote __P((DB_LOCKTAB *, DB_LOCKOBJ *, u_int32_t));
 */
int
__lock_promote(lt, obj, flags)
	DB_LOCKTAB *lt;
	DB_LOCKOBJ *obj;
	u_int32_t flags;
{
	struct __db_lock *lp_w, *lp_h, *next_waiter;
	DB_LOCKER *sh_locker;
	DB_LOCKREGION *region;
	u_int32_t locker_ndx;
	int had_waiters, state_changed;

	region = lt->reginfo.primary;
	had_waiters = 0;

	/*
	 * We need to do lock promotion.  We also need to determine if we're
	 * going to need to run the deadlock detector again.  If we release
	 * locks, and there are waiters, but no one gets promoted, then we
	 * haven't fundamentally changed the lockmgr state, so we may still
	 * have a deadlock and we have to run again.  However, if there were
	 * no waiters, or we actually promoted someone, then we are OK and we
	 * don't have to run it immediately.
	 *
	 * During promotion, we look for state changes so we can return this
	 * information to the caller.
	 */

	for (lp_w = SH_TAILQ_FIRST(&obj->waiters, __db_lock),
	    state_changed = lp_w == NULL;
	    lp_w != NULL;
	    lp_w = next_waiter) {
		had_waiters = 1;
		next_waiter = SH_TAILQ_NEXT(lp_w, links, __db_lock);

		/* Waiter may have aborted or expired. */
		if (lp_w->status != DB_LSTAT_WAITING)
			continue;
		/* Are we switching locks? */
		if (LF_ISSET(DB_LOCK_NOWAITERS) && lp_w->mode == DB_LOCK_WAIT)
			continue;

		if (LF_ISSET(DB_LOCK_REMOVE)) {
			__lock_remove_waiter(lt, obj, lp_w, DB_LSTAT_NOTEXIST);
			continue;
		}
		for (lp_h = SH_TAILQ_FIRST(&obj->holders, __db_lock);
		    lp_h != NULL;
		    lp_h = SH_TAILQ_NEXT(lp_h, links, __db_lock)) {
			if (lp_h->holder != lp_w->holder &&
			    CONFLICTS(lt, region, lp_h->mode, lp_w->mode)) {
				LOCKER_LOCK(lt,
				    region, lp_w->holder, locker_ndx);
				if ((__lock_getlocker(lt, lp_w->holder,
				    locker_ndx, 0, &sh_locker)) != 0) {
					DB_ASSERT(0);
					break;
				}
				if (!__lock_is_parent(lt,
				    lp_h->holder, sh_locker))
					break;
			}
		}
		if (lp_h != NULL)	/* Found a conflict. */
			break;

		/* No conflict, promote the waiting lock. */
		SH_TAILQ_REMOVE(&obj->waiters, lp_w, links, __db_lock);
		lp_w->status = DB_LSTAT_PENDING;
		SH_TAILQ_INSERT_TAIL(&obj->holders, lp_w, links);

		/* Wake up waiter. */
		MUTEX_UNLOCK(lt->dbenv, &lp_w->mutex);
		state_changed = 1;
	}

	/*
	 * If this object had waiters and doesn't any more, then we need
	 * to remove it from the dd_obj list.
	 */
	if (had_waiters && SH_TAILQ_FIRST(&obj->waiters, __db_lock) == NULL)
		SH_TAILQ_REMOVE(&region->dd_objs, obj, dd_links, __db_lockobj);
	return (state_changed);
}

/*
 * __lock_remove_waiter --
 *	Any lock on the waitlist has a process waiting for it.  Therefore,
 * we can't return the lock to the freelist immediately.  Instead, we can
 * remove the lock from the list of waiters, set the status field of the
 * lock, and then let the process waking up return the lock to the
 * free list.
 *
 * This must be called with the Object bucket locked.
 */
static void
__lock_remove_waiter(lt, sh_obj, lockp, status)
	DB_LOCKTAB *lt;
	DB_LOCKOBJ *sh_obj;
	struct __db_lock *lockp;
	db_status_t status;
{
	DB_LOCKREGION *region;
	int do_wakeup;

	region = lt->reginfo.primary;

	do_wakeup = lockp->status == DB_LSTAT_WAITING;

	SH_TAILQ_REMOVE(&sh_obj->waiters, lockp, links, __db_lock);
	lockp->links.stqe_prev = -1;
	lockp->status = status;
	if (SH_TAILQ_FIRST(&sh_obj->waiters, __db_lock) == NULL)
		SH_TAILQ_REMOVE(
		    &region->dd_objs,
		    sh_obj, dd_links, __db_lockobj);

	/*
	 * Wake whoever is waiting on this lock.
	 */
	if (do_wakeup)
		MUTEX_UNLOCK(lt->dbenv, &lockp->mutex);
}

/*
 * __lock_expires -- set the expire time given the time to live.
 * We assume that if timevalp is set then it contains "now".
 * This avoids repeated system calls to get the time.
 */
static void
__lock_expires(dbenv, timevalp, timeout)
	DB_ENV *dbenv;
	db_timeval_t *timevalp;
	db_timeout_t timeout;
{
	if (!LOCK_TIME_ISVALID(timevalp))
		__os_clock(dbenv, &timevalp->tv_sec, &timevalp->tv_usec);
	if (timeout > 1000000) {
		timevalp->tv_sec += timeout / 1000000;
		timevalp->tv_usec += timeout % 1000000;
	} else
		timevalp->tv_usec += timeout;

	if (timevalp->tv_usec > 1000000) {
		timevalp->tv_sec++;
		timevalp->tv_usec -= 1000000;
	}
}

/*
 * __lock_expired -- determine if a lock has expired.
 *
 * PUBLIC: int __lock_expired __P((DB_ENV *, db_timeval_t *, db_timeval_t *));
 */
int
__lock_expired(dbenv, now, timevalp)
	DB_ENV *dbenv;
	db_timeval_t *now, *timevalp;
{
	if (!LOCK_TIME_ISVALID(timevalp))
		return (0);

	if (!LOCK_TIME_ISVALID(now))
		__os_clock(dbenv, &now->tv_sec, &now->tv_usec);

	return (now->tv_sec > timevalp->tv_sec ||
	    (now->tv_sec == timevalp->tv_sec &&
	    now->tv_usec >= timevalp->tv_usec));
}

/*
 * __lock_trade --
 *
 * Trade locker ids on a lock.  This is used to reassign file locks from
 * a transactional locker id to a long-lived locker id.  This should be
 * called with the region mutex held.
 */
static int
__lock_trade(dbenv, lock, new_locker)
	DB_ENV *dbenv;
	DB_LOCK *lock;
	u_int32_t new_locker;
{
	struct __db_lock *lp;
	DB_LOCKREGION *region;
	DB_LOCKTAB *lt;
	DB_LOCKER *sh_locker;
	int ret;
	u_int32_t locker_ndx;

	lt = dbenv->lk_handle;
	region = lt->reginfo.primary;

	lp = (struct __db_lock *)R_ADDR(&lt->reginfo, lock->off);

	/* If the lock is already released, simply return. */
	if (lp->gen != lock->gen)
		return (DB_NOTFOUND);

	/* Make sure that we can get new locker and add this lock to it. */
	LOCKER_LOCK(lt, region, new_locker, locker_ndx);
	if ((ret =
	    __lock_getlocker(lt, new_locker, locker_ndx, 0, &sh_locker)) != 0)
		return (ret);

	if (sh_locker == NULL) {
		__db_err(dbenv, "Locker does not exist");
		return (EINVAL);
	}

	/* Remove the lock from its current locker. */
	if ((ret = __lock_freelock(lt, lp, lp->holder, DB_LOCK_UNLINK)) != 0)
		return (ret);

	/* Add lock to its new locker. */
	SH_LIST_INSERT_HEAD(&sh_locker->heldby, lp, locker_links, __db_lock);
	sh_locker->nlocks++;
	if (IS_WRITELOCK(lp->mode))
		sh_locker->nwrites++;
	lp->holder = new_locker;

	return (0);
}

/*
 * Lock list routines.
 *	The list is composed of a 32-bit count of locks followed by
 * each lock.  A lock is represented by a 16-bit page-count, a lock
 * object and a page list.  A lock object consists of a 16-bit size
 * and the object itself.   In a pseudo BNF notation, you get:
 *
 * LIST = COUNT32 LOCK*
 * LOCK = COUNT16 LOCKOBJ PAGELIST
 * LOCKOBJ = COUNT16 OBJ
 * PAGELIST = COUNT32*
 *
 * (Recall that X* means "0 or more X's")
 *
 * In most cases, the OBJ is a struct __db_ilock and the page list is
 * a series of (32-bit) page numbers that should get written into the
 * pgno field of the __db_ilock.  So, the actual number of pages locked
 * is the number of items in the PAGELIST plus 1. If this is an application-
 * specific lock, then we cannot interpret obj and the pagelist must
 * be empty.
 *
 * Consider a lock list for: File A, pages 1&2, File B pages 3-5, Applock
 * This would be represented as:
 *	5 1 [fid=A;page=1] 2 2 [fid=B;page=3] 4 5 0 APPLOCK
 *        ------------------ -------------------- ---------
 *         LOCK for file A    LOCK for file B     application-specific lock
 */

#define MAX_PGNOS	0xffff

/*
 * These macros are bigger than one might exepect becasue the
 * Solaris compiler says that a cast does not return an lvalue,
 * so constructs like *(u_int32_t*)dp = count; generate warnings.
 */

#define RET_SIZE(size, count)  ((size) + 			\
     sizeof(u_int32_t) + (count) * 2 * sizeof(u_int16_t))

#define PUT_COUNT(dp, count) 	do {	u_int32_t *ip = (u_int32_t *)dp;\
					*ip = count;			\
					dp = (u_int8_t *)dp +		\
					     sizeof(u_int32_t);		\
				} while (0)
#define PUT_PCOUNT(dp, count) 	do {	u_int16_t *ip = (u_int16_t *)dp;\
					*ip = count;			\
					dp = (u_int8_t *)dp +		\
					    sizeof(u_int16_t);		\
				} while (0)
#define PUT_SIZE(dp, size) 	do {	u_int16_t *ip = (u_int16_t *)dp;\
					*ip = size;			\
					dp = (u_int8_t *)dp +		\
					    sizeof(u_int16_t);		\
				} while (0)
#define PUT_PGNO(dp, pgno) 	do {	db_pgno_t *ip = (db_pgno_t *)dp;\
					*ip = pgno;			\
					dp = (u_int8_t *)dp +		\
					    sizeof(db_pgno_t);		\
				} while (0)
#define COPY_OBJ(dp, obj)	do {					   \
					memcpy(dp, obj->data, obj->size);  \
					dp = (u_int8_t *)dp +		   \
					     ALIGN(obj->size,		   \
					     sizeof(u_int32_t)); 	   \
				} while (0)

#define GET_COUNT(dp, count)	do {					\
					(count) = *(u_int32_t *) dp;	\
					dp = (u_int8_t *)dp +		\
					     sizeof(u_int32_t); 	\
				} while (0);
#define GET_PCOUNT(dp, count)	do {					\
					(count) = *(u_int16_t *) dp;	\
					dp = (u_int8_t *)dp +		\
					     sizeof(u_int16_t); 	\
				} while (0);
#define GET_SIZE(dp, size)	do {					\
					(size) = *(u_int16_t *) dp;	\
					dp = (u_int8_t *)dp +		\
					     sizeof(u_int16_t); 	\
				} while (0);
#define GET_PGNO(dp, pgno)	do {					\
					(pgno) = *(db_pgno_t *) dp;	\
					dp = (u_int8_t *)dp +		\
					     sizeof(db_pgno_t); 	\
				} while (0);

static int 
__lock_fix_list(dbenv, list_dbt, nlocks)
	DB_ENV *dbenv;
	DBT *list_dbt;
	u_int32_t nlocks;
{
	DBT *obj;
	DB_LOCK_ILOCK *lock, *plock;
	u_int32_t i, j, nfid, npgno, size;
	int ret;
	u_int8_t *data, *dp;

	size = list_dbt->size;
	if (size == 0)
		return (0);

	obj = (DBT *) list_dbt->data;

	/*
	 * If necessary sort the list of locks so that locks
	 * on the same fileid are together.  We do not sort
	 * 1 or 2 locks because by definition if there are
	 * locks on the same fileid they will be together.
	 * The sort will also move any locks that do not
	 * look like page locks to the end of the list
	 * so we can stop looking for locks we can combine
	 * when we hit one.
	 */
	switch (nlocks) {
	case 1:
		size = RET_SIZE(obj->size, 1);
		if ((ret = __os_malloc(dbenv, size, &data)) != 0)
			return (ret);

		dp = data;
		PUT_COUNT(dp, 1);
		PUT_PCOUNT(dp, 0);
		PUT_SIZE(dp, obj->size);
		COPY_OBJ(dp, obj);
		break;

	default:
		/* Sort so that all locks with same fileid are together. */
		qsort(list_dbt->data, nlocks, sizeof(DBT), __lock_sort_cmp);
		/* FALL THROUGH */
	case 2:
		nfid = npgno = 0;
		i = 0;
		if (obj->size != sizeof(DB_LOCK_ILOCK))
			goto not_ilock;

		nfid = 1;
		plock = (DB_LOCK_ILOCK *)obj->data;

		/* We use ulen to keep track of the number of pages. */
		j = 0;
		obj[0].ulen = 0;
		for (i = 1; i < nlocks; i++) {
			if (obj[i].size != sizeof(DB_LOCK_ILOCK))
				break;
			lock = (DB_LOCK_ILOCK *)obj[i].data;
			if (obj[j].ulen < MAX_PGNOS &&
			     lock->type == plock->type &&
			     memcmp(lock->fileid,
			     plock->fileid, DB_FILE_ID_LEN) == 0) {
				obj[j].ulen++;
				npgno++;
			} else {
				nfid++;
				plock = lock;
				j = i;
				obj[j].ulen = 0;
			}
		}

not_ilock:
		size = nfid * sizeof(DB_LOCK_ILOCK);
		size += npgno * sizeof(db_pgno_t);
		/* Add the number of nonstandard locks and get their size. */
		nfid += nlocks - i;
		for (; i < nlocks; i++) {
			size += obj[i].size;
			obj[i].ulen = 0;
		}

		size = RET_SIZE(size, nfid);
		if ((ret = __os_malloc(dbenv, size, &data)) != 0)
			return (ret);

		dp = data;
		PUT_COUNT(dp, nfid);

		for (i = 0; i < nlocks; i = j) {
			PUT_PCOUNT(dp, obj[i].ulen);
			PUT_SIZE(dp, obj[i].size);
			COPY_OBJ(dp, obj);
			lock = (DB_LOCK_ILOCK *)obj[i].data;
			for (j = i + 1; j <= i + obj[i].ulen; j++) {
				lock = (DB_LOCK_ILOCK *)obj[j].data;
				PUT_PGNO(dp, lock->pgno);
			}
		}
	}

	(void)__os_free(dbenv, list_dbt->data);

	list_dbt->data = data;
	list_dbt->size = size;

	return (0);
}

/*
 * PUBLIC: int __lock_get_list __P((DB_ENV *, u_int32_t, u_int32_t,
 * PUBLIC:	      db_lockmode_t, DBT *));
 */
int
__lock_get_list(dbenv, locker, flags, lock_mode, list)
	DB_ENV *dbenv;
	u_int32_t locker, flags;
	db_lockmode_t lock_mode;
	DBT *list;
{
	DBT obj_dbt;
	DB_LOCK ret_lock;
	DB_LOCK_ILOCK *lock;
	DB_LOCKTAB *lt;
	DB_LOCKREGION *region;
	db_pgno_t save_pgno;
	u_int16_t npgno, size;
	u_int32_t i, nlocks;
	int ret;
	void *dp;

	if (list->size == 0)
		return (0);
	ret = 0;
	lt = dbenv->lk_handle;
	region = lt->reginfo.primary;
	dp = list->data;

	GET_COUNT(dp, nlocks);
	LOCKREGION(dbenv, (DB_LOCKTAB *)dbenv->lk_handle);

	for (i = 0; i < nlocks; i++) {
		GET_PCOUNT(dp, npgno);
		GET_SIZE(dp, size);
		lock = (DB_LOCK_ILOCK *) dp;
		save_pgno = lock->pgno;
		obj_dbt.data = dp;
		obj_dbt.size = size;
		dp = ((u_int8_t *)dp) + ALIGN(size, sizeof(u_int32_t));
		do {
			if ((ret = __lock_get_internal(lt, locker,
			     flags, &obj_dbt, lock_mode, 0, &ret_lock)) != 0) {
			     lock->pgno = save_pgno;
			     goto err;
			}
			if (npgno != 0)
				GET_PGNO(dp, lock->pgno);
		} while (npgno-- != 0);
		lock->pgno = save_pgno;
	}

err:
	UNLOCKREGION(dbenv, (DB_LOCKTAB *)dbenv->lk_handle);
	return (ret);
}

static int
__lock_sort_cmp(a, b)
	const void *a, *b;
{
	const DBT *d1, *d2;
	DB_LOCK_ILOCK *l1, *l2;

	d1 = a;
	d2 = b;

	/* Force all non-standard locks to sort at end. */
	if (d1->size != sizeof(DB_LOCK_ILOCK)) {
		if (d2->size != sizeof(DB_LOCK_ILOCK))
			return (d1->size - d2->size);
		else
			return (1);
	} else if (d2->size != sizeof(DB_LOCK_ILOCK))
		return (-1);

	l1 = d1->data;
	l2 = d2->data;
	if (l1->type != l2->type)
		return (l1->type - l2->type);
	return (memcmp(l1->fileid, l2->fileid, DB_FILE_ID_LEN));
}
