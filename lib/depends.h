#ifndef H_DEPENDS
#define H_DEPENDS

#include "header.h"

int headerMatchesDepFlags(Header h, const char * reqInfo, int reqFlags);

struct availablePackage {
    Header h;
    char ** provides;
    char ** files;
    char * name, * version, * release;
    int epoch, hasEpoch, providesCount, filesCount;
    const void * key;
    rpmRelocation * relocs;
    FD_t fd;
} ;

enum indexEntryType { IET_NAME, IET_PROVIDES, IET_FILE };

struct availableIndexEntry {
    struct availablePackage * package;
    char * entry;
    enum indexEntryType type;
} ;

struct availableIndex {
    struct availableIndexEntry * index ;
    int size;
} ;

struct availableList {
    struct availablePackage * list;
    struct availableIndex index;
    int size, alloced;
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
    rpmdb db;					/* may be NULL */
    int * removedPackages;
    int numRemovedPackages, allocedRemovedPackages;
    struct availableList addedPackages, availablePackages;
    struct transactionElement * order;
    int orderCount, orderAlloced;
    char * root;
    FD_t scriptFd;
};

struct problemsSet {
    struct rpmDependencyConflict * problems;
    int num;
    int alloced;
};


#endif	/* H_DEPENDS */
