/* RPM - Copyright (C) 1995 Red Hat Software
 * 
 * pack.c - routines for packaging
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>
#include <ftw.h>

#include "header.h"
#include "specP.h"
#include "rpmlib.h"
#include "rpmerr.h"
#include "misc.h"
#include "pack.h"
#include "messages.h"
#include "md5.h"
#include "var.h"

#define BINARY_HEADER 0
#define SOURCE_HEADER 1

static unsigned char magic[] = { 0xed, 0xab, 0xee, 0xdb };

struct file_entry {
    char file[1024];
    int isdoc;
    int isconf;
    struct stat statbuf;
    struct file_entry *next;
};

static int cpio_gzip(Header header, int fd);
static int writeMagic(Spec s, struct PackageRec *pr,
		      int fd, char *name, unsigned char type);
static int add_file(struct file_entry **festack,
		    char *name, int isdoc, int isconf, int isdir);
static int process_filelist(Header header, StringBuf sb);
static int add_file_aux(char *file, struct stat *sb, int flag);

static int writeMagic(Spec s, struct PackageRec *pr,
		      int fd, char *name, unsigned char type)
{
    char header[74];

    header[0] = 2; /* major */
    header[1] = 0; /* minor */
    header[2] = 0;
    header[3] = type;
    header[4] = 0;
    header[5] = getArchNum();
    strncpy(&(header[6]), name, 66);
    header[72] = 0;
    header[73] = getOsNum();
    
    write(fd, &magic, 4);
    write(fd, &header, 74);

    return 0;
}

static int cpio_gzip(Header header, int fd)
{
    char **f, *s;
    int count;
    FILE *inpipeF;
    int cpioPID, gzipPID;
    int inpipe[2];
    int outpipe[2];
    int status;

    pipe(inpipe);
    pipe(outpipe);
    
    if (!(cpioPID = fork())) {
	close(0);
	close(1);
	close(inpipe[1]);
	close(outpipe[0]);
	close(fd);
	
	dup2(inpipe[0], 0);  /* Make stdin the in pipe */
	dup2(outpipe[1], 1); /* Make stdout the out pipe */

	if (getVar(RPMVAR_ROOT)) {
	    if (chdir(getVar(RPMVAR_ROOT))) {
		error(RPMERR_EXEC, "Couldn't chdir to %s",
		      getVar(RPMVAR_ROOT));
		exit(RPMERR_EXEC);
	    }
	} else {
	    chdir("/");
	}

	execlp("cpio", "cpio", (isVerbose()) ? "-ovH" : "-oH", "crc", NULL);
	error(RPMERR_EXEC, "Couldn't exec cpio");
	exit(RPMERR_EXEC);
    }
    if (cpioPID < 0) {
	error(RPMERR_FORK, "Couldn't fork");
	return RPMERR_FORK;
    }

    if (!(gzipPID = fork())) {
	close(0);
	close(1);
	close(inpipe[1]);
	close(inpipe[0]);
	close(outpipe[1]);

	dup2(outpipe[0], 0); /* Make stdin the out pipe */
	dup2(fd, 1);         /* Make stdout the passed-in file descriptor */
	close(fd);

	execlp("gzip", "gzip", "-c9fn", NULL);
	error(RPMERR_EXEC, "Couldn't exec gzip");
	exit(RPMERR_EXEC);
    }
    if (gzipPID < 0) {
	error(RPMERR_FORK, "Couldn't fork");
	return RPMERR_FORK;
    }

    close(inpipe[0]);
    close(outpipe[1]);
    close(outpipe[0]);

    if (getEntry(header, RPMTAG_FILENAMES, NULL, (void **) &f, &count)) {
	inpipeF = fdopen(inpipe[1], "w");
	while (count--) {
	    /* This business strips the leading "/" for cpio */
	    s = *f;
	    s++;
	    fprintf(inpipeF, "%s\n", s);
	    f++;
	}
	fclose(inpipeF);
    } else {
	close(inpipe[1]);
    }

    waitpid(cpioPID, &status, 0);
    if (!WIFEXITED(status) || WEXITSTATUS(status)) {
	error(RPMERR_CPIO, "cpio failed");
	return 1;
    }
    waitpid(gzipPID, &status, 0);
    if (!WIFEXITED(status) || WEXITSTATUS(status)) {
	error(RPMERR_GZIP, "gzip failed");
	return 1;
    }

    return 0;
}

/* Need three globals to keep track of things in ftw() */
static int Gisdoc;
static int Gisconf;
static int Gcount;
static struct file_entry **Gfestack;

static int add_file(struct file_entry **festack,
		    char *name, int isdoc, int isconf, int isdir)
{
    struct file_entry *p;
    char fullname[1024];

    /* Set these up for ftw() */
    Gfestack = festack;
    Gisdoc = isdoc;
    Gisconf = isconf;

    /* XXX do globbing and %doc expansion here */

    p = malloc(sizeof(struct file_entry));
    strcpy(p->file, name);
    p->isdoc = isdoc;
    p->isconf = isconf;
    if (getVar(RPMVAR_ROOT)) {
	sprintf(fullname, "%s%s", getVar(RPMVAR_ROOT), name);
    } else {
	strcpy(fullname, name);
    }
    if (lstat(fullname, &p->statbuf)) {
	return 0;
    }

    if ((! isdir) && S_ISDIR(p->statbuf.st_mode)) {
	/* This means we need to decend with ftw() */
	Gcount = 0;

	ftw(fullname, add_file_aux, 3);
	
	free(p);

	return Gcount;
    } else {
	/* Link it in */
	p->next = *festack;
	*festack = p;

	message(MESS_DEBUG, "ADDING: %s\n", name);

	/* return number of entries added */
	return 1;
    }
}

static int add_file_aux(char *file, struct stat *sb, int flag)
{
    char *name = file;

    if (getVar(RPMVAR_ROOT)) {
	name += strlen(getVar(RPMVAR_ROOT));
    }

    /* The 1 will cause add_file() to *not* descend */
    /* directories -- ftw() is already doing it!    */
    Gcount += add_file(Gfestack, name, Gisdoc, Gisconf, 1);

    return 0; /* for ftw() */
}

static int process_filelist(Header header, StringBuf sb)
{
    char buf[1024];
    char **files, **fp;
    struct file_entry *fes, *fest;
    int isdoc, isconf, isdir;
    char *filename, *s;
    char *str;
    int count = 0;
    int c;

    fes = NULL;

    str = getStringBuf(sb);
    files = splitString(str, strlen(str), '\n');
    fp = files;

    while (*fp) {
	strcpy(buf, *fp);  /* temp copy */
	isdoc = 0;
	isconf = 0;
	isdir = 0;
	filename = NULL;
	s = strtok(buf, " \t\n");
	while (s) {
	    if (!strcmp(s, "%doc")) {
		isdoc = 1;
	    } else if (!strcmp(s, "%config")) {
		isconf = 1;
	    } else if (!strcmp(s, "%dir")) {
		isdir = 1;
	    } else {
		filename = s;
	    }
	    s = strtok(NULL, " \t\n");
	}
	if (! filename) {
	    fp++;
	    continue;
	}

	/* check that file starts with leading "/" */
	if (*filename != '/') {
	    error(RPMERR_BADSPEC, "File needs leading \"/\": %s", filename);
	    return(RPMERR_BADSPEC);
	}

	c = add_file(&fes, filename, isdoc, isconf, isdir);
	if (! c) {
	    error(RPMERR_BADSPEC, "File not found: %s", filename);
	    return(RPMERR_BADSPEC);
	}
	count += c;
	
	fp++;
    }

    /* If there are no files, don't add anything to the header */
    if (count) {
	char ** fileList;
	char ** fileMD5List;
	char ** fileLinktoList;
	int_32 * fileSizeList;
	int_32 * fileUIDList;
	int_32 * fileGIDList;
	int_32 * fileMtimesList;
	int_32 * fileFlagsList;
	int_16 * fileModesList;
	int_16 * fileRDevsList;

	fileList = malloc(sizeof(char *) * count);
	fileLinktoList = malloc(sizeof(char *) * count);
	fileMD5List = malloc(sizeof(char *) * count);
	fileSizeList = malloc(sizeof(int_32) * count);
	fileUIDList = malloc(sizeof(int_32) * count);
	fileGIDList = malloc(sizeof(int_32) * count);
	fileMtimesList = malloc(sizeof(int_32) * count);
	fileFlagsList = malloc(sizeof(int_32) * count);
	fileModesList = malloc(sizeof(int_16) * count);
	fileRDevsList = malloc(sizeof(int_16) * count);

	fest = fes;
	c = count;
	while (c--) {
	    fileList[c] = strdup(fes->file);
	    if (S_ISREG(fes->statbuf.st_mode)) {
		mdfile(fes->file, buf);
		fileMD5List[c] = strdup(buf);
		message(MESS_DEBUG, "md5(%s) = %s\n", fes->file, buf);
	    } else {
		/* This is stupid */
		fileMD5List[c] = strdup("");
	    }
	    fileSizeList[c] = fes->statbuf.st_size;
	    fileUIDList[c] = fes->statbuf.st_uid;
	    fileGIDList[c] = fes->statbuf.st_gid;
	    fileMtimesList[c] = fes->statbuf.st_mtime;
	    fileFlagsList[c] = 0;
	    if (fes->isdoc) 
		fileFlagsList[c] |= RPMFILE_DOC;
	    if (fes->isconf)
		fileFlagsList[c] |= RPMFILE_CONFIG;

	    fileModesList[c] = fes->statbuf.st_mode;
	    fileRDevsList[c] = fes->statbuf.st_rdev;

	    if (S_ISLNK(fes->statbuf.st_mode)) {
		if (getVar(RPMVAR_ROOT)) {
		    sprintf(buf, "%s%s", getVar(RPMVAR_ROOT), fes->file);
		} else {
		    strcpy(buf, fes->file);
		}
		readlink(buf, buf, 1024);
		fileLinktoList[c] = strdup(buf);
	    } else {
		/* This is stupid */
		fileLinktoList[c] = strdup("");
	    }

	    fes = fes->next;
	}

	/* Add the header entries */
	c = count;
	addEntry(header, RPMTAG_FILENAMES, STRING_ARRAY_TYPE, fileList, c);
	addEntry(header, RPMTAG_FILELINKTOS, STRING_ARRAY_TYPE, fileLinktoList, c);
	addEntry(header, RPMTAG_FILEMD5S, STRING_ARRAY_TYPE, fileMD5List, c);
	addEntry(header, RPMTAG_FILESIZES, INT32_TYPE, fileSizeList, c);
	addEntry(header, RPMTAG_FILEUIDS, INT32_TYPE, fileUIDList, c);
	addEntry(header, RPMTAG_FILEGIDS, INT32_TYPE, fileGIDList, c);
	addEntry(header, RPMTAG_FILEMTIMES, INT32_TYPE, fileMtimesList, c);
	addEntry(header, RPMTAG_FILEFLAGS, INT32_TYPE, fileFlagsList, c);
	addEntry(header, RPMTAG_FILEMODES, INT16_TYPE, fileModesList, c);
	addEntry(header, RPMTAG_FILERDEVS, INT16_TYPE, fileRDevsList, c);
	
	/* Free the allocated strings */
	c = count;
	while (c--) {
	    free(fileList[c]);
	    free(fileMD5List[c]);
	    free(fileLinktoList[c]);
	}
	
	/* Free the file entry stack */
	while (fest) {
	    fes = fest->next;
	    free(fest);
	    fest = fes;
	}
    }
    
    freeSplitString(files);
    return 0;
}

int packageBinaries(Spec s)
{
    char name[1024];
    char filename[1024];
    struct PackageRec *pr;
    Header outHeader;
    HeaderIterator headerIter;
    int_32 tag, type, c;
    void *ptr;
    int fd;
    time_t buildtime;
    char *version;
    char *release;

    if (!getEntry(s->packages->header, RPMTAG_VERSION, NULL,
		  (void *) &version, NULL)) {
	error(RPMERR_BADSPEC, "No version field");
	return RPMERR_BADSPEC;
    }
    if (!getEntry(s->packages->header, RPMTAG_RELEASE, NULL,
		  (void *) &release, NULL)) {
	error(RPMERR_BADSPEC, "No release field");
	return RPMERR_BADSPEC;
    }

    buildtime = time(NULL);
    
    /* Look through for each package */
    pr = s->packages;
    while (pr) {
	if (pr->files == -1) {
	    pr = pr->next;
	    continue;
	}
	
	if (pr->subname) {
	    strcpy(name, s->name);
	    strcat(name, "-");
	    strcat(name, pr->subname);
	} else if (pr->newname) {
	    strcpy(name, pr->newname);
	} else {
	    strcpy(name, s->name);
	}
	strcat(name, "-");
	strcat(name, version);
	strcat(name, "-");
	strcat(name, release);
	
	/* First build the header structure.            */
	/* Here's the plan: copy the package's header,  */
	/* then add entries from the primary header     */
	/* that don't already exist.                    */
	outHeader = copyHeader(pr->header);
	headerIter = initIterator(s->packages->header);
	while (nextIterator(headerIter, &tag, &type, &ptr, &c)) {
	    /* Some tags we don't copy */
	    switch (tag) {
	      case RPMTAG_PREIN:
	      case RPMTAG_POSTIN:
	      case RPMTAG_PREUN:
	      case RPMTAG_POSTUN:
		  continue;
		  break;  /* Shouldn't need this */
	      default:
		  if (! isEntry(outHeader, tag)) {
		      addEntry(outHeader, tag, type, ptr, c);
		  }
	    }
	}
	freeIterator(headerIter);
	
	if (process_filelist(outHeader, pr->filelist)) {
	    return 1;
	}
	
	sprintf(filename, "%s.%s.rpm", name, getArchName());
	fd = open(filename, O_WRONLY|O_CREAT|O_TRUNC, 0644);
	if (writeMagic(s, pr, fd, name, BINARY_HEADER)) {
	    return 1;
	}

	/* Add some final entries to the header */
	addEntry(outHeader, RPMTAG_BUILDTIME, INT32_TYPE, &buildtime, 1);
	writeHeader(fd, outHeader);
	
	/* Now do the cpio | gzip thing */
	if (cpio_gzip(outHeader, fd)) {
	    return 1;
	}
    
	close(fd);

	freeHeader(outHeader);
	pr = pr->next;
    }
    
    return 0;
}

int packageSource(Spec s)
{
    return 0;
}
