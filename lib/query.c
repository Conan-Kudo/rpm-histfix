/** \ingroup rpmcli
 * \file lib/query.c
 * Display tag values from package metadata.
 */

#include "system.h"

#include <errno.h>
#include <inttypes.h>
#include <ctype.h>

#include <rpm/rpmcli.h>
#include <rpm/header.h>
#include <rpm/rpmdb.h>
#include <rpm/rpmfi.h>
#include <rpm/rpmgi.h>
#include <rpm/rpmts.h>
#include <rpm/rpmlog.h>
#include <rpm/rpmfileutil.h>	/* rpmCleanPath */

#include "lib/manifest.h"

#include "debug.h"


/**
 */
static void printFileInfo(const char * name,
			  rpm_loff_t size, unsigned short mode,
			  unsigned int mtime,
			  unsigned short rdev, unsigned int nlink,
			  const char * owner, const char * group,
			  const char * linkto)
{
    char sizefield[21];
    char ownerfield[8+1], groupfield[8+1];
    char timefield[100];
    time_t when = mtime;  /* important if sizeof(int32_t) ! sizeof(time_t) */
    struct tm * tm;
    static time_t now;
    static struct tm nowtm;
    char * perms = rpmPermsString(mode);
    char *link = NULL;

    /* On first call, grab snapshot of now */
    if (now == 0) {
	now = time(NULL);
	tm = localtime(&now);
	if (tm) nowtm = *tm;	/* structure assignment */
    }

    rstrlcpy(ownerfield, owner, sizeof(ownerfield));
    rstrlcpy(groupfield, group, sizeof(groupfield));

    /* this is normally right */
    snprintf(sizefield, sizeof(sizefield), "%20" PRIu64, size);

    /* this knows too much about dev_t */

    if (S_ISLNK(mode)) {
	rasprintf(&link, "%s -> %s", name, linkto);
    } else if (S_ISCHR(mode)) {
	perms[0] = 'c';
	snprintf(sizefield, sizeof(sizefield), "%3u, %3u", ((unsigned)(rdev >> 8) & 0xff),
			((unsigned)rdev & 0xff));
    } else if (S_ISBLK(mode)) {
	perms[0] = 'b';
	snprintf(sizefield, sizeof(sizefield), "%3u, %3u", ((unsigned)(rdev >> 8) & 0xff),
			((unsigned)rdev & 0xff));
    }

    /* Convert file mtime to display format */
    tm = localtime(&when);
    timefield[0] = '\0';
    if (tm != NULL)
    {	const char *fmt;
	if (now > when + 6L * 30L * 24L * 60L * 60L ||	/* Old. */
	    now < when - 60L * 60L)			/* In the future.  */
	{
	/* The file is fairly old or in the future.
	 * POSIX says the cutoff is 6 months old;
	 * approximate this by 6*30 days.
	 * Allow a 1 hour slop factor for what is considered "the future",
	 * to allow for NFS server/client clock disagreement.
	 * Show the year instead of the time of day.
	 */        
	    fmt = "%b %e  %Y";
	} else {
	    fmt = "%b %e %H:%M";
	}
	(void)strftime(timefield, sizeof(timefield) - 1, fmt, tm);
    }

    rpmlog(RPMLOG_NOTICE, "%s %4d %-8s%-8s %10s %s %s\n", perms,
	(int)nlink, ownerfield, groupfield, sizefield, timefield, 
	link ? link : name);
    free(perms);
    free(link);
}

int showQueryPackage(QVA_t qva, rpmts ts, Header h)
{
    rpmfi fi = NULL;
    rpmfiFlags fiflags =  (RPMFI_NOHEADER | RPMFI_FLAGS_QUERY);
    int rc = 0;		/* XXX FIXME: need real return code */

    if (qva->qva_queryFormat != NULL) {
	const char *errstr;
	char *str = headerFormat(h, qva->qva_queryFormat, &errstr);

	if ( str != NULL ) {
	    rpmlog(RPMLOG_NOTICE, "%s", str);
	    free(str);
	} else {
	    rpmlog(RPMLOG_ERR, _("incorrect format: %s\n"), errstr);
	}
    }

    if (!(qva->qva_flags & QUERY_FOR_LIST))
	goto exit;

    if (!(qva->qva_flags & QUERY_FOR_DUMPFILES))
	fiflags |= RPMFI_NOFILEDIGESTS;

    fi = rpmfiNew(ts, h, RPMTAG_BASENAMES, fiflags);
    if (rpmfiFC(fi) <= 0) {
	rpmlog(RPMLOG_NOTICE, _("(contains no files)\n"));
	goto exit;
    }

    fi = rpmfiInit(fi, 0);
    while (rpmfiNext(fi) >= 0) {
	rpmfileAttrs fflags = rpmfiFFlags(fi);
	rpm_mode_t fmode = rpmfiFMode(fi);
	rpm_rdev_t frdev = rpmfiFRdev(fi);
	rpm_time_t fmtime = rpmfiFMtime(fi);
	rpmfileState fstate = rpmfiFState(fi);
	rpm_loff_t fsize = rpmfiFSize(fi);
	const char *fn = rpmfiFN(fi);
	const char *fuser = rpmfiFUser(fi);
	const char *fgroup = rpmfiFGroup(fi);
	const char *flink = rpmfiFLink(fi);
	char *buf = NULL;

	/* If querying only docs, skip non-doc files. */
	if ((qva->qva_flags & QUERY_FOR_DOCS) && !(fflags & RPMFILE_DOC))
	    continue;

	/* If querying only configs, skip non-config files. */
	if ((qva->qva_flags & QUERY_FOR_CONFIG) && !(fflags & RPMFILE_CONFIG))
	    continue;

	/* If not querying %ghost, skip ghost files. */
	if ((qva->qva_fflags & RPMFILE_GHOST) && (fflags & RPMFILE_GHOST))
	    continue;

	if (qva->qva_flags & QUERY_FOR_STATE) {
	    switch (fstate) {
	    case RPMFILE_STATE_NORMAL:
		rstrcat(&buf, _("normal        "));
		break;
	    case RPMFILE_STATE_REPLACED:
		rstrcat(&buf, _("replaced      "));
		break;
	    case RPMFILE_STATE_NOTINSTALLED:
		rstrcat(&buf, _("not installed "));
		break;
	    case RPMFILE_STATE_NETSHARED:
		rstrcat(&buf, _("net shared    "));
		break;
	    case RPMFILE_STATE_WRONGCOLOR:
		rstrcat(&buf, _("wrong color   "));
		break;
	    case RPMFILE_STATE_MISSING:
		rstrcat(&buf, _("(no state)    "));
		break;
	    default:
		rasprintf(&buf, _("(unknown %3d) "), fstate);
		break;
	    }
	}

	if (qva->qva_flags & QUERY_FOR_DUMPFILES) {
	    char *add, *fdigest;
	    fdigest = rpmfiFDigestHex(fi, NULL);
	    rasprintf(&add, "%s %" PRIu64 " %d %s 0%o ", 
		      fn, fsize, fmtime, fdigest ? fdigest : "", fmode);
	    rstrcat(&buf, add);
	    free(add);
	    free(fdigest);

	    if (fuser && fgroup) {
		rasprintf(&add, "%s %s", fuser, fgroup);
		rstrcat(&buf, add);
		free(add);
	    } else {
		rpmlog(RPMLOG_ERR,
			_("package has not file owner/group lists\n"));
	    }

	    rasprintf(&add, " %s %s %u %s",
				 fflags & RPMFILE_CONFIG ? "1" : "0",
				 fflags & RPMFILE_DOC ? "1" : "0",
				 frdev,
				 (flink && *flink ? flink : "X"));
	    rpmlog(RPMLOG_NOTICE, "%s%s\n", buf, add);
	    free(add);
	} else
	if (!rpmIsVerbose()) {
	    rpmlog(RPMLOG_NOTICE, "%s%s\n", buf ? buf : "", fn);
	}
	else {
	    uint32_t fnlink = rpmfiFNlink(fi);

	    /* XXX Adjust directory link count and size for display output. */
	    if (S_ISDIR(fmode)) {
		fnlink++;
		fsize = 0;
	    }

	    if (fuser && fgroup) {
		if (buf) {
		    rpmlog(RPMLOG_NOTICE, "%s", buf);
		}
		printFileInfo(fn, fsize, fmode, fmtime, frdev, fnlink,
					fuser, fgroup, flink);
	    } else {
		rpmlog(RPMLOG_ERR,
			_("package has neither file owner or id lists\n"));
	    }
	}
	free(buf);
    }

    rc = 0;

exit:
    fi = rpmfiFree(fi);
    return rc;
}

void rpmDisplayQueryTags(FILE * fp)
{
    static const char * const tagTypeNames[] = {
	"", "char", "int8", "int16", "int32", "int64",
	"string", "blob", "argv", "i18nstring"
    };
    const char *tname, *sname;
    rpmtd names = rpmtdNew();
    (void) rpmTagGetNames(names, 1);

    while ((tname = rpmtdNextString(names))) {
	sname = tname + strlen("RPMTAG_");
	if (rpmIsVerbose()) {
	    rpmTag tag = rpmTagGetValue(sname);
	    rpmTagType type = rpmTagGetType(tag) & RPM_MASK_TYPE;
	    fprintf(fp, "%-20s %6d", sname, tag);
	    if (type > RPM_NULL_TYPE && type <= RPM_MAX_TYPE)
		fprintf(fp, " %s", tagTypeNames[type]);
	} else {
	    fprintf(fp, "%s", sname);
	}
	fprintf(fp, "\n");
    }
    rpmtdFreeData(names);
    rpmtdFree(names);
}

static int rpmgiShowMatches(QVA_t qva, rpmts ts)
{
    rpmgi gi = qva->qva_gi;
    int ec = 0;
    Header h;

    while ((h = rpmgiNext(gi)) != NULL) {
	int rc;

	rpmdbCheckSignals();
	if ((rc = qva->qva_showPackage(qva, ts, h)) != 0)
	    ec = rc;
	headerFree(h);
    }
    return ec + rpmgiNumErrors(gi);
}

int rpmcliShowMatches(QVA_t qva, rpmts ts)
{
    Header h;
    int ec = 0;

    while ((h = rpmdbNextIterator(qva->qva_mi)) != NULL) {
	int rc;
	rpmdbCheckSignals();
	if ((rc = qva->qva_showPackage(qva, ts, h)) != 0)
	    ec = rc;
    }
    qva->qva_mi = rpmdbFreeIterator(qva->qva_mi);
    return ec;
}

int rpmQueryVerify(QVA_t qva, rpmts ts, const char * arg)
{
    int res = 0;
    const char * s;
    int i;
    int provides_checked = 0;

    (void) rpmdbCheckSignals();

    if (qva->qva_showPackage == NULL)
	return 1;

    switch (qva->qva_source) {
    case RPMQV_RPM:
	res = rpmgiShowMatches(qva, ts);
	break;

    case RPMQV_ALL:
	res = rpmcliShowMatches(qva, ts);
	break;

    case RPMQV_SPECFILE:
	res = ((qva->qva_specQuery != NULL)
		? qva->qva_specQuery(ts, qva, arg) : 1);
	break;

    case RPMQV_GROUP:
	qva->qva_mi = rpmtsInitIterator(ts, RPMTAG_GROUP, arg, 0);
	if (qva->qva_mi == NULL) {
	    rpmlog(RPMLOG_NOTICE,
		_("group %s does not contain any packages\n"), arg);
	    res = 1;
	} else
	    res = rpmcliShowMatches(qva, ts);
	break;

    case RPMQV_TRIGGEREDBY:
	qva->qva_mi = rpmtsInitIterator(ts, RPMTAG_TRIGGERNAME, arg, 0);
	if (qva->qva_mi == NULL) {
	    rpmlog(RPMLOG_NOTICE, _("no package triggers %s\n"), arg);
	    res = 1;
	} else
	    res = rpmcliShowMatches(qva, ts);
	break;

    case RPMQV_PKGID:
    {	unsigned char MD5[16];
	unsigned char * t;

	for (i = 0, s = arg; *s && isxdigit(*s); s++, i++)
	    {};
	if (i != 32) {
	    rpmlog(RPMLOG_ERR, _("malformed %s: %s\n"), "pkgid", arg);
	    return 1;
	}

	MD5[0] = '\0';
        for (i = 0, t = MD5, s = arg; i < 16; i++, t++, s += 2)
            *t = (rnibble(s[0]) << 4) | rnibble(s[1]);
	
	qva->qva_mi = rpmtsInitIterator(ts, RPMTAG_SIGMD5, MD5, sizeof(MD5));
	if (qva->qva_mi == NULL) {
	    rpmlog(RPMLOG_NOTICE, _("no package matches %s: %s\n"),
			"pkgid", arg);
	    res = 1;
	} else
	    res = rpmcliShowMatches(qva, ts);
    }	break;

    case RPMQV_HDRID:
	for (i = 0, s = arg; *s && isxdigit(*s); s++, i++)
	    {};
	if (i != 40) {
	    rpmlog(RPMLOG_ERR, _("malformed %s: %s\n"), "hdrid", arg);
	    return 1;
	}

	qva->qva_mi = rpmtsInitIterator(ts, RPMTAG_SHA1HEADER, arg, 0);
	if (qva->qva_mi == NULL) {
	    rpmlog(RPMLOG_NOTICE, _("no package matches %s: %s\n"),
			"hdrid", arg);
	    res = 1;
	} else
	    res = rpmcliShowMatches(qva, ts);
	break;

    case RPMQV_FILEID:
    {	unsigned char *digest, *t;
	size_t diglen;

	for (i = 0, s = arg; *s && isxdigit(*s); s++, i++)
	    {};
	/* XXX dunno the algorithm yet, just check we're in the ballpark */
	if (i % 32 != 0 || i < 32 || i > 512) {
	    rpmlog(RPMLOG_ERR, _("malformed %s: %s\n"), "fileid", arg);
	    return 1;
	}

	diglen = i / 2;
	digest = t = xcalloc(diglen, sizeof(*digest));
        for (i = 0, s = arg; i < diglen; i++, t++, s += 2)
            *t = (rnibble(s[0]) << 4) | rnibble(s[1]);

	qva->qva_mi = rpmtsInitIterator(ts, RPMTAG_FILEDIGESTS, digest, diglen);
	if (qva->qva_mi == NULL) {
	    rpmlog(RPMLOG_NOTICE, _("no package matches %s: %s\n"),
			"fileid", arg);
	    res = 1;
	} else
	    res = rpmcliShowMatches(qva, ts);

	free(digest);
    }	break;

    case RPMQV_TID:
    {	char * end = NULL;
	rpm_tid_t iid = strtoul(arg, &end, 0);

	if ((*end) || (end == arg) || (iid == UINT_MAX)) {
	    rpmlog(RPMLOG_ERR, _("malformed %s: %s\n"), "tid", arg);
	    return 1;
	}
	qva->qva_mi = rpmtsInitIterator(ts, RPMTAG_INSTALLTID, &iid, sizeof(iid));
	if (qva->qva_mi == NULL) {
	    rpmlog(RPMLOG_NOTICE, _("no package matches %s: %s\n"),
			"tid", arg);
	    res = 1;
	} else
	    res = rpmcliShowMatches(qva, ts);
    }	break;

    case RPMQV_WHATREQUIRES:
	qva->qva_mi = rpmtsInitIterator(ts, RPMTAG_REQUIRENAME, arg, 0);
	if (qva->qva_mi == NULL) {
	    rpmlog(RPMLOG_NOTICE, _("no package requires %s\n"), arg);
	    res = 1;
	} else
	    res = rpmcliShowMatches(qva, ts);
	break;

    case RPMQV_WHATPROVIDES:
	if (arg[0] != '/') {
	    provides_checked = 1;
	    qva->qva_mi = rpmtsInitIterator(ts, RPMTAG_PROVIDENAME, arg, 0);
	    if (qva->qva_mi == NULL) {
		rpmlog(RPMLOG_NOTICE, _("no package provides %s\n"), arg);
		res = 1;
	    } else
		res = rpmcliShowMatches(qva, ts);
	    break;
	}
    case RPMQV_PATH:
    {   char * fn;

	for (s = arg; *s != '\0'; s++)
	    if (!(*s == '.' || *s == '/'))
		break;

	if (*s == '\0') {
	    char fnbuf[PATH_MAX];
	    fn = realpath(arg, fnbuf);
	    fn = xstrdup( (fn != NULL ? fn : arg) );
	} else if (*arg != '/') {
	    char *curDir = rpmGetCwd();
	    fn = (char *) rpmGetPath(curDir, "/", arg, NULL);
	    curDir = _free(curDir);
	} else
	    fn = xstrdup(arg);
	(void) rpmCleanPath(fn);

	qva->qva_mi = rpmtsInitIterator(ts, RPMTAG_BASENAMES, fn, 0);
	if (qva->qva_mi == NULL && !provides_checked)
	    qva->qva_mi = rpmtsInitIterator(ts, RPMTAG_PROVIDENAME, fn, 0);

	if (qva->qva_mi == NULL) {
	    struct stat sb;
	    if (lstat(fn, &sb) != 0)
		rpmlog(RPMLOG_ERR, _("file %s: %s\n"), fn, strerror(errno));
	    else
		rpmlog(RPMLOG_NOTICE,
			_("file %s is not owned by any package\n"), fn);
	    res = 1;
	} else
	    res = rpmcliShowMatches(qva, ts);

	fn = _free(fn);
    }	break;

    case RPMQV_DBOFFSET:
    {	char * end = NULL;
	unsigned int recOffset = strtoul(arg, &end, 0);

	if ((*end) || (end == arg) || (recOffset == UINT_MAX)) {
	    rpmlog(RPMLOG_ERR, _("invalid package number: %s\n"), arg);
	    return 1;
	}
	rpmlog(RPMLOG_DEBUG, "package record number: %u\n", recOffset);
	/* RPMDBI_PACKAGES */
	qva->qva_mi = rpmtsInitIterator(ts, RPMDBI_PACKAGES, &recOffset, sizeof(recOffset));
	if (qva->qva_mi == NULL) {
	    rpmlog(RPMLOG_ERR,
		_("record %u could not be read\n"), recOffset);
	    res = 1;
	} else
	    res = rpmcliShowMatches(qva, ts);
    }	break;

    case RPMQV_PACKAGE:
    {
	int matches = 0;
	rpmdbMatchIterator mi;
	mi = rpmtsInitIterator(ts, RPMDBI_LABEL, arg, 0);
	while (rpmdbNextIterator(mi) != NULL) {
	    matches++;
	}
	rpmdbFreeIterator(mi);
	if (! matches) {
	    rpmlog(RPMLOG_NOTICE, _("package %s is not installed\n"), arg);
	    res = 1;
	} else {
	    qva->qva_mi = rpmtsInitIterator(ts, RPMDBI_LABEL, arg, 0);
	    res = rpmcliShowMatches(qva, ts);
	}
	break;
    }
    
    }
   
    return res;
}

/*
 * Apply extra query filters. By default patterns applied to package
 * name, others can be specified with <tagname>=<pattern>
 */
static rpmRC applyFilters(rpmdbMatchIterator mi, ARGV_const_t argv)
{
    rpmRC rc = RPMRC_OK;

    for (ARGV_const_t arg = argv; arg && *arg != NULL; arg++) {
	rpmTag tag = RPMTAG_NAME;
	char a[strlen(*arg)+1], *ae;
	const char *pat = a;

	strcpy(a, *arg);

	/* Parse for "tag=pattern" args. */
	if ((ae = strchr(a, '=')) != NULL) {
	    *ae++ = '\0';
	    tag = rpmTagGetValue(a);
	    if (tag == RPMTAG_NOT_FOUND) {
		rpmlog(RPMLOG_ERR, _("unknown tag: \"%s\"\n"), a);
		rc = RPMRC_FAIL;
		break;
	    }
	    pat = ae;
	}

	rpmdbSetIteratorRE(mi, tag, RPMMIRE_DEFAULT, pat);
    }

    return rc;
}

int rpmcliArgIter(rpmts ts, QVA_t qva, ARGV_const_t argv)
{
    int ec = 0;

    /* 
     * This is all wonderfully inconsistent... sort it out once
     * rpmgi is out of the picture.
     */
    switch (qva->qva_source) {
    case RPMQV_ALL:
	qva->qva_mi = rpmtsInitIterator(ts, RPMDBI_PACKAGES, NULL, 0);
	if (applyFilters(qva->qva_mi, argv) != RPMRC_OK) {
	    ec = 1;
	    qva->qva_mi = rpmdbFreeIterator(qva->qva_mi);
	} else {
	    ec += rpmQueryVerify(qva, ts, NULL);
	}
	break;
    case RPMQV_RPM:
	qva->qva_gi = rpmgiNew(ts, giFlags, argv);
	ec = rpmQueryVerify(qva, ts, NULL);
	qva->qva_gi = rpmgiFree(qva->qva_gi);
	break;
    default:
	for (ARGV_const_t arg = argv; arg && *arg; arg++)
	    ec += rpmQueryVerify(qva, ts, *arg);
	break;
    }

    return ec;
}

int rpmcliQuery(rpmts ts, QVA_t qva, char * const * argv)
{
    rpmVSFlags vsflags, ovsflags;
    int ec = 0;

    if (qva->qva_showPackage == NULL)
	qva->qva_showPackage = showQueryPackage;

    /* If --queryformat unspecified, then set default now. */
    if (!(qva->qva_flags & _QUERY_FOR_BITS) && qva->qva_queryFormat == NULL) {
	char * fmt = rpmExpand("%{?_query_all_fmt}\n", NULL);
	if (fmt == NULL || strlen(fmt) <= 1) {
	    fmt = _free(fmt);
	    fmt = xstrdup("%{nvra}\n");
	}
	qva->qva_queryFormat = fmt;
    }

    vsflags = rpmExpandNumeric("%{?_vsflags_query}");
    if (qva->qva_flags & VERIFY_DIGEST)
	vsflags |= _RPMVSF_NODIGESTS;
    if (qva->qva_flags & VERIFY_SIGNATURE)
	vsflags |= _RPMVSF_NOSIGNATURES;
    if (qva->qva_flags & VERIFY_HDRCHK)
	vsflags |= RPMVSF_NOHDRCHK;

    ovsflags = rpmtsSetVSFlags(ts, vsflags);
    ec = rpmcliArgIter(ts, qva, argv);
    vsflags = rpmtsSetVSFlags(ts, ovsflags);

    if (qva->qva_showPackage == showQueryPackage)
	qva->qva_showPackage = NULL;

    return ec;
}
