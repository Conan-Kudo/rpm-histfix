#include "system.h"

#ifndef PATH_MAX
# define PATH_MAX 255
#endif

#include "build/rpmbuild.h"
#include "popt/popt.h"
#include "url.h"

static void printFileInfo(char * name, unsigned int size, unsigned short mode,
			  unsigned int mtime, unsigned short rdev,
			  char * owner, char * group, int uid, int gid,
			  char * linkto);

#define POPT_QUERYFORMAT	1000
#define POPT_WHATREQUIRES	1001
#define POPT_WHATPROVIDES	1002
#define POPT_QUERYBYNUMBER	1003
#define POPT_TRIGGEREDBY	1004
#define POPT_DUMP		1005
#define POPT_SPECFILE		1006

/* ========== Query/Verify source popt args */
static void rpmQVSourceArgCallback(poptContext con, enum poptCallbackReason reason,
			     const struct poptOption * opt, const char * arg, 
			     QVA_t *qva) {

    switch (opt->val) {
      case 'a': qva->qva_source |= RPMQV_ALL; qva->qva_sourceCount++; break;
      case 'f': qva->qva_source |= RPMQV_PATH; qva->qva_sourceCount++; break;
      case 'g': qva->qva_source |= RPMQV_GROUP; qva->qva_sourceCount++; break;
      case 'p': qva->qva_source |= RPMQV_RPM; qva->qva_sourceCount++; break;
      case POPT_WHATPROVIDES: qva->qva_source |= RPMQV_WHATPROVIDES; 
			      qva->qva_sourceCount++; break;
      case POPT_WHATREQUIRES: qva->qva_source |= RPMQV_WHATREQUIRES; 
			      qva->qva_sourceCount++; break;
      case POPT_TRIGGEREDBY: qva->qva_source |= RPMQV_TRIGGEREDBY;
			      qva->qva_sourceCount++; break;

/* XXX SPECFILE is not verify sources */
      case POPT_SPECFILE:
	qva->qva_source |= RPMQV_SPECFILE;
	qva->qva_sourceCount++;
	break;
      case POPT_QUERYBYNUMBER:
	qva->qva_source |= RPMQV_DBOFFSET; 
	qva->qva_sourceCount++;
	break;
    }
}

struct poptOption rpmQVSourcePoptTable[] = {
	{ NULL, '\0', POPT_ARG_CALLBACK | POPT_CBFLAG_INC_DATA, 
		rpmQVSourceArgCallback, 0, NULL, NULL },
	{ "file", 'f', 0, 0, 'f',
		N_("query package owning file"), "FILE" },
	{ "group", 'g', 0, 0, 'g',
		N_("query packages in group"), "GROUP" },
	{ "package", 'p', 0, 0, 'p',
		N_("query a package file"), NULL },
	{ "querybynumber", '\0', POPT_ARGFLAG_DOC_HIDDEN, 0, 
		POPT_QUERYBYNUMBER, NULL, NULL },
	{ "specfile", '\0', 0, 0, POPT_SPECFILE,
		N_("query a spec file"), NULL },
	{ "triggeredby", '\0', 0, 0, POPT_TRIGGEREDBY, 
		N_("query the pacakges triggered by the package"), "PACKAGE" },
	{ "whatrequires", '\0', 0, 0, POPT_WHATREQUIRES, 
		N_("query the packages which require a capability"), "CAPABILITY" },
	{ "whatprovides", '\0', 0, 0, POPT_WHATPROVIDES, 
		N_("query the packages which provide a capability"), "CAPABILITY" },
	{ 0, 0, 0, 0, 0,	NULL, NULL }
};

/* ========== Query specific popt args */
extern char *specedit;

static void queryArgCallback(poptContext con, enum poptCallbackReason reason,
			     const struct poptOption * opt, const char * arg, 
			     QVA_t *qva) {
    switch (opt->val) {
      case 'c': qva->qva_flags |= QUERY_FOR_CONFIG | QUERY_FOR_LIST; break;
      case 'd': qva->qva_flags |= QUERY_FOR_DOCS | QUERY_FOR_LIST; break;
      case 'l': qva->qva_flags |= QUERY_FOR_LIST; break;
      case 's': qva->qva_flags |= QUERY_FOR_STATE | QUERY_FOR_LIST; break;
      case POPT_DUMP: qva->qva_flags |= QUERY_FOR_DUMPFILES | QUERY_FOR_LIST; break;
      case 'v': rpmIncreaseVerbosity();	 break;

      case POPT_QUERYFORMAT:
      {	char *qf = (char *)qva->qva_queryFormat;
	if (qf) {
	    int len = strlen(qf) + strlen(arg) + 1;
	    qf = realloc(qf, len);
	    strcat(qf, arg);
	} else {
	    qf = malloc(strlen(arg) + 1);
	    strcpy(qf, arg);
	}
	qva->qva_queryFormat = qf;
      }	break;
    }
}

struct poptOption rpmQueryPoptTable[] = {
	{ NULL, '\0', POPT_ARG_CALLBACK | POPT_CBFLAG_INC_DATA, 
		queryArgCallback, 0, NULL, NULL },
	{ "configfiles", 'c', 0, 0, 'c',
		N_("list all configuration files"), NULL },
	{ "docfiles", 'd', 0, 0, 'd',
		N_("list all documentation files"), NULL },
	{ "dump", '\0', 0, 0, POPT_DUMP,
		N_("dump basic file information"), NULL },
	{ "list", 'l', 0, 0, 'l',
		N_("list files in package"), NULL },
	{ "qf", '\0', POPT_ARG_STRING | POPT_ARGFLAG_DOC_HIDDEN, 0, 
		POPT_QUERYFORMAT, NULL, NULL },
	{ "queryformat", '\0', POPT_ARG_STRING, 0, POPT_QUERYFORMAT,
		N_("use the following query format"), "QUERYFORMAT" },
	{ "specedit", '\0', POPT_ARG_STRING, &specedit, 0,
		N_("substitute i18n sections from the following catalogue"),
			"METACATALOGUE" },
	{ "state", 's', 0, 0, 's',
		N_("display the states of the listed files"), NULL },
	{ "verbose", 'v', 0, 0, 'v',
		N_("display a verbose file listing"), NULL },
	{ 0, 0, 0, 0, 0,	NULL, NULL }
};

/* ======================================================================== */
static int queryHeader(Header h, const char * chptr) {
    char * str;
    char * error;

    str = headerSprintf(h, chptr, rpmTagTable, rpmHeaderFormats, &error);
    if (!str) {
	fprintf(stderr, _("error in format: %s\n"), error);
	return 1;
    }

    fputs(str, stdout);

    return 0;
}

int showQueryPackage(QVA_t *qva, rpmdb db, Header h)
{
    int queryFlags = qva->qva_flags;
    const char *queryFormat = qva->qva_queryFormat;

    char * name, * version, * release;
    int_32 count, type;
    char * prefix = NULL;
    char ** fileList, ** fileMD5List;
    char * fileStatesList;
    char ** fileOwnerList = NULL;
    char ** fileGroupList = NULL;
    char ** fileLinktoList;
    int_32 * fileFlagsList, * fileMTimeList, * fileSizeList;
    int_32 * fileUIDList, * fileGIDList;
    uint_16 * fileModeList;
    uint_16 * fileRdevList;
    int i;

    headerGetEntry(h, RPMTAG_NAME, &type, (void **) &name, &count);
    headerGetEntry(h, RPMTAG_VERSION, &type, (void **) &version, &count);
    headerGetEntry(h, RPMTAG_RELEASE, &type, (void **) &release, &count);

    if (!queryFormat && !queryFlags) {
	fprintf(stdout, "%s-%s-%s\n", name, version, release);
    } else {
	if (queryFormat)
	    queryHeader(h, queryFormat);

	if (queryFlags & QUERY_FOR_LIST) {
	    if (!headerGetEntry(h, RPMTAG_FILENAMES, &type, (void **) &fileList, 
		 &count)) {
		fputs(_("(contains no files)"), stdout);
		fputs("\n", stdout);
	    } else {
		if (!headerGetEntry(h, RPMTAG_FILESTATES, &type, 
			 (void **) &fileStatesList, &count)) {
		    fileStatesList = NULL;
		}
		headerGetEntry(h, RPMTAG_FILEFLAGS, &type, 
			 (void **) &fileFlagsList, &count);
		headerGetEntry(h, RPMTAG_FILESIZES, &type, 
			 (void **) &fileSizeList, &count);
		headerGetEntry(h, RPMTAG_FILEMODES, &type, 
			 (void **) &fileModeList, &count);
		headerGetEntry(h, RPMTAG_FILEMTIMES, &type, 
			 (void **) &fileMTimeList, &count);
		headerGetEntry(h, RPMTAG_FILERDEVS, &type, 
			 (void **) &fileRdevList, &count);
		headerGetEntry(h, RPMTAG_FILELINKTOS, &type, 
			 (void **) &fileLinktoList, &count);
		headerGetEntry(h, RPMTAG_FILEMD5S, &type, 
			 (void **) &fileMD5List, &count);

		if (!headerGetEntry(h, RPMTAG_FILEUIDS, &type, 
			 (void **) &fileUIDList, &count)) {
		    fileUIDList = NULL;
		} else {
		    headerGetEntry(h, RPMTAG_FILEGIDS, &type, 
			     (void **) &fileGIDList, &count);
		}

		if (!headerGetEntry(h, RPMTAG_FILEUSERNAME, &type, 
			 (void **) &fileOwnerList, &count)) {
		    fileOwnerList = NULL;
		} else {
		    headerGetEntry(h, RPMTAG_FILEGROUPNAME, &type, 
			     (void **) &fileGroupList, &count);
		}

		for (i = 0; i < count; i++) {
		    if (!((queryFlags & QUERY_FOR_DOCS) || 
			  (queryFlags & QUERY_FOR_CONFIG)) 
			|| ((queryFlags & QUERY_FOR_DOCS) && 
			    (fileFlagsList[i] & RPMFILE_DOC))
			|| ((queryFlags & QUERY_FOR_CONFIG) && 
			    (fileFlagsList[i] & RPMFILE_CONFIG))) {

			if (!rpmIsVerbose())
			    prefix ? fputs(prefix, stdout) : 0;

			if (queryFlags & QUERY_FOR_STATE) {
			    if (fileStatesList) {
				switch (fileStatesList[i]) {
				  case RPMFILE_STATE_NORMAL:
				    fputs(_("normal        "), stdout); break;
				  case RPMFILE_STATE_REPLACED:
				    fputs(_("replaced      "), stdout); break;
				  case RPMFILE_STATE_NETSHARED:
				    fputs(_("net shared    "), stdout); break;
				  case RPMFILE_STATE_NOTINSTALLED:
				    fputs(_("not installed "), stdout); break;
				  default:
				    fprintf(stdout, _("(unknown %3d) "), 
					  fileStatesList[i]);
				}
			    } else {
				fputs(    _("(no state)    "), stdout);
			    }
			}
			    
			if (queryFlags & QUERY_FOR_DUMPFILES) {
			    fprintf(stdout, "%s %d %d %s 0%o ", fileList[i],
				   fileSizeList[i], fileMTimeList[i],
				   fileMD5List[i], fileModeList[i]);

			    if (fileOwnerList)
				fprintf(stdout, "%s %s", fileOwnerList[i], 
						fileGroupList[i]);
			    else if (fileUIDList)
				fprintf(stdout, "%d %d", fileUIDList[i], 
						fileGIDList[i]);
			    else {
				rpmError(RPMERR_INTERNAL, _("package has "
					"neither file owner or id lists"));
			    }

			    fprintf(stdout, " %s %s %u ", 
				 fileFlagsList[i] & RPMFILE_CONFIG ? "1" : "0",
				 fileFlagsList[i] & RPMFILE_DOC ? "1" : "0",
				 (unsigned)fileRdevList[i]);

			    if (strlen(fileLinktoList[i]))
				fprintf(stdout, "%s\n", fileLinktoList[i]);
			    else
				fprintf(stdout, "X\n");

			} else if (!rpmIsVerbose()) {
			    fputs(fileList[i], stdout);
			    fputs("\n", stdout);
			} else if (fileOwnerList) 
			    printFileInfo(fileList[i], fileSizeList[i],
					  fileModeList[i], fileMTimeList[i],
					  fileRdevList[i], fileOwnerList[i], 
					  fileGroupList[i], -1, 
					  -1, fileLinktoList[i]);
			else if (fileUIDList) {
			    printFileInfo(fileList[i], fileSizeList[i],
					  fileModeList[i], fileMTimeList[i],
					  fileRdevList[i], NULL, 
					  NULL, fileUIDList[i], 
					  fileGIDList[i], fileLinktoList[i]);
			} else {
			    rpmError(RPMERR_INTERNAL, _("package has "
				    "neither file owner or id lists"));
			}
		    }
		}
	    
		free(fileList);
		free(fileLinktoList);
		free(fileMD5List);
		if (fileOwnerList) free(fileOwnerList);
		if (fileGroupList) free(fileGroupList);
	    }
	}
    }
    return 0;	/* XXX FIXME: need real return code */
}

static char * permsString(int mode) {
    static char perms[11];

    strcpy(perms, "----------");
   
    if (mode & S_IRUSR) perms[1] = 'r';
    if (mode & S_IWUSR) perms[2] = 'w';
    if (mode & S_IXUSR) perms[3] = 'x';
 
    if (mode & S_IRGRP) perms[4] = 'r';
    if (mode & S_IWGRP) perms[5] = 'w';
    if (mode & S_IXGRP) perms[6] = 'x';

    if (mode & S_IROTH) perms[7] = 'r';
    if (mode & S_IWOTH) perms[8] = 'w';
    if (mode & S_IXOTH) perms[9] = 'x';


    if (mode & S_ISVTX)
	perms[9] = ((mode & S_IXOTH) ? 't' : 'T');

    if (mode & S_ISUID) {
	if (mode & S_IXUSR) 
	    perms[3] = 's'; 
	else
	    perms[3] = 'S'; 
    }

    if (mode & S_ISGID) {
	if (mode & S_IXGRP) 
	    perms[6] = 's'; 
	else
	    perms[6] = 'S'; 
    }

    if (S_ISDIR(mode)) 
	perms[0] = 'd';
    else if (S_ISLNK(mode)) {
	perms[0] = 'l';
    }
    else if (S_ISFIFO(mode)) 
	perms[0] = 'p';
    else if (S_ISSOCK(mode)) 
	perms[0] = 's';
    else if (S_ISCHR(mode)) {
	perms[0] = 'c';
    } else if (S_ISBLK(mode)) {
	perms[0] = 'b';
    }

    return perms;
}

static void printFileInfo(char * name, unsigned int size, unsigned short mode,
			  unsigned int mtime, unsigned short rdev,
			  char * owner, char * group, int uid, int gid,
			  char * linkto) {
    char sizefield[15];
    char ownerfield[9], groupfield[9];
    char timefield[100] = "";
    time_t themtime;
    time_t currenttime;
    static int thisYear = 0;
    static int thisMonth = 0;
    struct tm * tstruct;
    char * namefield = name;
    char * perms;

    perms = permsString(mode);

    if (!thisYear) {
	currenttime = time(NULL);
	tstruct = localtime(&currenttime);
	thisYear = tstruct->tm_year;
	thisMonth = tstruct->tm_mon;
    }

    ownerfield[8] = groupfield[8] = '\0';

    if (owner) 
	strncpy(ownerfield, owner, 8);
    else
	sprintf(ownerfield, "%-8d", uid);

    if (group) 
	strncpy(groupfield, group, 8);
    else 
	sprintf(groupfield, "%-8d", gid);

    /* this is normally right */
    sprintf(sizefield, "%10u", size);

    /* this knows too much about dev_t */

    if (S_ISLNK(mode)) {
	namefield = alloca(strlen(name) + strlen(linkto) + 10);
	sprintf(namefield, "%s -> %s", name, linkto);
    } else if (S_ISCHR(mode)) {
	perms[0] = 'c';
	sprintf(sizefield, "%3u, %3u", (rdev >> 8) & 0xff, rdev & 0xFF);
    } else if (S_ISBLK(mode)) {
	perms[0] = 'b';
	sprintf(sizefield, "%3u, %3u", (rdev >> 8) & 0xff, rdev & 0xFF);
    }

    /* this is important if sizeof(int_32) ! sizeof(time_t) */
    themtime = mtime;
    tstruct = localtime(&themtime);

    if (tstruct->tm_year == thisYear || 
      ((tstruct->tm_year + 1) == thisYear && tstruct->tm_mon > thisMonth)) 
	(void)strftime(timefield, sizeof(timefield) - 1, "%b %d %H:%M",tstruct);
    else
	(void)strftime(timefield, sizeof(timefield) - 1, "%b %d  %Y", tstruct);

    fprintf(stdout, "%s %8s %8s %10s %s %s\n", perms, ownerfield, groupfield, 
		sizefield, timefield, namefield);
}

static void
printNewSpecfile(Spec spec)
{
    struct speclines *sl = spec->sl;
    struct spectags *st = spec->st;
    char buf[8192];
    int i, j;

    if (sl == NULL || st == NULL)
	return;

    for (i = 0; i < st->st_ntags; i++) {
	char *msgstr;
	struct spectag *t;
	t = st->st_t + i;

	/* XXX Summary tag often precedes name, so build msgid now. */
	if (t->t_msgid == NULL) {
	    char *n;
	    headerGetEntry(spec->packages->header, RPMTAG_NAME, NULL,
		(void **) &n, NULL);
	    sprintf(buf, "%s(%s)", n, tagName(t->t_tag));
	    t->t_msgid = strdup(buf);
	}
	msgstr = strdup(dgettext(specedit, t->t_msgid));

	switch(t->t_tag) {
	case RPMTAG_SUMMARY:
	case RPMTAG_GROUP:
	    FREE(sl->sl_lines[t->t_startx]);
	    if (t->t_lang && strcmp(t->t_lang, RPMBUILD_DEFAULT_LANG))
		continue;
	    sprintf(buf, "%s: %s\n",
		((t->t_tag == RPMTAG_GROUP) ? "Group" : "Summary"),
		msgstr);
	    sl->sl_lines[t->t_startx] = strdup(buf);
	    break;
	case RPMTAG_DESCRIPTION:
	    for (j = 1; j < t->t_nlines; j++)
		FREE(sl->sl_lines[t->t_startx + j]);
	    if (t->t_lang && strcmp(t->t_lang, RPMBUILD_DEFAULT_LANG)) {
		FREE(sl->sl_lines[t->t_startx]);
		continue;
	    }
	    sl->sl_lines[t->t_startx + 1] = strdup(msgstr);
	    if (t->t_nlines > 2)
		sl->sl_lines[t->t_startx + 2] = strdup("\n\n");
	    break;
	}
    }

    for (i = 0; i < sl->sl_nlines; i++) {
	if (sl->sl_lines[i] == NULL)
	    continue;
	printf("%s", sl->sl_lines[i]);
    }
}

void rpmDisplayQueryTags(FILE * f) {
    const struct headerTagTableEntry * t;
    int i;
    const struct headerSprintfExtension * ext = rpmHeaderFormats;

    for (i = 0, t = rpmTagTable; i < rpmTagTableSize; i++, t++) {
	fprintf(f, "%s\n", t->name + 7);
    }

    while (ext->name) {
	if (ext->type == HEADER_EXT_TAG)
	    fprintf(f, "%s\n", ext->name + 7), ext++;
	else if (ext->type == HEADER_EXT_MORE)
	    ext = ext->u.more;
	else
	    ext++;
    }
}

int showMatches(QVA_t *qva, rpmdb db, dbiIndexSet matches, QVF_t showPackage)
{
    Header h;
    int ec = 0;
    int i;

    for (i = 0; i < dbiIndexSetCount(matches); i++) {
	int rc;
	unsigned int recOffset = dbiIndexRecordOffset(matches, i);
	if (recOffset == 0)
	    continue;
	rpmMessage(RPMMESS_DEBUG, _("record number %u\n"), recOffset);
	    
	h = rpmdbGetRecord(db, recOffset);
	if (h == NULL) {
		fprintf(stderr, _("error: could not read database record\n"));
		ec = 1;
	} else {
		if ((rc = showPackage(qva, db, h)) != 0)
		    ec = rc;
		headerFree(h);
	}
    }
    return ec;
}

int rpmQueryVerify(QVA_t *qva, enum rpmQVSources source, const char * arg,
	rpmdb db, QVF_t showPackage)
{
    dbiIndexSet matches;
    Header h;
    int offset;
    int rc;
    int isSource;
    int recNumber;
    int retcode = 0;
    char *end = NULL;

    switch (source) {
      case RPMQV_RPM:
      {	FD_t fd;

	fd = ufdOpen(arg, O_RDONLY, 0);
	if (fdFileno(fd) < 0) {
	    fprintf(stderr, _("open of %s failed: %s\n"), arg,urlStrerror(arg));
	    ufdClose(fd);
	    retcode = 1;
	    break;
	}

	retcode = rpmReadPackageHeader(fd, &h, &isSource, NULL, NULL);

	ufdClose(fd);

	switch (retcode) {
	case 0:
	    if (h == NULL) {
		fprintf(stderr, _("old format source packages cannot "
			"be queried\n"));
		retcode = 1;
		break;
	    }
	    retcode = showPackage(qva, db, h);
	    headerFree(h);
	    break;
	case 1:
	    fprintf(stderr, _("%s does not appear to be a RPM package\n"), arg);
	    /* fallthrough */
	case 2:
	    fprintf(stderr, _("query of %s failed\n"), arg);
	    retcode = 1;
	    break;
	}
      } break;

      case RPMQV_SPECFILE:
	if (showPackage != showQueryPackage)
	    return 1;
      { Spec spec = NULL;
	Package pkg;
	char * buildRoot = NULL;
	int inBuildArch = 0;
	char * passPhrase = "";
	char *cookie = NULL;
	int anyarch = 1;
	int force = 1;
	rc = parseSpec(&spec, arg, buildRoot, inBuildArch, passPhrase, cookie,
	    anyarch, force);
	if (rc || spec == NULL) {
	    
	    fprintf(stderr, _("query of specfile %s failed, can't parse\n"), arg);
	    if (spec != NULL) freeSpec(spec);
	    retcode = 1;
	    break;
	}

	if (specedit != NULL) {
	    printNewSpecfile(spec);
	    freeSpec(spec);
	    retcode = 0;
	    break;
	}

	for (pkg = spec->packages; pkg != NULL; pkg = pkg->next) {
#if 0
	    char *binRpm, *errorString;
	    binRpm = headerSprintf(pkg->header,
		rpmGetPath("%{_rpmfilename}", NULL),
		rpmTagTable, rpmHeaderFormats, &errorString);
	    if (!(pkg == spec->packages && pkg->next == NULL))
		fprintf(stdout, "====== %s\n", binRpm);
	    free(binRpm);
#endif
	    showPackage(qva, NULL, pkg->header);
	}
	freeSpec(spec);
      }	break;

      case RPMQV_ALL:
	for (offset = rpmdbFirstRecNum(db);
	     offset != 0;
	     offset = rpmdbNextRecNum(db, offset)) {
		h = rpmdbGetRecord(db, offset);
		if (h == NULL) {
		    fprintf(stderr, _("could not read database record!\n"));
		    return 1;
		}
		if ((rc = showPackage(qva, db, h)) != 0)
		    retcode = rc;
		headerFree(h);
	}
	break;

      case RPMQV_GROUP:
	if (rpmdbFindByGroup(db, arg, &matches)) {
	    fprintf(stderr, _("group %s does not contain any packages\n"), arg);
	    retcode = 1;
	} else {
	    retcode = showMatches(qva, db, matches, showPackage);
	    dbiFreeIndexRecord(matches);
	}
	break;

      case RPMQV_WHATPROVIDES:
	if (rpmdbFindByProvides(db, arg, &matches)) {
	    fprintf(stderr, _("no package provides %s\n"), arg);
	    retcode = 1;
	} else {
	    retcode = showMatches(qva, db, matches, showPackage);
	    dbiFreeIndexRecord(matches);
	}
	break;

      case RPMQV_TRIGGEREDBY:
	if (rpmdbFindByTriggeredBy(db, arg, &matches)) {
	    fprintf(stderr, _("no package triggers %s\n"), arg);
	    retcode = 1;
	} else {
	    retcode = showMatches(qva, db, matches, showPackage);
	    dbiFreeIndexRecord(matches);
	}
	break;

      case RPMQV_WHATREQUIRES:
	if (rpmdbFindByRequiredBy(db, arg, &matches)) {
	    fprintf(stderr, _("no package requires %s\n"), arg);
	    retcode = 1;
	} else {
	    retcode = showMatches(qva, db, matches, showPackage);
	    dbiFreeIndexRecord(matches);
	}
	break;

      case RPMQV_PATH:
	if (rpmdbFindByFile(db, arg, &matches)) {
	    int myerrno = 0;
	    if (access(arg, F_OK) != 0)
		myerrno = errno;
	    switch (myerrno) {
	    default:
		fprintf(stderr, _("file %s: %s\n"), arg, strerror(myerrno));
		break;
	    case 0:
		fprintf(stderr, _("file %s is not owned by any package\n"), arg);
		break;
	    }
	    retcode = 1;
	} else {
	    retcode = showMatches(qva, db, matches, showPackage);
	    dbiFreeIndexRecord(matches);
	}
	break;

      case RPMQV_DBOFFSET:
	recNumber = strtoul(arg, &end, 10);
	if ((*end) || (end == arg) || (recNumber == ULONG_MAX)) {
	    fprintf(stderr, _("invalid package number: %s\n"), arg);
	    return 1;
	}
	rpmMessage(RPMMESS_DEBUG, _("package record number: %d\n"), recNumber);
	h = rpmdbGetRecord(db, recNumber);
	if (h == NULL)  {
	    fprintf(stderr, _("record %d could not be read\n"), recNumber);
	    retcode = 1;
	} else {
	    retcode = showPackage(qva, db, h);
	    headerFree(h);
	}
	break;

      case RPMQV_PACKAGE:
	rc = rpmdbFindByLabel(db, arg, &matches);
	if (rc == 1) {
	    retcode = 1;
	    fprintf(stderr, _("package %s is not installed\n"), arg);
	} else if (rc == 2) {
	    retcode = 1;
	    fprintf(stderr, _("error looking for package %s\n"), arg);
	} else {
	    retcode = showMatches(qva, db, matches, showPackage);
	    dbiFreeIndexRecord(matches);
	}
	break;
    }
   
    return retcode;
}

int rpmQuery(QVA_t *qva, enum rpmQVSources source, const char * arg)
{
    rpmdb db = NULL;
    int rc;

    switch (source) {
    case RPMQV_RPM:
    case RPMQV_SPECFILE:
	break;
    default:
	if (rpmdbOpen(qva->qva_prefix, &db, O_RDONLY, 0644)) {
	    fprintf(stderr, _("rpmQuery: rpmdbOpen() failed\n"));
	    return 1;
	}
	break;
    }

    rc = rpmQueryVerify(qva, source, arg, db, showQueryPackage);

    if (db)
	rpmdbClose(db);

    return rc;
}
