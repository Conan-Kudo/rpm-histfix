#ifndef H_INSTALL
#define H_INSTALL

#include "header.h"
#include "rpmlib.h"

struct sharedFile {
    int mainFileNumber;
    int secRecOffset;
    int secFileNumber;
} ;

int findSharedFiles(rpmdb db, int offset, char ** fileList, int fileCount,
		    struct sharedFile ** listPtr, int * listCountPtr);
int runInstScript(char * prefix, Header h, int scriptTag, int progTag,
	          int arg, int norunScripts, int err);
/* this looks for triggers in the database which h would set off */
int runTriggers(char * root, rpmdb db, int sense, Header h,
		int countCorrection);
/* while this looks for triggers in h which are set off by things in the db
   database to calculate arguments to the trigger */
int runImmedTriggers(char * root, rpmdb db, int sense, Header h,
		     int countCorrection);

#endif	/* H_INSTALL */
