/**
 * \file system.h
 */

#ifndef	H_SYSTEM
#define	H_SYSTEM

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>

#include <sys/stat.h>
#include <stdio.h>

#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif

/* <unistd.h> should be included before any preprocessor test
   of _POSIX_VERSION.  */
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#if !defined(__GLIBC__)
#ifdef __APPLE__
#include <crt_externs.h>
#define environ (*_NSGetEnviron())
#else
extern char ** environ;
#endif /* __APPLE__ */
#endif
#endif

#ifdef HAVE_STRING_H
# if !STDC_HEADERS && HAVE_MEMORY_H
#  include <memory.h>
# endif
# include <string.h>
#else
# include <strings.h>
#endif

#if !defined(HAVE_STPCPY)
char * stpcpy(char * dest, const char * src);
#endif

#if !defined(HAVE_STPNCPY)
char * stpncpy(char * dest, const char * src, size_t n);
#endif

#include <errno.h>
#ifndef errno
extern int errno;
#endif

#if HAVE___SECURE_GETENV
#define	getenv(_s)	__secure_getenv(_s)
#endif

#ifdef STDC_HEADERS
/* FIX: shrug */
#define getopt system_getopt
#include <stdlib.h>
#undef getopt
#else /* not STDC_HEADERS */
char *getenv (const char *name);
#endif /* STDC_HEADERS */

/* XXX solaris2.5.1 has not */
#if !defined(EXIT_FAILURE)
#define	EXIT_FAILURE	1
#endif

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#else
#include <sys/file.h>
#endif

#ifdef HAVE_DIRENT_H
# include <dirent.h>
# define NLENGTH(direct) (strlen((direct)->d_name))
#else /* not HAVE_DIRENT_H */
# define dirent direct
# define NLENGTH(direct) ((direct)->d_namlen)
# ifdef HAVE_SYS_NDIR_H
#  include <sys/ndir.h>
# endif /* HAVE_SYS_NDIR_H */
# ifdef HAVE_SYS_DIR_H
#  include <sys/dir.h>
# endif /* HAVE_SYS_DIR_H */
# ifdef HAVE_NDIR_H
#  include <ndir.h>
# endif /* HAVE_NDIR_H */
#endif /* HAVE_DIRENT_H */

#include <ctype.h>

#if HAVE_LIMITS_H
#include <limits.h>
#endif

#ifndef PATH_MAX
#ifdef _POSIX_PATH_MAX
#define PATH_MAX _POSIX_PATH_MAX
#elif defined MAXPATHLEN
#define PATH_MAX MAXPATHLEN
#else
#define PATH_MAX 256
#endif
#endif

#if WITH_SELINUX
#include <selinux/selinux.h>
#else
typedef	char * security_context_t;

#define	freecon(_c)

#define	setfilecon(_fn, _c)	(-1)
#define	lsetfilecon(_fn, _c)	(-1)

#define	is_selinux_enabled()	(0)

#define matchpathcon_init(_fn)			(-1)
#define matchpathcon_fini()			(0)
#define matchpathcon(_fn, _fm, _c)		(-1)

#define rpm_execcon(_v, _fn, _av, _envp)	(0)
#endif

#if WITH_CAP
#include <sys/capability.h>
#else
typedef void * cap_t;
#endif

#include "rpmio/rpmutil.h"
/* compatibility macros to avoid a mass-renaming all over the codebase */
#define xmalloc(_size) rmalloc((_size))
#define xcalloc(_nmemb, _size) rcalloc((_nmemb), (_size))
#define xrealloc(_ptr, _size) rrealloc((_ptr), (_size))
#define xstrdup(_str) rstrdup((_str))
#define _free(_ptr) rfree((_ptr))

/* Retrofit glibc __progname */
#if defined __GLIBC__ && __GLIBC__ >= 2
#if __GLIBC_MINOR__ >= 1
#define	__progname	__assert_program_name
#endif
#define	setprogname(pn)
#else
#define	__progname	program_name
#define	setprogname(pn)	\
  { if ((__progname = strrchr(pn, '/')) != NULL) __progname++; \
    else __progname = pn;		\
  }
#endif
extern const char *__progname;

/* Take care of NLS matters.  */

#if HAVE_LOCALE_H
# include <locale.h>
#endif
#if !HAVE_SETLOCALE
# define setlocale(Category, Locale) /* empty */
#endif

#if ENABLE_NLS
# include <libintl.h>
# define _(Text) dgettext (PACKAGE, Text)
#else
# undef bindtextdomain
# define bindtextdomain(Domain, Directory) /* empty */
# undef textdomain
# define textdomain(Domain) /* empty */
# define _(Text) Text
# undef dgettext
# define dgettext(DomainName, Text) Text
#endif

#define N_(Text) Text

/* ============== from misc/miscfn.h */

#if !defined(USE_GNU_GLOB) 
#if HAVE_FNMATCH_H
#include <fnmatch.h>
#endif

#if HAVE_GLOB_H 
#include <glob.h>
#endif
#else
#include "misc/glob.h"
#include "misc/fnmatch.h"
#endif

#if NEED_STRINGS_H
#include <strings.h>
#endif

#if HAVE_GETMNTINFO || HAVE_GETMNTINFO_R || HAVE_MNTCTL
# define GETMNTENT_ONE 0
# define GETMNTENT_TWO 0
# if HAVE_SYS_MNTCTL_H
#  include <sys/mntctl.h>
# endif
# if HAVE_SYS_VMOUNT_H
#  include <sys/vmount.h>
# endif
# if HAVE_SYS_MOUNT_H
#  include <sys/mount.h>
# endif
#elif HAVE_MNTENT_H || !(HAVE_GETMNTENT) || HAVE_STRUCT_MNTTAB
# if HAVE_MNTENT_H
#  include <stdio.h>
#  include <mntent.h>
#  define our_mntent struct mntent
#  define our_mntdir mnt_dir
# elif HAVE_STRUCT_MNTTAB
#  include <stdio.h>
#  include <mnttab.h>
   struct our_mntent {
       char * our_mntdir;
   };
   struct our_mntent *getmntent(FILE *filep);
#  define our_mntent struct our_mntent
# else
#  include <stdio.h>
   struct our_mntent {
       char * our_mntdir;
   };
   struct our_mntent *getmntent(FILE *filep);
#  define our_mntent struct our_mntent
# endif
# define GETMNTENT_ONE 1
# define GETMNTENT_TWO 0
#elif HAVE_SYS_MNTTAB_H
# include <stdio.h>
# include <sys/mnttab.h>
# define GETMNTENT_ONE 0
# define GETMNTENT_TWO 1
# define our_mntent struct mnttab
# define our_mntdir mnt_mountp
#else /* if !HAVE_MNTCTL */
# error Neither mntent.h, mnttab.h, or mntctl() exists. I cannot build on this system.
#endif

#ifndef MOUNTED
#define MOUNTED "/etc/mnttab"
#endif

#endif	/* H_SYSTEM */
