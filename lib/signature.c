/* signature.c - RPM signature functions */

/* NOTES
 *
 * A PGP 2.6.2 1024 bit key generates a 152 byte signature
 * A ViaCryptPGP 2.7.1 1024 bit key generates a 152 byte signature
 * A PGP 2.6.2  768 bit key generates a 120 byte signature
 *
 * This code only only works with 1024 bit keys!
 *
 * Sometimes we get 151 byte signatures.  Not sure why, but if we
 * do, we toss it and try once more to get a 152 bytes signature.
 * If we still get a 151 byte sig, fail.  :-(
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <strings.h>

#include "signature.h"
#include "rpmlib.h"
#include "rpmerr.h"

static int makePGPSignature(char *file, int ofd, char *passPhrase);
static int verifyPGPSignature(int fd, void *sig, char *result);
static int checkPassPhrase(char *passPhrase);

int readSignature(int fd, short sig_type, void **sig)
{
    unsigned char pgpbuf[256];
    
    switch (sig_type) {
    case RPMSIG_NONE:
	if (sig) {
	    *sig = NULL;
	}
	break;
    case RPMSIG_PGP262_1024:
	if (read(fd, pgpbuf, 256) != 256) {
	   return 0;
	}
	if (sig) {
	    *sig = malloc(152);
	    memcpy(*sig, pgpbuf, 152);
	}
	break;
    }

    return 1;
}

int makeSignature(char *file, short sig_type, int ofd, char *passPhrase)
{
    int res;
    
    switch (sig_type) {
    case RPMSIG_PGP262_1024:
	if ((res = makePGPSignature(file, ofd, passPhrase)) == -1) {
	    /* This is the 151 byte sig hack */
	    return makePGPSignature(file, ofd, passPhrase);
	}
        return res;
	break;
    case RPMSIG_NONE:
    }

    return 0;
}

#if 0
void ttycbreak(void)
{
    int tty;

    if ((tty = open("/dev/tty", O_RDWR)) < 0) {
	fprintf(stderr, "Unable to open tty.  Using standard input.\n");
	tty = 0;
    }

    
}
#endif

char *getPassPhrase(char *prompt)
{
    char *pass;

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
    char secring[1024];
    char pubring[1024];
    char name[1024];
    int passPhrasePipe[2];
    FILE *fpipe;
    int pid, status;
    int fd;

    sprintf(name, "+myname=\"%s\"", getVar(RPMVAR_PGP_NAME));
    sprintf(secring, "+secring=\"%s\"", getVar(RPMVAR_PGP_SECRING));
    sprintf(pubring, "+pubring=\"%s\"", getVar(RPMVAR_PGP_PUBRING));

    pipe(passPhrasePipe);
    if (!(pid = fork())) {
	close(0);
	close(1);
	close(2);
	if ((fd = open("/dev/null", O_RDONLY)) != 0) {
	    dup2(fd, 0);
	}
	if ((fd = open("/dev/null", O_WRONLY)) != 1) {
	    dup2(fd, 1);
	}
	dup2(passPhrasePipe[0], 3);
	setenv("PGPPASSFD", "3", 1);
	setenv("PGPPATH", getVar(RPMVAR_PGP_PATH), 1);
	execlp("pgp", "pgp",
	       "+batchmode=on", "+verbose=0",
	       name, secring, pubring,
	       "-sf",
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

static int makePGPSignature(char *file, int ofd, char *passPhrase)
{
    char secring[1024];
    char pubring[1024];
    char name[1024];
    char sigfile[1024];
    int pid, status;
    int fd, inpipe[2];
    unsigned char sigbuf[256];   /* 1024bit sig is 152 bytes */
    FILE *fpipe;
    struct stat statbuf;

    sprintf(name, "+myname=\"%s\"", getVar(RPMVAR_PGP_NAME));
    sprintf(secring, "+secring=\"%s\"", getVar(RPMVAR_PGP_SECRING));
    sprintf(pubring, "+pubring=\"%s\"", getVar(RPMVAR_PGP_PUBRING));

    sprintf(sigfile, "%s.sig", file);

    pipe(inpipe);
    
    if (!(pid = fork())) {
	close(0);
	dup2(inpipe[0], 3);
	close(inpipe[1]);
	setenv("PGPPASSFD", "3", 1);
	setenv("PGPPATH", getVar(RPMVAR_PGP_PATH), 1);
	/* setenv("PGPPASS", passPhrase, 1); */
	execlp("pgp", "pgp",
	       "+batchmode=on", "+verbose=0", "+armor=off",
	       name, secring, pubring,
	       "-sb", file, sigfile,
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
    if (statbuf.st_size != 152) {
	/* 151 byte sig hack */
	unlink(sigfile);
	error(RPMERR_SIGGEN, "pgp failed to write 152 byte signature");
	return -1;
    }
    
    fd = open(sigfile, O_RDONLY);
    if (read(fd, sigbuf, 152) != 152) {       /* signature is 152 bytes */
	unlink(sigfile);
	close(fd);
	error(RPMERR_SIGGEN, "unable to read 152 bytes of signature");
	return 1;
    }
    close(fd);
    unlink(sigfile);

    write(ofd, sigbuf, 256);   /* We write an even 256 bytes */
    
    return 0;
}

static int verifyPGPSignature(int fd, void *sig, char *result)
{
    char *sigfile;
    char *datafile;
    int count, sfd, pid, status, outpipe[2];
    unsigned char buf[8192];
    char secring[1024];
    char pubring[1024];
    FILE *file;

    /* Write out the signature */
    sigfile = tempnam("/var/tmp", "rpmsig");
    sfd = open(sigfile, O_WRONLY|O_CREAT|O_TRUNC, 0644);
    write(sfd, sig, 152);
    close(sfd);

    /* Write out the data */
    datafile = tempnam("/var/tmp", "rpmsig");
    sfd = open(datafile, O_WRONLY|O_CREAT|O_TRUNC, 0644);
    while((count = read(fd, buf, 8192)) > 0) {
	write(sfd, buf, count);
    }
    close(sfd);

    /* Now run PGP */
    sprintf(secring, "+secring=\"%s\"", getVar(RPMVAR_PGP_SECRING));
    sprintf(pubring, "+pubring=\"%s\"", getVar(RPMVAR_PGP_PUBRING));
    pipe(outpipe);

    if (!(pid = fork())) {
	close(1);
	close(outpipe[0]);
	dup2(outpipe[1], 1);
	setenv("PGPPATH", getVar(RPMVAR_PGP_PATH), 1);
	execlp("pgp", "pgp",
	       "+batchmode=on", "+verbose=0",
	       secring, pubring,
	       sigfile, datafile,
	       NULL);
	printf("exec failed!\n");
	error(RPMERR_EXEC, "Couldn't exec pgp");
	exit(RPMERR_EXEC);
    }

    close(outpipe[1]);
    file = fdopen(outpipe[0], "r");
    result[0] = '\0';
    while (fgets(buf, 1024, file)) {
	if (strncmp("File '", buf, 6) && strncmp("Text is assu", buf, 12)) {
	    strcat(result, buf);
	}
    }
    fclose(file);

    waitpid(pid, &status, 0);
    unlink(datafile);
    unlink(sigfile);
    if (!WIFEXITED(status) || WEXITSTATUS(status)) {
	return(1);
    }
    
    return(0);
}

int verifySignature(int fd, short sig_type, void *sig, char *result)
{
    switch (sig_type) {
    case RPMSIG_NONE:
	strcpy(result, "No signature information available\n");
	return RPMSIG_NOSIG;
	break;
    case RPMSIG_PGP262_1024:
	if (verifyPGPSignature(fd, sig, result)) {
	    return RPMSIG_BADSIG;
	}
	break;
    default:
	sprintf(result, "Unimplemented signature type\n");
	return RPMSIG_UNKNOWNSIG;
	break;
    }

    return RPMSIG_SIGOK;
}

unsigned short sigLookupType(void)
{
    char *name;

    if (! (name = getVar(RPMVAR_SIGTYPE))) {
	return RPMSIG_NONE;
    }

    if (!strcasecmp(name, "none")) {
	return RPMSIG_NONE;
    } else if (!strcasecmp(name, "pgp")) {
	return RPMSIG_PGP262_1024;
    } else {
	return RPMSIG_BAD;
    }
}
