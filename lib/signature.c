/* signature.c - RPM signature functions */

/* NOTES
 *
 * Things have been cleaned up wrt PGP.  We can now handle
 * signatures of any length (which means you can use any
 * size key you like).  We also honor PGPPATH finally.
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <asm/byteorder.h>
#include <fcntl.h>
#include <strings.h>

#include "signature.h"
#include "md5.h"
#include "rpmlib.h"
#include "rpmlead.h"
#include "rpmerr.h"

static int makePGPSignature(char *file, void **sig, int_32 *size,
			    char *passPhrase);
static int checkSize(int fd, int size, int sigsize);
static int verifySizeSignature(char *datafile, int_32 size, char *result);
static int verifyMD5Signature(char *datafile, unsigned char *sig,
			      char *result);
static int verifyPGPSignature(char *datafile, void *sig,
			      int count, char *result);
static int checkPassPhrase(char *passPhrase);

int sigLookupType(void)
{
    char *name;

    if (! (name = getVar(RPMVAR_SIGTYPE))) {
	return 0;
    }

    if (!strcasecmp(name, "none")) {
	return 0;
    } else if (!strcasecmp(name, "pgp")) {
	return SIGTAG_PGP;
    } else {
	return -1;
    }
}

/* readSignature() emulates the new style signatures if it finds an */
/* old-style one.  It also immediately verifies the header+archive  */
/* size and returns an error if it doesn't match.                   */

int readSignature(int fd, Header *header, short sig_type)
{
    unsigned char buf[2048];
    int sigSize, pad;
    int_32 type, count;
    int_32 *archSize;
    Header h;

    if (header) {
	*header = NULL;
    }
    
    switch (sig_type) {
      case RPMSIG_NONE:
	message(MESS_DEBUG, "No signature\n");
	break;
      case RPMSIG_PGP262_1024:
	message(MESS_DEBUG, "Old PGP signature\n");
	/* These are always 256 bytes */
	if (read(fd, buf, 256) != 256) {
	    return 1;
	}
	if (header) {
	    *header = newHeader();
	    addEntry(*header, SIGTAG_PGP, BIN_TYPE, buf, 152);
	}
	break;
      case RPMSIG_MD5:
      case RPMSIG_MD5_PGP:
	error(RPMERR_BADSIGTYPE,
	      "Old (internal-only) signature!  How did you get that!?");
	return 1;
	break;
      case RPMSIG_HEADERSIG:
	message(MESS_DEBUG, "New Header signature\n");
	/* This is a new style signature */
	h = readHeader(fd, HEADER_MAGIC);
	if (! h) {
	    return 1;
	}
	sigSize = sizeofHeader(h, HEADER_MAGIC);
	pad = (8 - (sigSize % 8)) % 8; /* 8-byte pad */
	message(MESS_DEBUG, "Signature size: %d\n", sigSize);
	message(MESS_DEBUG, "Signature pad : %d\n", pad);
	if (! getEntry(h, SIGTAG_SIZE, &type, (void **)&archSize, &count)) {
	    freeHeader(h);
	    return 1;
	}
	if (checkSize(fd, *archSize, sigSize + pad)) {
	    freeHeader(h);
	    return 1;
	}
	if (pad) {
	    if (read(fd, buf, pad) != pad) {
		freeHeader(h);
		return 1;
	    }
	}
	if (header) {
	    *header = h;
	} else {
	    freeHeader(h);
	}
	break;
      default:
	return 1;
    }

    return 0;
}

int writeSignature(int fd, Header header)
{
    int sigSize, pad;
    unsigned char buf[8];
    
    writeHeader(fd, header, HEADER_MAGIC);
    sigSize = sizeofHeader(header, HEADER_MAGIC);
    pad = (8 - (sigSize % 8)) % 8;
    if (pad) {
	message(MESS_DEBUG, "Signature size: %d\n", sigSize);
	message(MESS_DEBUG, "Signature pad : %d\n", pad);
	memset(buf, 0, pad);
	write(fd, buf, pad);
    }
    return 0;
}

Header newSignature(void)
{
    return newHeader();
}

void freeSignature(Header h)
{
    freeHeader(h);
}

int addSignature(Header header, char *file, int_32 sigTag, char *passPhrase)
{
    struct stat statbuf;
    int_32 size;
    unsigned char buf[16];
    void *sig;
    
    switch (sigTag) {
      case SIGTAG_SIZE:
	stat(file, &statbuf);
	size = statbuf.st_size;
	addEntry(header, SIGTAG_SIZE, INT32_TYPE, &size, 1);
	break;
      case SIGTAG_MD5:
	mdbinfile(file, buf);
	addEntry(header, sigTag, BIN_TYPE, buf, 16);
	break;
      case SIGTAG_PGP:
	makePGPSignature(file, &sig, &size, passPhrase);
	addEntry(header, sigTag, BIN_TYPE, sig, size);
	break;
    }

    return 0;
}

static int makePGPSignature(char *file, void **sig, int_32 *size,
			    char *passPhrase)
{
    char name[1024];
    char sigfile[1024];
    int pid, status;
    int fd, inpipe[2];
    FILE *fpipe;
    struct stat statbuf;

    sprintf(name, "+myname=\"%s\"", getVar(RPMVAR_PGP_NAME));

    sprintf(sigfile, "%s.sig", file);

    pipe(inpipe);
    
    if (!(pid = fork())) {
	close(0);
	dup2(inpipe[0], 3);
	close(inpipe[1]);
	setenv("PGPPASSFD", "3", 1);
	if (getVar(RPMVAR_PGP_PATH)) {
	    setenv("PGPPATH", getVar(RPMVAR_PGP_PATH), 1);
	}
	/* setenv("PGPPASS", passPhrase, 1); */
	execlp("pgp", "pgp",
	       "+batchmode=on", "+verbose=0", "+armor=off",
	       name, "-sb", file, sigfile,
	       NULL);
	error(RPMERR_EXEC, "Couldn't exec pgp");
	exit(RPMERR_EXEC);
    }

    fpipe = fdopen(inpipe[1], "w");
    close(inpipe[0]);
    fprintf(fpipe, "%s\n", passPhrase);
    fclose(fpipe);

    waitpid(pid, &status, 0);
    if (!WIFEXITED(status) || WEXITSTATUS(status)) {
	error(RPMERR_SIGGEN, "pgp failed");
	return 1;
    }

    if (stat(sigfile, &statbuf)) {
	/* PGP failed to write signature */
	unlink(sigfile);  /* Just in case */
	error(RPMERR_SIGGEN, "pgp failed to write signature");
	return 1;
    }

    *size = statbuf.st_size;
    message(MESS_DEBUG, "PGP sig size: %d\n", *size);
    *sig = malloc(*size);
    
    fd = open(sigfile, O_RDONLY);
    if (read(fd, *sig, *size) != *size) {
	unlink(sigfile);
	close(fd);
	free(*sig);
	error(RPMERR_SIGGEN, "unable to read the signature");
	return 1;
    }
    close(fd);
    unlink(sigfile);

    message(MESS_DEBUG, "Got %d bytes of PGP sig\n", *size);
    
    return 0;
}

static int checkSize(int fd, int size, int sigsize)
{
    int headerArchiveSize;
    struct stat statbuf;

    fstat(fd, &statbuf);
    headerArchiveSize = statbuf.st_size - sizeof(struct rpmlead) - sigsize;

    message(MESS_DEBUG, "sigsize         : %d\n", sigsize);
    message(MESS_DEBUG, "Header + Archive: %d\n", headerArchiveSize);
    message(MESS_DEBUG, "expected size   : %d\n", size);

    return size - headerArchiveSize;
}

int verifySignature(char *file, int_32 sigTag, void *sig, int count,
		    char *result)
{
    switch (sigTag) {
      case SIGTAG_SIZE:
	if (verifySizeSignature(file, *(int_32 *)sig, result)) {
	    return RPMSIG_BAD;
	}
	break;
      case SIGTAG_MD5:
	if (verifyMD5Signature(file, sig, result)) {
	    return 1;
	}
	break;
      case SIGTAG_PGP:
	return verifyPGPSignature(file, sig, count, result);
	break;
      default:
	sprintf(result, "Do not know how to verify sig type %d\n", sigTag);
	return RPMSIG_UNKNOWN;
    }

    return RPMSIG_OK;
}

static int verifySizeSignature(char *datafile, int_32 size, char *result)
{
    struct stat statbuf;

    stat(datafile, &statbuf);
    if (size != statbuf.st_size) {
	sprintf(result, "Header+Archive size mismatch.\n"
		"Expected %d, saw %d.\n",
		size, (int)statbuf.st_size);
	return 1;
    }

    sprintf(result, "Header+Archive size OK: %d bytes\n", size);
    return 0;
}

static int verifyMD5Signature(char *datafile, unsigned char *sig, char *result)
{
    unsigned char md5sum[16];

    mdbinfile(datafile, md5sum);
    if (memcmp(md5sum, sig, 16)) {
	sprintf(result, "MD5 sum mismatch\n"
		"Expected: %02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x"
		"%02x%02x%02x%02x%02x\n"
		"Saw     : %02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x"
		"%02x%02x%02x%02x%02x\n",
		sig[0],  sig[1],  sig[2],  sig[3],
		sig[4],  sig[5],  sig[6],  sig[7],
		sig[8],  sig[9],  sig[10], sig[11],
		sig[12], sig[13], sig[14], sig[15],
		md5sum[0],  md5sum[1],  md5sum[2],  md5sum[3],
		md5sum[4],  md5sum[5],  md5sum[6],  md5sum[7],
		md5sum[8],  md5sum[9],  md5sum[10], md5sum[11],
		md5sum[12], md5sum[13], md5sum[14], md5sum[15]);
	return 1;
    }

    sprintf(result, "MD5 sum OK: %02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x"
                    "%02x%02x%02x%02x%02x\n",
	    md5sum[0],  md5sum[1],  md5sum[2],  md5sum[3],
	    md5sum[4],  md5sum[5],  md5sum[6],  md5sum[7],
	    md5sum[8],  md5sum[9],  md5sum[10], md5sum[11],
	    md5sum[12], md5sum[13], md5sum[14], md5sum[15]);

    return 0;
}

static int verifyPGPSignature(char *datafile, void *sig,
			      int count, char *result)
{
    int pid, status, outpipe[2], sfd;
    char *sigfile;
    unsigned char buf[8192];
    FILE *file;
    int res = RPMSIG_OK;

    /* Write out the signature */
    sigfile = tempnam("/var/tmp", "rpmsig");
    sfd = open(sigfile, O_WRONLY|O_CREAT|O_TRUNC, 0644);
    write(sfd, sig, count);
    close(sfd);

    /* Now run PGP */
    pipe(outpipe);

    if (!(pid = fork())) {
	close(1);
	close(outpipe[0]);
	dup2(outpipe[1], 1);
	if (getVar(RPMVAR_PGP_PATH)) {
	    setenv("PGPPATH", getVar(RPMVAR_PGP_PATH), 1);
	}
	execlp("pgp", "pgp",
	       "+batchmode=on", "+verbose=0",
	       sigfile, datafile,
	       NULL);
	printf("exec failed!\n");
	error(RPMERR_EXEC, "Could not run pgp.  Use --nopgp to skip PGP checks.");
	exit(RPMERR_EXEC);
    }

    close(outpipe[1]);
    file = fdopen(outpipe[0], "r");
    result[0] = '\0';
    while (fgets(buf, 1024, file)) {
	if (strncmp("File '", buf, 6) &&
	    strncmp("Text is assu", buf, 12) &&
	    buf[0] != '\n') {
	    strcat(result, buf);
	}
	if (!strncmp("WARNING: Can't find the right public key", buf, 40)) {
	    res = RPMSIG_NOKEY;
	}
    }
    fclose(file);

    waitpid(pid, &status, 0);
    unlink(sigfile);
    if (!res && (!WIFEXITED(status) || WEXITSTATUS(status))) {
	res = RPMSIG_BAD;
    }
    
    return res;
}

char *getPassPhrase(char *prompt)
{
    char *pass;

    if (! getVar(RPMVAR_PGP_NAME)) {
	error(RPMERR_SIGGEN,
	      "You must set \"pgp_name:\" in your rpmrc file");
	return NULL;
    }

    if (prompt) {
        pass = getpass(prompt);
    } else {
        pass = getpass("");
    }

    if (checkPassPhrase(pass)) {
	return NULL;
    }

    return pass;
}

static int checkPassPhrase(char *passPhrase)
{
    char name[1024];
    int passPhrasePipe[2];
    FILE *fpipe;
    int pid, status;
    int fd;

    sprintf(name, "+myname=\"%s\"", getVar(RPMVAR_PGP_NAME));

    pipe(passPhrasePipe);
    if (!(pid = fork())) {
	close(0);
	close(1);
	if (! isVerbose()) {
	    close(2);
	}
	if ((fd = open("/dev/null", O_RDONLY)) != 0) {
	    dup2(fd, 0);
	}
	if ((fd = open("/dev/null", O_WRONLY)) != 1) {
	    dup2(fd, 1);
	}
	dup2(passPhrasePipe[0], 3);
	setenv("PGPPASSFD", "3", 1);
	if (getVar(RPMVAR_PGP_PATH)) {
	    setenv("PGPPATH", getVar(RPMVAR_PGP_PATH), 1);
	}
	execlp("pgp", "pgp",
	       "+batchmode=on", "+verbose=0",
	       name, "-sf",
	       NULL);
	error(RPMERR_EXEC, "Couldn't exec pgp");
	exit(RPMERR_EXEC);
    }

    fpipe = fdopen(passPhrasePipe[1], "w");
    close(passPhrasePipe[0]);
    fprintf(fpipe, "%s\n", passPhrase);
    fclose(fpipe);

    waitpid(pid, &status, 0);
    if (!WIFEXITED(status) || WEXITSTATUS(status)) {
	return 1;
    }

    /* passPhrase is good */
    return 0;
}
