/* rpmarchive: spit out the main archive portion of a package */

#include "system.h"

#include <zlib.h>

#include "rpmlib.h"

char *zlib_err [] = {
   "No",
   "Unix",
   "Data",
   "Memory",
   "Buffer",
   "Version"
};

int main(int argc, char **argv)
{
    int fd;
    Header hd;
    int rc, isSource;
    char buffer[1024];
    int ct;
    gzFile stream;
    
    if (argc == 1) {
	fd = 0;
    } else {
	fd = open(argv[1], O_RDONLY, 0644);
    }

    if (fd < 0) {
	perror("cannot open package");
	exit(1);
    }

    rc = rpmReadPackageHeader(fd, &hd, &isSource, NULL, NULL);
    if (rc == 1) {
	fprintf(stderr, "argument is not an RPM package\n");
	exit(1);
    } else if (rc) {
	fprintf(stderr, "error reading header from package\n");
	exit(1);
    }

    stream = gzdopen(fd, "r");

    while ((ct = gzread(stream, &buffer, 1024)) > 0) {
	write(1, &buffer, ct);
    }
    if (ct < 0){
        int zerror;
       
        gzerror (stream, &zerror);
        if (zerror == Z_ERRNO){
	    perror ("While uncompressing");
	    gzclose(stream);
	    return 1;
	}
        fprintf (stderr, "rpm2cpio: zlib: %s error\n", zlib_err [-zerror - 1]);
    }

    gzclose(stream);

    return 0;
}
