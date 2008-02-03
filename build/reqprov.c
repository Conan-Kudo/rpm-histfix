/** \ingroup rpmbuild
 * \file build/reqprov.c
 *  Add dependency tags to package header(s).
 */

#include "system.h"

#include <rpm/header.h>
#include <rpm/rpmbuild.h>
#include "debug.h"

int addReqProv(rpmSpec spec, Header h, rpm_tag_t tagN,
		const char * N, const char * EVR, rpmsenseFlags Flags,
		int index)
{
    HGE_t hge = (HGE_t)headerGetEntryMinMemory;
    HFD_t hfd = headerFreeData;
    const char ** names;
    rpm_tagtype_t dnt;
    rpm_tag_t nametag = 0;
    rpm_tag_t versiontag = 0;
    rpm_tag_t flagtag = 0;
    rpm_tag_t indextag = 0;
    rpm_count_t len;
    rpmsenseFlags extra = RPMSENSE_ANY;
    int xx;
    
    if (Flags & RPMSENSE_PROVIDES) {
	nametag = RPMTAG_PROVIDENAME;
	versiontag = RPMTAG_PROVIDEVERSION;
	flagtag = RPMTAG_PROVIDEFLAGS;
	extra = Flags & RPMSENSE_FIND_PROVIDES;
    } else if (Flags & RPMSENSE_OBSOLETES) {
	nametag = RPMTAG_OBSOLETENAME;
	versiontag = RPMTAG_OBSOLETEVERSION;
	flagtag = RPMTAG_OBSOLETEFLAGS;
    } else if (Flags & RPMSENSE_CONFLICTS) {
	nametag = RPMTAG_CONFLICTNAME;
	versiontag = RPMTAG_CONFLICTVERSION;
	flagtag = RPMTAG_CONFLICTFLAGS;
    } else if (Flags & RPMSENSE_PREREQ) {
	nametag = RPMTAG_REQUIRENAME;
	versiontag = RPMTAG_REQUIREVERSION;
	flagtag = RPMTAG_REQUIREFLAGS;
	extra = Flags & _ALL_REQUIRES_MASK;
    } else if (Flags & RPMSENSE_TRIGGER) {
	nametag = RPMTAG_TRIGGERNAME;
	versiontag = RPMTAG_TRIGGERVERSION;
	flagtag = RPMTAG_TRIGGERFLAGS;
	indextag = RPMTAG_TRIGGERINDEX;
	extra = Flags & RPMSENSE_TRIGGER;
    } else {
	nametag = RPMTAG_REQUIRENAME;
	versiontag = RPMTAG_REQUIREVERSION;
	flagtag = RPMTAG_REQUIREFLAGS;
	extra = Flags & _ALL_REQUIRES_MASK;
    }

    Flags = (Flags & RPMSENSE_SENSEMASK) | extra;

    if (EVR == NULL)
	EVR = "";
    
    /* Check for duplicate dependencies. */
    if (hge(h, nametag, &dnt, (rpm_data_t *) &names, &len)) {
	const char ** versions = NULL;
	rpm_tagtype_t dvt = RPM_STRING_ARRAY_TYPE;
	int *flags = NULL;
	int *indexes = NULL;
	int duplicate = 0;

	if (flagtag) {
	    xx = hge(h, versiontag, &dvt, (rpm_data_t *) &versions, NULL);
	    xx = hge(h, flagtag, NULL, (rpm_data_t *) &flags, NULL);
	}
	if (indextag)
	    xx = hge(h, indextag, NULL, (rpm_data_t *) &indexes, NULL);

	while (len > 0) {
	    len--;
	    if (strcmp(names[len], N))
		continue;
	    if (flagtag && versions != NULL &&
		(strcmp(versions[len], EVR) || flags[len] != Flags))
		continue;
	    if (indextag && indexes != NULL && indexes[len] != index)
		continue;

	    /* This is a duplicate dependency. */
	    duplicate = 1;

	    break;
	}
	names = hfd(names, dnt);
	versions = hfd(versions, dvt);
	if (duplicate)
	    return 0;
    }

    /* Add this dependency. */
    xx = headerAddOrAppendEntry(h, nametag, RPM_STRING_ARRAY_TYPE, &N, 1);
    if (flagtag) {
	xx = headerAddOrAppendEntry(h, versiontag,
			       RPM_STRING_ARRAY_TYPE, &EVR, 1);
	xx = headerAddOrAppendEntry(h, flagtag,
			       RPM_INT32_TYPE, &Flags, 1);
    }
    if (indextag)
	xx = headerAddOrAppendEntry(h, indextag, RPM_INT32_TYPE, &index, 1);

    return 0;
}

int rpmlibNeedsFeature(Header h, const char * feature, const char * featureEVR)
{
    char * reqname = alloca(sizeof("rpmlib()") + strlen(feature));

    (void) stpcpy( stpcpy( stpcpy(reqname, "rpmlib("), feature), ")");

    /* XXX 1st arg is unused */
   return addReqProv(NULL, h, RPMTAG_REQUIRENAME, reqname, featureEVR,
	RPMSENSE_RPMLIB|(RPMSENSE_LESS|RPMSENSE_EQUAL), 0);
}
