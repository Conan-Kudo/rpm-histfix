#ifndef	H_RPMSW
#define	H_RPMSW

/** \ingroup rpmio
 * \file rpmio/rpmsw.h
 */

#ifdef __cplusplus
extern "C" {
#endif

/** \ingroup rpmio
 */
typedef unsigned long int rpmtime_t;

/** \ingroup rpmio
 */
typedef struct rpmsw_s * rpmsw;

/** \ingroup rpmio
 */
typedef struct rpmop_s * rpmop;

/** \ingroup rpmio
 */
struct rpmsw_s {
    union {
	struct timeval tv;
	unsigned long long int ticks;
	unsigned long int tocks[2];
    } u;
};

/** \ingroup rpmio
 * Cumulative statistics for an operation.
 */
struct rpmop_s {
    struct rpmsw_s	begin;	/*!< Starting time stamp. */
    int			count;	/*!< Number of operations. */
    size_t		bytes;	/*!< Number of bytes transferred. */
    rpmtime_t		usecs;	/*!< Number of ticks. */
};

/** Return benchmark time stamp.
 * @param *sw		time stamp
 * @return		0 on success
 */
rpmsw rpmswNow(rpmsw sw);

/** Return benchmark time stamp difference.
 * @param *end		end time stamp
 * @param *begin	begin time stamp
 * @return		difference in micro-seconds
 */
rpmtime_t rpmswDiff(rpmsw end, rpmsw begin);

/** Return benchmark time stamp overhead.
 * @return		overhead in micro-seconds
 */
rpmtime_t rpmswInit(void);

/** \ingroup rpmio
 * Enter timed operation.
 * @param op			operation statistics
 * @param rc			-1 clears usec counter
 * @return			0 always
 */
int rpmswEnter(rpmop op, ssize_t rc);

/** \ingroup rpmio
 * Exit timed operation.
 * @param op			operation statistics
 * @param rc			per-operation data (e.g. bytes transferred)
 * @return			cumulative usecs for operation
 */
rpmtime_t rpmswExit(rpmop op, ssize_t rc);

/** \ingroup rpmio
 * Sum statistic counters.
 * @param to			result statistics
 * @param from			operation statistics
 * @return			cumulative usecs for operation
 */
rpmtime_t rpmswAdd(rpmop to, rpmop from);

/** \ingroup rpmio
 * Subtract statistic counters.
 * @param to			result statistics
 * @param from			operation statistics
 * @return			cumulative usecs for operation
 */
rpmtime_t rpmswSub(rpmop to, rpmop from);

#ifdef __cplusplus
}
#endif

#endif	/* H_RPMSW */
