/** \ingroup signature.c
 * \file lib/md5sum.c
 * Generate/check MD5 Message Digests.
 * Compile and link with md5.c.  If you don't have getopt() in your library
 * also include getopt.c.  For MSDOS you can also link with the wildcard
 * initialization function (wildargs.obj for Turbo C and setargv.obj for MSC)
 * so that you can use wildcards on the commandline.
 *
 * Written March 1993 by Branko Lankester
 * Modified June 1993 by Colin Plumb for altered md5.c.
 * Modified October 1995 by Erik Troan for RPM
 */
#include "system.h"

#include "md5.h"

/**
 * Calculate MD5 sum for file.
 * @param fn		file name
 * @retval digest	address of md5sum
 * @param asAscii	return md5sum as ascii string?
 * @param brokenEndian	calculate broken MD5 sum?
 * @return		0 on success, 1 on error
 */
static int domd5(const char * fn, unsigned char * digest, int asAscii,
		 int brokenEndian) {
    unsigned char buf[1024];
    unsigned char bindigest[16];
    FILE * fp;
    MD5_CTX ctx;
    int n;

    memset(bindigest, 0, sizeof(bindigest));
    fp = fopen(fn, "r");
    if (!fp) {
	return 1;
    }

    rpmMD5Init(&ctx, brokenEndian);
    while ((n = fread(buf, 1, sizeof(buf), fp)) > 0)
	    rpmMD5Update(&ctx, buf, n);
    rpmMD5Final(bindigest, &ctx);
    if (ferror(fp)) {
	fclose(fp);
	return 1;
    }

    if (!asAscii) {
	memcpy(digest, bindigest, 16);
    } else {
	sprintf(digest, "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x"
			"%02x%02x%02x%02x%02x",
		(unsigned)bindigest[0],
		(unsigned)bindigest[1],
		(unsigned)bindigest[2],
		(unsigned)bindigest[3],
		(unsigned)bindigest[4],
		(unsigned)bindigest[5],
		(unsigned)bindigest[6],
		(unsigned)bindigest[7],
		(unsigned)bindigest[8],
		(unsigned)bindigest[9],
		(unsigned)bindigest[10],
		(unsigned)bindigest[11],
		(unsigned)bindigest[12],
		(unsigned)bindigest[13],
		(unsigned)bindigest[14],
		(unsigned)bindigest[15]);

    }
    fclose(fp);

    return 0;
}

int mdbinfile(const char *fn, unsigned char *bindigest) {
    return domd5(fn, bindigest, 0, 0);
}

int mdbinfileBroken(const char *fn, unsigned char *bindigest) {
    return domd5(fn, bindigest, 0, 1);
}

int mdfile(const char *fn, unsigned char *digest) {
    return domd5(fn, digest, 1, 0);
}

int mdfileBroken(const char *fn, unsigned char *digest) {
    return domd5(fn, digest, 1, 1);
}
