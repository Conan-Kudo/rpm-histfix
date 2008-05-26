/**
 * \file lib/legacy.c
 */

#include "system.h"

#include <rpm/header.h>
#include <rpm/rpmmacro.h>
#include <rpm/rpmstring.h>
#include <rpm/rpmfi.h>
#include <rpm/rpmds.h>

#include "lib/legacy.h"

#include "debug.h"

int _noDirTokens = 0;

static int dncmp(const void * a, const void * b)
{
    const char *const * first = a;
    const char *const * second = b;
    return strcmp(*first, *second);
}

void compressFilelist(Header h)
{
    HAE_t hae = (HAE_t)headerAddEntry;
    HRE_t hre = (HRE_t)headerRemoveEntry;
    struct rpmtd_s fileNames;
    char ** dirNames;
    const char ** baseNames;
    uint32_t * dirIndexes;
    rpm_count_t count;
    int xx, i;
    int dirIndex = -1;

    printf(">> %s\n", __func__);

    /*
     * This assumes the file list is already sorted, and begins with a
     * single '/'. That assumption isn't critical, but it makes things go
     * a bit faster.
     */

    if (headerIsEntry(h, RPMTAG_DIRNAMES)) {
	xx = hre(h, RPMTAG_OLDFILENAMES);
	return;		/* Already converted. */
    }

    if (!headerGet(h, RPMTAG_OLDFILENAMES, &fileNames, HEADERGET_MINMEM)) 
	return;
    count = rpmtdCount(&fileNames);
    if (count < 1) 
	return;

    dirNames = xmalloc(sizeof(*dirNames) * count);	/* worst case */
    baseNames = xmalloc(sizeof(*dirNames) * count);
    dirIndexes = xmalloc(sizeof(*dirIndexes) * count);

    if (headerIsSource(h)) {
	/* HACK. Source RPM, so just do things differently */
	dirIndex = 0;
	dirNames[dirIndex] = xstrdup("");
	while ((i = rpmtdNext(&fileNames)) >= 0) {
	    dirIndexes[i] = dirIndex;
	    baseNames[i] = rpmtdGetString(&fileNames);
	}
	goto exit;
    }

    while ((i = rpmtdNext(&fileNames)) >= 0) {
	char ** needle;
	char savechar;
	char * baseName;
	size_t len;
	const char *filename = rpmtdGetString(&fileNames);

	if (filename == NULL)	/* XXX can't happen */
	    continue;
	baseName = strrchr(filename, '/') + 1;
	len = baseName - filename;
	needle = dirNames;
	savechar = *baseName;
	*baseName = '\0';
	if (dirIndex < 0 ||
	    (needle = bsearch(filename, dirNames, dirIndex + 1, sizeof(dirNames[0]), dncmp)) == NULL) {
	    char *s = xmalloc(len + 1);
	    rstrlcpy(s, filename, len + 1);
	    dirIndexes[i] = ++dirIndex;
	    dirNames[dirIndex] = s;
	} else
	    dirIndexes[i] = needle - dirNames;

	*baseName = savechar;
	baseNames[i] = baseName;
    }

exit:
    if (count > 0) {
	xx = hae(h, RPMTAG_DIRINDEXES, RPM_INT32_TYPE, dirIndexes, count);
	xx = hae(h, RPMTAG_BASENAMES, RPM_STRING_ARRAY_TYPE,
			baseNames, count);
	xx = hae(h, RPMTAG_DIRNAMES, RPM_STRING_ARRAY_TYPE,
			dirNames, (rpm_count_t) dirIndex + 1);
    }

    rpmtdFreeData(&fileNames);
    for (i = 0; i <= dirIndex; i++) {
	free(dirNames[i]);
    }
    free(dirNames);
    free(baseNames);
    free(dirIndexes);

    xx = hre(h, RPMTAG_OLDFILENAMES);
}

void expandFilelist(Header h)
{
    HRE_t hre = (HRE_t)headerRemoveEntry;
    struct rpmtd_s filenames;

    if (!headerIsEntry(h, RPMTAG_OLDFILENAMES)) {
	(void) headerGet(h, RPMTAG_FILENAMES, &filenames, HEADERGET_EXT);
	if (rpmtdCount(&filenames) < 1)
	    return;
	rpmtdSetTag(&filenames, RPMTAG_OLDFILENAMES);
	headerPut(h, &filenames, HEADERPUT_DEFAULT);
	rpmtdFreeData(&filenames);
    }

    (void) hre(h, RPMTAG_DIRNAMES);
    (void) hre(h, RPMTAG_BASENAMES);
    (void) hre(h, RPMTAG_DIRINDEXES);
}

/*
 * Up to rpm 3.0.4, packages implicitly provided their own name-version-release.
 * Retrofit an explicit "Provides: name = epoch:version-release.
 */
static void providePackageNVR(Header h)
{
    HGE_t hge = (HGE_t)headerGetEntryMinMemory;
    HFD_t hfd = headerFreeData;
    const char *name;
    char *pEVR;
    rpmsenseFlags pFlags = RPMSENSE_EQUAL;
    const char ** provides = NULL;
    const char ** providesEVR = NULL;
    rpmTagType pnt, pvt;
    rpmsenseFlags * provideFlags = NULL;
    rpm_count_t providesCount, i;
    int xx;
    int bingo = 1;

    /* Generate provides for this package name-version-release. */
    pEVR = headerGetEVR(h, &name);
    if (!(name && pEVR))
	return;

    /*
     * Rpm prior to 3.0.3 does not have versioned provides.
     * If no provides at all are available, we can just add.
     */
    if (!hge(h, RPMTAG_PROVIDENAME, &pnt, (rpm_data_t *) &provides, &providesCount))
	goto exit;

    /*
     * Otherwise, fill in entries on legacy packages.
     */
    if (!hge(h, RPMTAG_PROVIDEVERSION, &pvt, (rpm_data_t *) &providesEVR, NULL)) {
	for (i = 0; i < providesCount; i++) {
	    const char * vdummy = "";
	    rpmsenseFlags fdummy = RPMSENSE_ANY;
	    xx = headerAddOrAppendEntry(h, RPMTAG_PROVIDEVERSION, RPM_STRING_ARRAY_TYPE,
			&vdummy, 1);
	    xx = headerAddOrAppendEntry(h, RPMTAG_PROVIDEFLAGS, RPM_INT32_TYPE,
			&fdummy, 1);
	}
	goto exit;
    }

    xx = hge(h, RPMTAG_PROVIDEFLAGS, NULL, (rpm_data_t *) &provideFlags, NULL);

   	/* LCL: providesEVR is not NULL */
    if (provides && providesEVR && provideFlags)
    for (i = 0; i < providesCount; i++) {
        if (!(provides[i] && providesEVR[i]))
            continue;
	if (!(provideFlags[i] == RPMSENSE_EQUAL &&
	    !strcmp(name, provides[i]) && !strcmp(pEVR, providesEVR[i])))
	    continue;
	bingo = 0;
	break;
    }

exit:
    provides = hfd(provides, pnt);
    providesEVR = hfd(providesEVR, pvt);

    if (bingo) {
	xx = headerAddOrAppendEntry(h, RPMTAG_PROVIDENAME, RPM_STRING_ARRAY_TYPE,
		&name, 1);
	xx = headerAddOrAppendEntry(h, RPMTAG_PROVIDEFLAGS, RPM_INT32_TYPE,
		&pFlags, 1);
	xx = headerAddOrAppendEntry(h, RPMTAG_PROVIDEVERSION, RPM_STRING_ARRAY_TYPE,
		&pEVR, 1);
    }
    free(pEVR);
}

void legacyRetrofit(Header h)
{
    const char * prefix;

    /*
     * We don't use these entries (and rpm >= 2 never has) and they are
     * pretty misleading. Let's just get rid of them so they don't confuse
     * anyone.
     */
    if (headerIsEntry(h, RPMTAG_FILEUSERNAME))
	(void) headerRemoveEntry(h, RPMTAG_FILEUIDS);
    if (headerIsEntry(h, RPMTAG_FILEGROUPNAME))
	(void) headerRemoveEntry(h, RPMTAG_FILEGIDS);

    /*
     * We switched the way we do relocatable packages. We fix some of
     * it up here, though the install code still has to be a bit 
     * careful. This fixup makes queries give the new values though,
     * which is quite handy.
     */
    if (headerGetEntry(h, RPMTAG_DEFAULTPREFIX, NULL, (rpm_data_t *) &prefix, NULL))
    {
	char * nprefix = stripTrailingChar(xstrdup(prefix), '/');
	(void) headerAddEntry(h, RPMTAG_PREFIXES, RPM_STRING_ARRAY_TYPE,
		&nprefix, 1); 
	free(nprefix);
    }

    /*
     * The file list was moved to a more compressed format which not
     * only saves memory (nice), but gives fingerprinting a nice, fat
     * speed boost (very nice). Go ahead and convert old headers to
     * the new style (this is a noop for new headers).
     */
     compressFilelist(h);

    /* XXX binary rpms always have RPMTAG_SOURCERPM, source rpms do not */
    if (headerIsSource(h)) {
	int32_t one = 1;
	if (!headerIsEntry(h, RPMTAG_SOURCEPACKAGE))
	    (void) headerAddEntry(h, RPMTAG_SOURCEPACKAGE, RPM_INT32_TYPE,
				&one, 1);
    } else {
	/* Retrofit "Provide: name = EVR" for binary packages. */
	providePackageNVR(h);
    }
}
