#include "system.h"

#include <regex.h>
#include <signal.h>	/* getOutputFrom() */

#include "rpmbuild.h"
#include "buildio.h"

#include "myftw.h"
#include "md5.h"
#include "rpmmacro.h"

#define MAXDOCDIR 1024

typedef struct {
    const char *diskName; /* get file from here       */
    const char *fileName; /* filename in cpio archive */
    mode_t mode;
    uid_t uid;
    gid_t gid;
    dev_t device;
    ino_t inode;
    const char *uname;
    const char *gname;
    int flags;
    int verifyFlags;
    int size;
    int mtime;
    dev_t rdev;
    const char *lang;
} FileListRec;

struct AttrRec {
    const char *PmodeString;
    int Pmode;
    const char *PdirmodeString;
    int Pdirmode;
    const char *Uname;
    const char *Gname;
};

struct FileList {
    const char *buildRoot;
    const char *prefix;

    int fileCount;
    int totalFileSize;
    int processingFailed;

    int passedSpecialDoc;
    int isSpecialDoc;
    
    int isDir;
    int inFtw;
    int currentFlags;
    int currentVerifyFlags;
    struct AttrRec current;
    struct AttrRec def;
    int defVerifyFlags;
    const char *currentLang;

    /* Hard coded limit of MAXDOCDIR docdirs.         */
    /* If you break it you are doing something wrong. */
    char *docDirs[MAXDOCDIR];
    int docDirCount;
    
    FileListRec *fileList;
    int fileListRecsAlloced;
    int fileListRecsUsed;
};

#ifdef	DYING
static int processPackageFiles(Spec spec, Package pkg,
			       int installSpecialDoc, int test);
static void freeFileList(FileListRec *fileList, int count);
static int compareFileListRecs(const void *ap, const void *bp);
static int isDoc(struct FileList *fl, const char *fileName);
static int processBinaryFile(Package pkg, struct FileList *fl, char *fileName);
static int addFile(struct FileList *fl, char *name, struct stat *statp);
static int parseForSimple(Spec spec, Package pkg, char *buf,
			  struct FileList *fl, char **fileName);
static int parseForVerify(char *buf, struct FileList *fl);
static int parseForLang(char *buf, struct FileList *fl);
static int parseForAttr(char *buf, struct FileList *fl);
static int parseForConfig(char *buf, struct FileList *fl);
static int parseForRegexLang(char *fileName, char **lang);
static int myGlobPatternP(char *pattern);
static int glob_error(const char *foo, int bar);
static void timeCheck(int tc, Header h);
static void genCpioListAndHeader(struct FileList *fl,
				 struct cpioFileMapping **cpioList,
				 int *cpioCount, Header h, int isSrc);
static char *strtokWithQuotes(char *s, char *delim);
#endif

/* glob_pattern_p() taken from bash
 * Copyright (C) 1985, 1988, 1989 Free Software Foundation, Inc.
 *
 * Return nonzero if PATTERN has any special globbing chars in it.
 */
static int myGlobPatternP (char *pattern)
{
    register char *p = pattern;
    register char c;
    int open = 0;
  
    while ((c = *p++) != '\0')
	switch (c) {
	case '?':
	case '*':
	    return (1);
	case '[':      /* Only accept an open brace if there is a close */
	    open++;    /* brace to match it.  Bracket expressions must be */
	    continue;  /* complete, according to Posix.2 */
	case ']':
	    if (open)
		return (1);
	    continue;      
	case '\\':
	    if (*p++ == '\0')
		return (0);
	}

    return (0);
}

static int glob_error(const char *foo, int bar)
{
    return 1;
}

/* strtokWithQuotes() modified from glibc strtok() */
/* Copyright (C) 1991, 1996 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the GNU C Library; see the file COPYING.LIB.  If
   not, write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

static char *strtokWithQuotes(char *s, char *delim)
{
    static char *olds = NULL;
    char *token;

    if (s == NULL) {
	s = olds;
    }

    /* Skip leading delimiters */
    s += strspn(s, delim);
    if (*s == '\0') {
	return NULL;
    }

    /* Find the end of the token.  */
    token = s;
    if (*token == '"') {
	token++;
	/* Find next " char */
	s = strchr(token, '"');
    } else {
	s = strpbrk(token, delim);
    }

    /* Terminate it */
    if (s == NULL) {
	/* This token finishes the string */
	olds = strchr(token, '\0');
    } else {
	/* Terminate the token and make olds point past it */
	*s = '\0';
	olds = s+1;
    }

    return token;
}

static void timeCheck(int tc, Header h)
{
    int *mtime;
    char **file;
    int count, x, currentTime;

    headerGetEntry(h, RPMTAG_FILENAMES, NULL, (void **) &file, &count);
    headerGetEntry(h, RPMTAG_FILEMTIMES, NULL, (void **) &mtime, NULL);

    currentTime = time(NULL);
    
    for (x = 0; x < count; x++) {
	if (currentTime - mtime[x] > tc) {
	    rpmMessage(RPMMESS_WARNING, _("TIMECHECK failure: %s\n"), file[x]);
	}
    }
}

static int parseForVerify(char *buf, struct FileList *fl)
{
    char *p, *q, *start, *end, *name;
    char ourbuf[BUFSIZ];
    int not, verifyFlags;
    int *resultVerify;

    if (!(p = start = strstr(buf, "%verify"))) {
	if (!(p = start = strstr(buf, "%defverify"))) {
	    return 0;
	}
	name = "%defverify";
	resultVerify = &(fl->defVerifyFlags);
	p += 10;
    } else {
	name = "%verify";
	resultVerify = &(fl->currentVerifyFlags);
	p += 7;
    }

    SKIPSPACE(p);

    if (*p != '(') {
	rpmError(RPMERR_BADSPEC, _("Bad %s() syntax: %s"), name, buf);
	fl->processingFailed = 1;
	return RPMERR_BADSPEC;
    }
    p++;

    end = p;
    while (*end && *end != ')') {
	end++;
    }

    if (! *end) {
	rpmError(RPMERR_BADSPEC, _("Bad %s() syntax: %s"), name, buf);
	fl->processingFailed = 1;
	return RPMERR_BADSPEC;
    }

    strncpy(ourbuf, p, end-p);
    ourbuf[end-p] = '\0';
    while (start <= end) {
	*start++ = ' ';
    }

    not = 0;
    verifyFlags = RPMVERIFY_NONE;

    q = ourbuf;
    while ((p = strtok(q, ", \n\t")) != NULL) {
	q = NULL;
	if (!strcmp(p, "not")) {
	    not = 1;
	} else if (!strcmp(p, "md5")) {
	    verifyFlags |= RPMVERIFY_MD5;
	} else if (!strcmp(p, "size")) {
	    verifyFlags |= RPMVERIFY_FILESIZE;
	} else if (!strcmp(p, "link")) {
	    verifyFlags |= RPMVERIFY_LINKTO;
	} else if (!strcmp(p, "user")) {
	    verifyFlags |= RPMVERIFY_USER;
	} else if (!strcmp(p, "group")) {
	    verifyFlags |= RPMVERIFY_GROUP;
	} else if (!strcmp(p, "mtime")) {
	    verifyFlags |= RPMVERIFY_MTIME;
	} else if (!strcmp(p, "mode")) {
	    verifyFlags |= RPMVERIFY_MODE;
	} else if (!strcmp(p, "rdev")) {
	    verifyFlags |= RPMVERIFY_RDEV;
	} else {
	    rpmError(RPMERR_BADSPEC, _("Invalid %s token: %s"), name, p);
	    fl->processingFailed = 1;
	    return RPMERR_BADSPEC;
	}
    }

    *resultVerify = not ? ~(verifyFlags) : verifyFlags;

    return 0;
}

static int parseForAttr(char *buf, struct FileList *fl)
{
    char *p, *s, *start, *end, *name;
    char ourbuf[1024];
    int x, defattr = 0;
    struct AttrRec *resultAttr;

    if (!(p = start = strstr(buf, "%attr"))) {
	if (!(p = start = strstr(buf, "%defattr"))) {
	    return 0;
	}
	defattr = 1;
	name = "%defattr";
	resultAttr = &(fl->def);
	p += 8;
    } else {
	name = "%attr";
	resultAttr = &(fl->current);
	p += 5;
    }

    resultAttr->PmodeString = resultAttr->Uname = resultAttr->Gname = NULL;

    SKIPSPACE(p);

    if (*p != '(') {
	rpmError(RPMERR_BADSPEC, _("Bad %s() syntax: %s"), name, buf);
	fl->processingFailed = 1;
	return RPMERR_BADSPEC;
    }
    p++;

    end = p;
    while (*end && *end != ')') {
	end++;
    }

    if (! *end) {
	rpmError(RPMERR_BADSPEC, _("Bad %s() syntax: %s"), name, buf);
	fl->processingFailed = 1;
	return RPMERR_BADSPEC;
    }

    if (defattr) {
	s = end;
	s++;
	SKIPSPACE(s);
	if (*s) {
	    rpmError(RPMERR_BADSPEC,
		     _("No files after %%defattr(): %s"), buf);
	    fl->processingFailed = 1;
	    return RPMERR_BADSPEC;
	}
    }

    strncpy(ourbuf, p, end-p);
    ourbuf[end-p] = '\0';

    resultAttr->PmodeString = strtok(ourbuf, ", \n\t");
    resultAttr->Uname = strtok(NULL, ", \n\t");
    resultAttr->Gname = strtok(NULL, ", \n\t");
    resultAttr->PdirmodeString = strtok(NULL, ", \n\t");

    if (! (resultAttr->PmodeString &&
	   resultAttr->Uname && resultAttr->Gname)) {
	rpmError(RPMERR_BADSPEC, _("Bad %s() syntax: %s"), name, buf);
	resultAttr->PmodeString = resultAttr->Uname = resultAttr->Gname = NULL;
	fl->processingFailed = 1;
	return RPMERR_BADSPEC;
    }

    /* Do a quick test on the mode argument and adjust for "-" */
    if (!strcmp(resultAttr->PmodeString, "-")) {
	resultAttr->PmodeString = NULL;
    } else {
	x = sscanf(resultAttr->PmodeString, "%o", (unsigned *)&(resultAttr->Pmode));
	if ((x == 0) || (resultAttr->Pmode >> 12)) {
	    rpmError(RPMERR_BADSPEC, _("Bad %s() mode spec: %s"), name, buf);
	    resultAttr->PmodeString = resultAttr->Uname =
		resultAttr->Gname = NULL;
	    fl->processingFailed = 1;
	    return RPMERR_BADSPEC;
	}
	resultAttr->PmodeString = strdup(resultAttr->PmodeString);
    }
    if (resultAttr->PdirmodeString) {
	/* The processing here is slightly different to maintain */
	/* compatibility with old spec files.                    */
	if (!strcmp(resultAttr->PdirmodeString, "-")) {
	    resultAttr->PdirmodeString = strdup(resultAttr->PdirmodeString);
	} else {
	    x = sscanf(resultAttr->PdirmodeString, "%o",
		       (unsigned *)&(resultAttr->Pdirmode));
	    if ((x == 0) || (resultAttr->Pdirmode >> 12)) {
		rpmError(RPMERR_BADSPEC,
			 _("Bad %s() dirmode spec: %s"), name, buf);
		resultAttr->PmodeString = resultAttr->Uname =
		    resultAttr->Gname = resultAttr->PdirmodeString = NULL;
		fl->processingFailed = 1;
		return RPMERR_BADSPEC;
	    }
	    resultAttr->PdirmodeString = strdup(resultAttr->PdirmodeString);
	}
    }
    if (!strcmp(resultAttr->Uname, "-")) {
	resultAttr->Uname = NULL;
    } else {
	resultAttr->Uname = strdup(resultAttr->Uname);
    }
    if (!strcmp(resultAttr->Gname, "-")) {
	resultAttr->Gname = NULL;
    } else {
	resultAttr->Gname = strdup(resultAttr->Gname);
    }
    
    /* Set everything we just parsed to blank spaces */
    while (start <= end) {
	*start++ = ' ';
    }

    return 0;
}

static int parseForConfig(char *buf, struct FileList *fl)
{
    char *p, *start, *end;
    char ourbuf[1024];

    if (!(p = start = strstr(buf, "%config"))) {
	return 0;
    }
    fl->currentFlags = RPMFILE_CONFIG;

    p += 7;
    SKIPSPACE(p);

    if (*p != '(') {
	while (start < p) {
	    *start++ = ' ';
	}
	return 0;
    }
    p++;

    end = p;
    while (*end && *end != ')') {
	end++;
    }

    if (! *end) {
	rpmError(RPMERR_BADSPEC, _("Bad %%config() syntax: %s"), buf);
	fl->processingFailed = 1;
	return RPMERR_BADSPEC;
    }

    strncpy(ourbuf, p, end-p);
    ourbuf[end-p] = '\0';
    while (start <= end) {
	*start++ = ' ';
    }

    p = strtok(ourbuf, ", \n\t");
    while (p) {
	if (!strcmp(p, "missingok")) {
	    fl->currentFlags |= RPMFILE_MISSINGOK;
	} else if (!strcmp(p, "noreplace")) {
	    fl->currentFlags |= RPMFILE_NOREPLACE;
	} else {
	    rpmError(RPMERR_BADSPEC, _("Invalid %%config token: %s"), p);
	    fl->processingFailed = 1;
	    return RPMERR_BADSPEC;
	}
	p = strtok(NULL, ", \n\t");
    }

    return 0;
}

static int parseForLang(char *buf, struct FileList *fl)
{
    char *p, *start, *end;
    char ourbuf[1024];

    if (!(p = start = strstr(buf, "%lang"))) {
	return 0;
    }

    p += 5;
    SKIPSPACE(p);

    if (*p != '(') {
	rpmError(RPMERR_BADSPEC, _("Bad %%lang() syntax: %s"), buf);
	fl->processingFailed = 1;
	return RPMERR_BADSPEC;
    }
    p++;

    end = p;
    while (*end && *end != ')') {
	end++;
    }

    if (! *end) {
	rpmError(RPMERR_BADSPEC, _("Bad %%lang() syntax: %s"), buf);
	fl->processingFailed = 1;
	return RPMERR_BADSPEC;
    }

    strncpy(ourbuf, p, end-p);
    ourbuf[end-p] = '\0';
    while (start <= end) {
	*start++ = ' ';
    }

    p = strtok(ourbuf, ", \n\t");
    if (!p) {
	rpmError(RPMERR_BADSPEC, _("Bad %%lang() syntax: %s"), buf);
	fl->processingFailed = 1;
	return RPMERR_BADSPEC;
    }
#if 0
    if (strlen(p) != 2) {
	rpmError(RPMERR_BADSPEC, _("%%lang() entries are 2 characters: %s"), buf);
	fl->processingFailed = 1;
	return RPMERR_BADSPEC;
    }
#endif
    if (strtok(NULL, ", \n\t")) {
	rpmError(RPMERR_BADSPEC, _("Only one entry in %%lang(): %s"), buf);
	fl->processingFailed = 1;
	return RPMERR_BADSPEC;
    }
    fl->currentLang = strdup(p);
    
    return 0;
}

static int parseForRegexLang(char *fileName, char **lang)
{
    static int initialized = 0;
    static int hasRegex = 0;
    static regex_t compiledPatt;
    static char buf[BUFSIZ];
    int x;
    regmatch_t matches[2];
    char *patt, *s;

    if (! initialized) {
	initialized = 1;
	patt = rpmGetVar(RPMVAR_LANGPATT);
	if (! patt) {
	    return 1;
	}
	if (regcomp(&compiledPatt, patt, REG_EXTENDED)) {
	    return -1;
	}
	hasRegex = 1;
    }
    
    if (! hasRegex) {
	return 1;
    }

    if (! regexec(&compiledPatt, fileName, 2, matches, REG_NOTEOL)) {
	/* Got match */
	s = fileName + matches[1].rm_eo - 1;
	x = matches[1].rm_eo - matches[1].rm_so;
	buf[x] = '\0';
	while (x) {
	    buf[--x] = *s--;
	}
	*lang = buf;
	return 0;
    }

    return 1;
}

typedef struct VFA {
	char *	attribute;
	int	flag;
} VFA_t;

VFA_t virtualFileAttributes[] = {
	{ "%dir",	0 },	/* XXX why not RPMFILE_DIR? */
	{ "%doc",	RPMFILE_DOC },
	{ "%ghost",	RPMFILE_GHOST },
	{ "%readme",	RPMFILE_README },
	{ "%license",	RPMFILE_LICENSE },

#if WHY_NOT
	{ "%spec",	RPMFILE_SPEC },
	{ "%config",	RPMFILE_CONFIG },
	{ "%donotuse",	RPMFILE_DONOTUSE },	/* XXX WTFO? */
	{ "%missingok",	RPMFILE_CONFIG|RPMFILE_MISSINGOK },
	{ "%noreplace",	RPMFILE_CONFIG|RPMFILE_NOREPLACE },
#endif

	{ NULL, 0 }
};

static int parseForSimple(Spec spec, Package pkg, char *buf,
			  struct FileList *fl, char **fileName)
{
    char *s, *t;
    int res, specialDoc = 0;
    char specialDocBuf[BUFSIZ];

    specialDocBuf[0] = '\0';
    *fileName = NULL;
    res = 0;

    t = buf;
    while ((s = strtokWithQuotes(t, " \t\n")) != NULL) {
	t = NULL;
	if (!strcmp(s, "%docdir")) {
	    s = strtokWithQuotes(NULL, " \t\n");
	    if (fl->docDirCount == MAXDOCDIR) {
		rpmError(RPMERR_INTERNAL, _("Hit limit for %%docdir"));
		fl->processingFailed = 1;
		res = 1;
	    }
	    fl->docDirs[fl->docDirCount++] = strdup(s);
	    if (strtokWithQuotes(NULL, " \t\n")) {
		rpmError(RPMERR_INTERNAL, _("Only one arg for %%docdir"));
		fl->processingFailed = 1;
		res = 1;
	    }
	    break;
	}

    /* Set flags for virtual file attributes */
    {	VFA_t *vfa;
	for (vfa = virtualFileAttributes; vfa->attribute != NULL; vfa++) {
	    if (strcmp(s, vfa->attribute))
		continue;
	    if (!strcmp(s, "%dir"))
		fl->isDir = 1;	/* XXX why not RPMFILE_DIR? */
	    else
		fl->currentFlags |= vfa->flag;
	    break;
	}
	/* if we got an attribute, continue with next token */
	if (vfa->attribute != NULL)
	    continue;
    }

	if (*fileName) {
	    /* We already got a file -- error */
	    rpmError(RPMERR_BADSPEC,
		_("Two files on one line: %s"), *fileName);
	    fl->processingFailed = 1;
	    res = 1;
	}

	if (*s != '/') {
	    if (fl->currentFlags & RPMFILE_DOC) {
		specialDoc = 1;
		strcat(specialDocBuf, " ");
		strcat(specialDocBuf, s);
	    } else {
		/* not in %doc, does not begin with / -- error */
		rpmError(RPMERR_BADSPEC,
		    _("File must begin with \"/\": %s"), s);
		fl->processingFailed = 1;
		res = 1;
	    }
	} else {
	    *fileName = s;
	}
    }

    if (specialDoc) {
	if (*fileName || (fl->currentFlags & ~(RPMFILE_DOC))) {
	    rpmError(RPMERR_BADSPEC,
		     _("Can't mix special %%doc with other forms: %s"),
		     *fileName);
	    fl->processingFailed = 1;
	    res = 1;
	} else {
	/* XXX FIXME: this is easy to do as macro expansion */
	    strcpy(buf, "%{_docdir}/%{name}-%{version}");
	    expandMacros(spec, spec->macros, buf, BUFSIZ);

	    if (! fl->passedSpecialDoc) {
		pkg->specialDoc = newStringBuf();
		appendStringBuf(pkg->specialDoc, "DOCDIR=$RPM_BUILD_ROOT");
		appendLineStringBuf(pkg->specialDoc, buf);
		appendLineStringBuf(pkg->specialDoc, "export DOCDIR");
		appendLineStringBuf(pkg->specialDoc, "rm -rf $DOCDIR");
		appendLineStringBuf(pkg->specialDoc, MKDIR_P " $DOCDIR");

		*fileName = buf;
		fl->passedSpecialDoc = 1;
		fl->isSpecialDoc = 1;
	    }

	    appendStringBuf(pkg->specialDoc, "cp -pr ");
	    appendStringBuf(pkg->specialDoc, specialDocBuf);
	    appendLineStringBuf(pkg->specialDoc, " $DOCDIR");
	}
    }

    return res;
}

static int compareFileListRecs(const void *ap, const void *bp)
{
    const char *a, *b;

    a = ((FileListRec *)ap)->fileName;
    b = ((FileListRec *)bp)->fileName;

    return strcmp(a, b);
}

static int isDoc(struct FileList *fl, const char *fileName)
{
    int x = fl->docDirCount;

    while (x--) {
        if (strstr(fileName, fl->docDirs[x]) == fileName) {
	    return 1;
        }
    }
    return 0;
}

static void genCpioListAndHeader(struct FileList *fl,
				 struct cpioFileMapping **cpioList,
				 int *cpioCount, Header h, int isSrc)
{
    int skipLen;
    int count;
    FileListRec *flp;
    struct cpioFileMapping *clp;
    char *s, buf[BUFSIZ];
    
    /* Sort the big list */
    qsort(fl->fileList, fl->fileListRecsUsed,
	  sizeof(*(fl->fileList)), compareFileListRecs);
    
    /* Generate the cpio list and the header */
    skipLen = 0;
    if (! isSrc) {
	skipLen = 1;
	if (fl->prefix)
	    skipLen += strlen(fl->prefix);
    }

    *cpioCount = 0;
    clp = *cpioList = malloc(sizeof(**cpioList) * fl->fileListRecsUsed);

    for (flp = fl->fileList, count = fl->fileListRecsUsed; count > 0; flp++, count--) {
	if ((count > 1) && !strcmp(flp->fileName, flp[1].fileName)) {
	    rpmError(RPMERR_BADSPEC, _("File listed twice: %s"), flp->fileName);
	    fl->processingFailed = 1;
	}
	
	/* Make the cpio list */
	if (! (flp->flags & RPMFILE_GHOST)) {
	    clp->fsPath = strdup(flp->diskName);
	    clp->archivePath = strdup(flp->fileName + skipLen);
	    clp->finalMode = flp->mode;
	    clp->finalUid = flp->uid;
	    clp->finalGid = flp->gid;
	    clp->mapFlags = CPIO_MAP_PATH | CPIO_MAP_MODE |
		CPIO_MAP_UID | CPIO_MAP_GID;
	    if (isSrc) {
		clp->mapFlags |= CPIO_FOLLOW_SYMLINKS;
	    }
	    clp++;
	    (*cpioCount)++;
	}
	
	/* Make the header */
	headerAddOrAppendEntry(h, RPMTAG_FILENAMES, RPM_STRING_ARRAY_TYPE,
			       &(flp->fileName), 1);
	headerAddOrAppendEntry(h, RPMTAG_FILESIZES, RPM_INT32_TYPE,
			       &(flp->size), 1);
	headerAddOrAppendEntry(h, RPMTAG_FILEUSERNAME, RPM_STRING_ARRAY_TYPE,
			       &(flp->uname), 1);
	headerAddOrAppendEntry(h, RPMTAG_FILEGROUPNAME, RPM_STRING_ARRAY_TYPE,
			       &(flp->gname), 1);
	headerAddOrAppendEntry(h, RPMTAG_FILEMTIMES, RPM_INT32_TYPE,
			       &(flp->mtime), 1);
    if (sizeof(flp->mode) != sizeof(uint_16)) {
	uint_16 pmode = (uint_16)flp->mode;
	headerAddOrAppendEntry(h, RPMTAG_FILEMODES, RPM_INT16_TYPE,
			       &(pmode), 1);
    } else {
	headerAddOrAppendEntry(h, RPMTAG_FILEMODES, RPM_INT16_TYPE,
			       &(flp->mode), 1);
    }
    if (sizeof(flp->rdev) != sizeof(uint_16)) {
	uint_16 prdev = (uint_16)flp->rdev;
	headerAddOrAppendEntry(h, RPMTAG_FILERDEVS, RPM_INT16_TYPE,
			       &(prdev), 1);
    } else {
	headerAddOrAppendEntry(h, RPMTAG_FILERDEVS, RPM_INT16_TYPE,
			       &(flp->rdev), 1);
    }
    if (sizeof(flp->device) != sizeof(uint_32)) {
	uint_32 pdevice = (uint_32)flp->device;
	headerAddOrAppendEntry(h, RPMTAG_FILEDEVICES, RPM_INT32_TYPE,
			       &(pdevice), 1);
    } else {
	headerAddOrAppendEntry(h, RPMTAG_FILEDEVICES, RPM_INT32_TYPE,
			       &(flp->device), 1);
    }
	headerAddOrAppendEntry(h, RPMTAG_FILEINODES, RPM_INT32_TYPE,
			       &(flp->inode), 1);
	headerAddOrAppendEntry(h, RPMTAG_FILELANGS, RPM_STRING_ARRAY_TYPE,
			       &(flp->lang), 1);
	
	/* We used to add these, but they should not be needed */
	/* headerAddOrAppendEntry(h, RPMTAG_FILEUIDS,
	 *		   RPM_INT32_TYPE, &(flp->uid), 1);
	 * headerAddOrAppendEntry(h, RPMTAG_FILEGIDS,
	 *		   RPM_INT32_TYPE, &(flp->gid), 1);
	 */
	
	buf[0] = '\0';
	if (S_ISREG(flp->mode))
	    mdfile(flp->diskName, buf);
	s = buf;
	headerAddOrAppendEntry(h, RPMTAG_FILEMD5S, RPM_STRING_ARRAY_TYPE,
			       &s, 1);
	
	buf[0] = '\0';
	if (S_ISLNK(flp->mode))
	    buf[readlink(flp->diskName, buf, BUFSIZ)] = '\0';
	s = buf;
	headerAddOrAppendEntry(h, RPMTAG_FILELINKTOS, RPM_STRING_ARRAY_TYPE,
			       &s, 1);
	
	if (flp->flags & RPMFILE_GHOST) {
	    flp->verifyFlags &= ~(RPMVERIFY_MD5 | RPMVERIFY_FILESIZE |
				RPMVERIFY_LINKTO | RPMVERIFY_MTIME);
	}
	headerAddOrAppendEntry(h, RPMTAG_FILEVERIFYFLAGS, RPM_INT32_TYPE,
			       &(flp->verifyFlags), 1);
	
	if (!isSrc && isDoc(fl, flp->fileName))
	    flp->flags |= RPMFILE_DOC;
	if (S_ISDIR(flp->mode))
	    flp->flags &= ~(RPMFILE_CONFIG|RPMFILE_DOC);

	headerAddOrAppendEntry(h, RPMTAG_FILEFLAGS, RPM_INT32_TYPE,
			       &(flp->flags), 1);
    }
    headerAddEntry(h, RPMTAG_SIZE, RPM_INT32_TYPE,
		   &(fl->totalFileSize), 1);
}

static void freeFileList(FileListRec *fileList, int count)
{
    while (count--) {
	FREE(fileList[count].diskName);
	FREE(fileList[count].fileName);
	FREE(fileList[count].lang);
    }
    FREE(fileList);
}

static int addFile(struct FileList *fl, char *name, struct stat *statp)
{
    FileListRec *flp;
    char fileName[BUFSIZ];
    char diskName[BUFSIZ];
    struct stat statbuf;
    int_16 fileMode;
    int fileUid, fileGid;
    char *fileUname, *fileGname;
    char *lang;
    
    strcpy(fileName, cleanFileName(name));

    if (fl->inFtw) {
	/* Any buildRoot is already prepended */
	strcpy(diskName, fileName);
	if (fl->buildRoot) {
	    strcpy(fileName, diskName + strlen(fl->buildRoot));
	    /* Special case for "/" */
	    if (*fileName == '\0') {
		fileName[0] = '/';
		fileName[1] = '\0';
	    }
	}
    } else {
	if (fl->buildRoot) {
	    sprintf(diskName, "%s%s", fl->buildRoot, fileName);
	} else {
	    strcpy(diskName, fileName);
	}
    }

    /* If we are using a prefix, validate the file */
    if (!fl->inFtw && fl->prefix) {
	const char *prefixTest = fileName;
	const char *prefixPtr = fl->prefix;

	while (*prefixPtr && *prefixTest && (*prefixTest == *prefixPtr)) {
	    prefixPtr++;
	    prefixTest++;
	}
	if (*prefixPtr || (*prefixTest && *prefixTest != '/')) {
	    rpmError(RPMERR_BADSPEC, _("File doesn't match prefix (%s): %s"),
		     fl->prefix, fileName);
	    fl->processingFailed = 1;
	    return RPMERR_BADSPEC;
	}
    }

    if (! statp) {
	statp = &statbuf;
	if (lstat(diskName, statp)) {
	    rpmError(RPMERR_BADSPEC, _("File not found: %s"), diskName);
	    fl->processingFailed = 1;
	    return RPMERR_BADSPEC;
	}
    }

    if ((! fl->isDir) && S_ISDIR(statp->st_mode)) {
	/* We use our own ftw() call, because ftw() uses stat()    */
	/* instead of lstat(), which causes it to follow symlinks! */
	/* It also has better callback support.                    */
	
	fl->inFtw = 1;  /* Flag to indicate file has buildRoot prefixed */
	fl->isDir = 1;  /* Keep it from following myftw() again         */
	myftw(diskName, 16, (myftwFunc) addFile, fl);
	fl->isDir = 0;
	fl->inFtw = 0;
	return 0;
    }

    fileMode = statp->st_mode;
    fileUid = statp->st_uid;
    fileGid = statp->st_gid;

    /* %attr ? */
    if (S_ISDIR(fileMode) && fl->current.PdirmodeString) {
	if (fl->current.PdirmodeString[0] != '-') {
	    fileMode &= S_IFMT;
	    fileMode |= fl->current.Pdirmode;
	}
    } else {
	if (fl->current.PmodeString) {
	    fileMode &= S_IFMT;
	    fileMode |= fl->current.Pmode;
	}
    }
    if (fl->current.Uname) {
	fileUname = getUnameS(fl->current.Uname);
    } else {
	fileUname = getUname(fileUid);
    }
    if (fl->current.Gname) {
	fileGname = getGnameS(fl->current.Gname);
    } else {
	fileGname = getGname(fileGid);
    }
	
    if (! (fileUname && fileGname)) {
	rpmError(RPMERR_BADSPEC, _("Bad owner/group: %s\n"), diskName);
	fl->processingFailed = 1;
	return RPMERR_BADSPEC;
    }
    
    rpmMessage(RPMMESS_DEBUG, _("File %d: %s\n"), fl->fileCount, fileName);

    /* Add to the file list */
    if (fl->fileListRecsUsed == fl->fileListRecsAlloced) {
	fl->fileListRecsAlloced += 128;
	fl->fileList = realloc(fl->fileList,
			fl->fileListRecsAlloced * sizeof(*(fl->fileList)));
    }
	    
    flp = &fl->fileList[fl->fileListRecsUsed];

    flp->fileName = strdup(fileName);
    flp->diskName = strdup(diskName);
    flp->mode = fileMode;
    flp->uid = fileUid;
    flp->gid = fileGid;
    flp->uname = fileUname;
    flp->gname = fileGname;

    if (fl->currentLang) {
	flp->lang = strdup(fl->currentLang);
    } else if (! parseForRegexLang(fileName, &lang)) {
	flp->lang = strdup(lang);
    } else {
	flp->lang = strdup("");
    }

    flp->flags = fl->currentFlags;
    flp->verifyFlags = fl->currentVerifyFlags;

    flp->size = statp->st_size;
    flp->mtime = statp->st_mtime;
    flp->rdev = statp->st_rdev;
    flp->device = statp->st_dev;
    flp->inode = statp->st_ino;

    fl->fileListRecsUsed++;

    fl->totalFileSize += flp->size;
    fl->fileCount++;

    return 0;
}

static int processBinaryFile(Package pkg, struct FileList *fl, char *fileName)
{
    char fullname[BUFSIZ];
    glob_t glob_result;
    int x, offset, rc = 0;
    
    /* check that file starts with leading "/" */
    if (*fileName != '/') {
	rpmError(RPMERR_BADSPEC, _("File needs leading \"/\": %s"), *fileName);
	fl->processingFailed = 1;
	return 1;
    }
    
    if (myGlobPatternP(fileName)) {
	if (fl->buildRoot) {
	    sprintf(fullname, "%s%s", fl->buildRoot, fileName);
	    offset = strlen(fl->buildRoot);
	} else {
	    strcpy(fullname, fileName);
	    offset = 0;
	}
	
	if (glob(fullname, 0, glob_error, &glob_result) ||
	    (glob_result.gl_pathc < 1)) {
	    rpmError(RPMERR_BADSPEC, _("File not found: %s"), fullname);
	    fl->processingFailed = 1;
	    globfree(&glob_result);
	    return 1;
	}
	
	for (x = 0; x < glob_result.gl_pathc; x++) {
	    rc = addFile(fl, &(glob_result.gl_pathv[x][offset]), NULL);
	}
	globfree(&glob_result);
    } else {
	rc = addFile(fl, fileName, NULL);
    }

    return rc;
}

static int processPackageFiles(Spec spec, Package pkg,
			       int installSpecialDoc, int test)
{
    struct FileList fl;
    char *s, **files, **fp, *fileName;
    char buf[BUFSIZ];
    FILE *f;

    struct AttrRec specialDocAttrRec;
    char *specialDoc = NULL;
    
    pkg->cpioList = NULL;
    pkg->cpioCount = 0;

    if (pkg->fileFile) {
	strcpy(buf, "%{_builddir}/");
	expandMacros(spec, spec->macros, buf, sizeof(buf));
	if (spec->buildSubdir) {
	    strcat(buf, spec->buildSubdir);
	    strcat(buf, "/");
	}
	strcat(buf, pkg->fileFile);

	if ((f = fopen(buf, "r")) == NULL) {
	    rpmError(RPMERR_BADFILENAME,
		     _("Could not open %%files file: %s"), pkg->fileFile);
	    return RPMERR_BADFILENAME;
	}
	while (fgets(buf, sizeof(buf), f)) {
	    handleComments(buf);
	    if (expandMacros(spec, spec->macros, buf, sizeof(buf))) {
		rpmError(RPMERR_BADSPEC, _("line: %s"), buf);
		return RPMERR_BADSPEC;
	    }
	    appendStringBuf(pkg->fileList, buf);
	}
	fclose(f);
    }
    
    /* Init the file list structure */
    
    fl.buildRoot = spec->buildRoot ? spec->buildRoot : "";
    if (headerGetEntry(pkg->header, RPMTAG_DEFAULTPREFIX,
		       NULL, (void *)&fl.prefix, NULL)) {
	fl.prefix = strdup(fl.prefix);
    } else {
	fl.prefix = NULL;
    }

    fl.fileCount = 0;
    fl.totalFileSize = 0;
    fl.processingFailed = 0;

    fl.passedSpecialDoc = 0;
    
    fl.current.PmodeString = NULL;
    fl.current.PdirmodeString = NULL;
    fl.current.Uname = NULL;
    fl.current.Gname = NULL;
    fl.def.PmodeString = NULL;
    fl.def.PdirmodeString = NULL;
    fl.def.Uname = NULL;
    fl.def.Gname = NULL;
    fl.def.Pmode = 0;
    fl.def.Pdirmode = 0;
    fl.currentLang = NULL;

    fl.defVerifyFlags = RPMVERIFY_ALL;

    fl.docDirCount = 0;
    fl.docDirs[fl.docDirCount++] = strdup("/usr/doc");
    fl.docDirs[fl.docDirCount++] = strdup("/usr/man");
    fl.docDirs[fl.docDirCount++] = strdup("/usr/info");
    fl.docDirs[fl.docDirCount++] = strdup("/usr/X11R6/man");
    {	strcpy(buf, "%{_docdir}");
	expandMacros(spec, spec->macros, buf, sizeof(buf));
	fl.docDirs[fl.docDirCount++] = strdup(buf);
    }
    
    fl.fileList = NULL;
    fl.fileListRecsAlloced = 0;
    fl.fileListRecsUsed = 0;

    s = getStringBuf(pkg->fileList);
    files = splitString(s, strlen(s), '\n');

    for (fp = files; *fp != NULL; fp++) {
	s = *fp;
	SKIPSPACE(s);
	if (*s == '\0')
	    continue;
	fileName = NULL;
	strcpy(buf, s);
	
	/* Reset for a new line in %files */
	fl.isDir = 0;
	fl.inFtw = 0;
	fl.currentFlags = 0;
	fl.currentVerifyFlags = fl.defVerifyFlags;
	fl.current.Pmode = fl.def.Pmode;
	fl.current.Pdirmode = fl.def.Pdirmode;
	fl.isSpecialDoc = 0;

	FREE(fl.current.PmodeString);
	FREE(fl.current.PdirmodeString);
	FREE(fl.current.Uname);
	FREE(fl.current.Gname);
	FREE(fl.currentLang);
	if (fl.def.PmodeString)
	    fl.current.PmodeString = strdup(fl.def.PmodeString);
	if (fl.def.PdirmodeString)
	    fl.current.PdirmodeString = strdup(fl.def.PdirmodeString);
	if (fl.def.Uname)
	    fl.current.Uname = strdup(fl.def.Uname);
	if (fl.def.Gname)
	    fl.current.Gname = strdup(fl.def.Gname);

	if (parseForVerify(buf, &fl))
	    continue;
	if (parseForAttr(buf, &fl))
	    continue;
	if (parseForConfig(buf, &fl))
	    continue;
	if (parseForLang(buf, &fl))
	    continue;
	if (parseForSimple(spec, pkg, buf, &fl, &fileName))
	    continue;
	if (fileName == NULL)
	    continue;

	if (fl.isSpecialDoc) {
	    /* Save this stuff for last */
	    specialDoc = strdup(fileName);
	    specialDocAttrRec = fl.current;
	    if (specialDocAttrRec.PmodeString) {
		specialDocAttrRec.PmodeString =
		    strdup(specialDocAttrRec.PmodeString);
	    }
	    if (specialDocAttrRec.PdirmodeString) {
		specialDocAttrRec.PdirmodeString =
		    strdup(specialDocAttrRec.PdirmodeString);
	    }
	    if (specialDocAttrRec.Uname) {
		specialDocAttrRec.Uname = strdup(specialDocAttrRec.Uname);
	    }
	    if (specialDocAttrRec.Gname) {
		specialDocAttrRec.Gname = strdup(specialDocAttrRec.Gname);
	    }
	} else {
	    processBinaryFile(pkg, &fl, fileName);
	}
    }

    /* Now process special doc, if there is one */
    if (specialDoc) {
	if (installSpecialDoc) {
	    doScript(spec, RPMBUILD_STRINGBUF, "%doc", pkg->specialDoc, test);
	}

	/* fl.current now takes on "ownership" of the specialDocAttrRec */
	/* allocated string data.                                       */
	fl.current = specialDocAttrRec;
	fl.isDir = 0;
	fl.inFtw = 0;
	fl.currentFlags = 0;
	fl.currentVerifyFlags = 0;
	processBinaryFile(pkg, &fl, specialDoc);
	FREE(specialDoc);
    }
    
    freeSplitString(files);

    if (! fl.processingFailed) {
	genCpioListAndHeader(&fl, &(pkg->cpioList), &(pkg->cpioCount),
			     pkg->header, 0);

	if (spec->timeCheck) {
	    timeCheck(spec->timeCheck, pkg->header);
	}
    }
    
    /* Clean up */
    FREE(fl.prefix);
    FREE(fl.current.PmodeString);
    FREE(fl.current.PdirmodeString);
    FREE(fl.current.Uname);
    FREE(fl.current.Gname);
    FREE(fl.def.PmodeString);
    FREE(fl.def.PdirmodeString);
    FREE(fl.def.Uname);
    FREE(fl.def.Gname);
    FREE(fl.currentLang);
    freeFileList(fl.fileList, fl.fileListRecsUsed);
    while (fl.docDirCount--) {
        FREE(fl.docDirs[fl.docDirCount]);
    }
    return fl.processingFailed;
}

int processSourceFiles(Spec spec)
{
    struct Source *srcPtr;
    char buf[BUFSIZ];
    StringBuf sourceFiles;
    int x, isSpec = 1;
    struct FileList fl;
    char *s, **files, **fp, *fn;
    HeaderIterator hi;
    int tag, type, count;
    Package pkg;
    void * ptr;

    sourceFiles = newStringBuf();
    spec->sourceHeader = headerNew();

    /* Only specific tags are added to the source package header */
    hi = headerInitIterator(spec->packages->header);
    while (headerNextIterator(hi, &tag, &type, &ptr, &count)) {
	switch (tag) {
	  case RPMTAG_NAME:
	  case RPMTAG_VERSION:
	  case RPMTAG_RELEASE:
	  case RPMTAG_EPOCH:
	  case RPMTAG_SUMMARY:
	  case RPMTAG_DESCRIPTION:
	  case RPMTAG_PACKAGER:
	  case RPMTAG_DISTRIBUTION:
	  case RPMTAG_VENDOR:
	  case RPMTAG_LICENSE:
	  case RPMTAG_GROUP:
	  case RPMTAG_OS:
	  case RPMTAG_ARCH:
	  case RPMTAG_CHANGELOGTIME:
	  case RPMTAG_CHANGELOGNAME:
	  case RPMTAG_CHANGELOGTEXT:
	  case RPMTAG_URL:
	  case HEADER_I18NTABLE:
	    headerAddEntry(spec->sourceHeader, tag, type, ptr, count);
	    break;
	  default:
	    /* do not copy */
	    break;
	}
	if (type == RPM_STRING_ARRAY_TYPE || type == RPM_I18NSTRING_TYPE) {
	    FREE(ptr);
	}
    }
    headerFreeIterator(hi);

    /* Construct the file list and source entries */
    appendLineStringBuf(sourceFiles, spec->specFile);
    for (srcPtr = spec->sources; srcPtr != NULL; srcPtr = srcPtr->next) {
	if (srcPtr->flags & RPMBUILD_ISSOURCE) {
	    headerAddOrAppendEntry(spec->sourceHeader, RPMTAG_SOURCE,
				   RPM_STRING_ARRAY_TYPE, &srcPtr->source, 1);
	    if (srcPtr->flags & RPMBUILD_ISNO) {
		headerAddOrAppendEntry(spec->sourceHeader, RPMTAG_NOSOURCE,
				       RPM_INT32_TYPE, &srcPtr->num, 1);
	    }
	}
	if (srcPtr->flags & RPMBUILD_ISPATCH) {
	    headerAddOrAppendEntry(spec->sourceHeader, RPMTAG_PATCH,
				   RPM_STRING_ARRAY_TYPE, &srcPtr->source, 1);
	    if (srcPtr->flags & RPMBUILD_ISNO) {
		headerAddOrAppendEntry(spec->sourceHeader, RPMTAG_NOPATCH,
				       RPM_INT32_TYPE, &srcPtr->num, 1);
	    }
	}

      {	char *s = buf;
	if (srcPtr->flags & RPMBUILD_ISNO)
	    *s++ = '!';
	strcpy(s, "%{_sourcedir}/");
      }
	expandMacros(spec, spec->macros, buf, sizeof(buf));
	strcat(buf, srcPtr->source);
	appendLineStringBuf(sourceFiles, buf);
    }

    for (pkg = spec->packages; pkg != NULL; pkg = pkg->next) {
	for (srcPtr = pkg->icon; srcPtr != NULL; srcPtr = srcPtr->next) {
	    char *s = buf;
	    if (srcPtr->flags & RPMBUILD_ISNO)
		*s++ = '!';
	    strcpy(s, "%{_sourcedir}/");
	    expandMacros(spec, spec->macros, buf, sizeof(buf));
	    strcat(buf, srcPtr->source);
	    appendLineStringBuf(sourceFiles, buf);
	}
    }

    spec->sourceCpioList = NULL;
    spec->sourceCpioCount = 0;

    fl.fileList = malloc((spec->numSources + 1) * sizeof(FileListRec));
    fl.processingFailed = 0;
    fl.fileListRecsUsed = 0;
    fl.totalFileSize = 0;
    fl.prefix = NULL;

    s = getStringBuf(sourceFiles);
    files = splitString(s, strlen(s), '\n');

    /* The first source file is the spec file */
    x = 0;
    for (fp = files; *fp != NULL; fp++) {
	FileListRec *flp;
	s = *fp;
	SKIPSPACE(s);
	if (! *s)
	    continue;

	flp = &fl.fileList[x];

	flp->flags = isSpec ? RPMFILE_SPECFILE : 0;
	/* files with leading ! are no source files */
	if (*s == '!') {
	    flp->flags |= RPMFILE_GHOST;
	    s++;
	}
	flp->diskName = strdup(s);
	fn = strrchr(s, '/');
	if (fn) {
	    fn++;
	} else {
	    fn = s;
	}
	flp->fileName = strdup(fn);
	flp->verifyFlags = RPMVERIFY_ALL;

    {	struct stat sb;
	stat(s, &sb);
	flp->mode = sb.st_mode;
	flp->uid = sb.st_uid;
	flp->gid = sb.st_gid;
	flp->size = sb.st_size;
	flp->mtime = sb.st_mtime;
	flp->rdev = sb.st_rdev;
	flp->device = sb.st_dev;
	flp->inode = sb.st_ino;
    }

	flp->uname = getUname(flp->uid);
	flp->gname = getGname(flp->gid);
	flp->lang = strdup("");
	
	fl.totalFileSize += flp->size;
	
	if (! (flp->uname && flp->gname)) {
	    rpmError(RPMERR_BADSPEC, _("Bad owner/group: %s"), s);
	    fl.processingFailed = 1;
	}

	isSpec = 0;
	x++;
    }
    fl.fileListRecsUsed = x;
    freeSplitString(files);

    if (! fl.processingFailed) {
	genCpioListAndHeader(&fl, &(spec->sourceCpioList),
			     &(spec->sourceCpioCount), spec->sourceHeader, 1);
    }

    freeStringBuf(sourceFiles);
    freeFileList(fl.fileList, fl.fileListRecsUsed);
    return fl.processingFailed;
}

static StringBuf getOutputFrom(char *dir, char *argv[],
			char *writePtr, int writeBytesLeft,
			int failNonZero)
{
    int progPID;
    int toProg[2];
    int fromProg[2];
    int status;
    void *oldhandler;
    int bytesWritten;
    StringBuf readBuff;
    int bytes;
    unsigned char buf[8193];

    oldhandler = signal(SIGPIPE, SIG_IGN);

    pipe(toProg);
    pipe(fromProg);
    
    if (!(progPID = fork())) {
	close(toProg[1]);
	close(fromProg[0]);
	
	dup2(toProg[0], STDIN_FILENO);   /* Make stdin the in pipe */
	dup2(fromProg[1], STDOUT_FILENO); /* Make stdout the out pipe */

	close(toProg[0]);
	close(fromProg[1]);

	if (dir) {
	    chdir(dir);
	}
	
	execvp(argv[0], argv);
	rpmError(RPMERR_EXEC, _("Couldn't exec %s"), argv[0]);
	_exit(RPMERR_EXEC);
    }
    if (progPID < 0) {
	rpmError(RPMERR_FORK, _("Couldn't fork %s"), argv[0]);
	return NULL;
    }

    close(toProg[0]);
    close(fromProg[1]);

    /* Do not block reading or writing from/to prog. */
    fcntl(fromProg[0], F_SETFL, O_NONBLOCK);
    fcntl(toProg[1], F_SETFL, O_NONBLOCK);
    
    readBuff = newStringBuf();

    do {
	/* Write any data to program */
        if (writeBytesLeft) {
	    if ((bytesWritten =
		  write(toProg[1], writePtr,
		    (1024<writeBytesLeft) ? 1024 : writeBytesLeft)) < 0) {
	        if (errno != EAGAIN) {
		    perror("getOutputFrom()");
	            exit(EXIT_FAILURE);
		}
	        bytesWritten = 0;
	    }
	    writeBytesLeft -= bytesWritten;
	    writePtr += bytesWritten;
	} else if (toProg[1] >= 0) {	/* close write fd */
	    close(toProg[1]);
	    toProg[1] = -1;
	}
	
	/* Read any data from prog */
	while ((bytes = read(fromProg[0], buf, sizeof(buf)-1)) > 0) {
	    buf[bytes] = '\0';
	    appendStringBuf(readBuff, buf);
	}

	/* terminate on (non-blocking) EOF or error */
    } while (!(bytes == 0 || (bytes < 0 && errno != EAGAIN)));

    /* Clean up */
    if (toProg[1] >= 0)
    	close(toProg[1]);
    close(fromProg[0]);
    (void)signal(SIGPIPE, oldhandler);

    /* Collect status from prog */
    (void)waitpid(progPID, &status, 0);
    if (failNonZero && (!WIFEXITED(status) || WEXITSTATUS(status))) {
	rpmError(RPMERR_EXEC, _("%s failed"), argv[0]);
	return NULL;
    }
    if (writeBytesLeft) {
	rpmError(RPMERR_EXEC, _("failed to write all data to %s"), argv[0]);
	return NULL;
    }
    return readBuff;
}

static int generateAutoReqProv(Spec spec, Package pkg,
			struct cpioFileMapping *cpioList, int cpioCount)
{
    StringBuf writeBuf;
    int writeBytes;
    StringBuf readBuf;
    char *argv[2];
    char **f, **fsave;

    if (!cpioCount) {
	return 0;
    }

    if (! (spec->autoReq || spec->autoProv)) {
	return 0;
    }
    
    writeBuf = newStringBuf();
    writeBytes = 0;
    while (cpioCount--) {
	writeBytes += strlen(cpioList->fsPath) + 1;
	appendLineStringBuf(writeBuf, cpioList->fsPath);
	cpioList++;
    }

    /*** Do Provides ***/

    if (spec->autoProv) {
	rpmMessage(RPMMESS_NORMAL, _("Finding provides...\n"));
    
	argv[0] = FINDPROVIDES;
	argv[1] = NULL;
	readBuf = getOutputFrom(NULL, argv,
				getStringBuf(writeBuf), writeBytes, 1);
	if (readBuf == NULL) {
	    rpmError(RPMERR_EXEC, _("Failed to find provides"));
	    freeStringBuf(writeBuf);
	    return RPMERR_EXEC;
	}
	
	fsave = splitString(getStringBuf(readBuf),
				strlen(getStringBuf(readBuf)), '\n');
	freeStringBuf(readBuf);
	for (f = fsave; *f != NULL; f++) {
	    if (**f) {
		addReqProv(spec, pkg, RPMSENSE_PROVIDES, *f, NULL, 0);
	    }
	}
	freeSplitString(fsave);
    }

    /*** Do Requires ***/

    if (spec->autoReq) {
	rpmMessage(RPMMESS_NORMAL, _("Finding requires...\n"));

	argv[0] = FINDREQUIRES;
	argv[1] = NULL;
	readBuf = getOutputFrom(NULL, argv,
				getStringBuf(writeBuf), writeBytes, 0);
	if (readBuf == NULL) {
	    rpmError(RPMERR_EXEC, _("Failed to find requires"));
	    freeStringBuf(writeBuf);
	    return RPMERR_EXEC;
	}

	fsave = splitString(getStringBuf(readBuf),
				strlen(getStringBuf(readBuf)), '\n');
	freeStringBuf(readBuf);
	for (f = fsave; *f != NULL; f++) {
	    if (**f) {
		addReqProv(spec, pkg, RPMSENSE_ANY, *f, NULL, 0);
	    }
	}
	freeSplitString(fsave);
    }

    /*** Clean Up ***/

    freeStringBuf(writeBuf);

    return 0;
}

static void printReqs(Spec spec, Package pkg)
{
    int startedPreReq = 0;
    int startedReq = 0;

    char **names;
    int x, count;
    int *flags;

    if (headerGetEntry(pkg->header, RPMTAG_PROVIDES,
		       NULL, (void **) &names, &count)) {
	rpmMessage(RPMMESS_NORMAL, _("Provides:"));
	for (x = 0; x < count; x++) {
	    rpmMessage(RPMMESS_NORMAL, " %s", names[x]);
	}
	rpmMessage(RPMMESS_NORMAL, "\n");
	FREE(names);
    }

    if (headerGetEntry(pkg->header, RPMTAG_REQUIRENAME,
		       NULL, (void **) &names, &count)) {
	headerGetEntry(pkg->header, RPMTAG_REQUIREFLAGS,
		       NULL, (void **) &flags, NULL);
	for (x = 0; x < count; x++) {
	    if (flags[x] & RPMSENSE_PREREQ) {
		if (! startedPreReq) {
		    rpmMessage(RPMMESS_NORMAL, _("Prereqs:"));
		    startedPreReq = 1;
		}
		rpmMessage(RPMMESS_NORMAL, " %s", names[x]);
	    }
	}
	if (startedPreReq) {
	    rpmMessage(RPMMESS_NORMAL, "\n");
	}
	for (x = 0; x < count; x++) {
	    if (! (flags[x] & RPMSENSE_PREREQ)) {
		if (! startedReq) {
		    rpmMessage(RPMMESS_NORMAL, _("Requires:"));
		    startedReq = 1;
		}
		rpmMessage(RPMMESS_NORMAL, " %s", names[x]);
	    }
	}
	rpmMessage(RPMMESS_NORMAL, "\n");
	FREE(names);
    }
}

int processBinaryFiles(Spec spec, int installSpecialDoc, int test)
{
    Package pkg;
    int res, rc;
    char *name;
    
    res = 0;
    for (pkg = spec->packages; pkg != NULL; pkg = pkg->next) {
	if (pkg->fileList == NULL) {
	    continue;
	}

	headerGetEntry(pkg->header, RPMTAG_NAME, NULL, (void **)&name, NULL);
	rpmMessage(RPMMESS_NORMAL, _("Processing files: %s\n"), name);
		   
	if ((rc = processPackageFiles(spec, pkg, installSpecialDoc, test))) {
	    res = rc;
	}

	generateAutoReqProv(spec, pkg, pkg->cpioList, pkg->cpioCount);
	printReqs(spec, pkg);
	
    }

    return res;
}
