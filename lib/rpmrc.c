#include <ctype.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/utsname.h>

#include "messages.h"
#include "misc.h"
#include "rpmerr.h"
#include "rpmlib.h"

/* the rpmrc is read from /etc/rpmrc or $HOME/.rpmrc - it is not affected
   by a --root option */

struct option {
    char * name;
    int var;
    int archSpecific;
};

struct archosCacheEntry {
    char * name;
    int count;
    char ** equivs;
    int visited;
};

struct archosCache {
    struct archosCacheEntry * cache;
    int size;
};

struct archosEquivInfo {
    char * name;
    int score;
};

struct archosEquivTable {
    int count;
    struct archosEquivInfo * list;
};

struct defaultEntry {
    char *name;
    char *defName;
};

struct canonEntry {
    char *name;
    char *short_name;
    short num;
};

static int findArchOsScore(struct archosEquivTable * table, char * name);
static struct archosCacheEntry * 
  archosCacheFindEntry(struct archosCache * cache, char * key);
static void archosFindEquivs(struct archosCache * cache, 
			     struct archosEquivTable * table,
			     char * key);
static void archosAddEquiv(struct archosEquivTable * table, char * name,
			   int distance);
static void archosCacheEntryVisit(struct archosCache * cache, 
				  struct archosEquivTable * table, 
				  char * name,
	  			  int distance);
static int archosCompatCacheAdd(char * name, char * fn, int linenum,
				struct archosCache * cache);
static struct archosEquivInfo * archosEquivSearch(
		struct archosEquivTable * table, char * name);

/* this *must* be kept in alphabetical order */
struct option optionTable[] = {
    { "builddir",		RPMVAR_BUILDDIR,		0 },
    { "buildroot",              RPMVAR_BUILDROOT,               0 },
    { "dbpath",			RPMVAR_DBPATH,			0 },
    { "distribution",		RPMVAR_DISTRIBUTION,		0 },
    { "excludedocs",	        RPMVAR_EXCLUDEDOCS,             0 },
    { "ftpproxy",		RPMVAR_FTPPROXY,		0 },
    { "messagelevel",		RPMVAR_MESSAGELEVEL,		0 },
    { "optflags",		RPMVAR_OPTFLAGS,		1 },
    { "packager",               RPMVAR_PACKAGER,                0 },
    { "pgp_name",               RPMVAR_PGP_NAME,                0 },
    { "pgp_path",               RPMVAR_PGP_PATH,                0 },
    { "require_distribution",	RPMVAR_REQUIREDISTRIBUTION,	0 },
    { "require_icon",		RPMVAR_REQUIREICON,		0 },
    { "require_vendor",		RPMVAR_REQUIREVENDOR,		0 },
    { "root",			RPMVAR_ROOT,			0 },
    { "rpmdir",			RPMVAR_RPMDIR,			0 },
    { "signature",		RPMVAR_SIGTYPE,			0 },
    { "sourcedir",		RPMVAR_SOURCEDIR,		0 },
    { "specdir",		RPMVAR_SPECDIR,			0 },
    { "srcrpmdir",		RPMVAR_SRPMDIR,			0 },
    { "timecheck",		RPMVAR_TIMECHECK,		0 },
    { "tmppath",		RPMVAR_TMPPATH,			0 },
    { "topdir",			RPMVAR_TOPDIR,			0 },
    { "vendor",			RPMVAR_VENDOR,			0 },
};

static int optionTableSize = sizeof(optionTable) / sizeof(struct option);

#define READ_TABLES       1
#define READ_OTHER        2

static int readRpmrc(FILE * fd, char * fn, int readWhat);
static void setDefaults(void);
static void setPathDefault(int var, char * s);
static int optionCompare(const void * a, const void * b);

static void setArchOs(char *arch, char *os, int building);
static int addCanon(struct canonEntry **table, int *tableLen, char *line,
		   char *fn, int lineNum);
static int addDefault(struct defaultEntry **table, int *tableLen, char *line,
		      char *fn, int lineNum);
static struct canonEntry *lookupInCanonTable(char *name,
					   struct canonEntry *table,
					   int tableLen);
static char *lookupInDefaultTable(char *name,
				  struct defaultEntry *table,
				  int tableLen);

static int archDefaultTableLen = 0;
static int osDefaultTableLen = 0;
static int archCanonTableLen = 0;
static int osCanonTableLen = 0;
static struct defaultEntry * archDefaultTable = NULL;
static struct defaultEntry * osDefaultTable = NULL;
static struct canonEntry * archCanonTable = NULL;
static struct canonEntry * osCanonTable = NULL;

static struct archosCache archCache;
static struct archosCache osCache;
static struct archosEquivTable archEquivTable;
static struct archosEquivTable osEquivTable;

static int optionCompare(const void * a, const void * b) {
    return strcasecmp(((struct option *) a)->name,
		      ((struct option *) b)->name);
}

static struct archosEquivInfo * archosEquivSearch(
		struct archosEquivTable * table, char * name) {
    int i;

    for (i = 0; i < table->count; i++)
	if (!strcmp(table->list[i].name, name)) 
	    return table->list + i;

    return NULL;
}

static int findArchOsScore(struct archosEquivTable * table, char * name) {
    struct archosEquivInfo * info;

    info = archosEquivSearch(table, name);
    if (info) 
	return info->score;
    else
	return 0;
}

int rpmArchScore(char * test) {
    return findArchOsScore(&archEquivTable, test);
}

int rpmOsScore(char * test) {
    return findArchOsScore(&osEquivTable, test);
}

static struct archosCacheEntry * 
  archosCacheFindEntry(struct archosCache * cache, char * key) {
    int i;

    for (i = 0; i < cache->size; i++)
	if (!strcmp(cache->cache[i].name, key)) return cache->cache + i;

    return NULL;
}

static void archosFindEquivs(struct archosCache * cache, 
			     struct archosEquivTable * table,
			     char * key) {
    int i;

    for (i = 0; i < cache->size; i++)
	cache->cache[i].visited = 0;

    table->count = 0;

    /* We have a general graph built using strings instead of pointers.
       Yuck. We have to start at a point at traverse it, remembering how
       far away everything is. */
    archosAddEquiv(table, key, 1);
    archosCacheEntryVisit(cache, table, key, 2);
}

static void archosAddEquiv(struct archosEquivTable * table, char * name,
			   int distance) {
    struct archosEquivInfo * equiv;

    equiv = archosEquivSearch(table, name);
    if (!equiv) {
	if (table->count)
	    table->list = realloc(table->list, (table->count + 1)
				    * sizeof(*table->list));
	else
	    table->list = malloc(sizeof(*table->list));
    
	table->list[table->count].name = strdup(name);
	table->list[table->count++].score = distance;
    }
}

static void archosCacheEntryVisit(struct archosCache * cache, 
				  struct archosEquivTable * table, 
				  char * name,
	  			  int distance) {
    struct archosCacheEntry * entry;
    int i;

    entry = archosCacheFindEntry(cache, name);
    if (!entry || entry->visited) return;

    entry->visited = 1;

    for (i = 0; i < entry->count; i++) {
	archosAddEquiv(table, entry->equivs[i], distance);
    }

    for (i = 0; i < entry->count; i++) {
	archosCacheEntryVisit(cache, table, entry->equivs[i], distance + 1);
    }
};

static int archosCompatCacheAdd(char * name, char * fn, int linenum,
				struct archosCache * cache) {
    char * chptr, * equivs;
    int delEntry = 0;
    int i;
    struct archosCacheEntry * entry = NULL;
  
    chptr = name;
    while (*chptr && *chptr != ':') chptr++;
    if (!*chptr) {
	error(RPMERR_RPMRC, "missing second ':' at %s:%d\n", fn, linenum);
	return 1;
    } else if (chptr == name) {
	error(RPMERR_RPMRC, "missing architecture name at %s:%d\n", fn, 
			     linenum);
	return 1;
    }

    while (*chptr == ':' || isspace(*chptr)) chptr--;
    *(++chptr) = '\0';
    equivs = chptr + 1;
    while (*equivs && isspace(*equivs)) equivs++;
    if (!*equivs) {
	delEntry = 1;
    }

    if (cache->size) {
	entry = archosCacheFindEntry(cache, name);
	if (entry) {
	    for (i = 0; i < entry->count; i++)
		free(entry->equivs[i]);
	    if (entry->count) free(entry->equivs);
	    entry->count = 0;
	}
    }

    if (!entry) {
	cache->cache = realloc(cache->cache, 
			       (cache->size + 1) * sizeof(*cache->cache));
	entry = cache->cache + cache->size++;
	entry->name = strdup(name);
	entry->count = 0;
	entry->visited = 0;
    }

    if (delEntry) return 0;
	
    chptr = strtok(equivs, " ");
    while (chptr) {
	if (strlen(chptr)) {		/* does strtok() return "" ever?? */
	    if (entry->count)
		entry->equivs = realloc(entry->equivs, sizeof(*entry->equivs) 
					* (entry->count + 1));
	    else
		entry->equivs = malloc(sizeof(*entry->equivs));

	    entry->equivs[entry->count] = strdup(chptr);
	    entry->count++;
	}

	chptr = strtok(NULL, " ");
    }

    return 0;
}

static int addCanon(struct canonEntry **table, int *tableLen, char *line,
		   char *fn, int lineNum)
{
    struct canonEntry *t;
    char *s, *s1;
    
    if (! *tableLen) {
	*tableLen = 2;
	*table = malloc(2 * sizeof(struct canonEntry));
    } else {
	(*tableLen) += 2;
	*table = realloc(*table, sizeof(struct canonEntry) * (*tableLen));
    }
    t = & ((*table)[*tableLen - 2]);

    t->name = strtok(line, ": \t");
    t->short_name = strtok(NULL, " \t");
    s = strtok(NULL, " \t");
    if (! (t->name && t->short_name && s)) {
	error(RPMERR_RPMRC, "Incomplete data line at %s:%d\n", fn, lineNum);
	return RPMERR_RPMRC;
    }
    if (strtok(NULL, " \t")) {
	error(RPMERR_RPMRC, "Too many args in data line at %s:%d\n",
	      fn, lineNum);
	return RPMERR_RPMRC;
    }

    t->num = strtoul(s, &s1, 10);
    if ((*s1) || (s1 == s) || (t->num == ULONG_MAX)) {
	error(RPMERR_RPMRC, "Bad arch/os number: %s (%s:%d)\n", s,
	      fn, lineNum);
	return(RPMERR_RPMRC);
    }

    t->name = strdup(t->name);
    t->short_name = strdup(t->short_name);

    /* From A B C entry */
    /* Add  B B C entry */
    t[1].name = strdup(t->short_name);
    t[1].short_name = strdup(t->short_name);
    t[1].num = t->num;

    return 0;
}

static int addDefault(struct defaultEntry **table, int *tableLen, char *line,
		      char *fn, int lineNum)
{
    struct defaultEntry *t;
    
    if (! *tableLen) {
	*tableLen = 1;
	*table = malloc(sizeof(struct defaultEntry));
    } else {
	(*tableLen)++;
	*table = realloc(*table, sizeof(struct defaultEntry) * (*tableLen));
    }
    t = & ((*table)[*tableLen - 1]);

    t->name = strtok(line, ": \t");
    t->defName = strtok(NULL, " \t");
    if (! (t->name && t->defName)) {
	error(RPMERR_RPMRC, "Incomplete default line at %s:%d\n", fn, lineNum);
	return RPMERR_RPMRC;
    }
    if (strtok(NULL, " \t")) {
	error(RPMERR_RPMRC, "Too many args in default line at %s:%d\n",
	      fn, lineNum);
	return RPMERR_RPMRC;
    }

    t->name = strdup(t->name);
    t->defName = strdup(t->defName);

    return 0;
}

static struct canonEntry *lookupInCanonTable(char *name,
					   struct canonEntry *table,
					   int tableLen)
{
    while (tableLen) {
	tableLen--;
	if (!strcmp(name, table[tableLen].name)) {
	    return &(table[tableLen]);
	}
    }

    return NULL;
}

static char *lookupInDefaultTable(char *name,
				  struct defaultEntry *table,
				  int tableLen)
{
    while (tableLen) {
	tableLen--;
	if (!strcmp(name, table[tableLen].name)) {
	    return table[tableLen].defName;
	}
    }

    return name;
}

static int readRpmrc(FILE * f, char * fn, int readWhat) {
    char buf[1024];
    char * start;
    char * chptr;
    char * archName = NULL;
    int linenum = 0;
    struct option * option, searchOption;

    while (fgets(buf, sizeof(buf), f)) {
	linenum++;

	/* strip off leading spaces */
	start = buf;
	while (*start && isspace(*start)) start++;

	/* I guess #.* should be a comment, but I don't know that. I'll
	   just assume that and hope I'm right. For kicks, I'll take
	   \# to escape it */

	chptr = start;
	while (*chptr) {
	    switch (*chptr) {
	      case '#':
		*chptr = '\0';
		break;

	      case '\n':
		*chptr = '\0';
		break;

	      case '\\':
		chptr++;
		/* fallthrough */
	      default:
		chptr++;
	    }
	}

	/* strip trailing spaces - chptr is at the end of the line already*/
	for (chptr--; chptr >= start && isspace(*chptr); chptr--);
	*(chptr + 1) = '\0';

	if (!start[0]) continue;			/* blank line */
	
	/* look for a :, the id part ends there */
	for (chptr = start; *chptr && *chptr != ':'; chptr++);

	if (! *chptr) {
	    error(RPMERR_RPMRC, "missing ':' at %s:%d\n", fn, linenum);
	    return 1;
	}

	*chptr = '\0';
	chptr++;	 /* now points at beginning of argument */
	while (*chptr && isspace(*chptr)) chptr++;

	if (! *chptr) {
	    error(RPMERR_RPMRC, "missing argument for %s at %s:%d\n", 
		  start, fn, linenum);
	    return 1;
	}

	message(MESS_DEBUG, "got var '%s' arg '%s'\n", start, chptr);

	/* these are options that don't just get stuffed in a VAR somewhere */
	if (!strcasecmp(start, "arch_compat")) {
	    if (readWhat != READ_TABLES) continue;
	    if (archosCompatCacheAdd(chptr, fn, linenum, &archCache))
		return 1;
	} else if (!strcasecmp(start, "os_compat")) {
	    if (readWhat != READ_TABLES) continue;
	    if (archosCompatCacheAdd(chptr, fn, linenum, &osCache))
		return 1;
	} else if (!strcasecmp(start, "arch_canon")) {
	    if (readWhat != READ_TABLES) continue;
	    if (addCanon(&archCanonTable, &archCanonTableLen,
			chptr, fn, linenum))
		return 1;
	} else if (!strcasecmp(start, "os_canon")) {
	    if (readWhat != READ_TABLES) continue;
	    if (addCanon(&osCanonTable, &osCanonTableLen,
			chptr, fn, linenum))
		return 1;
	} else if (!strcasecmp(start, "buildarchtranslate")) {
	    if (readWhat != READ_TABLES) continue;
	    if (addDefault(&archDefaultTable, &archDefaultTableLen,
			   chptr, fn, linenum))
		return 1;
	} else if (!strcasecmp(start, "buildostranslate")) {
	    if (readWhat != READ_TABLES) continue;
	    if (addDefault(&osDefaultTable, &osDefaultTableLen,
			   chptr, fn, linenum))
		return 1;
	}
	if (readWhat == READ_TABLES) {
	    continue;
	}

	/* Parse the argument a little further */
	searchOption.name = start;
	option = bsearch(&searchOption, optionTable, optionTableSize,
			 sizeof(struct option), optionCompare);
	if (!option) {
	    error(RPMERR_RPMRC, "bad option '%s' at %s:%d\n", 
			start, fn, linenum);
	    return 1;
	}

	if (readWhat == READ_OTHER) {
	    if (option->archSpecific) {
		start = chptr;

		/* getArchName() should be safe by now */
		if (!archName) archName = getArchName();

		for (chptr = start; *chptr && !isspace(*chptr); chptr++);
		if (! *chptr) {
		    error(RPMERR_RPMRC, "missing argument for %s at %s:%d\n", 
			  option->name, fn, linenum);
		    return 1;
		}
		*chptr++ = '\0';
		
		while (*chptr && isspace(*chptr)) chptr++;
		if (! *chptr) {
		    error(RPMERR_RPMRC, "missing argument for %s at %s:%d\n", 
			  option->name, fn, linenum);
		    return 1;
		}
		
		if (!strcmp(archName, start)) {
		    message(MESS_DEBUG, "%s is arg for %s platform", chptr,
			    archName);
		    setVar(option->var, chptr);
		}
	    } else {
		setVar(option->var, chptr);
	    }
	    continue;
	}
	error(RPMERR_INTERNAL, "Bad readWhat: %d", readWhat);
	exit(1);
    }

    return 0;
}

static void setDefaults(void) {
    setVar(RPMVAR_OPTFLAGS, "-O2");
    setVar(RPMVAR_SIGTYPE, "none");
    setVar(RPMVAR_PGP_PATH, NULL);
    setVar(RPMVAR_PGP_NAME, NULL);
}

static int readConfigFilesAux(char *file, int readWhat)
{
    FILE * f;
    char * fn;
    char * home;
    int rc = 0;

    f = fopen(LIBRPMRC_FILENAME, "r");
    if (f) {
	rc = readRpmrc(f, LIBRPMRC_FILENAME, readWhat);
	fclose(f);
	if (rc) return rc;
    } else {
	error(RPMERR_RPMRC, "Unable to read " LIBRPMRC_FILENAME);
	return RPMERR_RPMRC;
    }
    
    if (file) 
	fn = file;
    else
	fn = "/etc/rpmrc";

    f = fopen(fn, "r");
    if (f) {
	rc = readRpmrc(f, fn, readWhat);
	fclose(f);
	if (rc) return rc;
    }

    if (!file) {
	home = getenv("HOME");
	if (home) {
	    fn = alloca(strlen(home) + 8);
	    strcpy(fn, home);
	    strcat(fn, "/.rpmrc");
	    f = fopen(fn, "r");
	    if (f) {
		rc |= readRpmrc(f, fn, readWhat);
		fclose(f);
		if (rc) return rc;
	    }
	}
    }

    return 0;
}

int rpmReadConfigFiles(char * file, char * arch, char * os, int building)
{
    int rc = 0;
    int tc;
    char *tcs, *tcse;
    static int alreadyInit = 0;

    if (alreadyInit)
      return 1;
    alreadyInit = 1;
    
    setDefaults();
    
    rc = readConfigFilesAux(file, READ_TABLES);
    if (rc) return rc;

    setArchOs(arch, os, building);

    rc = readConfigFilesAux(file, READ_OTHER);
    if (rc) return rc;

    /* set default directories */

    if (!getVar(RPMVAR_TMPPATH))
	setVar(RPMVAR_TMPPATH, "/tmp");

    setPathDefault(RPMVAR_BUILDDIR, "BUILD");    
    setPathDefault(RPMVAR_RPMDIR, "RPMS");    
    setPathDefault(RPMVAR_SRPMDIR, "SRPMS");    
    setPathDefault(RPMVAR_SOURCEDIR, "SOURCES");    
    setPathDefault(RPMVAR_SPECDIR, "SPECS");

    /* setup arch equivalences */
    
    archosFindEquivs(&archCache, &archEquivTable, getArchName());
    archosFindEquivs(&osCache, &osEquivTable, getOsName());

    /* Do some checking */
    if ((tcs = getVar(RPMVAR_TIMECHECK))) {
	tcse = NULL;
	tc = strtoul(tcs, &tcse, 10);
	if ((*tcse) || (tcse == tcs) || (tc == ULONG_MAX)) {
	    error(RPMERR_RPMRC, "Bad arg to timecheck: %s", tcs);
	    return(RPMERR_RPMRC);
	}
    }
    
    return 0;
}

static void setPathDefault(int var, char * s) {
    char * topdir;
    char * fn;

    if (getVar(var)) return;

    topdir = getVar(RPMVAR_TOPDIR);
    if (!topdir) topdir = getVar(RPMVAR_TMPPATH);
	
    fn = alloca(strlen(topdir) + strlen(s) + 2);
    strcpy(fn, topdir);
    if (fn[strlen(topdir) - 1] != '/')
	strcat(fn, "/");
    strcat(fn, s);
   
    setVar(var, fn);
}

static int osnum;
static int archnum;
static char *osname;
static char *archname;
static int archOsIsInit = 0;

static void setArchOs(char *arch, char *os, int build)
{
    struct utsname un;
    struct canonEntry *archCanon, *osCanon;

    if (archOsIsInit) {
	error(RPMERR_INTERNAL, "Internal error: Arch/OS already initialized!");
        error(RPMERR_INTERNAL, "Arch: %d\nOS: %d", archnum, osnum);
	exit(1);
    }

    uname(&un);
    if (build) {
	if (! arch) {
	    arch = lookupInDefaultTable(un.machine, archDefaultTable,
					archDefaultTableLen);
	}
	if (! os) {
	    os = lookupInDefaultTable(un.sysname, osDefaultTable,
				      osDefaultTableLen);
	}
    } else {
	arch = un.machine;
	os = un.sysname;
    }

    archCanon = lookupInCanonTable(arch, archCanonTable, archCanonTableLen);
    osCanon = lookupInCanonTable(os, osCanonTable, osCanonTableLen);
    if (archCanon) {
	archnum = archCanon->num;
	archname = strdup(archCanon->short_name);
    } else {
	archnum = 255;
	archname = strdup(arch);
	message(MESS_WARNING, "Unknown architecture: %s\n", arch);
	message(MESS_WARNING, "Please contact rpm-list@redhat.com\n");
    }
    if (osCanon) {
	osnum = osCanon->num;
	osname = strdup(osCanon->short_name);
    } else {
	osnum = 255;
	osname = strdup(os);
	message(MESS_WARNING, "Unknown OS: %s\n", os);
	message(MESS_WARNING, "Please contact rpm-list@redhat.com\n");
    }

    archOsIsInit = 1;
}

#define FAIL_IF_NOT_INIT \
{\
    if (! archOsIsInit) {\
	error(RPMERR_INTERNAL, "Internal error: Arch/OS not initialized!");\
        error(RPMERR_INTERNAL, "Arch: %d\nOS: %d", archnum, osnum);\
	exit(1);\
    }\
}

int getOsNum(void)
{
    FAIL_IF_NOT_INIT;
    return osnum;
}

int getArchNum(void)
{
    FAIL_IF_NOT_INIT;
    return archnum;
}

char *getOsName(void)
{
    FAIL_IF_NOT_INIT;
    return osname;
}

char *getArchName(void)
{
    FAIL_IF_NOT_INIT;
    return archname;
}

int rpmShowRC(FILE *f)
{
    struct option *opt;
    int count = 0;
    char *s;
    int i;

    fprintf(f, "ARCHITECTURE AND OS:\n");
    fprintf(f, "build arch           : %s\n", getArchName());
    fprintf(f, "build os             : %s\n", getOsName());

    /* This is a major hack */
    archOsIsInit = 0;
    setArchOs(NULL, NULL, 0);
    archosFindEquivs(&archCache, &archEquivTable, getArchName());
    archosFindEquivs(&osCache, &osEquivTable, getOsName());
    fprintf(f, "install arch         : %s\n", getArchName());
    fprintf(f, "install os           : %s\n", getOsName());
    fprintf(f, "compatible arch list :");
    for (i = 0; i < archEquivTable.count; i++)
	fprintf(f," %s", archEquivTable.list[i].name);
    fprintf(f, "\n");
    fprintf(f, "compatible os list   :");
    for (i = 0; i < osEquivTable.count; i++)
	fprintf(f," %s", osEquivTable.list[i].name);
    fprintf(f, "\n");

    fprintf(f, "RPMRC VALUES:\n");
    opt = optionTable;
    while (count < optionTableSize) {
	s = getVar(opt->var);
	fprintf(f, "%-20s : %s\n", opt->name, s ? s : "(not set)");
	opt++;
	count++;
    }
    
    return 0;
}
