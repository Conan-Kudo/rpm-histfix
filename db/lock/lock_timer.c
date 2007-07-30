/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996,2007 Oracle.  All rights reserved.
 *
 * $Id: lock_timer.c,v 12.13 2007/05/17 15:15:43 bostic Exp $
 */

#include "db_config.h"

#include "db_int.h"
#include "dbinc/lock.h"

/*
 * __lock_set_timeout --
 *	Set timeout values in shared memory.
 *
 * This is called from the transaction system.  We either set the time that
 * this transaction expires or the amount of time a lock for this transaction
 * is permitted to wait.
 *
 * PUBLIC: int __lock_set_timeout __P((DB_ENV *,
 * PUBLIC:      DB_LOCKER *, db_timeout_t, u_int32_t));
 */
int
__lock_set_timeout(dbenv, locker, timeout, op)
	DB_ENV *dbenv;
	DB_LOCKER *locker;
	db_timeout_t timeout;
	u_int32_t op;
{
	int ret;

	if (locker == NULL)
		return (0);
	LOCK_REGION_LOCK(dbenv);
	ret = __lock_set_timeout_internal(dbenv, locker, timeout, op);
	LOCK_REGION_UNLOCK(dbenv);
	return (ret);
}

/*
 * __lock_set_timeout_internal
 *		-- set timeout values in shared memory.
 *
 * This is the internal version called from the lock system.  We either set
 * the time that this transaction expires or the amount of time that a lock
 * for this transaction is permitted to wait.
 *
 * PUBLIC: int __lock_set_timeout_internal
 * PUBLIC:     __P((DB_ENV *, DB_LOCKER *, db_timeout_t, u_int32_t));
 */
int
__lock_set_timeout_internal(dbenv, sh_locker, timeout, op)
	DB_ENV *dbenv;
	DB_LOCKER *sh_locker;
	db_timeout_t timeout;
	u_int32_t op;
{
	DB_LOCKREGION *region;
	region = dbenv->lk_handle->reginfo.primary;

	if (op == DB_SET_TXN_TIMEOUT) {
		if (timeout == 0)
			timespecclear(&sh_locker->tx_expire);
		else
			__lock_expires(dbenv, &sh_locker->tx_expire, timeout);
	} else if (op == DB_SET_LOCK_TIMEOUT) {
		sh_locker->lk_timeout = timeout;
		F_SET(sh_locker, DB_LOCKER_TIMEOUT);
	} else if (op == DB_SET_TXN_NOW) {
		timespecclear(&sh_locker->tx_expire);
		__lock_expires(dbenv, &sh_locker->tx_expire, 0);
		sh_locker->lk_expire = sh_locker->tx_expire;
		if (!timespecisset(&region->next_timeout) ||
		    timespeccmp(
			&region->next_timeout, &sh_locker->lk_expire, >))
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
 * PUBLIC: int __lock_inherit_timeout __P((DB_ENV *, DB_LOCKER *, DB_LOCKER *));
 */
int
__lock_inherit_timeout(dbenv, parent, locker)
	DB_ENV *dbenv;
	DB_LOCKER *parent, *locker;
{
	int ret;

	ret = 0;
	LOCK_REGION_LOCK(dbenv);

	/*
	 * If the parent is not there yet, thats ok.  If it
	 * does not have any timouts set, then avoid creating
	 * the child locker at this point.
	 */
	if (parent == NULL ||
	    (timespecisset(&parent->tx_expire) &&
	    !F_ISSET(parent, DB_LOCKER_TIMEOUT))) {
		ret = EINVAL;
		goto err;
	}

	locker->tx_expire = parent->tx_expire;

	if (F_ISSET(parent, DB_LOCKER_TIMEOUT)) {
		locker->lk_timeout = parent->lk_timeout;
		F_SET(locker, DB_LOCKER_TIMEOUT);
		if (!timespecisset(&parent->tx_expire))
			ret = EINVAL;
	}

err:	LOCK_REGION_UNLOCK(dbenv);
	return (ret);
}

/*
 * __lock_expires --
 *	Set the expire time given the time to live.
 *
 * PUBLIC: void __lock_expires __P((DB_ENV *, db_timespec *, db_timeout_t));
 */
void
__lock_expires(dbenv, timespecp, timeout)
	DB_ENV *dbenv;
	db_timespec *timespecp;
	db_timeout_t timeout;
{
	db_timespec v;

	/*
	 * If timespecp is set then it contains "now".  This avoids repeated
	 * system calls to get the time.
	 */
	if (!timespecisset(timespecp))
		__os_gettime(dbenv, timespecp);

	/* Convert the microsecond timeout argument to a timespec. */
	DB_TIMEOUT_TO_TIMESPEC(timeout, &v);

	/* Add the timeout to "now". */
	timespecadd(timespecp, &v);
}

/*
 * __lock_expired -- determine if a lock has expired.
 *
 * PUBLIC: int __lock_expired __P((DB_ENV *, db_timespec *, db_timespec *));
 */
int
__lock_expired(dbenv, now, timespecp)
	DB_ENV *dbenv;
	db_timespec *now, *timespecp;
{
	if (!timespecisset(timespecp))
		return (0);

	if (!timespecisset(now))
		__os_gettime(dbenv, now);

	return (timespeccmp(now, timespecp, >=));
}
