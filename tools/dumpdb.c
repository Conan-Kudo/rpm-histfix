#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "header.h"
#include "rpmlib.h"

void main(int argc, char ** argv)
{
    Header h;
    int offset;
    int dspBlockNum = 0;			/* default to all */
    int blockNum = 0;
    rpmdb db;

    if (argc == 2) {
	dspBlockNum = atoi(argv[1]);
    } else if (argc != 1) {
	fprintf(stderr, "dumpdb <block num>\n");
	exit(1);
    }

    if (!rpmdbOpen("", &db, O_RDONLY, 0644)) {
	fprintf(stderr, "cannot open /var/lib/rpm/packages.rpm\n");
	exit(1);
    }

    offset = rpmdbFirstRecNum(db);
    while (offset) {
	blockNum++;

	if (!dspBlockNum || dspBlockNum == blockNum) {
	    h = rpmdbGetRecord(db, offset);
	    if (!h) {
		fprintf(stderr, "readHeader failed\n");
		exit(1);
	    }
	  
	    dumpHeader(h, stdout, 1);
	    printf("Offset: %d\n", offset);
	    freeHeader(h);
	}
    
	if (dspBlockNum && blockNum > dspBlockNum) exit(0);

	offset = rpmdbNextRecNum(db, offset);
    }

    rpmdbClose(db);
}

  
