#include <stdarg.h>
#include <stdio.h>

#include "messages.h"
#include "rpmlib.h"

static minLevel = RPMMESS_NORMAL;

void rpmIncreaseVerbosity(void) {
    minLevel--;
}

void rpmSetVerbosity(int level) {
    minLevel = level;
}

int rpmGetVerbosity(void)
{
   return minLevel;
}

int rpmIsDebug(void)
{
    return (minLevel <= RPMMESS_DEBUG);
}

int rpmIsVerbose(void)
{
    return (minLevel <= RPMMESS_VERBOSE);
}

void rpmMessage(int level, char * format, ...) {
    va_list args;

    va_start(args, format);
    if (level >= minLevel) {
	switch (level) {
	  case RPMMESS_VERBOSE:
	  case RPMMESS_NORMAL:
	    vfprintf(stdout, format, args);
	    fflush(stdout);
	    break;
	    
	  case RPMMESS_DEBUG:
	    fprintf(stdout, "D: ");
	    vfprintf(stdout, format, args);
	    fflush(stdout);
	    break;

	  case RPMMESS_WARNING:
	    fprintf(stderr, "warning: ");
	    vfprintf(stderr, format, args);
	    fflush(stderr);
	    break;

	  case RPMMESS_ERROR:
	    fprintf(stderr, "error: ");
	    vfprintf(stderr, format, args);
	    fflush(stderr);
	    break;

	  case RPMMESS_FATALERROR:
	    fprintf(stderr, "fatal error: ");
	    vfprintf(stderr, format, args);
	    fflush(stderr);
	    exit(1);
	    break;
	}
    }
}
