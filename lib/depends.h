#ifndef H_DEPENDS
#define H_DEPENDS

#include <header.h>

struct availablePackage {
    Header h;
/*@owned@*/ const char ** provides;
/*@owned@*/ const char ** providesEVR;
/*@dependent@*/ int * provideFlags;
/*@owned@*/ const char ** baseNames;
/*@dependent@*/ const char * name;
/*@dependent@*/ const char * version;
/*@dependent@*/ const char * release;
/*@dependent@*/ int_32 * epoch;
    int providesCount;
    int filesCount;
    uint_32 multiLib;	/* MULTILIB */
/*@dependent@*/ const void * key;
    rpmRelocation * relocs;
/*@null@*/ FD_t fd;
} ;

enum indexEntryType { IET_PROVIDES=1 };

struct availableIndexEntry {
/*@dependent@*/ struct availablePackage * package;
/*@dependent@*/ const char * entry;
    size_t entryLen;
    enum indexEntryType type;
} ;

struct fileIndexEntry {
    int pkgNum;
    int fileFlags;	/* MULTILIB */
/*@dependent@*/ const char * baseName;
} ;

struct dirInfo {
/*@owned@*/ char * dirName;			/* xstrdup'd */
    int dirNameLen;
/*@owned@*/ struct fileIndexEntry * files;	/* xmalloc'd */
    int numFiles;
} ;

struct availableIndex {
/*@null@*/ struct availableIndexEntry * index ;
    int size;
} ;

struct availableList {
/*@owned@*/ /*@null@*/ struct availablePackage * list;
    struct availableIndex index;
    int size;
    int alloced;
    int numDirs;
/*@owned@*/ struct dirInfo * dirs;		/* xmalloc'd */
};

struct transactionElement {
    enum rpmTransactionType { TR_ADDED, TR_REMOVED } type;
    union { 
	int addedIndex;
	struct {
	    int dboffset;
	    int dependsOnIndex;
	} removed;
    } u;
};

struct rpmTransactionSet_s {
/*@owned@*/ /*@null@*/ rpmdb rpmdb;			/* may be NULL */
/*@only@*/ int * removedPackages;
    int numRemovedPackages;
    int allocedRemovedPackages;
    struct availableList addedPackages;
    struct availableList availablePackages;
/*@only@*/ struct transactionElement * order;
    int orderCount;
    int orderAlloced;
/*@only@*/ const char * rootDir;
/*@only@*/ const char * currDir;
/*@null@*/ FD_t scriptFd;
};

struct problemsSet {
    struct rpmDependencyConflict * problems;
    int num;
    int alloced;
};

#ifdef __cplusplus
extern "C" {
#endif

/* XXX lib/uninstall.c */
int headerMatchesDepFlags(Header h, const char *reqName, const char * reqInfo, int reqFlags);

#ifdef __cplusplus
}
#endif

#endif	/* H_DEPENDS */
