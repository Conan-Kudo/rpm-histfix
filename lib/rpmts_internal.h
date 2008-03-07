#ifndef _RPMTS_INTERNAL_H
#define _RPMTS_INTERNAL_H

#include <rpm/rpmts.h>
#include <rpm/rpmal.h>	/* XXX availablePackage/relocateFileList ,*/

#include "rpmdb/rpmhash.h"	/* XXX hashTable */
#include "lib/rpmtsscore.h"	/* for rpmtsScore */

/** \ingroup rpmts
 */
typedef	struct diskspaceInfo_s * rpmDiskSpaceInfo;

/** \ingroup rpmts
 */
struct diskspaceInfo_s {
    dev_t dev;			/*!< File system device number. */
    signed long bneeded;	/*!< No. of blocks needed. */
    signed long ineeded;	/*!< No. of inodes needed. */
    int bsize;			/*!< File system block size. */
    signed long long bavail;	/*!< No. of blocks available. */
    signed long long iavail;	/*!< No. of inodes available. */
};

/** \ingroup rpmts
 * Adjust for root only reserved space. On linux e2fs, this is 5%.
 */
#define	adj_fs_blocks(_nb)	(((_nb) * 21) / 20)

/* argon thought a shift optimization here was a waste of time...  he's
   probably right :-( */
#define BLOCK_ROUND(size, block) (((size) + (block) - 1) / (block))

/** \ingroup rpmts
 * The set of packages to be installed/removed atomically.
 */
struct rpmts_s {
    rpmtransFlags transFlags;	/*!< Bit(s) to control operation. */
    rpmtsType type;             /*!< default, rollback, autorollback */

    rpmdb sdb;			/*!< Solve database handle. */
    int sdbmode;		/*!< Solve database open mode. */
    int (*solve) (rpmts ts, rpmds key, const void * data);
                                /*!< Search for NEVRA key. */
    const void * solveData;	/*!< Solve callback data */
    int nsuggests;		/*!< No. of depCheck suggestions. */
    const void ** suggests;	/*!< Possible depCheck suggestions. */

    rpmCallbackFunction notify;	/*!< Callback function. */
    rpmCallbackData notifyData;	/*!< Callback private data. */

    rpmps probs;		/*!< Current problems in transaction. */
    rpmprobFilterFlags ignoreSet;
				/*!< Bits to filter current problems. */

    unsigned int filesystemCount;	/*!< No. of mounted filesystems. */
    const char ** filesystems;	/*!< Mounted filesystem names. */
    rpmDiskSpaceInfo dsi;	/*!< Per filesystem disk/inode usage. */

    rpmdb rdb;			/*!< Install database handle. */
    int dbmode;			/*!< Install database open mode. */
    hashTable ht;		/*!< Fingerprint hash table. */

    int * removedPackages;	/*!< Set of packages being removed. */
    int numRemovedPackages;	/*!< No. removed package instances. */
    int allocedRemovedPackages;	/*!< Size of removed packages array. */

    rpmal addedPackages;	/*!< Set of packages being installed. */
    int numAddedPackages;	/*!< No. added package instances. */

#ifndef	DYING
    rpmal availablePackages;	/*!< Universe of available packages. */
    int numAvailablePackages;	/*!< No. available package instances. */
#endif

    rpmte relocateElement;	/*!< Element to use when relocating packages. */

    rpmte * order;		/*!< Packages sorted by dependencies. */
    int orderCount;		/*!< No. of transaction elements. */
    int orderAlloced;		/*!< No. of allocated transaction elements. */
    int unorderedSuccessors;	/*!< Index of 1st element of successors. */
    int ntrees;			/*!< No. of dependency trees. */
    int maxDepth;		/*!< Maximum depth of dependency tree(s). */

    int selinuxEnabled;		/*!< Is SE linux enabled? */
    int chrootDone;		/*!< Has chroot(2) been been done? */
    char * rootDir;		/*!< Path to top of install tree. */
    char * currDir;		/*!< Current working directory. */
    FD_t scriptFd;		/*!< Scriptlet stdout/stderr. */
    int delta;			/*!< Delta for reallocation. */
    rpm_tid_t tid;		/*!< Transaction id. */

    rpm_color_t color;		/*!< Transaction color bits. */
    rpm_color_t prefcolor;	/*!< Preferred file color. */

    rpmVSFlags vsflags;		/*!< Signature/digest verification flags. */

    const char * fn;		/*!< Current package fn. */
    rpmSigTag  sigtag;		/*!< Current package signature tag. */
    rpmTagType  sigtype;	/*!< Current package signature data type. */
    rpm_data_t sig;		/*!< Current package signature. */
    size_t siglen;		/*!< Current package signature length. */

    uint8_t * pkpkt;/*!< Current pubkey packet. */
    size_t pkpktlen;		/*!< Current pubkey packet length. */
    pgpKeyID_t pksignid;	/*!< Current pubkey fingerprint. */

    struct rpmop_s ops[RPMTS_OP_MAX];

    pgpDig dig;			/*!< Current signature/pubkey parameters. */

    rpmSpec spec;		/*!< Spec file control structure. */

    rpmtsScore score;		/*!< Transaction Score (autorollback). */

    int nrefs;			/*!< Reference count. */
};

#endif /* _RPMTS_INTERNAL_H */
