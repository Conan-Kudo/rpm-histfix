#include "config.h"
#include "miscfn.h"

#if HAVE_ALLOCA_H
# include <alloca.h>
#endif

#include <errno.h>
#include <ctype.h>
#include <fcntl.h>
#include <grp.h>
#include <pwd.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "intl.h"
#include "misc.h"
#include "rpmlib.h"
#include "messages.h"

char * RPMVERSION = VERSION;

char ** splitString(char * str, int length, char sep) {
    char * s, * source, * dest;
    char ** list;
    int i;
    int fields;
   
    s = malloc(length + 1);
    
    fields = 1;
    for (source = str, dest = s, i = 0; i < length; i++, source++, dest++) {
	*dest = *source;
	if (*dest == sep) fields++;
    }

    *dest = '\0';

    list = malloc(sizeof(char *) * (fields + 1));

    dest = s;
    list[0] = dest;
    i = 1;
    while (i < fields) {
	if (*dest == sep) {
	    list[i++] = dest + 1;
	    *dest = 0;
	}
	dest++;
    }

    list[i] = NULL;

    return list;
}

void freeSplitString(char ** list) {
    free(list[0]);
    free(list);
}

int exists(char * filespec) {
    struct stat buf;

    if (stat(filespec, &buf)) {
	switch(errno) {
	   case ENOENT:
	   case EINVAL:
		return 0;
	}
    }

    return 1;
}

int rpmvercmp(char * one, char * two) {
    int num1, num2;
    char oldch1, oldch2;
    char * str1, * str2;
    int rc;
    int isnum;
    
    if (!strcmp(one, two)) return 0;

    str1 = alloca(strlen(one) + 1);
    str2 = alloca(strlen(two) + 1);

    strcpy(str1, one);
    strcpy(str2, two);

    one = str1;
    two = str2;

    while (*one && *two) {
	while (*one && !isalnum(*one)) one++;
	while (*two && !isalnum(*two)) two++;

	str1 = one;
	str2 = two;

	if (isdigit(*str1)) {
	    while (*str1 && isdigit(*str1)) str1++;
	    while (*str2 && isdigit(*str2)) str2++;
	    isnum = 1;
	} else {
	    while (*str1 && isalpha(*str1)) str1++;
	    while (*str2 && isalpha(*str2)) str2++;
	    isnum = 0;
	}
		
	oldch1 = *str1;
	*str1 = '\0';
	oldch2 = *str2;
	*str2 = '\0';

	if (one == str1) return -1;	/* arbitrary */
	if (two == str2) return -1;

	if (isnum) {
	    num1 = atoi(one);
	    num2 = atoi(two);

	    if (num1 < num2) 
		return -1;
	    else if (num1 > num2)
		return 1;
	} else {
	    rc = strcmp(one, two);
	    if (rc) return rc;
	}
	
	*str1 = oldch1;
	one = str1;
	*str2 = oldch2;
	two = str2;
    }

    if ((!*one) && (!*two)) return 0;

    if (!*one) return -1; else return 1;
}

void stripTrailingSlashes(char * str) {
    char * chptr;

    chptr = str + strlen(str) - 1;
    while (*chptr == '/' && chptr >= str) {
	*chptr = '\0';
	chptr--;
    }
}

int doputenv(const char *str) {
    char * a;
    
    /* FIXME: this leaks memory! */

    a = malloc(strlen(str) + 1);
    strcpy(a, str);

    return putenv(a);
}

int dosetenv(const char *name, const char *value, int overwrite) {
    int i;
    char * a;

    /* FIXME: this leaks memory! */

    if (!overwrite && getenv(name)) return 0;

    i = strlen(name) + strlen(value) + 2;
    a = malloc(i);
    if (!a) return 1;
    
    strcpy(a, name);
    strcat(a, "=");
    strcat(a, value);
    
    return putenv(a);
}

/* unameToUid(), uidTouname() and the group variants are really poorly
   implemented. They really ought to use hash tables. I just made the
   guess that most files would be owned by root or the same person/group
   who owned the last file. Those two values are cached, everything else
   is looked up via getpw() and getgr() functions.  If this performs
   too poorly I'll have to implement it properly :-( */

int unameToUid(char * thisUname, uid_t * uid) {
    static char * lastUname = NULL;
    static int lastUnameLen = 0;
    static int lastUnameAlloced;
    static uid_t lastUid;
    struct passwd * pwent;
    int thisUnameLen;

    if (!thisUname) {
	lastUnameLen = 0;
	return -1;
    } else if (!strcmp(thisUname, "root")) {
	*uid = 0;
	return 0;
    }

    thisUnameLen = strlen(thisUname);
    if (!lastUname || thisUnameLen != lastUnameLen || 
	strcmp(thisUname, lastUname)) {
	if (lastUnameAlloced < thisUnameLen + 1) {
	    lastUnameAlloced = thisUnameLen + 10;
	    lastUname = realloc(lastUname, lastUnameAlloced);
	}
	strcpy(lastUname, thisUname);

	pwent = getpwnam(thisUname);
	if (!pwent) {
	    endpwent();
	    pwent = getpwnam(thisUname);
	    if (!pwent) return -1;
	}

	lastUid = pwent->pw_uid;
    }

    *uid = lastUid;

    return 0;
}

int gnameToGid(char * thisGname, gid_t * gid) {
    static char * lastGname = NULL;
    static int lastGnameLen = 0;
    static int lastGnameAlloced;
    static uid_t lastGid;
    int thisGnameLen;
    struct group * grent;

    if (!thisGname) {
	lastGnameLen = 0;
	return -1;
    } else if (!strcmp(thisGname, "root")) {
	*gid = 0;
	return 0;
    }
   
    thisGnameLen = strlen(thisGname);
    if (!lastGname || thisGnameLen != lastGnameLen || 
	strcmp(thisGname, lastGname)) {
	if (lastGnameAlloced < thisGnameLen + 1) {
	    lastGnameAlloced = thisGnameLen + 10;
	    lastGname = realloc(lastGname, lastGnameAlloced);
	}
	strcpy(lastGname, thisGname);

	grent = getgrnam(thisGname);
	if (!grent) {
	    endgrent();
	    grent = getgrnam(thisGname);
	    if (!grent) return -1;
	}
	lastGid = grent->gr_gid;
    }

    *gid = lastGid;

    return 0;
}

char * uidToUname(uid_t uid) {
    static int lastUid = -1;
    static char * lastUname = NULL;
    static int lastUnameLen = 0;
    struct passwd * pwent;
    int len;

    if (uid == (uid_t) -1) {
	lastUid = -1;
	return NULL;
    } else if (!uid) {
	return "root";
    } else if (uid == lastUid) {
	return lastUname;
    } else {
	pwent = getpwuid(uid);
	if (!pwent) return NULL;

	lastUid = uid;
	len = strlen(pwent->pw_name);
	if (lastUnameLen < len + 1) {
	    lastUnameLen = len + 20;
	    lastUname = realloc(lastUname, lastUnameLen);
	}
	strcpy(lastUname, pwent->pw_name);

	return lastUname;
    }
}

char * gidToGname(gid_t gid) {
    static int lastGid = -1;
    static char * lastGname = NULL;
    static int lastGnameLen = 0;
    struct group * grent;
    int len;

    if (gid == (gid_t) -1) {
	lastGid = -1;
	return NULL;
    } else if (!gid) {
	return "root";
    } else if (gid == lastGid) {
	return lastGname;
    } else {
	grent = getgrgid(gid);
	if (!grent) return NULL;

	lastGid = gid;
	len = strlen(grent->gr_name);
	if (lastGnameLen < len + 1) {
	    lastGnameLen = len + 20;
	    lastGname = realloc(lastGname, lastGnameLen);
	}
	strcpy(lastGname, grent->gr_name);

	return lastGname;
    }
}

int makeTempFile(char * prefix, char ** fnptr, int * fdptr) {
    char * fn;
    int fd;
    int ran;
    char * tmpdir = rpmGetVar(RPMVAR_TMPPATH);
    struct stat sb;

    if (!prefix) prefix = "";

    fn = malloc(strlen(prefix) + 25 + strlen(tmpdir));

    srandom(time(NULL));
    ran = random() % 100000;
    do {
	sprintf(fn, "%s%s/rpm-tmp.%d", prefix, tmpdir, ran++);
    } while (!access(fn, X_OK));
    
    fd = open(fn, O_CREAT | O_RDWR | O_EXCL, 0700);

    if (fd < 0) {
	rpmError(RPMERR_SCRIPT, _("error creating temporary file %s"), fn);
	return 1;
    }

    if (!stat(fn, &sb) && S_ISLNK(sb.st_mode)) {
	rpmError(RPMERR_SCRIPT, _("error creating temporary file %s"), fn);
	return 1;
    }

    if (fnptr) *fnptr = fn;
    *fdptr = fd;

    return 0;
}
