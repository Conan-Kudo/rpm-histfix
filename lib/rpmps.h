#ifndef H_RPMPS
#define H_RPMPS

/** \ingroup rpmps
 * \file lib/rpmps.h
 * Structures and prototypes used for an "rpmps" problem set.
 */

#include <rpmmessages.h>	/* for fnpyKey */

#ifdef __cplusplus
extern "C" {
#endif

extern int _rpmps_debug;

/**
 * Raw data for an element of a problem set.
 */
typedef struct rpmProblem_s * rpmProblem;

/** \ingroup rpmps
 * Transaction problems found while processing a transaction set/
 */
typedef struct rpmps_s * rpmps;

typedef struct rpmpsi_s * rpmpsi;

/** \ingroup rpmps
 * Enumerate transaction set problem types.
 */
typedef enum rpmProblemType_e {
    RPMPROB_BADARCH,	/*!< package ... is for a different architecture */
    RPMPROB_BADOS,	/*!< package ... is for a different operating system */
    RPMPROB_PKG_INSTALLED, /*!< package ... is already installed */
    RPMPROB_BADRELOCATE,/*!< path ... is not relocatable for package ... */
    RPMPROB_REQUIRES,	/*!< package ... has unsatisfied Requires: ... */
    RPMPROB_CONFLICT,	/*!< package ... has unsatisfied Conflicts: ... */
    RPMPROB_NEW_FILE_CONFLICT, /*!< file ... conflicts between attemped installs of ... */
    RPMPROB_FILE_CONFLICT,/*!< file ... from install of ... conflicts with file from package ... */
    RPMPROB_OLDPACKAGE,	/*!< package ... (which is newer than ...) is already installed */
    RPMPROB_DISKSPACE,	/*!< installing package ... needs ... on the ... filesystem */
    RPMPROB_DISKNODES,	/*!< installing package ... needs ... on the ... filesystem */
 } rpmProblemType;

/** \ingroup rpmps
 * Create a problem item.
 * @param type		type of problem
 * @param pkgNEVR	package name
 * @param key		filename or python object address
 * @param dn		directory name
 * @param bn		file base name
 * @param altNEVR	related (e.g. through a dependency) package name
 * @param ulong1	generic pointer/long attribute
 * @return		rpmProblem
 */
rpmProblem rpmProblemCreate(rpmProblemType type,
                            const char * pkgNEVR,
                            fnpyKey key,
                            const char * dn, const char * bn,
                            const char * altNEVR,
                            unsigned long ulong1);

/** \ingroup rpmps
 * Destroy a problem item.
 * @param prob		rpm problem
 * @return		rpm problem (NULL)
 */
rpmProblem rpmProblemFree(rpmProblem prob);

/** \ingroup rpmps
 * Return package NEVR
 * @param prob		rpm problem
 * @return		package NEVR
 */
const char * rpmProblemGetPkgNEVR(const rpmProblem prob);
/** \ingroup rpmps
 * Return related (e.g. through a dependency) package NEVR
 * @param prob		rpm problem
 * @return		related (e.g. through a dependency) package NEVR
 */
const char * rpmProblemGetAltNEVR(const rpmProblem prob);

/** \ingroup rpmps
 * Return type of problem (dependency, diskpace etc)
 * @param prob		rpm problem
 * @return		type of problem
 */

rpmProblemType rpmProblemGetType(const rpmProblem prob);

/** \ingroup rpmps
 * Return filename or python object address of a problem
 * @param prob		rpm problem
 * @return		filename or python object address
 */
fnpyKey rpmProblemGetKey(const rpmProblem prob);

/** \ingroup rpmps
 * Return a generic data string from a problem
 * @param prob		rpm problem
 * @return		a generic data string
 * @todo		needs a better name
 */
const char * rpmProblemGetStr(const rpmProblem prob);
/** \ingroup rpmps
 * Return generic pointer/long attribute from a problem
 * @param prob		rpm problem
 * @return		a generic pointer/long attribute
 * @todo		needs a better name
 */
unsigned long rpmProblemGetLong(const rpmProblem prob);

/** \ingroup rpmps
 * Return formatted string representation of a problem.
 * @param prob		rpm problem
 * @return		formatted string (malloc'd)
 */
extern const char * rpmProblemString(const rpmProblem prob);

/** \ingroup rpmps
 * Unreference a problem set instance.
 * @param ps		problem set
 * @param msg
 * @return		problem set
 */
rpmps rpmpsUnlink (rpmps ps,
		const char * msg);

/** @todo Remove debugging entry from the ABI. */
rpmps XrpmpsUnlink (rpmps ps,
		const char * msg, const char * fn, unsigned ln);
#define	rpmpsUnlink(_ps, _msg)	XrpmpsUnlink(_ps, _msg, __FILE__, __LINE__)

/** \ingroup rpmps
 * Reference a problem set instance.
 * @param ps		transaction set
 * @param msg
 * @return		new transaction set reference
 */
rpmps rpmpsLink (rpmps ps, const char * msg);

/** @todo Remove debugging entry from the ABI. */
rpmps XrpmpsLink (rpmps ps,
		const char * msg, const char * fn, unsigned ln);
#define	rpmpsLink(_ps, _msg)	XrpmpsLink(_ps, _msg, __FILE__, __LINE__)

/** \ingroup rpmps
 * Return number of problems in set.
 * @param ps		problem set
 * @return		number of problems
 */
int rpmpsNumProblems(rpmps ps);

/** \ingroup rpmps
 * Initialize problem set iterator.
 * @param ps		problem set
 * @return		problem set iterator
 */
rpmpsi rpmpsInitIterator(rpmps ps);

/** \ingroup rpmps
 * Destroy problem set iterator.
 * @param psi		problem set iterator
 * @return		problem set iterator (NULL)
 */
rpmpsi rpmpsFreeIterator(rpmpsi psi);

/** \ingroup rpmps
 * Return next problem set iterator index
 * @param psi		problem set iterator
 * @return		iterator index, -1 on termination
 */
int rpmpsNextIterator(rpmpsi psi);

/** \ingroup rpmps
 * Return current problem from problem set
 * @param psi		problem set iterator
 * @return		current rpmProblem 
 */
rpmProblem rpmpsGetProblem(rpmpsi psi);

/** \ingroup rpmps
 * Create a problem set.
 * @return		new problem set
 */
rpmps rpmpsCreate(void);

/** \ingroup rpmps
 * Destroy a problem set.
 * @param ps		problem set
 * @return		NULL always
 */
rpmps rpmpsFree(rpmps ps);

/** \ingroup rpmps
 * Print problems to file handle.
 * @param fp		file handle (NULL uses stderr)
 * @param ps		problem set
 */
void rpmpsPrint(FILE *fp, rpmps ps);

/** \ingroup rpmps
 * Append a problem to current set of problems.
 * @param ps		problem set
 * @param prob		rpmProblem 
 */
void rpmpsAppendProblem(rpmps ps, rpmProblem prob);

/** \ingroup rpmps
 * Append a problem to current set of problems.
 * @param ps		problem set
 * @param type		type of problem
 * @param pkgNEVR	package name
 * @param key		filename or python object address
 * @param dn		directory name
 * @param bn		file base name
 * @param altNEVR	related (e.g. through a dependency) package name
 * @param ulong1	generic pointer/long attribute
 */
void rpmpsAppend(rpmps ps, rpmProblemType type,
		const char * pkgNEVR,
		fnpyKey key,
		const char * dn, const char * bn,
		const char * altNEVR,
		unsigned long ulong1);

/** \ingroup rpmps
 * Filter a problem set.
 *
 * As the problem sets are generated in an order solely dependent
 * on the ordering of the packages in the transaction, and that
 * ordering can't be changed, the problem sets must be parallel to
 * one another. Additionally, the filter set must be a subset of the
 * target set, given the operations available on transaction set.
 * This is good, as it lets us perform this trim in linear time, rather
 * then logarithmic or quadratic.
 *
 * @param ps		problem set
 * @param filter	problem filter (or NULL)
 * @return		0 no problems, 1 if problems remain
 */
int rpmpsTrim(rpmps ps, rpmps filter);

#ifdef __cplusplus
}
#endif

#endif	/* H_RPMPS */
