#include "read.h"
#include "part.h"
#include "rpmlib.h"

int parseBuildInstallClean(Spec spec, int parsePart)
{
    int nextPart;
    StringBuf *sbp = NULL;
    char *name = NULL;

    switch (parsePart) {
      case PART_BUILD:
	sbp = &(spec->build);
	name = "%build";
	break;
      case PART_INSTALL:
	sbp = &(spec->install);
	name = "%install";
	break;
      case PART_CLEAN:
	sbp = &(spec->clean);
	name = "%clean";
	break;
    }
    
    if (*sbp) {
	rpmError(RPMERR_BADSPEC, "line %d: second %s", spec->lineNum, name);
	return RPMERR_BADSPEC;
    }
    
    *sbp = newStringBuf();

    /* There are no options to %build, %install, or %clean */
    if (readLine(spec, STRIP_NOTHING) > 0) {
	return PART_NONE;
    }
    
    while (! (nextPart = isPart(spec->line))) {
	appendLineStringBuf(*sbp, spec->line);
	if (readLine(spec, STRIP_NOTHING) > 0) {
	    return PART_NONE;
	}
    }

    return nextPart;
}
