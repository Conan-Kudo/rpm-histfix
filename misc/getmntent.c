#include "miscfn.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __aix__
#define COMMENTCHAR '*'
#else
#define COMMENTCHAR '#'
#endif

struct mntent *getmntent(FILE *filep) {
    static struct mntent item = { NULL };
    char buf[1024], * start;
    char * chptr;

    if (item.mnt_dir) {
	free(item.mnt_dir);
    }
    
    while (fgets(buf, sizeof(buf) - 1, filep)) {
	/* chop off \n */
	buf[strlen(buf) - 1] = '\0';

	chptr = buf;
	while (isspace(*chptr)) chptr++;

	if (*chptr == COMMENTCHAR) continue;

	#if __aix__
	    /* aix uses a screwed up file format */
	    if (*chptr == '/') {
		start = chptr;
		while (*chptr != ':') chptr++;
		*chptr = '\0';
		item.mnt_dir = strdup(start);
		return &item;
	    }
	#else 
	    while (!isspace(*chptr) && (*chptr)) chptr++;
	    if (!*chptr) return NULL;

	    while (isspace(*chptr) && (*chptr)) chptr++;
	    if (!*chptr) return NULL;
	    start = chptr;
	
	    while (!isspace(*chptr) && (*chptr)) chptr++;
	    *chptr = '\0';

	    item.mnt_dir = strdup(start);
	    return &item;
	#endif
    }

    return NULL;
}
