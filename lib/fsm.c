/** \ingroup payload
 * \file lib/fsm.c
 *  Handle payloads within rpm packages.
 */

#include "system.h"

#include "psm.h"
#include "rpmerr.h"
#include "debug.h"

/*@access FD_t @*/
/*@access rpmTransactionSet @*/
/*@access TFI_t @*/
/*@access FSM_t @*/

#define	alloca_strdup(_s)	strcpy(alloca(strlen(_s)+1), (_s))

/**
 * Wrapper to free(3), hides const compilation noise, permit NULL, return NULL.
 * @param this		memory to free
 * @retval		NULL always
 */
static /*@null@*/ void * _free(/*@only@*/ /*@null@*/ const void * this) {
    if (this)	free((void *)this);
    return NULL;
}

int _fsm_debug = 0;

rpmTransactionSet fsmGetTs(const FSM_t fsm) {
    const FSMI_t iter = fsm->iter;
    return (iter ? iter->ts : NULL);
}

TFI_t fsmGetFi(const FSM_t fsm) {
    const FSMI_t iter = fsm->iter;
    return (iter ? iter->fi : NULL);
}

#define	SUFFIX_RPMORIG	".rpmorig"
#define	SUFFIX_RPMSAVE	".rpmsave"
#define	SUFFIX_RPMNEW	".rpmnew"

/** \ingroup payload
 * Build path to file from file info, ornamented with subdir and suffix.
 * @param fsm		file state machine data
 * @param st		file stat info
 * @param subdir	subdir to use (NULL disables)
 * @param suffix	suffix to use (NULL disables)
 * @retval		path to file
 */
static /*@only@*//*@null@*/ const char * fsmFsPath(/*@null@*/ const FSM_t fsm,
	/*@null@*/ const struct stat * st,
	/*@null@*/ const char * subdir,
	/*@null@*/ const char * suffix)
{
    const char * s = NULL;

    if (fsm) {
	int nb;
	char * t;
	nb = strlen(fsm->dirName) +
	    (st && subdir && !S_ISDIR(st->st_mode) ? strlen(subdir) : 0) +
	    (st && suffix && !S_ISDIR(st->st_mode) ? strlen(suffix) : 0) +
	    strlen(fsm->baseName) + 1;
	s = t = xmalloc(nb);
	t = stpcpy(t, fsm->dirName);
	if (st && subdir && !S_ISDIR(st->st_mode))
	    t = stpcpy(t, subdir);
	t = stpcpy(t, fsm->baseName);
	if (st && suffix && !S_ISDIR(st->st_mode))
	    t = stpcpy(t, suffix);
    }
    return s;
}

/** \ingroup payload
 * Destroy file info iterator.
 * @retval		NULL always
 */
static /*@null@*/ void * mapFreeIterator(/*@only@*//*@null@*/const void * this) {
    return _free((void *)this);
}

/** \ingroup payload
 */
static void *
mapInitIterator(/*@kept@*/ const void * this, /*@kept@*/ const void * that)
{
    rpmTransactionSet ts = (void *)this;
    TFI_t fi = (void *)that;
    FSMI_t iter = NULL;

    iter = xcalloc(1, sizeof(*iter));
    iter->ts = ts;
    iter->fi = fi;
    switch (fi->type) {
    case TR_ADDED:	iter->i = 0;		break;
    case TR_REMOVED:	iter->i = fi->fc - 1;	break;
    }
    iter->isave = iter->i;
    return iter;
}

/** \ingroup payload
 */
static int mapNextIterator(void * this) {
    FSMI_t iter = this;
    const TFI_t fi = iter->fi;
    int i = -1;

    switch (fi->type) {
    case TR_ADDED:	if (iter->i < fi->fc)	i = iter->i++;	break;
    case TR_REMOVED:	if (iter->i >= 0)	i = iter->i--;	break;
    }
    iter->isave = i;
    return i;
}

/** \ingroup payload
 */
typedef struct dnli_s {
/*@dependent@*/ TFI_t fi;
/*@only@*/ /*@null@*/ char * active;
    int reverse;
    int isave;
    int i;
} * DNLI_t;

/** \ingroup payload
 */
static /*@null@*/ void * dnlFreeIterator(/*@only@*//*@null@*/ const void * this)
{
    if (this) {
	DNLI_t dnli = (void *)this;
	if (dnli->active) free(dnli->active);
    }
    return _free(this);
}

/** \ingroup payload
 */
static inline int dnlCount(const DNLI_t dnli) {
    return (dnli ? dnli->fi->dc : 0);
}

/** \ingroup payload
 */
static inline int dnlIndex(const DNLI_t dnli) {
    return (dnli ? dnli->isave : -1);
}

/** \ingroup payload
 * @param fsm		file state machine data
 */
static /*@only@*/ void * dnlInitIterator(const FSM_t fsm, int reverse)
{
    TFI_t fi = fsmGetFi(fsm);
    DNLI_t dnli;
    int i, j;

    if (fi == NULL)
	return NULL;
    dnli = xcalloc(1, sizeof(*dnli));
    dnli->fi = fi;
    dnli->reverse = reverse;
    dnli->i = (reverse ? fi->dc : 0);

    if (fi->dc) {
	dnli->active = xcalloc(fi->dc, sizeof(*dnli->active));

	/* Identify parent directories not skipped. */
	for (i = 0; i < fi->fc; i++)
            if (!XFA_SKIPPING(fi->actions[i])) dnli->active[fi->dil[i]] = 1;

	/* Exclude parent directories that are explicitly included. */
	for (i = 0; i < fi->fc; i++) {
	    int dil, dnlen, bnlen;

	    if (!S_ISDIR(fi->fmodes[i]))
		continue;

	    dil = fi->dil[i];
	    dnlen = strlen(fi->dnl[dil]);
	    bnlen = strlen(fi->bnl[i]);

	    for (j = 0; j < fi->dc; j++) {
		const char * dnl;
		int jlen;

		if (!dnli->active[j] || j == dil) continue;
		dnl = fi->dnl[j];
		jlen = strlen(dnl);
		if (jlen != (dnlen+bnlen+1)) continue;
		if (strncmp(dnl, fi->dnl[dil], dnlen)) continue;
		if (strncmp(dnl+dnlen, fi->bnl[i], bnlen)) continue;
		if (dnl[dnlen+bnlen] != '/' || dnl[dnlen+bnlen+1] != '\0')
		    continue;
		/* This directory is included in the package. */
		dnli->active[j] = 0;
		break;
	    }
	}

	/* Print only once per package. */
	if (!reverse) {
	    j = 0;
	    for (i = 0; i < fi->dc; i++) {
		if (!dnli->active[i]) continue;
		if (j == 0) {
		    j = 1;
		    rpmMessage(RPMMESS_DEBUG,
	_("========= Directories not explictly included in package:\n"));
		}
		rpmMessage(RPMMESS_DEBUG, _("%9d %s\n"), i, fi->dnl[i]);
	    }
	    if (j)
		rpmMessage(RPMMESS_DEBUG, "=========\n");
	}
    }
    return dnli;
}

/** \ingroup payload
 */
static const char * dnlNextIterator(/*@null@*/ DNLI_t dnli) {
    const char * dn = NULL;

    if (dnli && dnli->active) {
	TFI_t fi = dnli->fi;
	int i = -1;

	do {
	    i = (!dnli->reverse ? dnli->i++ : --dnli->i);
	} while (i >= 0 && i < fi->dc && !dnli->active[i]);

	if (i >= 0 && i < fi->dc)
	    dn = fi->dnl[i];
	else
	    i = -1;
	dnli->isave = i;
    }
    return dn;
}

/** \ingroup payload
 */
static int cpioStrCmp(const void * a, const void * b) {
    const char * afn = *(const char **)a;
    const char * bfn = *(const char **)b;

    /* Match rpm-4.0 payloads with ./ prefixes. */
    if (afn[0] == '.' && afn[1] == '/')	afn += 2;
    if (bfn[0] == '.' && bfn[1] == '/')	bfn += 2;

    /* If either path is absolute, make it relative. */
    if (afn[0] == '/')	afn += 1;
    if (bfn[0] == '/')	bfn += 1;

    return strcmp(afn, bfn);
}

/** \ingroup payload
 */
static int mapFind(void * this, const char * fsmPath) {
    FSMI_t iter = this;
    const TFI_t fi = iter->fi;
    int ix = -1;

    if (fi) {
	const char ** p;

	p = bsearch(&fsmPath, fi->apath, fi->fc, sizeof(fsmPath), cpioStrCmp);
	if (p == NULL) {
	    fprintf(stderr, "*** not mapped %s\n", fsmPath);
	} else {
	    iter->i = p - fi->apath;
	    ix = mapNextIterator(iter);
	}
    }
    return ix;
}

/** \ingroup payload
 * Save hard link in chain.
 * @param fsm		file state machine data
 * @return		Is chain only partially filled?
 */
static int saveHardLink(FSM_t fsm)
{
    struct stat * st = &fsm->sb;
    int rc = 0;
    int ix = -1;
    int j;

    /* Find hard link set. */
    for (fsm->li = fsm->links; fsm->li; fsm->li = fsm->li->next) {
	if (fsm->li->inode == st->st_ino && fsm->li->dev == st->st_dev)
	    break;
    }

    /* New hard link encountered, add new link to set. */
    if (fsm->li == NULL) {
	fsm->li = xcalloc(1, sizeof(*fsm->li));
	fsm->li->next = NULL;
	fsm->li->nlink = st->st_nlink;
	fsm->li->dev = st->st_dev;
	fsm->li->inode = st->st_ino;
	fsm->li->linkIndex = -1;
	fsm->li->createdPath = -1;

	fsm->li->filex = xcalloc(st->st_nlink, sizeof(fsm->li->filex[0]));
	memset(fsm->li->filex, -1, (st->st_nlink * sizeof(fsm->li->filex[0])));
	fsm->li->nsuffix = xcalloc(st->st_nlink, sizeof(*fsm->li->nsuffix));

	if (fsm->goal == FSM_PKGBUILD)
	    fsm->li->linksLeft = st->st_nlink;
	if (fsm->goal == FSM_PKGINSTALL)
	    fsm->li->linksLeft = 0;

	fsm->li->next = fsm->links;
	fsm->links = fsm->li;
    }

    if (fsm->goal == FSM_PKGBUILD) --fsm->li->linksLeft;
    fsm->li->filex[fsm->li->linksLeft] = fsm->ix;
    /*@-observertrans@*/
    fsm->li->nsuffix[fsm->li->linksLeft] = fsm->nsuffix;
    /*@=observertrans@*/
    if (fsm->goal == FSM_PKGINSTALL) fsm->li->linksLeft++;

#if 0
fprintf(stderr, "*** %p link[%d:%d] %d filex %d %s\n", fsm->li, fsm->li->linksLeft, st->st_nlink, (int)st->st_size, fsm->li->filex[fsm->li->linksLeft], fsm->li->files[fsm->li->linksLeft]);
#endif

    if (fsm->goal == FSM_PKGBUILD)
	return (fsm->li->linksLeft > 0);

    if (fsm->goal != FSM_PKGINSTALL)
	return 0;

    if (!(st->st_size || fsm->li->linksLeft == st->st_nlink))
	return 1;

    /* Here come the bits, time to choose a non-skipped file name. */
    {	TFI_t fi = fsmGetFi(fsm);

	for (j = fsm->li->linksLeft - 1; j >= 0; j--) {
	    ix = fsm->li->filex[j];
	    if (ix < 0 || XFA_SKIPPING(fi->actions[ix]))
		continue;
	    break;
	}
    }

    /* Are all links skipped or not encountered yet? */
    if (ix < 0 || j < 0)
	return 1;	/* XXX W2DO? */

    /* Save the non-skipped file name and map index. */
    fsm->li->linkIndex = j;
    fsm->path = _free(fsm->path);
    fsm->ix = ix;
    rc = fsmStage(fsm, FSM_MAP);
    return rc;
}

/** \ingroup payload
 * Destroy set of hard links.
 * @param li		set of hard links
 */
static /*@null@*/ void * freeHardLink(/*@only@*/ /*@null@*/ struct hardLink * li)
{
    if (li) {
	li->nsuffix = _free(li->nsuffix);	/* XXX elements are shared */
	li->filex = _free(li->filex);
    }
    return _free(li);
}

FSM_t newFSM(void) {
    FSM_t fsm = xcalloc(1, sizeof(*fsm));
    return fsm;
}

FSM_t freeFSM(FSM_t fsm)
{
    if (fsm) {
	if (fsm->path)	free((void *)fsm->path);
	while ((fsm->li = fsm->links) != NULL) {
	    fsm->links = fsm->li->next;
	    fsm->li->next = NULL;
	    fsm->li = freeHardLink(fsm->li);
	}
	fsm->dnlx = _free(fsm->dnlx);
	fsm->ldn = _free(fsm->ldn);
	fsm->iter = mapFreeIterator(fsm->iter);
    }
    return _free(fsm);
}

int fsmSetup(FSM_t fsm, fileStage goal,
		const rpmTransactionSet ts, const TFI_t fi, FD_t cfd,
		unsigned int * archiveSize, const char ** failedFile)
{
    size_t pos = 0;
    int rc;

    fsm->goal = goal;
    if (cfd) {
	fsm->cfd = fdLink(cfd, "persist (fsm)");
	pos = fdGetCpioPos(fsm->cfd);
	fdSetCpioPos(fsm->cfd, 0);
    }
    fsm->iter = mapInitIterator(ts, fi);

    if (fsm->goal == FSM_PKGINSTALL) {
	if (ts && ts->notify) {
	    (void)ts->notify(fi->h, RPMCALLBACK_INST_START, 0, fi->archiveSize,
		(fi->ap ? fi->ap->key : NULL), ts->notifyData);
	}
    }

    fsm->archiveSize = archiveSize;
    if (fsm->archiveSize)
	*fsm->archiveSize = 0;
    fsm->failedFile = failedFile;
    if (fsm->failedFile)
	*fsm->failedFile = NULL;

    memset(fsm->sufbuf, 0, sizeof(fsm->sufbuf));
    if (fsm->goal == FSM_PKGINSTALL) {
	if (ts->id > 0)
	    sprintf(fsm->sufbuf, ";%08x", (unsigned)ts->id);
    }

    rc = fsm->rc = 0;
    rc = fsmStage(fsm, FSM_CREATE);

    rc = fsmStage(fsm, fsm->goal);

    if (!rc && fsm->archiveSize)
	*fsm->archiveSize = (fdGetCpioPos(fsm->cfd) - pos);

   return rc;
}

int fsmTeardown(FSM_t fsm) {
    int rc = fsm->rc;

    if (!rc)
	rc = fsmStage(fsm, FSM_DESTROY);

    fsm->iter = mapFreeIterator(fsm->iter);
    if (fsm->cfd) {
	fsm->cfd = fdFree(fsm->cfd, "persist (fsm)");
	fsm->cfd = NULL;
    }
    fsm->failedFile = NULL;
    return rc;
}

int fsmMapPath(FSM_t fsm)
{
    TFI_t fi = fsmGetFi(fsm);	/* XXX const except for fstates */
    int rc = 0;
    int i;

    fsm->osuffix = NULL;
    fsm->nsuffix = NULL;
    fsm->astriplen = 0;
    fsm->action = FA_UNKNOWN;
    fsm->mapFlags = 0;

    i = fsm->ix;
    if (fi && i >= 0 && i < fi->fc) {

	fsm->astriplen = fi->astriplen;
	fsm->action = (fi->actions ? fi->actions[i] : fi->action);
	fsm->fflags = (fi->fflags ? fi->fflags[i] : fi->flags);
	fsm->mapFlags = (fi->fmapflags ? fi->fmapflags[i] : fi->mapflags);

	/* src rpms have simple base name in payload. */
	fsm->archivePath =
		(fi->apath ? fi->apath[i] + fi->striplen : fi->bnl[i]);
	fsm->dirName = fi->dnl[fi->dil[i]];
	fsm->baseName = fi->bnl[i];

	switch (fsm->action) {
	case FA_SKIP:
	    break;
	case FA_SKIPMULTILIB:	/* XXX RPMFILE_STATE_MULTILIB? */
fprintf(stderr, "*** %s:%s %s\n", fiTypeString(fi), fileActionString(fsm->action), (fsm->path ? fsm->path : ""));
	    break;
	case FA_UNKNOWN:
fprintf(stderr, "*** %s:%s %s\n", fiTypeString(fi), fileActionString(fsm->action), (fsm->path ? fsm->path : ""));
	    break;

	case FA_CREATE:
	    assert(fi->type == TR_ADDED);
	    break;

	case FA_SKIPNSTATE:
fprintf(stderr, "*** %s:%s %s\n", fiTypeString(fi), fileActionString(fsm->action), (fsm->path ? fsm->path : ""));
	    if (fi->type == TR_ADDED)
		fi->fstates[i] = RPMFILE_STATE_NOTINSTALLED;
	    break;

	case FA_SKIPNETSHARED:
	    if (fi->type == TR_ADDED)
		fi->fstates[i] = RPMFILE_STATE_NETSHARED;
	    break;

	case FA_BACKUP:
fprintf(stderr, "*** %s:%s %s\n", fiTypeString(fi), fileActionString(fsm->action), (fsm->path ? fsm->path : ""));
	    switch (fi->type) {
	    case TR_ADDED:
		fsm->osuffix = SUFFIX_RPMORIG;
		break;
	    case TR_REMOVED:
		fsm->osuffix = SUFFIX_RPMSAVE;
		break;
	    }
	    break;

	case FA_ALTNAME:
	    assert(fi->type == TR_ADDED);
	    fsm->nsuffix = SUFFIX_RPMNEW;
	    break;

	case FA_SAVE:
fprintf(stderr, "*** %s:%s %s\n", fiTypeString(fi), fileActionString(fsm->action), (fsm->path ? fsm->path : ""));
	    assert(fi->type == TR_ADDED);
	    fsm->osuffix = SUFFIX_RPMSAVE;
	    break;
	case FA_ERASE:
	    assert(fi->type == TR_REMOVED);
	    break;
	default:
fprintf(stderr, "*** %s:%s %s\n", fiTypeString(fi), fileActionString(fsm->action), (fsm->path ? fsm->path : ""));
	    break;
	}

	if ((fsm->mapFlags & CPIO_MAP_PATH) || fsm->nsuffix) {
	    const struct stat * st = &fsm->sb;
	    fsm->path = _free(fsm->path);
	    fsm->path = fsmFsPath(fsm, st, fsm->subdir,
		(fsm->suffix ? fsm->suffix : fsm->nsuffix));
	}
    }
    return rc;
}

int fsmMapAttrs(FSM_t fsm)
{
    struct stat * st = &fsm->sb;
    TFI_t fi = fsmGetFi(fsm);
    int i = fsm->ix;

    if (fi && i >= 0 && i < fi->fc) {
	mode_t perms =
		(S_ISDIR(st->st_mode) ? fi->dperms : fi->fperms);
	mode_t finalMode =
		(fi->fmodes ? fi->fmodes[i] : perms);
	uid_t finalUid =
		(fi->fuids ? fi->fuids[i] : fi->uid); /* XXX chmod u-s */
	gid_t finalGid =
		(fi->fgids ? fi->fgids[i] : fi->gid); /* XXX chmod g-s */

	if (fsm->mapFlags & CPIO_MAP_MODE)
	    st->st_mode = (st->st_mode & S_IFMT) | finalMode;
	if (fsm->mapFlags & CPIO_MAP_UID)
	    st->st_uid = finalUid;
	if (fsm->mapFlags & CPIO_MAP_GID)
	    st->st_gid = finalGid;

	fsm->fmd5sum = (fi->fmd5s ? fi->fmd5s[i] : NULL);

    }
    return 0;
}

/** \ingroup payload
 * Create file from payload stream.
 * @todo Legacy: support brokenEndian MD5 checks?
 * @param fsm		file state machine data
 * @return		0 on success
 */
static int expandRegular(FSM_t fsm)
		/*@modifies fileSystem, fsm @*/
{
    const char * fmd5sum;
    const struct stat * st = &fsm->sb;
    int left = st->st_size;
    int rc = 0;

    rc = fsmStage(fsm, FSM_WOPEN);
    if (rc)
	goto exit;

    /* XXX md5sum's will break on repackaging that includes modified files. */
    fmd5sum = fsm->fmd5sum;

    /* XXX This doesn't support brokenEndian checks. */
    if (st->st_size > 0 && fmd5sum)
	fdInitMD5(fsm->wfd, 0);

    while (left) {

	fsm->wrlen = (left > fsm->wrsize ? fsm->wrsize : left);
	rc = fsmStage(fsm, FSM_DREAD);
	if (rc)
	    goto exit;

	rc = fsmStage(fsm, FSM_WRITE);
	if (rc)
	    goto exit;

	left -= fsm->wrnb;

	/* don't call this with fileSize == fileComplete */
	if (!rc && left)
	    (void) fsmStage(fsm, FSM_NOTIFY);
    }

    if (st->st_size > 0 && fmd5sum) {
	const char * md5sum = NULL;

	Fflush(fsm->wfd);
	fdFiniMD5(fsm->wfd, (void **)&md5sum, NULL, 1);

	if (md5sum == NULL) {
	    rc = CPIOERR_MD5SUM_MISMATCH;
	} else {
	    if (strcmp(md5sum, fmd5sum))
		rc = CPIOERR_MD5SUM_MISMATCH;
	    md5sum = _free(md5sum);
	}
    }

exit:
    (void) fsmStage(fsm, FSM_WCLOSE);
    return rc;
}

/** \ingroup payload
 * Write next item to payload stream.
 * @param fsm		file state machine data
 * @param writeData	should data be written?
 * @return		0 on success
 */
static int writeFile(FSM_t fsm, int writeData)
	/*@modifies fsm @*/
{
    const char * path = fsm->path;
    const char * opath = fsm->opath;
    struct stat * st = &fsm->sb;
    struct stat * ost = &fsm->osb;
    size_t pos = fdGetCpioPos(fsm->cfd);
    int left;
    int rc;

    st->st_size = (writeData ? ost->st_size : 0);
    if (S_ISDIR(st->st_mode)) {
	st->st_size = 0;
    } else if (S_ISLNK(st->st_mode)) {
	/*
	 * While linux puts the size of a symlink in the st_size field,
	 * I don't think that's a specified standard.
	 */
	/* XXX NUL terminated result in fsm->rdbuf, len in fsm->rdnb. */
	rc = fsmStage(fsm, FSM_READLINK);
	if (rc) goto exit;
	st->st_size = fsm->rdnb;
    }

    if (fsm->mapFlags & CPIO_MAP_PATH)
	fsm->path = fsm->archivePath;
    rc = fsmStage(fsm, FSM_HWRITE);
    fsm->path = path;
    if (rc) goto exit;

    if (writeData && S_ISREG(st->st_mode)) {
#if HAVE_MMAP
	char * rdbuf = NULL;
	void * mapped = (void *)-1;
	size_t nmapped;
#endif

	rc = fsmStage(fsm, FSM_ROPEN);
	if (rc) goto exit;

	/* XXX unbuffered mmap generates *lots* of fdio debugging */
#if HAVE_MMAP
	nmapped = 0;
	mapped = mmap(NULL, st->st_size, PROT_READ, MAP_SHARED, Fileno(fsm->rfd), 0);
	if (mapped != (void *)-1) {
	    rdbuf = fsm->rdbuf;
	    fsm->rdbuf = (char *) mapped;
	    fsm->rdlen = nmapped = st->st_size;
	}
#endif

	left = st->st_size;

	while (left) {
#if HAVE_MMAP
	  if (mapped != (void *)-1) {
	    fsm->rdnb = nmapped;
	  } else
#endif
	  {
	    fsm->rdlen = (left > fsm->rdsize ? fsm->rdsize : left),
	    rc = fsmStage(fsm, FSM_READ);
	    if (rc) goto exit;
	  }

	    /* XXX DWRITE uses rdnb for I/O length. */
	    rc = fsmStage(fsm, FSM_DWRITE);
	    if (rc) goto exit;

	    left -= fsm->wrnb;
	}

#if HAVE_MMAP
	if (mapped != (void *)-1) {
	    /*@-noeffect@*/ munmap(mapped, nmapped) /*@=noeffect@*/;
	    fsm->rdbuf = rdbuf;
	}
#endif

    } else if (writeData && S_ISLNK(st->st_mode)) {
	/* XXX DWRITE uses rdnb for I/O length. */
	rc = fsmStage(fsm, FSM_DWRITE);
	if (rc) goto exit;
    }

    rc = fsmStage(fsm, FSM_PAD);
    if (rc) goto exit;

    {	const rpmTransactionSet ts = fsmGetTs(fsm);
	TFI_t fi = fsmGetFi(fsm);
	if (ts && fi && ts->notify) {
	    size_t size = (fdGetCpioPos(fsm->cfd) - pos);
	    (void)ts->notify(fi->h, RPMCALLBACK_INST_PROGRESS, size, size,
			(fi->ap ? fi->ap->key : NULL), ts->notifyData);
	}
    }

    rc = 0;

exit:
    if (fsm->rfd)
	(void) fsmStage(fsm, FSM_RCLOSE);
    fsm->opath = opath;
    fsm->path = path;
    return rc;
}

/** \ingroup payload
 * Write set of linked files to payload stream.
 * @param fsm		file state machine data
 * @return		0 on success
 */
static int writeLinkedFile(FSM_t fsm)
	/*@modifies fsm @*/
{
    const char * path = fsm->path;
    const char * nsuffix = fsm->nsuffix;
    int iterIndex = fsm->ix;
    int ec = 0;
    int rc;
    int i;

    fsm->path = NULL;
    fsm->nsuffix = NULL;
    fsm->ix = -1;

    for (i = fsm->li->nlink - 1; i >= 0; i--) {
	if (fsm->li->filex[i] < 0) continue;

	fsm->ix = fsm->li->filex[i];
	rc = fsmStage(fsm, FSM_MAP);

	/* Write data after last link. */
	rc = writeFile(fsm, (i == 0));
	if (rc && fsm->failedFile && *fsm->failedFile == NULL) {
	    ec = rc;
	    *fsm->failedFile = xstrdup(fsm->path);
	}

	fsm->path = _free(fsm->path);
	fsm->li->filex[i] = -1;
    }

    fsm->ix = iterIndex;
    fsm->nsuffix = nsuffix;
    fsm->path = path;
    return ec;
}

/** \ingroup payload
 * @param fsm		file state machine data
 */
static int fsmMakeLinks(FSM_t fsm)
{
    const char * path = fsm->path;
    const char * opath = fsm->opath;
    const char * nsuffix = fsm->nsuffix;
    int iterIndex = fsm->ix;
    int ec = 0;
    int rc;
    int i;

    fsm->path = NULL;
    fsm->opath = NULL;
    fsm->nsuffix = NULL;
    fsm->ix = -1;

    fsm->ix = fsm->li->filex[fsm->li->createdPath];
    rc = fsmStage(fsm, FSM_MAP);
    fsm->opath = fsm->path;
    fsm->path = NULL;
    for (i = 0; i < fsm->li->nlink; i++) {
	if (fsm->li->filex[i] < 0) continue;
	if (i == fsm->li->createdPath) continue;

	fsm->ix = fsm->li->filex[i];
	rc = fsmStage(fsm, FSM_MAP);
	rc = fsmStage(fsm, FSM_VERIFY);
	if (!rc) continue;
	if (rc != CPIOERR_LSTAT_FAILED) break;

	/* XXX link(fsm->opath, fsm->path) */
	rc = fsmStage(fsm, FSM_LINK);
	if (rc && fsm->failedFile && *fsm->failedFile == NULL) {
	    ec = rc;
	    *fsm->failedFile = xstrdup(fsm->path);
	}

	fsm->li->linksLeft--;
    }
    fsm->opath = _free(fsm->opath);

    fsm->ix = iterIndex;
    fsm->nsuffix = nsuffix;
    fsm->path = path;
    fsm->opath = opath;
    return ec;
}

/** \ingroup payload
 * @param fsm		file state machine data
 */
static int fsmCommitLinks(FSM_t fsm)
{
    const char * path = fsm->path;
    const char * nsuffix = fsm->nsuffix;
    int iterIndex = fsm->ix;
    struct stat * st = &fsm->sb;
    int rc = 0;
    int i;

    fsm->path = NULL;
    fsm->nsuffix = NULL;
    fsm->ix = -1;

    for (fsm->li = fsm->links; fsm->li; fsm->li = fsm->li->next) {
	if (fsm->li->inode == st->st_ino && fsm->li->dev == st->st_dev)
	    break;
    }

    for (i = 0; i < fsm->li->nlink; i++) {
	if (fsm->li->filex[i] < 0) continue;
	fsm->ix = fsm->li->filex[i];
	rc = fsmStage(fsm, FSM_MAP);
	rc = fsmStage(fsm, FSM_COMMIT);
	fsm->path = _free(fsm->path);
	fsm->li->filex[i] = -1;
    }

    fsm->ix = iterIndex;
    fsm->nsuffix = nsuffix;
    fsm->path = path;
    return rc;
}

int fsmStage(FSM_t fsm, fileStage stage)
{
#ifdef	UNUSED
    fileStage prevStage = fsm->stage;
    const char * const prev = fileStageString(prevStage);
#endif
    static int modulo = 4;
    const char * const cur = fileStageString(stage);
    struct stat * st = &fsm->sb;
    struct stat * ost = &fsm->osb;
    int saveerrno = errno;
    int rc = fsm->rc;
    int left;
    int i;

    if (stage & FSM_DEAD) {
	/* do nothing */
    } else if (stage & FSM_INTERNAL) {
	if (_fsm_debug && !(stage & FSM_SYSCALL))
	    rpmMessage(RPMMESS_DEBUG, " %8s %06o%3d (%4d,%4d)%10d %s %s\n",
		cur,
		st->st_mode, st->st_nlink, st->st_uid, st->st_gid, st->st_size,
		(fsm->path ? fsm->path : ""),
		((fsm->action != FA_UNKNOWN && fsm->action != FA_CREATE)
			? fileActionString(fsm->action) : ""));
    } else {
	fsm->stage = stage;
	if (_fsm_debug || !(stage & FSM_VERBOSE))
	    rpmMessage(RPMMESS_DEBUG, "%-8s  %06o%3d (%4d,%4d)%10d %s %s\n",
		cur,
		st->st_mode, st->st_nlink, st->st_uid, st->st_gid, st->st_size,
		(fsm->path ? fsm->path + fsm->astriplen : ""),
		((fsm->action != FA_UNKNOWN && fsm->action != FA_CREATE)
			? fileActionString(fsm->action) : ""));
    }

    switch (stage) {
    case FSM_UNKNOWN:
	break;
    case FSM_PKGINSTALL:
	while (1) {
	    /* Clean fsm, free'ing memory. Read next archive header. */
	    rc = fsmStage(fsm, FSM_INIT);

	    /* Exit on end-of-payload. */
	    if (rc == CPIOERR_HDR_TRAILER) {
		rc = 0;
		break;
	    }

	    /* Exit on error. */
	    if (rc) {
		fsm->postpone = 1;
		(void) fsmStage(fsm, FSM_UNDO);
		break;
	    }

	    /* Extract file from archive. */
	    rc = fsmStage(fsm, FSM_PROCESS);
	    if (rc) {
		(void) fsmStage(fsm, FSM_UNDO);
		break;
	    }

	    /* Notify on success. */
	    (void) fsmStage(fsm, FSM_NOTIFY);

	    if (fsmStage(fsm, FSM_FINI))
		break;
	}
	break;
    case FSM_PKGERASE:
    case FSM_PKGCOMMIT:
	while (1) {
	    /* Clean fsm, free'ing memory. */
	    rc = fsmStage(fsm, FSM_INIT);

	    /* Exit on end-of-payload. */
	    if (rc == CPIOERR_HDR_TRAILER) {
		rc = 0;
		break;
	    }

	    /* Rename/erase next item. */
	    if (fsmStage(fsm, FSM_FINI))
		break;
	}
	break;
    case FSM_PKGBUILD:
	while (1) {

	    rc = fsmStage(fsm, FSM_INIT);

	    /* Exit on end-of-payload. */
	    if (rc == CPIOERR_HDR_TRAILER) {
		rc = 0;
		break;
	    }

	    /* Exit on error. */
	    if (rc) {
		fsm->postpone = 1;
		(void) fsmStage(fsm, FSM_UNDO);
		break;
	    }

	    /* Copy file into archive. */
	    rc = fsmStage(fsm, FSM_PROCESS);
	    if (rc) {
		(void) fsmStage(fsm, FSM_UNDO);
		break;
	    }

	    if (fsmStage(fsm, FSM_FINI))
		break;
	}

	if (!rc)
	    rc = fsmStage(fsm, FSM_TRAILER);
	break;
    case FSM_CREATE:
	{   rpmTransactionSet ts = fsmGetTs(fsm);
#define	_tsmask	(RPMTRANS_FLAG_PKGCOMMIT | RPMTRANS_FLAG_COMMIT)
	    fsm->commit = ((ts && (ts->transFlags & _tsmask) &&
			fsm->goal != FSM_PKGCOMMIT) ? 0 : 1);
#undef _tsmask
	}
	fsm->path = _free(fsm->path);
	fsm->opath = _free(fsm->opath);
	fsm->dnlx = _free(fsm->dnlx);

	fsm->ldn = _free(fsm->ldn);
	fsm->ldnalloc = fsm->ldnlen = 0;

	fsm->rdsize = fsm->wrsize = 0;
	fsm->rdbuf = fsm->rdb = _free(fsm->rdb);
	fsm->wrbuf = fsm->wrb = _free(fsm->wrb);
	if (fsm->goal == FSM_PKGINSTALL || fsm->goal == FSM_PKGBUILD) {
	    fsm->rdsize = 8 * BUFSIZ;
	    fsm->rdbuf = fsm->rdb = xmalloc(fsm->rdsize);
	    fsm->wrsize = 8 * BUFSIZ;
	    fsm->wrbuf = fsm->wrb = xmalloc(fsm->wrsize);
	}

	fsm->mkdirsdone = 0;
	fsm->ix = -1;
	fsm->links = NULL;
	fsm->li = NULL;
	errno = 0;	/* XXX get rid of EBADF */

	/* Detect and create directories not explicitly in package. */
	if (fsm->goal == FSM_PKGINSTALL) {
	    rc = fsmStage(fsm, FSM_MKDIRS);
	    if (!rc) fsm->mkdirsdone = 1;
	}

	break;
    case FSM_INIT:
	fsm->path = _free(fsm->path);
	fsm->postpone = 0;
	fsm->diskchecked = fsm->exists = 0;
	fsm->subdir = NULL;
	fsm->suffix = (fsm->sufbuf[0] != '\0' ? fsm->sufbuf : NULL);
	fsm->action = FA_UNKNOWN;
	fsm->osuffix = NULL;
	fsm->nsuffix = NULL;

	if (fsm->goal == FSM_PKGINSTALL) {
	    /* Read next header from payload, checking for end-of-payload. */
	    rc = fsmStage(fsm, FSM_NEXT);
	}
	if (rc) break;

	/* Identify mapping index. */
	fsm->ix = ((fsm->goal == FSM_PKGINSTALL)
		? mapFind(fsm->iter, fsm->path) : mapNextIterator(fsm->iter));

	/* On non-install, detect end-of-loop. */
	if (fsm->goal != FSM_PKGINSTALL && fsm->ix < 0) {
	    rc = CPIOERR_HDR_TRAILER;
	    break;
	}

	/* On non-install, mode must be known so that dirs don't get suffix. */
	if (fsm->goal != FSM_PKGINSTALL) {
	    TFI_t fi = fsmGetFi(fsm);
	    st->st_mode = fi->fmodes[fsm->ix];
	}

	/* Generate file path. */
	rc = fsmStage(fsm, FSM_MAP);
	if (rc) break;

	/* Perform lstat/stat for disk file. */
	rc = fsmStage(fsm, (!(fsm->mapFlags & CPIO_FOLLOW_SYMLINKS)
			? FSM_LSTAT : FSM_STAT));
	if (rc == CPIOERR_LSTAT_FAILED && errno == ENOENT) {
	    errno = saveerrno;
	    rc = 0;
	    fsm->exists = 0;
	} else if (rc == 0) {
	    fsm->exists = 1;
	}
	fsm->diskchecked = 1;
	if (rc) break;

	/* On non-install, the disk file stat is what's remapped. */
	if (fsm->goal != FSM_PKGINSTALL)
	    *st = *ost;			/* structure assignment */

	/* Remap file perms, owner, and group. */
	rc = fsmMapAttrs(fsm);
	if (rc) break;

	fsm->postpone = XFA_SKIPPING(fsm->action);
	if (fsm->goal == FSM_PKGINSTALL || fsm->goal == FSM_PKGBUILD) {
	    if (!S_ISDIR(st->st_mode) && st->st_nlink > 1)
		fsm->postpone = saveHardLink(fsm);
	}
	break;
    case FSM_PRE:
	break;
    case FSM_MAP:
	rc = fsmMapPath(fsm);
	break;
    case FSM_MKDIRS:
	{   const char * path = fsm->path;
	    mode_t st_mode = st->st_mode;
	    void * dnli = dnlInitIterator(fsm, 0);
	    char * dn = fsm->rdbuf;
	    int dc = dnlCount(dnli);

	    fsm->path = NULL;
	    dn[0] = '\0';
	    fsm->dnlx = (dc ? xcalloc(dc, sizeof(*fsm->dnlx)) : NULL);
	    while ((fsm->path = dnlNextIterator(dnli)) != NULL) {
		int dnlen = strlen(fsm->path);
		char * te;

		dc = dnlIndex(dnli);
		if (dc < 0) continue;
		fsm->dnlx[dc] = dnlen;
		if (dnlen <= 1)
		    continue;
		if (dnlen <= fsm->ldnlen && !strcmp(fsm->path, fsm->ldn))
		    continue;

		/* Copy to avoid const on fsm->path. */
		(void) stpcpy(dn, fsm->path);
		fsm->path = dn;

		/* Initial mode for created dirs is 0700 */
		st->st_mode &= ~07777; 		/* XXX abuse st->st_mode */
		st->st_mode |=  00700;

		/* Assume '/' directory, otherwise "mkdir -p" */
		for (i = 1, te = dn + 1; *te; te++, i++) {
		    if (*te != '/') continue;

		    *te = '\0';

		    /* Already validated? */
		    if (i < fsm->ldnlen &&
			(fsm->ldn[i] == '/' || fsm->ldn[i] == '\0') &&
			!strncmp(fsm->path, fsm->ldn, i))
		    {
			*te = '/';
			/* Move pre-existing path marker forward. */
			fsm->dnlx[dc] = (te - dn);
			continue;
		    }

		    /* Validate next component of path. */
		    rc = fsmStage(fsm, FSM_LSTAT);
		    *te = '/';

		    /* Directory already exists? */
		    if (rc == 0 && S_ISDIR(ost->st_mode)) {
			/* Move pre-existing path marker forward. */
			fsm->dnlx[dc] = (te - dn);
		    } else if (rc == CPIOERR_LSTAT_FAILED) {
			TFI_t fi = fsmGetFi(fsm);
			mode_t st_mode = st->st_mode;
			*te = '\0';
			st->st_mode = S_IFDIR | (fi->dperms & 07777);
			rc = fsmStage(fsm, FSM_MKDIR);
			if (!rc)
			    rpmMessage(RPMMESS_WARNING,
				_("%s directory created with perms %04o.\n"),
				fsm->path, (st->st_mode & 07777));
			*te = '/';
			st->st_mode = st_mode;
		    }
		    if (rc) break;
		}
		if (rc) break;

		/* Save last validated path. */
		if (fsm->ldnalloc < (dnlen + 1)) {
		    fsm->ldnalloc = dnlen + 100;
		    fsm->ldn = xrealloc(fsm->ldn, fsm->ldnalloc);
		}
		strcpy(fsm->ldn, fsm->path);
	 	fsm->ldnlen = dnlen;
	    }
	    dnli = dnlFreeIterator(dnli);
	    fsm->path = path;
	    st->st_mode = st_mode;		/* XXX restore st->st_mode */
	}
	break;
    case FSM_RMDIRS:
	if (fsm->dnlx) {
	    const char * path = fsm->path;
	    void * dnli = dnlInitIterator(fsm, 1);
	    char * dn = fsm->rdbuf;
	    int dc = dnlCount(dnli);

	    fsm->path = NULL;
	    dn[0] = '\0';
	    while ((fsm->path = dnlNextIterator(dnli)) != NULL) {
		int dnlen = strlen(fsm->path);
		char * te;

		dc = dnlIndex(dnli);
		if (fsm->dnlx[dc] < 1 || fsm->dnlx[dc] >= dnlen)
		    continue;

		/* Copy to avoid const on fsm->path. */
		te = stpcpy(dn, fsm->path) - 1;
		fsm->path = dn;

		/* Remove generated directories. */
		do {
		    if (*te == '/') {
			*te = '\0';
			rc = fsmStage(fsm, FSM_RMDIR);
			*te = '/';
		    }
		    if (rc) break;
		    te--;
		} while ((te - dn) > fsm->dnlx[dc]);
	    }
	    dnli = dnlFreeIterator(dnli);
	    fsm->path = path;
	}
	break;
    case FSM_PROCESS:
	if (fsm->postpone) {
	    if (fsm->goal == FSM_PKGINSTALL)
		rc = fsmStage(fsm, FSM_EAT);
	    break;
	}

	if (fsm->goal == FSM_PKGBUILD) {
	    if (!S_ISDIR(st->st_mode) && st->st_nlink > 1) {
		struct hardLink * li, * prev;
		rc = writeLinkedFile(fsm);
		if (rc) break;	/* W2DO? */

		for (li = fsm->links, prev = NULL; li; prev = li, li = li->next)
		     if (li == fsm->li) break;

		if (prev == NULL)
		    fsm->links = fsm->li->next;
		else
		    prev->next = fsm->li->next;
		fsm->li->next = NULL;
		fsm->li = freeHardLink(fsm->li);
	    } else {
		rc = writeFile(fsm, 1);
	    }
	    break;
	}

	if (fsm->goal != FSM_PKGINSTALL)
	    break;

	if (S_ISREG(st->st_mode)) {
	    const char * path = fsm->path;
	    if (fsm->osuffix)
		fsm->path = fsmFsPath(fsm, st, NULL, NULL);
	    rc = fsmStage(fsm, FSM_VERIFY);

	    if (rc == 0 && fsm->osuffix) {
		const char * opath = fsm->opath;
		fsm->opath = fsm->path;
		fsm->path = fsmFsPath(fsm, st, NULL, fsm->osuffix);
		rc = fsmStage(fsm, FSM_RENAME);
		if (!rc)
		    rpmMessage(RPMMESS_WARNING,
			_("%s saved as %s\n"), fsm->opath, fsm->path);
		fsm->path = _free(fsm->path);
		fsm->opath = opath;
	    }

	    fsm->path = path;
	    if (rc != CPIOERR_LSTAT_FAILED) return rc;
	    rc = expandRegular(fsm);
	} else if (S_ISDIR(st->st_mode)) {
	    mode_t st_mode = st->st_mode;
	    rc = fsmStage(fsm, FSM_VERIFY);
	    if (rc == CPIOERR_LSTAT_FAILED) {
		st->st_mode &= ~07777; 		/* XXX abuse st->st_mode */
		st->st_mode |=  00700;
		rc = fsmStage(fsm, FSM_MKDIR);
		st->st_mode = st_mode;		/* XXX restore st->st_mode */
	    }
	} else if (S_ISLNK(st->st_mode)) {
	    const char * opath = fsm->opath;

	    if ((st->st_size + 1) > fsm->rdsize) {
		rc = CPIOERR_HDR_SIZE;
		break;
	    }

	    fsm->wrlen = st->st_size;
	    rc = fsmStage(fsm, FSM_DREAD);
	    if (!rc && fsm->rdnb != fsm->wrlen)
		rc = CPIOERR_READ_FAILED;
	    if (rc) break;

	    fsm->wrbuf[st->st_size] = '\0';
	    /* XXX symlink(fsm->opath, fsm->path) */
	    fsm->opath = fsm->wrbuf;		/* XXX abuse fsm->path */
	    rc = fsmStage(fsm, FSM_VERIFY);
	    if (rc == CPIOERR_LSTAT_FAILED)
		rc = fsmStage(fsm, FSM_SYMLINK);
	    fsm->opath = opath;		/* XXX restore fsm->path */
	} else if (S_ISFIFO(st->st_mode)) {
	    mode_t st_mode = st->st_mode;
	    /* This mimics cpio S_ISSOCK() behavior but probably isnt' right */
	    rc = fsmStage(fsm, FSM_VERIFY);
	    if (rc == CPIOERR_LSTAT_FAILED) {
		st->st_mode = 0000;		/* XXX abuse st->st_mode */
		rc = fsmStage(fsm, FSM_MKFIFO);
		st->st_mode = st_mode;	/* XXX restore st->st_mode */
	    }
	} else if (S_ISCHR(st->st_mode) ||
		   S_ISBLK(st->st_mode) ||
		   S_ISSOCK(st->st_mode))
	{
	    rc = fsmStage(fsm, FSM_VERIFY);
	    if (rc == CPIOERR_LSTAT_FAILED)
		rc = fsmStage(fsm, FSM_MKNOD);
	} else {
	    rc = CPIOERR_UNKNOWN_FILETYPE;
	}
	if (!S_ISDIR(st->st_mode) && st->st_nlink > 1) {
	    fsm->li->createdPath = fsm->li->linkIndex;
	    rc = fsmMakeLinks(fsm);
	}
	break;
    case FSM_POST:
	break;
    case FSM_MKLINKS:
	break;
    case FSM_NOTIFY:		/* XXX move from fsm to psm -> tsm */
	if (fsm->goal == FSM_PKGINSTALL || fsm->goal == FSM_PKGBUILD) {
	    rpmTransactionSet ts = fsmGetTs(fsm);
	    TFI_t fi = fsmGetFi(fsm);
	    if (ts && ts->notify && fi)
		(void)ts->notify(fi->h, RPMCALLBACK_INST_PROGRESS,
			fdGetCpioPos(fsm->cfd), fi->archiveSize,
			(fi->ap ? fi->ap->key : NULL), ts->notifyData);
	}
	break;
    case FSM_UNDO:
	if (fsm->postpone)
	    break;
	if (fsm->goal == FSM_PKGINSTALL) {
	    (void) fsmStage(fsm,
		(S_ISDIR(st->st_mode) ? FSM_RMDIR : FSM_UNLINK));

#ifdef	NOTYET	/* XXX remove only dirs just created, not all. */
	    if (fsm->dnlx)
		(void) fsmStage(fsm, FSM_RMDIRS);
#endif
	    errno = saveerrno;
	}
	if (fsm->failedFile && *fsm->failedFile == NULL)
	    *fsm->failedFile = xstrdup(fsm->path);
	break;
    case FSM_FINI:
	if (!fsm->postpone && fsm->commit) {
	    if (fsm->goal == FSM_PKGINSTALL)
		rc = ((!S_ISDIR(st->st_mode) && st->st_nlink > 1)
			? fsmCommitLinks(fsm) : fsmStage(fsm, FSM_COMMIT));
	    if (fsm->goal == FSM_PKGCOMMIT)
		rc = fsmStage(fsm, FSM_COMMIT);
	    if (fsm->goal == FSM_PKGERASE)
		rc = fsmStage(fsm, FSM_COMMIT);
	}
	fsm->path = _free(fsm->path);
	fsm->opath = _free(fsm->opath);
	memset(st, 0, sizeof(*st));
	memset(ost, 0, sizeof(*ost));
	break;
    case FSM_COMMIT:
	/* Rename pre-existing modified or unmanaged file. */
	if (fsm->diskchecked && fsm->exists && fsm->osuffix) {
	    const char * opath = fsm->opath;
	    const char * path = fsm->path;
	    fsm->opath = fsmFsPath(fsm, st, NULL, NULL);
	    fsm->path = fsmFsPath(fsm, st, NULL, fsm->osuffix);
	    rc = fsmStage(fsm, FSM_RENAME);
	    if (!rc) {
		rpmMessage(RPMMESS_WARNING, _("%s saved as %s\n"),
				fsm->opath, fsm->path);
	    }
	    fsm->path = _free(fsm->path);
	    fsm->path = path;
	    fsm->opath = _free(fsm->opath);
	    fsm->opath = opath;
	}

	/* Remove erased files. */
	if (fsm->goal == FSM_PKGERASE) {
	    if (fsm->action == FA_ERASE) {
		TFI_t fi = fsmGetFi(fsm);
		if (S_ISDIR(st->st_mode)) {
		    rc = fsmStage(fsm, FSM_RMDIR);
		    if (!rc) break;
		    switch (errno) {
		    case ENOENT: /* XXX rmdir("/") linux 2.2.x kernel hack */
		    case ENOTEMPTY:
	/* XXX make sure that build side permits %missingok on directories. */
			if (fsm->fflags & RPMFILE_MISSINGOK)
			    break;

			/* XXX common error message. */
			rpmError(RPMERR_RMDIR, 
			    _("%s rmdir of %s failed: Directory not empty\n"), 
				fiTypeString(fi), fsm->path);
			break;
		    default:
			rpmError(RPMERR_RMDIR,
				_("%s rmdir of %s failed: %s\n"),
				fiTypeString(fi), fsm->path, strerror(errno));
			break;
		    }
		} else {
		    rc = fsmStage(fsm, FSM_UNLINK);
		    if (!rc) break;
		    if (!(errno == ENOENT && (fsm->fflags & RPMFILE_MISSINGOK)))
			rpmError(RPMERR_UNLINK,
				_("%s unlink of %s failed: %s\n"),
				fiTypeString(fi), fsm->path, strerror(errno));
		}
	    }
	    break;
	}

	if (!S_ISSOCK(st->st_mode)) {	/* XXX /dev/log et al are skipped */
	    /* Rename temporary to final file name. */
	    if (!S_ISDIR(st->st_mode) &&
		(fsm->subdir || fsm->suffix || fsm->nsuffix))
	    {
		fsm->opath = fsm->path;
		fsm->path = fsmFsPath(fsm, st, NULL, fsm->nsuffix);
		rc = fsmStage(fsm, FSM_RENAME);
		if (!rc && fsm->nsuffix) {
		    const char * opath = fsmFsPath(fsm, st, NULL, NULL);
		    rpmMessage(RPMMESS_WARNING, _("%s created as %s\n"),
				opath, fsm->path);
		    opath = _free(opath);
		}
		fsm->opath = _free(fsm->opath);
	    }
	    if (S_ISLNK(st->st_mode)) {
		if (!rc && !getuid())
		    rc = fsmStage(fsm, FSM_LCHOWN);
	    } else {
		if (!rc && !getuid())
		    rc = fsmStage(fsm, FSM_CHOWN);
		if (!rc)
		    rc = fsmStage(fsm, FSM_CHMOD);
		if (!rc) {
		    time_t st_mtime = st->st_mtime;
		    TFI_t fi = fsmGetFi(fsm);
		    if (fi->fmtimes)
			st->st_mtime = fi->fmtimes[fsm->ix];
		    rc = fsmStage(fsm, FSM_UTIME);
		    st->st_mtime = st_mtime;
		}
	    }
	}

	/* Notify on success. */
	if (!rc)		rc = fsmStage(fsm, FSM_NOTIFY);
	break;
    case FSM_DESTROY:
	fsm->path = _free(fsm->path);

	/* Create any remaining links (if no error), and clean up. */
	while ((fsm->li = fsm->links) != NULL) {
	    fsm->links = fsm->li->next;
	    fsm->li->next = NULL;
	    if (fsm->goal == FSM_PKGINSTALL && fsm->commit && fsm->li->linksLeft)
	    {
		for (i = 0 ; i < fsm->li->linksLeft; i++) {
		    if (fsm->li->filex[i] < 0) continue;
		    rc = CPIOERR_MISSING_HARDLINK;
		    if (fsm->failedFile && *fsm->failedFile == NULL) {
			fsm->ix = fsm->li->filex[i];
			if (!fsmStage(fsm, FSM_MAP)) {
			    *fsm->failedFile = fsm->path;
			    fsm->path = NULL;
			}
		    }
		    break;
		}
	    }
	    if (fsm->goal == FSM_PKGBUILD) {
		rc = CPIOERR_MISSING_HARDLINK;
	    }
	    fsm->li = freeHardLink(fsm->li);
	}
	fsm->ldn = _free(fsm->ldn);
	fsm->ldnalloc = fsm->ldnlen = 0;
	fsm->rdbuf = fsm->rdb = _free(fsm->rdb);
	fsm->wrbuf = fsm->wrb = _free(fsm->wrb);
	break;
    case FSM_VERIFY:
	if (fsm->diskchecked && !fsm->exists) {
	    rc = CPIOERR_LSTAT_FAILED;
	    break;
	}
	if (S_ISREG(st->st_mode)) {
	    char * path = alloca(strlen(fsm->path) + sizeof("-RPMDELETE"));
	    (void) stpcpy( stpcpy(path, fsm->path), "-RPMDELETE");
	    /*
	     * XXX HP-UX (and other os'es) don't permit unlink on busy
	     * XXX files.
	     */
	    fsm->opath = fsm->path;
	    fsm->path = path;
	    rc = fsmStage(fsm, FSM_RENAME);
	    if (!rc)
		    (void) fsmStage(fsm, FSM_UNLINK);
	    else
		    rc = CPIOERR_UNLINK_FAILED;
	    fsm->path = fsm->opath;
	    fsm->opath = NULL;
	    return (rc ? rc : CPIOERR_LSTAT_FAILED);	/* XXX HACK */
	    /*@notreached@*/ break;
	} else if (S_ISDIR(st->st_mode)) {
	    if (S_ISDIR(ost->st_mode))		return 0;
	    if (S_ISLNK(ost->st_mode)) {
		rc = fsmStage(fsm, FSM_STAT);
		if (rc == CPIOERR_STAT_FAILED && errno == ENOENT) rc = 0;
		if (rc) break;
		errno = saveerrno;
		if (S_ISDIR(ost->st_mode))	return 0;
	    }
	} else if (S_ISLNK(st->st_mode)) {
	    if (S_ISLNK(ost->st_mode)) {
	/* XXX NUL terminated result in fsm->rdbuf, len in fsm->rdnb. */
		rc = fsmStage(fsm, FSM_READLINK);
		errno = saveerrno;
		if (rc) break;
		if (!strcmp(fsm->opath, fsm->rdbuf))	return 0;
	    }
	} else if (S_ISFIFO(st->st_mode)) {
	    if (S_ISFIFO(ost->st_mode))		return 0;
	} else if (S_ISCHR(st->st_mode) || S_ISBLK(st->st_mode)) {
	    if ((S_ISCHR(ost->st_mode) || S_ISBLK(ost->st_mode)) &&
		(ost->st_rdev == st->st_rdev))	return 0;
	} else if (S_ISSOCK(st->st_mode)) {
	    if (S_ISSOCK(ost->st_mode))		return 0;
	}
	    /* XXX shouldn't do this with commit/undo. */
	rc = 0;
	if (fsm->stage == FSM_PROCESS) rc = fsmStage(fsm, FSM_UNLINK);
	if (rc == 0)	rc = CPIOERR_LSTAT_FAILED;
	return (rc ? rc : CPIOERR_LSTAT_FAILED);	/* XXX HACK */
	/*@notreached@*/ break;

    case FSM_UNLINK:
	rc = Unlink(fsm->path);
	if (_fsm_debug && (stage & FSM_SYSCALL))
	    rpmMessage(RPMMESS_DEBUG, " %8s (%s) %s\n", cur,
		fsm->path, (rc < 0 ? strerror(errno) : ""));
	if (rc < 0)	rc = CPIOERR_UNLINK_FAILED;
	break;
    case FSM_RENAME:
	rc = Rename(fsm->opath, fsm->path);
	if (_fsm_debug && (stage & FSM_SYSCALL))
	    rpmMessage(RPMMESS_DEBUG, " %8s (%s, %s) %s\n", cur,
		fsm->opath, fsm->path, (rc < 0 ? strerror(errno) : ""));
	if (rc < 0)	rc = CPIOERR_RENAME_FAILED;
	break;
    case FSM_MKDIR:
	rc = Mkdir(fsm->path, (st->st_mode & 07777));
	if (_fsm_debug && (stage & FSM_SYSCALL))
	    rpmMessage(RPMMESS_DEBUG, " %8s (%s, 0%04o) %s\n", cur,
		fsm->path, (st->st_mode & 07777),
		(rc < 0 ? strerror(errno) : ""));
	if (rc < 0)	rc = CPIOERR_MKDIR_FAILED;
	break;
    case FSM_RMDIR:
	rc = Rmdir(fsm->path);
	if (_fsm_debug && (stage & FSM_SYSCALL))
	    rpmMessage(RPMMESS_DEBUG, " %8s (%s) %s\n", cur,
		fsm->path, (rc < 0 ? strerror(errno) : ""));
	if (rc < 0)	rc = CPIOERR_RMDIR_FAILED;
	break;
    case FSM_CHOWN:
	rc = chown(fsm->path, st->st_uid, st->st_gid);
	if (_fsm_debug && (stage & FSM_SYSCALL))
	    rpmMessage(RPMMESS_DEBUG, " %8s (%s, %d, %d) %s\n", cur,
		fsm->path, st->st_uid, st->st_gid,
		(rc < 0 ? strerror(errno) : ""));
	if (rc < 0)	rc = CPIOERR_CHOWN_FAILED;
	break;
    case FSM_LCHOWN:
#if ! CHOWN_FOLLOWS_SYMLINK
	rc = lchown(fsm->path, st->st_uid, st->st_gid);
	if (_fsm_debug && (stage & FSM_SYSCALL))
	    rpmMessage(RPMMESS_DEBUG, " %8s (%s, %d, %d) %s\n", cur,
		fsm->path, st->st_uid, st->st_gid,
		(rc < 0 ? strerror(errno) : ""));
	if (rc < 0)	rc = CPIOERR_CHOWN_FAILED;
#endif
	break;
    case FSM_CHMOD:
	rc = chmod(fsm->path, (st->st_mode & 07777));
	if (_fsm_debug && (stage & FSM_SYSCALL))
	    rpmMessage(RPMMESS_DEBUG, " %8s (%s, 0%04o) %s\n", cur,
		fsm->path, (st->st_mode & 07777),
		(rc < 0 ? strerror(errno) : ""));
	if (rc < 0)	rc = CPIOERR_CHMOD_FAILED;
	break;
    case FSM_UTIME:
	{   struct utimbuf stamp;
	    stamp.actime = st->st_mtime;
	    stamp.modtime = st->st_mtime;
	    rc = utime(fsm->path, &stamp);
	    if (_fsm_debug && (stage & FSM_SYSCALL))
		rpmMessage(RPMMESS_DEBUG, " %8s (%s, 0x%x) %s\n", cur,
			fsm->path, (unsigned)st->st_mtime,
			(rc < 0 ? strerror(errno) : ""));
	    if (rc < 0)	rc = CPIOERR_UTIME_FAILED;
	}
	break;
    case FSM_SYMLINK:
	rc = symlink(fsm->opath, fsm->path);
	if (_fsm_debug && (stage & FSM_SYSCALL))
	    rpmMessage(RPMMESS_DEBUG, " %8s (%s, %s) %s\n", cur,
		fsm->opath, fsm->path, (rc < 0 ? strerror(errno) : ""));
	if (rc < 0)	rc = CPIOERR_SYMLINK_FAILED;
	break;
    case FSM_LINK:
	rc = Link(fsm->opath, fsm->path);
	if (_fsm_debug && (stage & FSM_SYSCALL))
	    rpmMessage(RPMMESS_DEBUG, " %8s (%s, %s) %s\n", cur,
		fsm->opath, fsm->path, (rc < 0 ? strerror(errno) : ""));
	if (rc < 0)	rc = CPIOERR_LINK_FAILED;
	break;
    case FSM_MKFIFO:
	rc = mkfifo(fsm->path, (st->st_mode & 07777));
	if (_fsm_debug && (stage & FSM_SYSCALL))
	    rpmMessage(RPMMESS_DEBUG, " %8s (%s, 0%04o) %s\n", cur,
		fsm->path, (st->st_mode & 07777),
		(rc < 0 ? strerror(errno) : ""));
	if (rc < 0)	rc = CPIOERR_MKFIFO_FAILED;
	break;
    case FSM_MKNOD:
	/*@-unrecog@*/
	rc = mknod(fsm->path, (st->st_mode & ~07777), st->st_rdev);
	if (_fsm_debug && (stage & FSM_SYSCALL))
	    rpmMessage(RPMMESS_DEBUG, " %8s (%s, 0%o, 0x%x) %s\n", cur,
		fsm->path, (st->st_mode & ~07777), (unsigned)st->st_rdev,
		(rc < 0 ? strerror(errno) : ""));
	if (rc < 0)	rc = CPIOERR_MKNOD_FAILED;
	/*@=unrecog@*/
	break;
    case FSM_LSTAT:
	rc = Lstat(fsm->path, ost);
	if (_fsm_debug && (stage & FSM_SYSCALL) && rc && errno != ENOENT)
	    rpmMessage(RPMMESS_DEBUG, " %8s (%s, ost) %s\n", cur,
		fsm->path, (rc < 0 ? strerror(errno) : ""));
	if (rc < 0)	rc = CPIOERR_LSTAT_FAILED;
	break;
    case FSM_STAT:
	rc = Stat(fsm->path, ost);
	if (_fsm_debug && (stage & FSM_SYSCALL) && rc && errno != ENOENT)
	    rpmMessage(RPMMESS_DEBUG, " %8s (%s, ost) %s\n", cur,
		fsm->path, (rc < 0 ? strerror(errno) : ""));
	if (rc < 0)	rc = CPIOERR_STAT_FAILED;
	break;
    case FSM_READLINK:
	/* XXX NUL terminated result in fsm->rdbuf, len in fsm->rdnb. */
	rc = Readlink(fsm->path, fsm->rdbuf, fsm->rdsize - 1);
	if (_fsm_debug && (stage & FSM_SYSCALL))
	    rpmMessage(RPMMESS_DEBUG, " %8s (%s, rdbuf, %d) %s\n", cur,
		fsm->path, fsm->rdlen, (rc < 0 ? strerror(errno) : ""));
	if (rc < 0)	rc = CPIOERR_READLINK_FAILED;
	else {
	    fsm->rdnb = rc;
	    fsm->rdbuf[fsm->rdnb] = '\0';
	    rc = 0;
	}
	break;
    case FSM_CHROOT:
	break;

    case FSM_NEXT:
	rc = fsmStage(fsm, FSM_HREAD);
	if (rc) break;
	if (!strcmp(fsm->path, CPIO_TRAILER)) { /* Detect end-of-payload. */
	    fsm->path = _free(fsm->path);
	    rc = CPIOERR_HDR_TRAILER;
	}
	if (!rc)
	    rc = fsmStage(fsm, FSM_POS);
	break;
    case FSM_EAT:
	for (left = st->st_size; left > 0; left -= fsm->rdnb) {
	    fsm->wrlen = (left > fsm->wrsize ? fsm->wrsize : left);
	    rc = fsmStage(fsm, FSM_DREAD);
	    if (rc) break;
	}
	break;
    case FSM_POS:
	left = (modulo - (fdGetCpioPos(fsm->cfd) % modulo)) % modulo;
	if (left) {
	    fsm->wrlen = left;
	    (void) fsmStage(fsm, FSM_DREAD);
	}
	break;
    case FSM_PAD:
	left = (modulo - (fdGetCpioPos(fsm->cfd) % modulo)) % modulo;
	if (left) {
	    memset(fsm->rdbuf, 0, left);
	    /* XXX DWRITE uses rdnb for I/O length. */
	    fsm->rdnb = left;
	    (void) fsmStage(fsm, FSM_DWRITE);
	}
	break;
    case FSM_TRAILER:
	rc = cpioTrailerWrite(fsm);
	break;
    case FSM_HREAD:
	rc = fsmStage(fsm, FSM_POS);
	if (!rc)
	    rc = cpioHeaderRead(fsm, st);	/* Read next payload header. */
	break;
    case FSM_HWRITE:
	rc = cpioHeaderWrite(fsm, st);		/* Write next payload header. */
	break;
    case FSM_DREAD:
	fsm->rdnb = Fread(fsm->wrbuf, sizeof(*fsm->wrbuf), fsm->wrlen, fsm->cfd);
	if (_fsm_debug && (stage & FSM_SYSCALL))
	    rpmMessage(RPMMESS_DEBUG, " %8s (%s, %d, cfd)\trdnb %d\n",
		cur, (fsm->wrbuf == fsm->wrb ? "wrbuf" : "mmap"),
		fsm->wrlen, fsm->rdnb);
if (fsm->rdnb != fsm->wrlen) fprintf(stderr, "*** short read, had %d, got %d\n", (int)fsm->rdnb, (int)fsm->wrlen);
#ifdef	NOTYET
	if (Ferror(fsm->rfd))
	    rc = CPIOERR_READ_FAILED;
#endif
	if (fsm->rdnb > 0)
	    fdSetCpioPos(fsm->cfd, fdGetCpioPos(fsm->cfd) + fsm->rdnb);
	break;
    case FSM_DWRITE:
	fsm->wrnb = Fwrite(fsm->rdbuf, sizeof(*fsm->rdbuf), fsm->rdnb, fsm->cfd);
	if (_fsm_debug && (stage & FSM_SYSCALL))
	    rpmMessage(RPMMESS_DEBUG, " %8s (%s, %d, cfd)\twrnb %d\n",
		cur, (fsm->rdbuf == fsm->rdb ? "rdbuf" : "mmap"),
		fsm->rdnb, fsm->wrnb);
if (fsm->rdnb != fsm->wrnb) fprintf(stderr, "*** short write, had %d, got %d\n", (int)fsm->rdnb, (int)fsm->wrnb);
#ifdef	NOTYET
	if (Ferror(fsm->wfd))
	    rc = CPIOERR_WRITE_FAILED;
#endif
	if (fsm->wrnb > 0)
	    fdSetCpioPos(fsm->cfd, fdGetCpioPos(fsm->cfd) + fsm->wrnb);
	break;

    case FSM_ROPEN:
	fsm->rfd = Fopen(fsm->path, "r.ufdio");
	if (fsm->rfd == NULL || Ferror(fsm->rfd)) {
	    if (fsm->rfd)	(void) fsmStage(fsm, FSM_RCLOSE);
	    fsm->rfd = NULL;
	    rc = CPIOERR_OPEN_FAILED;
	    break;
	}
	if (_fsm_debug && (stage & FSM_SYSCALL))
	    rpmMessage(RPMMESS_DEBUG, " %8s (%s, \"r\") rfd %p rdbuf %p\n", cur,
		fsm->path, fsm->rfd, fsm->rdbuf);
	break;
    case FSM_READ:
	fsm->rdnb = Fread(fsm->rdbuf, sizeof(*fsm->rdbuf), fsm->rdlen, fsm->rfd);
	if (_fsm_debug && (stage & FSM_SYSCALL))
	    rpmMessage(RPMMESS_DEBUG, " %8s (rdbuf, %d, rfd)\trdnb %d\n",
		cur, fsm->rdlen, fsm->rdnb);
if (fsm->rdnb != fsm->rdlen) fprintf(stderr, "*** short read, had %d, got %d\n", (int)fsm->rdnb, (int)fsm->rdlen);
#ifdef	NOTYET
	if (Ferror(fsm->rfd))
	    rc = CPIOERR_READ_FAILED;
#endif
	break;
    case FSM_RCLOSE:
	if (fsm->rfd) {
	    if (_fsm_debug && (stage & FSM_SYSCALL))
		rpmMessage(RPMMESS_DEBUG, " %8s (%p)\n", cur, fsm->rfd);
	    (void) Fclose(fsm->rfd);
	    errno = saveerrno;
	}
	fsm->rfd = NULL;
	break;
    case FSM_WOPEN:
	fsm->wfd = Fopen(fsm->path, "w.ufdio");
	if (fsm->wfd == NULL || Ferror(fsm->wfd)) {
	    if (fsm->wfd)	(void) fsmStage(fsm, FSM_WCLOSE);
	    fsm->wfd = NULL;
	    rc = CPIOERR_OPEN_FAILED;
	}
	if (_fsm_debug && (stage & FSM_SYSCALL))
	    rpmMessage(RPMMESS_DEBUG, " %8s (%s, \"w\") wfd %p wrbuf %p\n", cur,
		fsm->path, fsm->wfd, fsm->wrbuf);
	break;
    case FSM_WRITE:
	fsm->wrnb = Fwrite(fsm->wrbuf, sizeof(*fsm->wrbuf), fsm->rdnb, fsm->wfd);
	if (_fsm_debug && (stage & FSM_SYSCALL))
	    rpmMessage(RPMMESS_DEBUG, " %8s (wrbuf, %d, wfd)\twrnb %d\n",
		cur, fsm->rdnb, fsm->wrnb);
if (fsm->rdnb != fsm->wrnb) fprintf(stderr, "*** short write: had %d, got %d\n", (int)fsm->rdnb, (int)fsm->wrnb);
#ifdef	NOTYET
	if (Ferror(fsm->wfd))
	    rc = CPIOERR_WRITE_FAILED;
#endif
	break;
    case FSM_WCLOSE:
	if (fsm->wfd) {
	    if (_fsm_debug && (stage & FSM_SYSCALL))
		rpmMessage(RPMMESS_DEBUG, " %8s (%p)\n", cur, fsm->wfd);
	    (void) Fclose(fsm->wfd);
	    errno = saveerrno;
	}
	fsm->wfd = NULL;
	break;

    default:
	break;
    }

    if (!(stage & FSM_INTERNAL)) {
	fsm->rc = (rc == CPIOERR_HDR_TRAILER ? 0 : rc);
    }
    return rc;
}

/*@observer@*/ const char *const fileStageString(fileStage a) {
    switch(a) {
    case FSM_UNKNOWN:	return "unknown";

    case FSM_PKGINSTALL:return "pkginstall";
    case FSM_PKGERASE:	return "pkgerase";
    case FSM_PKGBUILD:	return "pkgbuild";
    case FSM_PKGCOMMIT:	return "pkgcommit";
    case FSM_PKGUNDO:	return "pkgundo";

    case FSM_CREATE:	return "create";
    case FSM_INIT:	return "init";
    case FSM_MAP:	return "map";
    case FSM_MKDIRS:	return "mkdirs";
    case FSM_RMDIRS:	return "rmdirs";
    case FSM_PRE:	return "pre";
    case FSM_PROCESS:	return "process";
    case FSM_POST:	return "post";
    case FSM_MKLINKS:	return "mklinks";
    case FSM_NOTIFY:	return "notify";
    case FSM_UNDO:	return "undo";
    case FSM_FINI:	return "fini";
    case FSM_COMMIT:	return "commit";
    case FSM_DESTROY:	return "destroy";
    case FSM_VERIFY:	return "verify";

    case FSM_UNLINK:	return "Unlink";
    case FSM_RENAME:	return "Rename";
    case FSM_MKDIR:	return "Mkdir";
    case FSM_RMDIR:	return "rmdir";
    case FSM_CHOWN:	return "chown";
    case FSM_LCHOWN:	return "lchown";
    case FSM_CHMOD:	return "chmod";
    case FSM_UTIME:	return "utime";
    case FSM_SYMLINK:	return "symlink";
    case FSM_LINK:	return "Link";
    case FSM_MKFIFO:	return "mkfifo";
    case FSM_MKNOD:	return "mknod";
    case FSM_LSTAT:	return "Lstat";
    case FSM_STAT:	return "Stat";
    case FSM_READLINK:	return "Readlink";
    case FSM_CHROOT:	return "chroot";

    case FSM_NEXT:	return "next";
    case FSM_EAT:	return "eat";
    case FSM_POS:	return "pos";
    case FSM_PAD:	return "pad";
    case FSM_TRAILER:	return "trailer";
    case FSM_HREAD:	return "hread";
    case FSM_HWRITE:	return "hwrite";
    case FSM_DREAD:	return "Fread";
    case FSM_DWRITE:	return "Fwrite";

    case FSM_ROPEN:	return "Fopen";
    case FSM_READ:	return "Fread";
    case FSM_RCLOSE:	return "Fclose";
    case FSM_WOPEN:	return "Fopen";
    case FSM_WRITE:	return "Fwrite";
    case FSM_WCLOSE:	return "Fclose";

    default:		return "???";
    }
    /*@noteached@*/
}
