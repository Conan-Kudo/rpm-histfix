/* rpmarchive: spit out the main archive portion of a package */

#include "system.h"
const char *__progname;

#include <rpm/rpmlib.h>		/* rpmReadPackageFile .. */
#include <rpm/rpmtag.h>
#include <rpm/rpmio.h>
#include <rpm/rpmpgp.h>

#include <rpm/rpmts.h>

#include "debug.h"

int main(int argc, char *argv[])
{
    FD_t fdi, fdo;
    Header h;
    char * rpmio_flags = NULL;
    rpmRC rc;
    FD_t gzdi;
    
    setprogname(argv[0]);	/* Retrofit glibc __progname */
    if (argc == 1)
	fdi = fdDup(STDIN_FILENO);
    else {
	if (rstreq(argv[1], "-h") || rstreq(argv[1], "--help")) {
	    fprintf(stderr, "Usage: rpm2cpio file.rpm\n");
	    exit(EXIT_FAILURE);
	}
	fdi = Fopen(argv[1], "r.ufdio");
    }

    if (Ferror(fdi)) {
	fprintf(stderr, "%s: %s: %s\n", argv[0],
		(argc == 1 ? "<stdin>" : argv[1]), Fstrerror(fdi));
	exit(EXIT_FAILURE);
    }
    fdo = fdDup(STDOUT_FILENO);
    rpmReadConfigFiles(NULL, NULL);

    {	rpmts ts = rpmtsCreate();
	rpmVSFlags vsflags = 0;

	/* XXX retain the ageless behavior of rpm2cpio */
        vsflags |= _RPMVSF_NODIGESTS;
        vsflags |= _RPMVSF_NOSIGNATURES;
        vsflags |= RPMVSF_NOHDRCHK;
	(void) rpmtsSetVSFlags(ts, vsflags);

	rc = rpmReadPackageFile(ts, fdi, "rpm2cpio", &h);

	ts = rpmtsFree(ts);
    }

    switch (rc) {
    case RPMRC_OK:
    case RPMRC_NOKEY:
    case RPMRC_NOTTRUSTED:
	break;
    case RPMRC_NOTFOUND:
	fprintf(stderr, _("argument is not an RPM package\n"));
	exit(EXIT_FAILURE);
	break;
    case RPMRC_FAIL:
    default:
	fprintf(stderr, _("error reading header from package\n"));
	exit(EXIT_FAILURE);
	break;
    }

    /* Retrieve type of payload compression. */
    {	const char *compr = NULL;
	struct rpmtd_s pc;

	headerGet(h, RPMTAG_PAYLOADCOMPRESSOR, &pc, HEADERGET_DEFAULT);
	compr = rpmtdGetString(&pc);
	rpmio_flags = rstrscat(NULL, "r.", compr ? compr : "gzip", NULL);
	rpmtdFreeData(&pc);
    }

    gzdi = Fdopen(fdi, rpmio_flags);	/* XXX gzdi == fdi */
    free(rpmio_flags);

    if (gzdi == NULL) {
	fprintf(stderr, _("cannot re-open payload: %s\n"), Fstrerror(gzdi));
	exit(EXIT_FAILURE);
    }

    rc = ufdCopy(gzdi, fdo);
    rc = (rc <= 0) ? EXIT_FAILURE : EXIT_SUCCESS;
    Fclose(fdo);

    Fclose(gzdi);	/* XXX gzdi == fdi */

    return rc;
}
