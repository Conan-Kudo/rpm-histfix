/* RPM - Copyright (C) 1995 Red Hat Software
 * 
 * build.c - routines for preparing and building the sources
 */

#include "miscfn.h"

#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <limits.h>

#include "build.h"
#include "files.h"
#include "header.h"
#include "spec.h"
#include "specP.h"
#include "rpmlib.h"
#include "messages.h"
#include "stringbuf.h"
#include "misc.h"
#include "pack.h"
#include "popt.h"

#include "names.h"

struct Script {
    char *name;
    FILE *file;
};

struct Script *openScript(Spec spec, int builddir, char *name);
void writeScript(struct Script *script, char *s);
int execScript(struct Script *script);
void freeScript(struct Script *script, int test);
int execPart(Spec s, char *sb, char *name, int builddir, int test);
static int doSetupMacro(Spec spec, StringBuf sb, char *line);
static int doPatchMacro(Spec spec, StringBuf sb, char *line);
static char *do_untar(Spec spec, int c, int quietly);
static char *do_patch(Spec spec, int c, int strip, char *dashb,
		      int reverse, int removeEmpties);
int isCompressed(char *file);
static void doSweep(Spec s);
static int doRmSource(Spec s);

char build_subdir[1024];

struct Script *openScript(Spec spec, int builddir, char *name)
{
    struct Script *script = malloc(sizeof(struct Script));
    struct PackageRec *main_package = spec->packages;
    char *s, * arch, * os;
    int fd;
    int_32 foo;

    rpmGetArchInfo(&arch, NULL);
    rpmGetOsInfo(&os, NULL);

    if (! main_package) {
	rpmError(RPMERR_INTERNAL, "Empty main package");
	exit(RPMERR_INTERNAL);
    }
    
    if (makeTempFile(NULL, &script->name, &fd))
	exit(1);
    script->file = fdopen(fd, "w");

    /* Prepare the script */
    fprintf(script->file,
	    "# Script generated by rpm\n\n");

    fprintf(script->file, "RPM_SOURCE_DIR=\"%s\"\n", rpmGetVar(RPMVAR_SOURCEDIR));
    fprintf(script->file, "RPM_BUILD_DIR=\"%s\"\n", rpmGetVar(RPMVAR_BUILDDIR));
    fprintf(script->file, "RPM_DOC_DIR=\"%s\"\n", rpmGetVar(RPMVAR_DEFAULTDOCDIR));
    fprintf(script->file, "RPM_OPT_FLAGS=\"%s\"\n", rpmGetVar(RPMVAR_OPTFLAGS));
    fprintf(script->file, "RPM_ARCH=\"%s\"\n", arch);
    fprintf(script->file, "RPM_OS=\"%s\"\n", os);
    if (rpmGetVar(RPMVAR_ROOT)) {
	fprintf(script->file, "RPM_ROOT_DIR=\"%s\"\n", rpmGetVar(RPMVAR_ROOT));
    } else {
	fprintf(script->file, "RPM_ROOT_DIR=\"\"\n");
    }
    if (rpmGetVar(RPMVAR_BUILDROOT)) {
	fprintf(script->file, "RPM_BUILD_ROOT=\"%s\"\n",
		rpmGetVar(RPMVAR_BUILDROOT));
    } else {
	fprintf(script->file, "RPM_BUILD_ROOT=\"\"\n");
    }

    fprintf(script->file, "RPM_PACKAGE_NAME=\"%s\"\n", spec->name);
    headerGetEntry(main_package->header, RPMTAG_VERSION, &foo, (void **)&s, &foo);
    fprintf(script->file, "RPM_PACKAGE_VERSION=\"%s\"\n", s);
    headerGetEntry(main_package->header, RPMTAG_RELEASE, &foo, (void **)&s, &foo);
    fprintf(script->file, "RPM_PACKAGE_RELEASE=\"%s\"\n", s);

    if (rpmIsVerbose()) {
	fprintf(script->file, "set -x\n\n");
    } else {
	fprintf(script->file, "exec > /dev/null\n\n");
    }

    /* Set the umask to a known value */
    fprintf(script->file, "umask 022\n");

    fprintf(script->file, "\necho Executing: %s\n", name);
    fprintf(script->file, "cd %s\n\n", rpmGetVar(RPMVAR_BUILDDIR));
    if (builddir) {
	/* Need to cd to the actual build directory. */
	/* Note that this means we have to parse the */
	/* %prep section even if we aren't using it. */
	fprintf(script->file, "cd %s\n\n", build_subdir);
    }

    return script;
}

void writeScript(struct Script *script, char *s)
{
    fprintf(script->file, "%s", s);
}

int execScript(struct Script *script)
{
    int pid;
    int status;
    
    writeScript(script, "\nexit 0;\n");
    fclose(script->file);
    script->file = NULL;
    chmod(script->name, 0600);

    if (!(pid = fork())) {
	execl("/bin/sh", "/bin/sh", "-e", script->name, script->name, NULL);
	rpmError(RPMERR_SCRIPT, "Exec failed");
	_exit(RPMERR_SCRIPT);
    }
    wait(&status);
    if (! WIFEXITED(status) || WEXITSTATUS(status)) {
	rpmError(RPMERR_SCRIPT, "Bad exit status");
	exit(RPMERR_SCRIPT);
    }
    return 0;
}

void freeScript(struct Script *script, int test)
{
    if (script->file)
	fclose(script->file);
    if (! test)
	unlink(script->name);
    free(script->name);
    free(script);
}

int execPart(Spec s, char *sb, char *name, int builddir, int test)
{
    struct Script *script;

    rpmMessage(RPMMESS_DEBUG, "RUNNING: %s\n", name);
    script = openScript(s, builddir, name);
    writeScript(script, sb);
    if (!test) {
	execScript(script);
    }
    freeScript(script, test);
    return 0;
}

static void doSweep(Spec s)
{
    char buf[1024];

    if (strcmp(build_subdir, ".")) {
        struct Script *script;
        script = openScript(s, 0, "sweep");
        sprintf(buf, "rm -rf %s\n", build_subdir);
        writeScript(script, buf);
        execScript(script);
        freeScript(script, 0);
    }
}

static int doRmSource(Spec s)
{
    char filename[1024];
    struct sources *source;
    struct PackageRec *package;

    /* spec file */
    sprintf(filename, "%s%s", rpmGetVar(RPMVAR_SPECDIR),
	    strrchr(s->specfile, '/'));
    unlink(filename);

    /* sources and patches */
    source = s->sources;
    while (source) {
	sprintf(filename, "%s/%s", rpmGetVar(RPMVAR_SOURCEDIR), source->source);
	unlink(filename);
	source = source->next;
    }

    /* icons */
    package = s->packages;
    while (package) {
	if (package->icon) {
	    sprintf(filename, "%s/%s", rpmGetVar(RPMVAR_SOURCEDIR),
		    package->icon);
	    unlink(filename);
	}
	package = package->next;
    }
    
    return 0;
}

static int doSetupMacro(Spec spec, StringBuf sb, char *line)
{
    char *version;
    int leaveDirs = 0, skipDefaultAction = 0;
    int createDir = 0, quietly = 0;
    char * dirName = NULL;
    char buf[1024];
    StringBuf before;
    StringBuf after;
    poptContext optCon;
    int argc;
    char ** argv;
    int arg;
    char * optArg;
    char * chptr;
    int rc;
    int num;
    struct poptOption optionsTable[] = {
	    { NULL, 'a', POPT_ARG_STRING, NULL, 'a' },
	    { NULL, 'b', POPT_ARG_STRING, NULL, 'b' },
	    { NULL, 'c', 0, &createDir, 0 },
	    { NULL, 'D', 0, &leaveDirs, 0 },
	    { NULL, 'n', POPT_ARG_STRING, &dirName, 0 },
	    { NULL, 'T', 0, &skipDefaultAction, 0 },
	    { NULL, 'q', 0, &quietly, 0 },
    };

    if ((rc = poptParseArgvString(line, &argc, &argv))) {
	rpmError(RPMERR_BADSPEC, "Error parsing %%setup: %s",
			poptStrerror(rc));
	return RPMERR_BADSPEC;
    }

    before = newStringBuf();
    after = newStringBuf();

    optCon = poptGetContext(NULL, argc, argv, optionsTable, 0);
    while ((arg = poptGetNextOpt(optCon)) > 0) {
	optArg = poptGetOptArg(optCon);

	/* We only parse -a and -b here */

	num = strtoul(optArg, &chptr, 10);
	if ((*chptr) || (chptr == optArg) || (num == ULONG_MAX)) {
	    rpmError(RPMERR_BADSPEC, "Bad arg to %%setup %c: %s", num, optArg);
	    free(argv);
	    freeStringBuf(before);
	    freeStringBuf(after);
	    poptFreeContext(optCon);
	    return(RPMERR_BADSPEC);
	}

	chptr = do_untar(spec, num, quietly);
	if (!chptr) return 1;

	if (arg == 'a')
	    appendLineStringBuf(after, chptr);
	else
	    appendLineStringBuf(before, chptr);
    }

    if (arg < -1) {
	rpmError(RPMERR_BADSPEC, "Bad %%setup option %s: %s",
		poptBadOption(optCon, POPT_BADOPTION_NOALIAS), 
		poptStrerror(arg));
	free(argv);
	freeStringBuf(before);
	freeStringBuf(after);
	poptFreeContext(optCon);
	return(RPMERR_BADSPEC);
    }

    if (dirName) {
	strcpy(build_subdir, dirName);
    } else {
	strcpy(build_subdir, spec->name);
	strcat(build_subdir, "-");
	/* We should already have a version field */
	headerGetEntry(spec->packages->header, RPMTAG_VERSION, NULL,
		 (void *) &version, NULL);
	strcat(build_subdir, version);
    }
    
    free(argv);
    poptFreeContext(optCon);

    /* cd to the build dir */
    sprintf(buf, "cd %s", rpmGetVar(RPMVAR_BUILDDIR));
    appendLineStringBuf(sb, buf);
    
    /* delete any old sources */
    if (!leaveDirs) {
	sprintf(buf, "rm -rf %s", build_subdir);
	appendLineStringBuf(sb, buf);
    }

    /* if necessary, create and cd into the proper dir */
    if (createDir) {
	sprintf(buf, "mkdir -p %s\ncd %s", build_subdir, build_subdir);
	appendLineStringBuf(sb, buf);
    }

    /* do the default action */
    if (!createDir && !skipDefaultAction) {
	chptr = do_untar(spec, 0, quietly);
	if (!chptr) return 1;
	appendLineStringBuf(sb, chptr);
    }

    appendStringBuf(sb, getStringBuf(before));
    freeStringBuf(before);

    if (!createDir) {
	sprintf(buf, "cd %s", build_subdir);
	appendLineStringBuf(sb, buf);
    }

    if (createDir && !skipDefaultAction) {
	chptr = do_untar(spec, 0, quietly);
	if (!chptr) return 1;
	appendLineStringBuf(sb, chptr);
    }
    
    appendStringBuf(sb, getStringBuf(after));
    freeStringBuf(after);

    /* clean up permissions etc */
    if (!geteuid()) {
	appendLineStringBuf(sb, "chown -R root .");
	appendLineStringBuf(sb, "chgrp -R root .");
    }

    if (rpmGetVar(RPMVAR_FIXPERMS)) {
	appendStringBuf(sb, "chmod -R ");
	appendStringBuf(sb, rpmGetVar(RPMVAR_FIXPERMS));
	appendLineStringBuf(sb, " .");
    }
    
    return 0;
}

int isCompressed(char *file)
{
    int fd;
    unsigned char magic[4];

    fd = open(file, O_RDONLY);
    read(fd, magic, 4);
    close(fd);

    if (((magic[0] == 0037) && (magic[1] == 0213)) ||  /* gzip */
	((magic[0] == 0037) && (magic[1] == 0236)) ||  /* old gzip */
	((magic[0] == 0037) && (magic[1] == 0036)) ||  /* pack */
	((magic[0] == 0037) && (magic[1] == 0240)) ||  /* SCO lzh */
	((magic[0] == 0037) && (magic[1] == 0235)) ||  /* compress */
	((magic[0] == 0120) && (magic[1] == 0113) &&
	 (magic[2] == 0003) && (magic[3] == 0004))     /* pkzip */
	) {
	return 1;
    }

    return 0;
}

static char *do_untar(Spec spec, int c, int quietly)
{
    static char buf[1024];
    char file[1024];
    char *s, *taropts;
    struct sources *sp;

    s = NULL;
    sp = spec->sources;
    while (sp) {
	if ((sp->ispatch == 0) && (sp->num == c)) {
	    s = sp->source;
	    break;
	}
	sp = sp->next;
    }
    if (! s) {
	rpmError(RPMERR_BADSPEC, "No source number %d", c);
	return NULL;
    }

    sprintf(file, "%s/%s", rpmGetVar(RPMVAR_SOURCEDIR), s);
    taropts = (rpmIsVerbose() && !quietly ? "-xvvf" : "-xf");
    
    if (isCompressed(file)) {
	sprintf(buf,
		"%s -dc %s | tar %s -\n"
		"if [ $? -ne 0 ]; then\n"
		"  exit $?\n"
		"fi",
		rpmGetVar(RPMVAR_GZIPBIN), file, taropts);
    } else {
	sprintf(buf, "tar %s %s", taropts, file);
    }

    return buf;
}

static char *do_patch(Spec spec, int c, int strip, char *db,
		      int reverse, int removeEmpties)
{
    static char buf[1024];
    char file[1024];
    char args[1024];
    char *s;
    struct sources *sp;

    s = NULL;
    sp = spec->sources;
    while (sp) {
	if ((sp->ispatch == 1) && (sp->num == c)) {
	    s = sp->source;
	    break;
	}
	sp = sp->next;
    }
    if (! s) {
	rpmError(RPMERR_BADSPEC, "No patch number %d", c);
	return NULL;
    }

    sprintf(file, "%s/%s", rpmGetVar(RPMVAR_SOURCEDIR), s);

    args[0] = '\0';
    if (db) {
	strcat(args, "-b ");
	strcat(args, db);
    }
    if (reverse) {
	strcat(args, " -R");
    }
    if (removeEmpties) {
	strcat(args, " -E");
    }

    if (isCompressed(file)) {
	sprintf(buf,
		"echo \"Patch #%d:\"\n"
		"%s -dc %s | patch -p%d %s -s\n"
		"if [ $? -ne 0 ]; then\n"
		"  exit $?\n"
		"fi",
		c, rpmGetVar(RPMVAR_GZIPBIN), file, strip, args);
    } else {
	sprintf(buf,
		"echo \"Patch #%d:\"\n"
		"patch -p%d %s -s < %s", c, strip, args, file);
    }

    return buf;
}

static int doPatchMacro(Spec spec, StringBuf sb, char *line)
{
    char *opt_b;
    int opt_P, opt_p, opt_R, opt_E;
    char *s, *s1;
    char buf[1024];
    int patch_nums[1024];  /* XXX - we can only handle 1024 patches! */
    int patch_index, x;

    opt_P = opt_p = opt_R = opt_E = 0;
    opt_b = NULL;
    patch_index = 0;

    if (! strchr(" \t\n", line[6])) {
	/* %patchN */
	sprintf(buf, "%%patch -P %s", line + 6);
    } else {
	strcpy(buf, line);
    }
    
    strtok(buf, " \t\n");  /* remove %patch */
    while ((s = strtok(NULL, " \t\n"))) {
	if (!strcmp(s, "-P")) {
	    opt_P = 1;
	} else if (!strcmp(s, "-R")) {
	    opt_R = 1;
	} else if (!strcmp(s, "-E")) {
	    opt_E = 1;
	} else if (!strcmp(s, "-b")) {
	    /* orig suffix */
	    opt_b = strtok(NULL, " \t\n");
	    if (! opt_b) {
		rpmError(RPMERR_BADSPEC, "Need arg to %%patch -b");
		return(RPMERR_BADSPEC);
	    }
	} else if (!strncmp(s, "-p", 2)) {
	    /* unfortunately, we must support -pX */
	    if (! strchr(" \t\n", s[2])) {
		s = s + 2;
	    } else {
		s = strtok(NULL, " \t\n");
		if (! s) {
		    rpmError(RPMERR_BADSPEC, "Need arg to %%patch -p");
		    return(RPMERR_BADSPEC);
		}
	    }
	    s1 = NULL;
	    opt_p = strtoul(s, &s1, 10);
	    if ((*s1) || (s1 == s) || (opt_p == ULONG_MAX)) {
		rpmError(RPMERR_BADSPEC, "Bad arg to %%patch -p: %s", s);
		return(RPMERR_BADSPEC);
	    }
	} else {
	    /* Must be a patch num */
	    if (patch_index == 1024) {
		rpmError(RPMERR_BADSPEC, "Too many patches!");
		return(RPMERR_BADSPEC);
	    }
	    s1 = NULL;
	    patch_nums[patch_index] = strtoul(s, &s1, 10);
	    if ((*s1) || (s1 == s) || (patch_nums[patch_index] == ULONG_MAX)) {
		rpmError(RPMERR_BADSPEC, "Bad arg to %%patch: %s", s);
		return(RPMERR_BADSPEC);
	    }
	    patch_index++;
	}
    }

    /* All args processed */

    if (! opt_P) {
	s = do_patch(spec, 0, opt_p, opt_b, opt_R, opt_E);
	if (! s) {
	    return 1;
	}
	appendLineStringBuf(sb, s);
    }

    x = 0;
    while (x < patch_index) {
	s = do_patch(spec, patch_nums[x], opt_p, opt_b, opt_R, opt_E);
	if (! s) {
	    return 1;
	}
	appendLineStringBuf(sb, s);
	x++;
    }
    
    return 0;
}

static int checkSources(Spec s)
{
    struct sources *source;
    struct PackageRec *package;
    char buf[1024];

    /* Check that we can access all the sources */
    source = s->sources;
    while (source) {
	sprintf(buf, "%s/%s", rpmGetVar(RPMVAR_SOURCEDIR), source->source);
	if (access(buf, R_OK)) {
	    rpmError(RPMERR_BADSPEC, "missing source or patch: %s", buf);
	    return RPMERR_BADSPEC;
	}
	source = source->next;
    }

    /* ... and icons */
    package = s->packages;
    while (package) {
	if (package->icon) {
	    sprintf(buf, "%s/%s", rpmGetVar(RPMVAR_SOURCEDIR), package->icon);
	    if (access(buf, R_OK)) {
		rpmError(RPMERR_BADSPEC, "missing icon: %s", buf);
		return RPMERR_BADSPEC;
	    }
	}
	package = package->next;
    }
    
    return 0;
}

int execPrep(Spec s, int really_exec, int test)
{
    char **lines, **lines1, *p;
    StringBuf out;
    int res;

    if (checkSources(s)) {
	return 1;
    }
    out = newStringBuf();
    
    p = getStringBuf(s->prep);
    lines = splitString(p, strlen(p), '\n');
    lines1 = lines;
    while (*lines) {
	if (! strncmp(*lines, "%setup", 6)) {
	    if (doSetupMacro(s, out, *lines)) {
		return 1;
	    }
	} else if (! strncmp(*lines, "%patch", 6)) {
	    if (doPatchMacro(s, out, *lines)) {
		return 1;
	    }
	} else {
	    appendLineStringBuf(out, *lines);
	}
	lines++;
    }

    freeSplitString(lines1);
    res = 0;
    if (really_exec) {
	res = execPart(s, getStringBuf(out), "%prep", 0, test);
    }
    freeStringBuf(out);
    return res;
}

int execBuild(Spec s, int test)
{
    return execPart(s, getStringBuf(s->build), "%build", 1, test);
}

int execInstall(Spec s, int test)
{
    int res;

    if ((res = execPart(s, getStringBuf(s->install), "%install", 1, test))) {
	return res;
    }
    if ((res = finish_filelists(s))) {
	return res;
    }
    return execPart(s, getStringBuf(s->doc), "special doc", 1, test);
}

int execClean(Spec s)
{
    return execPart(s, getStringBuf(s->clean), "%clean", 1, 0);
}

int verifyList(Spec s)
{
    int res;

    if ((res = finish_filelists(s))) {
	return res;
    }
    return packageBinaries(s, NULL, PACK_NOPACKAGE);
}

int doBuild(Spec s, int flags, char *passPhrase)
{
    int test;

    test = flags & RPMBUILD_TEST;

    strcpy(build_subdir, ".");

    if (s->buildArch) {
	rpmSetMachine(s->buildArch, NULL);
    }

    /* We always need to parse the %prep section */
    if (execPrep(s, (flags & RPMBUILD_PREP), test)) {
	return 1;
    }

    if (flags & RPMBUILD_LIST)
	return verifyList(s);

    if (flags & RPMBUILD_BUILD) {
	if (execBuild(s, test)) {
	    return 1;
	}
    }

    if (flags & RPMBUILD_INSTALL) {
	if (execInstall(s, test)) {
	    return 1;
	}
    }

    if (test) {
	return 0;
    }
    
    markBuildTime();
    
    if (flags & RPMBUILD_BINARY) {
	if (packageBinaries(s, passPhrase, PACK_PACKAGE)) {
	    return 1;
	}
	if (execClean(s)) {
	    return 1;
	}
    }

    if (flags & RPMBUILD_SOURCE) {
	if (packageSource(s, passPhrase)) {
	    return 1;
	}
    }

    if (flags & RPMBUILD_SWEEP) {
	doSweep(s);
    }

    if (flags & RPMBUILD_RMSOURCE) {
	doRmSource(s);
    }

    return 0;
}
