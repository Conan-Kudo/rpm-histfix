#include "system.h"

#if HAVE_SYS_SYSTEMCFG_H
#include <sys/systemcfg.h>
#else
#define __power_pc() 0
#endif

#include "rpmlib.h"
#include "rpmmacro.h"

#include "misc.h"

static char *usrlibrpmrc = LIBRPMRC_FILENAME;
static char *etcrpmrc = "/etc/rpmrc";
static char *macrofiles = MACROFILES;

struct MacroContext globalMacroContext;

struct machCacheEntry {
    char * name;
    int count;
    char ** equivs;
    int visited;
};

struct machCache {
    struct machCacheEntry * cache;
    int size;
};

struct machEquivInfo {
    char * name;
    int score;
};

struct machEquivTable {
    int count;
    struct machEquivInfo * list;
};

struct rpmvarValue {
    char * value;
    /* eventually, this arch will be replaced with a generic condition */
    char * arch;
    struct rpmvarValue * next;
};

struct rpmOption {
    char * name;
    int var;
    int archSpecific, required, macroize, localize;
    struct rpmOptionValue * value;
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

/* tags are 'key'canon, 'key'translate, 'key'compat

   for giggles, 'key'_canon, 'key'_compat, and 'key'_canon will also work */
struct tableType {
    char * key;
    int hasCanon, hasTranslate;
    struct machEquivTable equiv;
    struct machCache cache;
    struct defaultEntry * defaults;
    struct canonEntry * canons;
    int defaultsLength;
    int canonsLength;
};

/*@-fullinitblock@*/
static struct tableType tables[RPM_MACHTABLE_COUNT] = {
    { "arch", 1, 0 },
    { "os", 1, 0 },
    { "buildarch", 0, 1 },
    { "buildos", 0, 1 }
};

/* this *must* be kept in alphabetical order */
/* The order of the flags is archSpecific, required, macroize, localize */

static struct rpmOption optionTable[] = {
    { "arch",			RPMVAR_ARCH,			0, 1,	1, 2 },
    { "builddir",		RPMVAR_BUILDDIR,		0, 0,	1, 2 },
    { "buildarch",		RPMVAR_BUILDARCH,           	0, 1,	1, 0 },
    { "buildos",		RPMVAR_BUILDOS,           	0, 1,	1, 0 },
    { "buildplatform",		RPMVAR_BUILDPLATFORM,           0, 1,	1, 2 },
    { "buildroot",              RPMVAR_BUILDROOT,               0, 0,	1, 0 },
    { "buildshell",             RPMVAR_BUILDSHELL,              0, 0,	1, 0 },
    { "bzip2bin",		RPMVAR_BZIP2BIN,		0, 1,	1, 2 },
    { "dbpath",			RPMVAR_DBPATH,			0, 1,	1, 2 },
    { "defaultdocdir",		RPMVAR_DEFAULTDOCDIR,		0, 0,	1, 1 },
    { "distribution",		RPMVAR_DISTRIBUTION,		0, 0,	1, 0 },
    { "excludedocs",	        RPMVAR_EXCLUDEDOCS,             0, 0,	1, 0 },
    { "fixperms",		RPMVAR_FIXPERMS,		0, 1,	1, 2 },
    { "ftpport",		RPMVAR_FTPPORT,			0, 0,	1, 0 },
    { "ftpproxy",		RPMVAR_FTPPROXY,		0, 0,	1, 0 },
    { "gzipbin",		RPMVAR_GZIPBIN,			0, 1,	1, 2 },
    { "include",		RPMVAR_INCLUDE,			0, 1,	1, 2 },
    { "instchangelog",		RPMVAR_INSTCHANGELOG,		0, 0,	0, 0 },
    { "langpatt",               RPMVAR_LANGPATT,                0, 0,	1, 0 },
    { "macrofiles",		RPMVAR_MACROFILES,		0, 0,	1, 1 },
    { "messagelevel",		RPMVAR_MESSAGELEVEL,		0, 0,	1, 0 },
    { "netsharedpath",		RPMVAR_NETSHAREDPATH,		0, 0,	1, 0 },
    { "optflags",		RPMVAR_OPTFLAGS,		1, 0,	1, 0 },
    { "os",			RPMVAR_OS,                      0, 1,	1, 2 },
    { "packager",               RPMVAR_PACKAGER,                0, 0,	1, 0 },
    { "pgp_name",               RPMVAR_PGP_NAME,                0, 0,	1, 0 },
    { "pgp_path",               RPMVAR_PGP_PATH,                0, 0,	1, 0 },
    { "provides",               RPMVAR_PROVIDES,                0, 0,	1, 0 },
    { "require_distribution",	RPMVAR_REQUIREDISTRIBUTION,	0, 0,	1, 0 },
    { "require_icon",		RPMVAR_REQUIREICON,		0, 0,	1, 0 },
    { "require_vendor",		RPMVAR_REQUIREVENDOR,		0, 0,	1, 0 },
/* root is obsolete - use buildroot instead                             */
/*    { "root",			RPMVAR_ROOT,			0, 0,	1, 0 }, */
    { "rpmdir",			RPMVAR_RPMDIR,			0, 0,	1, 1 },
    { "rpmfilename",		RPMVAR_RPMFILENAME,		0, 1,	1, 2 },
#if defined(RPMVAR_SETENV)
    { "setenv",			RPMVAR_SETENV,			0, 1,	0. 0 },
#endif
    { "signature",		RPMVAR_SIGTYPE,			0, 0,	0, 0 },
    { "sourcedir",		RPMVAR_SOURCEDIR,		0, 0,	1, 1 },
    { "specdir",		RPMVAR_SPECDIR,			0, 0,	1, 1 },
    { "srcrpmdir",		RPMVAR_SRPMDIR,			0, 0,	1, 1 },
    { "timecheck",		RPMVAR_TIMECHECK,		0, 0,	1, 0 },
    { "tmppath",		RPMVAR_TMPPATH,			0, 1,	1, 2 },
    { "topdir",			RPMVAR_TOPDIR,			0, 0,	1, 1 },
    { "vendor",			RPMVAR_VENDOR,			0, 0,	1, 1 },
};
/*@=fullinitblock@*/
static int optionTableSize = sizeof(optionTable) / sizeof(*optionTable);

#define OS	0
#define ARCH	1

static char * current[2];
static int currTables[2] = { RPM_MACHTABLE_INSTOS, RPM_MACHTABLE_INSTARCH };
static struct rpmvarValue values[RPMVAR_NUM];

/* prototypes */
static void defaultMachine(char ** arch, char ** os);
static int doReadRC(FD_t fd, char * filename);
static int optionCompare(const void * a, const void * b);
static int addCanon(struct canonEntry **table, int *tableLen, char *line,
		   char *fn, int lineNum);
static int addDefault(struct defaultEntry **table, int *tableLen, char *line,
		      char *fn, int lineNum);
static void freeRpmVar(struct rpmvarValue * orig);
static void rpmSetVarArch(int var, char * val, char * arch);
static struct canonEntry *lookupInCanonTable(char *name,
					   struct canonEntry *table,
					   int tableLen);
static char *lookupInDefaultTable(char *name,
				  struct defaultEntry *table,
				  int tableLen);
static void setDefaults(void);
static void setPathDefault(int var, char * macroname, char * subdir);
static void rebuildCompatTables(int type, char * name);

/* compatiblity tables */
static int machCompatCacheAdd(char * name, char * fn, int linenum,
				struct machCache * cache);
static struct machCacheEntry * machCacheFindEntry(struct machCache * cache,
						  char * key);
static struct machEquivInfo * machEquivSearch(
		struct machEquivTable * table, char * name);
static void machAddEquiv(struct machEquivTable * table, char * name,
			   int distance);
static void machCacheEntryVisit(struct machCache * cache,
				  struct machEquivTable * table,
				  char * name,
	  			  int distance);
static void machFindEquivs(struct machCache * cache,
			     struct machEquivTable * table,
			     char * key);

static int optionCompare(const void * a, const void * b) {
    return strcasecmp(((struct rpmOption *) a)->name,
		      ((struct rpmOption *) b)->name);
}

static void rpmRebuildPlatformVars(char ** buildplatform, char **canonarch,
                             char **canonos);


static struct machCacheEntry * machCacheFindEntry(struct machCache * cache,
						  char * key) {
    int i;

    for (i = 0; i < cache->size; i++)
	if (!strcmp(cache->cache[i].name, key)) return cache->cache + i;

    return NULL;
}

static int machCompatCacheAdd(char * name, char * fn, int linenum,
				struct machCache * cache) {
    char * chptr, * equivs;
    int delEntry = 0;
    int i;
    struct machCacheEntry * entry = NULL;

    while (*name && isspace(*name)) name++;

    chptr = name;
    while (*chptr && *chptr != ':') chptr++;
    if (!*chptr) {
	rpmError(RPMERR_RPMRC, _("missing second ':' at %s:%d"), fn, linenum);
	return 1;
    } else if (chptr == name) {
	rpmError(RPMERR_RPMRC, _("missing architecture name at %s:%d"), fn,
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
	entry = machCacheFindEntry(cache, name);
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

static struct machEquivInfo * machEquivSearch(
		struct machEquivTable * table, char * name) {
    int i;

/*
 * XXX The strcasecmp below is necessary so the old (rpm < 2.90) style
 * XXX os-from-uname (e.g. "Linux") is compatible with the new
 * XXX os-from-platform (e.g "linux" from "sparc-*-linux").
 * XXX A copy of this string is embedded in headers and is
 * XXX used by rpmInstallPackage->{os,arch}Okay->rpmMachineScore->.
 * XXX to verify correct arch/os from headers.
 */
    for (i = 0; i < table->count; i++)
	if (!strcasecmp(table->list[i].name, name))
	    return table->list + i;

    return NULL;
}

static void machAddEquiv(struct machEquivTable * table, char * name,
			   int distance) {
    struct machEquivInfo * equiv;

    equiv = machEquivSearch(table, name);
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

static void machCacheEntryVisit(struct machCache * cache,
				  struct machEquivTable * table,
				  char * name,
	  			  int distance) {
    struct machCacheEntry * entry;
    int i;

    entry = machCacheFindEntry(cache, name);
    if (!entry || entry->visited) return;

    entry->visited = 1;

    for (i = 0; i < entry->count; i++) {
	machAddEquiv(table, entry->equivs[i], distance);
    }

    for (i = 0; i < entry->count; i++) {
	machCacheEntryVisit(cache, table, entry->equivs[i], distance + 1);
    }
}

static void machFindEquivs(struct machCache * cache,
			     struct machEquivTable * table,
			     char * key) {
    int i;

    for (i = 0; i < cache->size; i++)
	cache->cache[i].visited = 0;

    table->count = 0;

    /* We have a general graph built using strings instead of pointers.
       Yuck. We have to start at a point at traverse it, remembering how
       far away everything is. */
    machAddEquiv(table, key, 1);
    machCacheEntryVisit(cache, table, key, 2);
}

static int addCanon(struct canonEntry **table, int *tableLen, char *line,
		    char *fn, int lineNum) {
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
	rpmError(RPMERR_RPMRC, _("Incomplete data line at %s:%d"), fn, lineNum);
	return RPMERR_RPMRC;
    }
    if (strtok(NULL, " \t")) {
	rpmError(RPMERR_RPMRC, _("Too many args in data line at %s:%d"),
	      fn, lineNum);
	return RPMERR_RPMRC;
    }

    t->num = strtoul(s, &s1, 10);
    if ((*s1) || (s1 == s) || (t->num == ULONG_MAX)) {
	rpmError(RPMERR_RPMRC, _("Bad arch/os number: %s (%s:%d)"), s,
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
		      char *fn, int lineNum) {
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
	rpmError(RPMERR_RPMRC, _("Incomplete default line at %s:%d"),
		 fn, lineNum);
	return RPMERR_RPMRC;
    }
    if (strtok(NULL, " \t")) {
	rpmError(RPMERR_RPMRC, _("Too many args in default line at %s:%d"),
	      fn, lineNum);
	return RPMERR_RPMRC;
    }

    t->name = strdup(t->name);
    t->defName = strdup(t->defName);

    return 0;
}

static struct canonEntry *lookupInCanonTable(char *name,
					     struct canonEntry *table,
					     int tableLen) {
    while (tableLen) {
	tableLen--;
	if (!strcmp(name, table[tableLen].name)) {
	    return &(table[tableLen]);
	}
    }

    return NULL;
}

static char *lookupInDefaultTable(char *name, struct defaultEntry *table,
				  int tableLen) {
    while (tableLen) {
	tableLen--;
	if (!strcmp(name, table[tableLen].name)) {
	    return table[tableLen].defName;
	}
    }

    return name;
}

int rpmReadConfigFiles(char * file, char * arch, char * os, int building,
    char * buildplatform)
{

    char * canonarch, * canonos;

    rpmSetMachine(arch, os);
    if (!buildplatform) 
       rpmRebuildPlatformVars(&buildplatform, &canonarch, &canonos);
    else 
       addMacro(&globalMacroContext, "_buildplatform_preset", NULL,
          buildplatform, RMIL_RPMRC);

/*
    addMacro(&globalMacroContext, "buildplatform", NULL, buildplatform,
      RMIL_RPMRC);
*/

    if (rpmReadRC(file)) return -1;

    rpmRebuildPlatformVars(&buildplatform, &canonarch, &canonos);

    /* This is where we finally set the arch and os absolutely */

/*    rpmSetMachine(canonarch, canonos); */


   {	char *buildarch;
	char *buildos;

	/* XXX getMacroBody() may be nuked -- (see below) */
	/* XXX the macro names buildarch/buildos don't exist */
	buildarch = (char *) getMacroBody(&globalMacroContext, "buildarch");
	buildos = (char *) getMacroBody(&globalMacroContext, "buildos");

	rpmSetMachine(buildarch, buildos);
   }

    return 0;
}

static void setPathDefault(int var, char *macroname, char *subdir) {
    char * topdir;
    char * fn;

    if (rpmGetVar(var)) return;

    topdir = rpmGetVar(RPMVAR_TOPDIR);
    if (!topdir) topdir = rpmGetVar(RPMVAR_TMPPATH);

    fn = alloca(strlen(topdir) + strlen(subdir) + 2);
    strcpy(fn, topdir);
    if (fn[strlen(topdir) - 1] != '/')
	strcat(fn, "/");
    strcat(fn, subdir);

    rpmSetVar(var, fn);

    if (macroname != NULL) {
#define	_TOPDIRMACRO	"%{_topdir}/"
	char *body = alloca(sizeof(_TOPDIRMACRO) + strlen(subdir) + 2);
	strcpy(body, _TOPDIRMACRO);
	strcat(body, subdir);
	addMacro(&globalMacroContext, macroname, NULL, body, RMIL_RPMRC);
#undef _TOPDIRMACRO
    }
}

static void setDefaults(void) {

    /* Read in all macro files (and also set maximum recursion depth) */
    initMacros(&globalMacroContext, macrofiles);

    rpmSetVar(RPMVAR_OPTFLAGS, "-O2");
    rpmSetVar(RPMVAR_SIGTYPE, "none");
    rpmSetVar(RPMVAR_DEFAULTDOCDIR, "/usr/doc");
    rpmSetVar(RPMVAR_TOPDIR, "/usr/src/redhat");
    rpmSetVar(RPMVAR_BUILDSHELL, "/bin/sh");
}

int rpmReadRC(char * file) {
    FD_t fd;
    char * fn;
    char * home;
    int rc = 0;
    static int first = 1;

    if (first) {
	setDefaults();
	first = 0;
    }

    fd = fdOpen(usrlibrpmrc, O_RDONLY, 0);
    if (fdFileno(fd) >= 0) {
	rc = doReadRC(fd, usrlibrpmrc);
	fdClose(fd);
 	if (rc) return rc;
    } else {
	rpmError(RPMERR_RPMRC, _("Unable to open %s for reading: %s."),
		 usrlibrpmrc, strerror(errno));
	return 1;
    }

    if (file)
	fn = file;
    else
	fn = etcrpmrc;

    fd = fdOpen(fn, O_RDONLY, 0);
    if (fdFileno(fd) >= 0) {
	rc = doReadRC(fd, fn);
	fdClose(fd);
	if (rc) return rc;
    } else if (file) {
	rpmError(RPMERR_RPMRC, _("Unable to open %s for reading: %s."), file,
		 strerror(errno));
	return 1;
    }

    if (!file) {
	home = getenv("HOME");
	if (home) {
	    fn = alloca(strlen(home) + 8);
	    strcpy(fn, home);
	    strcat(fn, "/.rpmrc");
	    fd = fdOpen(fn, O_RDONLY, 0);
	    if (fdFileno(fd) >= 0) {
		rc |= doReadRC(fd, fn);
		fdClose(fd);
		if (rc) return rc;
	    }
	}
    }

    rpmSetMachine(NULL, NULL);	/* XXX WTFO? Why bother? */

    setPathDefault(RPMVAR_BUILDDIR, "_builddir", "BUILD");
    setPathDefault(RPMVAR_RPMDIR, "_rpmdir", "RPMS");
    setPathDefault(RPMVAR_SRPMDIR, "_srcrpmdir", "SRPMS");
    setPathDefault(RPMVAR_SOURCEDIR, "_sourcedir", "SOURCES");
    setPathDefault(RPMVAR_SPECDIR, "_specdir", "SPECS");

    if ((fn = rpmGetVar(RPMVAR_MACROFILES)) != NULL)
	initMacros(&globalMacroContext, fn);

    return 0;
}

static int doReadRC(FD_t fd, char * filename) {
    char buf[BUFSIZ];
    char * start, * chptr, * next, * rest;
    int linenum = 0;
    struct rpmOption searchOption, * option;
    int i;
    int gotit;
    int rc;

  { struct stat sb;
    fstat(fdFileno(fd), &sb);
    next = alloca(sb.st_size + 2);
    if (fdRead(fd, next, sb.st_size) != sb.st_size) {
	rpmError(RPMERR_RPMRC, _("Failed to read %s: %s."), filename,
		 strerror(errno));
	return 1;
    }
    next[sb.st_size] = '\n';
    next[sb.st_size + 1] = '\0';
  }

    while (*next) {
	linenum++;

	chptr = start = next;
	while (*chptr != '\n') chptr++;

	*chptr = '\0';
	next = chptr + 1;

	while (isspace(*start)) start++;

	/* we used to allow comments to begin anywhere, but not anymore */
	if (*start == '#' || !*start) continue;

	chptr = start;
	while (!isspace(*chptr) && *chptr != ':' && *chptr) chptr++;

	if (isspace(*chptr)) {
	    *chptr++ = '\0';
	    while (isspace(*chptr) && *chptr != ':' && *chptr) chptr++;
	}

	if (*chptr != ':') {
	    rpmError(RPMERR_RPMRC, _("missing ':' at %s:%d"),
		     filename, linenum);
	    return 1;
	}

	*chptr++ = '\0';

	searchOption.name = start;
	option = bsearch(&searchOption, optionTable, optionTableSize,
			 sizeof(struct rpmOption), optionCompare);

	if (option) {
	    start = chptr;
	    while (isspace(*start) && *start) start++;

	    if (! *start) {
		rpmError(RPMERR_RPMRC, _("missing argument for %s at %s:%d"),
		      option->name, filename, linenum);
		return 1;
	    }

	    switch (option->var) {
/*
 * XXX This can *easily* be achieved by just 
 *	1) Create a file someplace.
 *	2) Put whatever macros you want in it.
 *	3) Add the file name to MACROFILE in Makefile.inc.in --
 *	the compile parameter is actually a ':' separated list.
 * I'll change the name to MACROFILES :-)
 *
 *	Alternatively, I'd rather just slurp the entire rpmrc file
 *	and run it through expandMacros() before parsing here -- all %define
 *	lines will work just fine without adding more syntax here.
 */
#if defined(RPMVAR_SETENV)
	    case RPMVAR_SETENV:
	      {
 		char * macroname, *envname;

		/* The format is "setenv: macroname envname value" */

		/* Skip to the end of the first word, which is the
		macro name. */

		while (isspace(*start) && *start) start++;
		if (! *start) {
		  rpmError(RPMERR_RPMRC, _("no macroname for setenv %s:%d"),
		      	filename, linenum);
		  return 1;
		}
		macroname = start;
		while (!isspace(*start) && (*start)) start++;
		*start = '\0';

		start++;
		while (isspace(*start) && *start) start++;

		/* Skip to the end of the second word, which is the
		environment name. */
		envname = start;
		while (!isspace(*start) && (*start)) start++;
		*start = '\0';

		start++;

		addMacro(&globalMacroContext, start, NULL, macroname, RMIL_RPMRC);
		setenv(envname,start,1);
	      } break;
#endif	/* defined(RPMVAR_SETENV) */

	    case RPMVAR_INCLUDE:
	      {	FD_t fdinc;

		rpmRebuildPlatformVars(NULL,NULL,NULL);

		strcpy(buf, start);
		if (expandMacros(NULL, &globalMacroContext, buf, sizeof(buf))) {
		    rpmError(RPMERR_RPMRC, _("expansion failed at %s:d \"%s\""),
			filename, linenum, start);
		    return 1;
		}

		if (fdFileno(fdinc = fdOpen(buf, O_RDONLY, 0)) < 0) {
		    rpmError(RPMERR_RPMRC, _("cannot open %s at %s:%d"),
			buf, filename, linenum);
			return 1;
		}
		rc = doReadRC(fdinc, buf);
		fdClose(fdinc);
		if (rc) return rc;
	      }	break;
	    default:
		break;
	    }

	    chptr = start;
	    if (option->archSpecific) {
		while (!isspace(*chptr) && *chptr) chptr++;

		if (!*chptr) {
		    rpmError(RPMERR_RPMRC,
				_("missing architecture for %s at %s:%d"),
			  	option->name, filename, linenum);
		    return 1;
		}

		*chptr++ = '\0';

		while (isspace(*chptr) && *chptr) chptr++;
		if (!*chptr) {
		    rpmError(RPMERR_RPMRC,
				_("missing argument for %s at %s:%d"),
			  	option->name, filename, linenum);
		    return 1;
		}
		if (option->macroize && !strcmp(start, current[ARCH])) {
		    char *s = buf;
		    if (option->localize)
			*s++ = '_';
		    strcpy(s, option->name);
		    addMacro(&globalMacroContext, buf, NULL, chptr, RMIL_RPMRC);
		}
	    } else {
		start = NULL;	/* no arch */
		/* XXX for now only non-arch values can get macroized */
		if (option->macroize) {
		    char *s = buf;
		    if (option->localize)
			*s++ = '_';
		    strcpy(s, option->name);
		    addMacro(&globalMacroContext, buf, NULL, chptr, RMIL_RPMRC);
		}
	    }
	    rpmSetVarArch(option->var, chptr, start);
	} else {
	    gotit = 0;

	    for (i = 0; i < RPM_MACHTABLE_COUNT; i++) {
		if (!strncmp(tables[i].key, start, strlen(tables[i].key)))
		    break;
	    }

	    if (i < RPM_MACHTABLE_COUNT) {
		rest = start + strlen(tables[i].key);
		if (*rest == '_') rest++;

		if (!strcmp(rest, "compat")) {
		    if (machCompatCacheAdd(chptr, filename, linenum,
						&tables[i].cache))
			return 1;
		    gotit = 1;
		} else if (tables[i].hasTranslate &&
			   !strcmp(rest, "translate")) {
		    if (addDefault(&tables[i].defaults,
				   &tables[i].defaultsLength,
				   chptr, filename, linenum))
			return 1;
		    gotit = 1;
		} else if (tables[i].hasCanon &&
			   !strcmp(rest, "canon")) {
		    if (addCanon(&tables[i].canons, &tables[i].canonsLength,
				 chptr, filename, linenum))
			return 1;
		    gotit = 1;
		}
	    }

	    if (!gotit) {
		rpmError(RPMERR_RPMRC, _("bad option '%s' at %s:%d"),
			    start, filename, linenum);
	    }
	}
    }

    return 0;
}

static void defaultMachine(char ** arch, char ** os) {
    static struct utsname un;
    static int gotDefaults = 0;
    char * chptr;
    struct canonEntry * canon;

    if (!gotDefaults) {
	uname(&un);

#if !defined(__linux__)
#ifdef SNI
        /* USUALLY un.sysname on sinix does start with the word "SINIX"
         * let's be absolutely sure
         */
        sprintf(un.sysname,"SINIX");
#endif
	if (!strcmp(un.sysname, "AIX")) {
	    strcpy(un.machine, __power_pc() ? "ppc" : "rs6000");
            sprintf(un.sysname,"aix%s.%s",un.version,un.release);
	}
        else if (!strcmp(un.sysname, "SunOS")) {
           if (!strncmp(un.release,"4", 1)) /* SunOS 4.x */ {
	      int fd;
              for (fd=0;(un.release[fd] != 0 && (fd < sizeof(un.release)));fd++)
                 if (!isdigit(un.release[fd]) && (un.release[fd] != '.')) {
                    un.release[fd] = 0;
                    break;
                 }
              sprintf(un.sysname,"sunos%s",un.release);
           }
           
           else /* Solaris 2.x: n.x.x becomes n-3.x.x */
              sprintf(un.sysname,"solaris%1d%s",atoi(un.release)-3,un.release+1+(atoi(un.release)/10));
        }
        else if (!strcmp(un.sysname, "HP-UX"))
           /*make un.sysname look like hpux9.05 for example*/
           sprintf(un.sysname,"hpux%s",strpbrk(un.release,"123456789"));
        else if (!strcmp(un.sysname, "OSF1"))
           /*make un.sysname look like osf3.2 for example*/
           sprintf(un.sysname,"osf%s",strpbrk(un.release,"123456789"));
        else if (!strncmp(un.sysname, "IP", 2))
           un.sysname[2] = '\0';
        else if (!strncmp(un.sysname, "SINIX", 5)) {
           sprintf(un.sysname, "sinix%s",un.release);
           if (!strncmp(un.machine, "RM", 2))
              sprintf(un.machine, "mips");
        }
        else if ((!strncmp(un.machine, "34", 2) || \
                 !strncmp(un.machine, "33", 2)) && \
                 !strncmp(un.release, "4.0", 3)) {
           /* we are on ncr-sysv4 */
	   char *prelid = NULL;
           FD_t fd = fdOpen("/etc/.relid", O_RDONLY, 0700);
           if (fdFileno(fd) > 0) {
              chptr = (char *) calloc(256,1);
              if (chptr != NULL) {
                 int irelid = read(fd, (void *)chptr, 256);
                 fdClose(fd);
                 /* example: "112393 RELEASE 020200 Version 01 OS" */
                 if (irelid > 0) {
                    if ((prelid=strstr(chptr, "RELEASE "))){
                       prelid += strlen("RELEASE ")+1;
                       sprintf(un.sysname,"ncr-sysv4.%.*s",1,prelid);
                    }
		 }
                 free (chptr);
              }
           }           
           if (!prelid)
              /* parsing /etc/.relid file failed */
              strcpy(un.sysname,"ncr-sysv4");
           /* wrong, just for now, find out how to look for i586 later*/
           strcpy(un.machine,"i486");
        }
#endif
                   
	/* get rid of the hyphens in the sysname */
	chptr = un.machine;
	while (*chptr++)
	    if (*chptr == '/') *chptr = '-';

#	if defined(__MIPSEL__) || defined(__MIPSEL) || defined(_MIPSEL)
	    /* little endian */
	    strcpy(un.machine, "mipsel");
#	elif defined(__MIPSEB__) || defined(__MIPSEB) || defined(_MIPSEB)
	   /* big endian */
		strcpy(un.machine, "mipseb");
#	endif

#	if defined(__hpux) && defined(_SC_CPU_VERSION)
	{
#	    if !defined(CPU_PA_RISC1_2)
#                define CPU_PA_RISC1_2  0x211 /* HP PA-RISC1.2 */
#           endif
#           if !defined(CPU_PA_RISC2_0)
#               define CPU_PA_RISC2_0  0x214 /* HP PA-RISC2.0 */
#           endif
	    int cpu_version = sysconf(_SC_CPU_VERSION);

#	    if defined(CPU_HP_MC68020)
		if (cpu_version == CPU_HP_MC68020)
		    strcpy(un.machine, "m68k");
#	    endif
#	    if defined(CPU_HP_MC68030)
		if (cpu_version == CPU_HP_MC68030)
		    strcpy(un.machine, "m68k");
#	    endif
#	    if defined(CPU_HP_MC68040)
		if (cpu_version == CPU_HP_MC68040)
		    strcpy(un.machine, "m68k");
#	    endif

#	    if defined(CPU_PA_RISC1_0)
		if (cpu_version == CPU_PA_RISC1_0)
		    strcpy(un.machine, "hppa1.0");
#	    endif
#	    if defined(CPU_PA_RISC1_1)
		if (cpu_version == CPU_PA_RISC1_1)
		    strcpy(un.machine, "hppa1.1");
#	    endif
#	    if defined(CPU_PA_RISC1_2)
		if (cpu_version == CPU_PA_RISC1_2)
		    strcpy(un.machine, "hppa1.2");
#	    endif
#	    if defined(CPU_PA_RISC2_0)
		if (cpu_version == CPU_PA_RISC2_0)
		    strcpy(un.machine, "hppa2.0");
#	    endif
	}
#	endif

	/* the uname() result goes through the arch_canon table */
	canon = lookupInCanonTable(un.machine,
				   tables[RPM_MACHTABLE_INSTARCH].canons,
				   tables[RPM_MACHTABLE_INSTARCH].canonsLength);
	if (canon)
	    strcpy(un.machine, canon->short_name);

	canon = lookupInCanonTable(un.sysname,
				   tables[RPM_MACHTABLE_INSTOS].canons,
				   tables[RPM_MACHTABLE_INSTOS].canonsLength);
	if (canon)
	    strcpy(un.sysname, canon->short_name);
    }

    if (arch) *arch = un.machine;
    if (os) *os = un.sysname;
}

static char * rpmGetVarArch(int var, char * arch) {
    struct rpmvarValue * next;

    if (!arch) arch = current[ARCH];

    if (arch) {
	next = &values[var];
	while (next) {
	    if (next->arch && !strcmp(next->arch, arch)) return next->value;
	    next = next->next;
	}
    }

    next = values + var;
    while (next && next->arch) next = next->next;

    return next ? next->value : NULL;
}

char *rpmGetVar(int var)
{
    return rpmGetVarArch(var, NULL);
}

int rpmGetBooleanVar(int var) {
    char * val;
    int num;
    char * chptr;

    val = rpmGetVar(var);
    if (!val) return 0;

    if (val[0] == 'y' || val[0] == 'Y') return 1;

    num = strtol(val, &chptr, 0);
    if (chptr && *chptr == '\0') {
	return num != 0;
    }

    return 0;
}

void rpmSetVar(int var, char *val) {
    freeRpmVar(&values[var]);

    values[var].arch = NULL;
    values[var].next = NULL;

    if (val)
	values[var].value = strdup(val);
    else
	values[var].value = NULL;
}

/* this doesn't free the passed pointer! */
static void freeRpmVar(struct rpmvarValue * orig) {
    struct rpmvarValue * next, * var = orig;

    while (var) {
	next = var->next;
	if (var->arch) free(var->arch);
	if (var->value) free(var->value);

	if (var != orig) free(var);
	var = next;
    }
}

static void rpmSetVarArch(int var, char * val, char * arch) {
    struct rpmvarValue * next = values + var;

    if (next->value) {
	if (arch) {
	    while (next->next) {
		if (next->arch && !strcmp(next->arch, arch)) break;
		next = next->next;
	    }
	} else {
	    while (next->next) {
		if (!next->arch) break;
		next = next->next;
	    }
	}

	if (next->arch && arch && !strcmp(next->arch, arch)) {
	    if (next->value) free(next->value);
	    if (next->arch) free(next->arch);
	} else if (next->arch || arch) {
	    next->next = malloc(sizeof(*next->next));
	    next = next->next;
	    next->next = NULL;
	}
    }

    next->value = strdup(val);
    if (arch)
	next->arch = strdup(arch);
    else
	next->arch = NULL;
}

void rpmSetTables(int archTable, int osTable) {
    char * arch, * os;

    defaultMachine(&arch, &os);

    if (currTables[ARCH] != archTable) {
	currTables[ARCH] = archTable;
	rebuildCompatTables(ARCH, arch);
    }

    if (currTables[OS] != osTable) {
	currTables[OS] = osTable;
	rebuildCompatTables(OS, os);
    }
}

int rpmMachineScore(int type, char * name) {
    struct machEquivInfo * info;

    info = machEquivSearch(&tables[type].equiv, name);
    if (info)
	return info->score;
    else
	return 0;
}

void rpmGetMachine(char **arch, char **os)
{
    if (arch) {
	*arch = current[ARCH];
    }

    if (os) {
	*os = current[OS];
    }
}

void rpmSetMachine(char * arch, char * os) {
    int transOs = os == NULL;
    int transArch = arch == NULL;
    char * realArch, * realOs;

    defaultMachine(&realArch, &realOs);

    if (!arch)
	arch = realArch;
    if (!os)
	os = realOs;

    if (transArch && tables[currTables[ARCH]].hasTranslate)
	arch = lookupInDefaultTable(arch,
			    tables[currTables[ARCH]].defaults,
			    tables[currTables[ARCH]].defaultsLength);
    if (transOs && tables[currTables[OS]].hasTranslate)
	os = lookupInDefaultTable(os,
			    tables[currTables[OS]].defaults,
			    tables[currTables[OS]].defaultsLength);

    if (!current[ARCH] || strcmp(arch, current[ARCH])) {
	if (current[ARCH]) free(current[ARCH]);
	current[ARCH] = strdup(arch);
	rebuildCompatTables(ARCH, realArch);
    }

    if (!current[OS] || strcmp(os, current[OS])) {
	if (current[OS]) free(current[OS]);
	current[OS] = strdup(os);
	/*
	 * XXX Capitalizing the 'L' is needed to insure that old
	 * XXX os-from-uname (e.g. "Linux") is compatible with the new
	 * XXX os-from-platform (e.g "linux" from "sparc-*-linux").
	 * XXX A copy of this string is embedded in headers and is
	 * XXX used by rpmInstallPackage->{os,arch}Okay->rpmMachineScore->
	 * XXX to verify correct arch/os from headers.
	 */
	if (!strcmp(current[OS], "linux"))
	    *current[OS]= 'L';
	rebuildCompatTables(OS, realOs);
    }
}

static void rebuildCompatTables(int type, char * name) {
    machFindEquivs(&tables[currTables[type]].cache,
		   &tables[currTables[type]].equiv,
		   name);
}

static void getMachineInfo(int type, char ** name, int * num) {
    struct canonEntry * canon;
    int which = currTables[type];

    /* use the normal canon tables, even if we're looking up build stuff */
    if (which >= 2) which -= 2;

    canon = lookupInCanonTable(current[type],
			       tables[which].canons,
			       tables[which].canonsLength);

    if (canon) {
	if (num) *num = canon->num;
	if (name) *name = canon->short_name;
    } else {
	if (num) *num = 255;
	if (name) *name = current[type];

	if (tables[currTables[type]].hasCanon) {
	    rpmMessage(RPMMESS_WARNING, _("Unknown system: %s\n"), current[type]);
	    rpmMessage(RPMMESS_WARNING, _("Please contact rpm-list@redhat.com\n"));
	}
    }
}

void rpmGetArchInfo(char ** name, int * num) {
    getMachineInfo(ARCH, name, num);
}

void rpmGetOsInfo(char ** name, int * num) {
    getMachineInfo(OS, name, num);
}

void rpmRebuildPlatformVars(char ** buildplatform, char **canonarch,
                             char **canonos) {

/* If buildplatform == NULL, don't return anything  */


    char * b = NULL, * ca = NULL, * co = NULL;
    char * presetbuildplatform = NULL;
    int x;
/*
*/

/*
 * XXX getMacroBody() may be nuked -- I originally added it for tdyas and it's
 * kinda half-baked (remember, each macro is actually a stack so it's not
 * clear which body is intended).
 *
 * You can, however, always do
 *	char buf[BUFSIZ];
 *	strcpy(buf, "%_buildplatform_preset")
 *	expandMacros(NULL, &globalMacroContext, buf, sizeof(buf))) {
 *	presetbuildplatform = strdup(buf);
 *
 */
    presetbuildplatform= (char *) getMacroBody(&globalMacroContext,
              "_buildplatform_preset");

      /* Rebuild the compat table to recalculate the
         current buildarch.  */

      rpmSetMachine(NULL, NULL);
      rpmSetTables(RPM_MACHTABLE_INSTARCH, RPM_MACHTABLE_INSTOS);
      rpmSetTables(RPM_MACHTABLE_BUILDARCH, RPM_MACHTABLE_BUILDOS);

      rpmGetArchInfo(&ca,NULL);
      rpmGetOsInfo(&co,NULL);

      if (!ca) defaultMachine(&ca,NULL);
      if (!co) defaultMachine(NULL,&co);

      for (x = 0; ca[x]; x++)
              ca[x] = tolower(ca[x]);
      for (x = 0; co[x]; x++)
              co[x] = tolower(co[x]);

      b = malloc(strlen(co)+strlen(ca)+2);
      sprintf(b,"%s-%s",ca,co);

/*
 * XXX All this macro pokery/jiggery could be achieved (I think)
 * by doing a delayed
 *	initMacros(&globalMacroContext, PER-PLATFORM-MACRO-FILE-NAMES);
 * (I haven't looked at the code :-)
 *
 * In fact, if you want to get really sophisticatedf, you could encapsulate
 * within per-platform MacroContexts (but I'm not quite ready for that yet).
 */
      delMacro(&globalMacroContext, "buildplatform");

    if (!presetbuildplatform) {
      addMacro(&globalMacroContext, "buildplatform", NULL, b,
        RMIL_RPMRC);
    } else {
      addMacro(&globalMacroContext, "buildplatform", NULL,
        presetbuildplatform, RMIL_RPMRC);
    }
      delMacro(&globalMacroContext, "arch");
      addMacro(&globalMacroContext, "arch", NULL, ca, 
          RMIL_RPMRC);
      delMacro(&globalMacroContext, "os");
      addMacro(&globalMacroContext, "os", NULL, co, 
          RMIL_RPMRC);

    if (buildplatform) {
      if (presetbuildplatform) 
         *buildplatform = presetbuildplatform;
      else
         *buildplatform = b;
    }
    if (canonarch) *canonarch = ca;
    if (canonos) *canonos = co;

}

int rpmShowRC(FILE *f)
{
    struct rpmOption *opt;
    int count = 0;
    char *s;
    int i;
    struct machEquivTable * equivTable;

    /* the caller may set the build arch which should be printed here */
    fprintf(f, "ARCHITECTURE AND OS:\n");
    fprintf(f, "build arch            : %s\n", current[ARCH]);

    fprintf(f, "compatible build archs:");
    equivTable = &tables[RPM_MACHTABLE_BUILDARCH].equiv;
    for (i = 0; i < equivTable->count; i++)
	fprintf(f," %s", equivTable->list[i].name);
    fprintf(f, "\n");

    fprintf(f, "build os              : %s\n", current[OS]);

    fprintf(f, "compatible build os's :");
    equivTable = &tables[RPM_MACHTABLE_BUILDOS].equiv;
    for (i = 0; i < equivTable->count; i++)
	fprintf(f," %s", equivTable->list[i].name);
    fprintf(f, "\n");

    rpmSetTables(RPM_MACHTABLE_INSTARCH, RPM_MACHTABLE_INSTOS);
    rpmSetMachine(NULL, NULL);	/* XXX WTFO? Why bother? */

    fprintf(f, "install arch          : %s\n", current[ARCH]);
    fprintf(f, "install os            : %s\n", current[OS]);

    fprintf(f, "compatible archs      :");
    equivTable = &tables[RPM_MACHTABLE_INSTARCH].equiv;
    for (i = 0; i < equivTable->count; i++)
	fprintf(f," %s", equivTable->list[i].name);
    fprintf(f, "\n");

    fprintf(f, "compatible os's       :");
    equivTable = &tables[RPM_MACHTABLE_INSTOS].equiv;
    for (i = 0; i < equivTable->count; i++)
	fprintf(f," %s", equivTable->list[i].name);
    fprintf(f, "\n");

    fprintf(f, "\nRPMRC VALUES:\n");
    opt = optionTable;
    while (count < optionTableSize) {
	s = rpmGetVar(opt->var);
	if (s != NULL || rpmGetVerbosity() < RPMMESS_NORMAL)
	    fprintf(f, "%-21s : %s\n", opt->name, s ? s : "(not set)");
	opt++;
	count++;
    }

    return 0;
}
