/** \ingroup rpmbuild
 * \file build/parseDescription.c
 *  Parse %description section from spec file.
 */

#include "system.h"

#include "rpmbuild.h"
#include "rpmerr.h"
#include "debug.h"

extern int noLang;

/* These have to be global scope to make up for *stupid* compilers */
    static const char *name = NULL;
    static const char *lang = NULL;

    static struct poptOption optionsTable[] = {
	{ NULL, 'n', POPT_ARG_STRING, &name, 'n',	NULL, NULL},
	{ NULL, 'l', POPT_ARG_STRING, &lang, 'l',	NULL, NULL},
	{ 0, 0, 0, 0, 0,	NULL, NULL}
    };

int parseDescription(rpmSpec spec)
{
    int nextPart = RPMLOG_ERR;	/* assume error */
    StringBuf sb;
    int flag = PART_SUBNAME;
    Package pkg;
    int rc, argc;
    int arg;
    const char **argv = NULL;
    poptContext optCon = NULL;
    spectag t = NULL;

    name = NULL;
    lang = RPMBUILD_DEFAULT_LANG;

    if ((rc = poptParseArgvString(spec->line, &argc, &argv))) {
	rpmlog(RPMLOG_ERR, _("line %d: Error parsing %%description: %s\n"),
		 spec->lineNum, poptStrerror(rc));
	return RPMLOG_ERR;
    }

    optCon = poptGetContext(NULL, argc, argv, optionsTable, 0);
    while ((arg = poptGetNextOpt(optCon)) > 0) {
	if (arg == 'n') {
	    flag = PART_NAME;
	}
    }

    if (arg < -1) {
	rpmlog(RPMLOG_ERR, _("line %d: Bad option %s: %s\n"),
		 spec->lineNum,
		 poptBadOption(optCon, POPT_BADOPTION_NOALIAS), 
		 spec->line);
	goto exit;
    }

    if (poptPeekArg(optCon)) {
	if (name == NULL)
	    name = poptGetArg(optCon);
	if (poptPeekArg(optCon)) {
	    rpmlog(RPMLOG_ERR, _("line %d: Too many names: %s\n"),
		     spec->lineNum,
		     spec->line);
	    goto exit;
	}
    }

    if (lookupPackage(spec, name, flag, &pkg)) {
	rpmlog(RPMLOG_ERR, _("line %d: Package does not exist: %s\n"),
		 spec->lineNum, spec->line);
	goto exit;
    }


    /******************/

#if 0    
    if (headerIsEntry(pkg->header, RPMTAG_DESCRIPTION)) {
	rpmlog(RPMLOG_ERR, _("line %d: Second description\n"),
		spec->lineNum);
	goto exit;
    }
#endif

    t = stashSt(spec, pkg->header, RPMTAG_DESCRIPTION, lang);
    
    sb = newStringBuf();

    if ((rc = readLine(spec, STRIP_TRAILINGSPACE | STRIP_COMMENTS)) > 0) {
	nextPart = PART_NONE;
    } else {
	if (rc) {
	    nextPart = RPMLOG_ERR;
	    goto exit;
	}
	while (! (nextPart = isPart(spec->line))) {
	    appendLineStringBuf(sb, spec->line);
	    if (t) t->t_nlines++;
	    if ((rc =
		readLine(spec, STRIP_TRAILINGSPACE | STRIP_COMMENTS)) > 0) {
		nextPart = PART_NONE;
		break;
	    }
	    if (rc) {
		nextPart = RPMLOG_ERR;
		goto exit;
	    }
	}
    }
    
    stripTrailingBlanksStringBuf(sb);
    if (!(noLang && strcmp(lang, RPMBUILD_DEFAULT_LANG))) {
	(void) headerAddI18NString(pkg->header, RPMTAG_DESCRIPTION,
			getStringBuf(sb), lang);
    }
    
    sb = freeStringBuf(sb);
     
exit:
    argv = _free(argv);
    optCon = poptFreeContext(optCon);
    return nextPart;
}
