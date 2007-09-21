/** \ingroup rpmio
 * \file rpmio/rpmio.c
 */

#include "system.h"
#include <stdarg.h>

#if HAVE_MACHINE_TYPES_H
# include <machine/types.h>
#endif

#if HAVE_LIBIO_H && defined(_G_IO_IO_FILE_VERSION)
#define	_USE_LIBIO	1
#endif

/* XXX HP-UX w/o -D_XOPEN_SOURCE needs */
#if !defined(HAVE_HERRNO) && (defined(__hpux))
extern int h_errno;
#endif

#ifndef IPPORT_FTP
#define IPPORT_FTP	21
#endif
#ifndef	IPPORT_HTTP
#define	IPPORT_HTTP	80
#endif

#include <rpmio_internal.h>
#undef	fdFileno

#include "ugid.h"
#include "rpmmessages.h"
#include "argv.h"
#include "rpmerr.h"
#include "rpmmacro.h"

#include "debug.h"

#define FDNREFS(fd)	(fd ? ((FD_t)fd)->nrefs : -9)
#define FDTO(fd)	(fd ? ((FD_t)fd)->rd_timeoutsecs : -99)
#define FDCPIOPOS(fd)	(fd ? ((FD_t)fd)->fd_cpioPos : -99)

#define	FDONLY(fd)	assert(fdGetIo(fd) == fdio)
#define	GZDONLY(fd)	assert(fdGetIo(fd) == gzdio)
#define	BZDONLY(fd)	assert(fdGetIo(fd) == bzdio)

#define	UFDONLY(fd)	/* assert(fdGetIo(fd) == ufdio) */

#define	fdGetFILE(_fd)	((FILE *)fdGetFp(_fd))

/**
 */
#if _USE_LIBIO
int noLibio = 0;
#else
int noLibio = 1;
#endif

#define TIMEOUT_SECS 60

/**
 */
int _rpmio_debug = 0;

/* =============================================================== */

static const char * fdbg(FD_t fd)
{
    static char buf[BUFSIZ];
    char *be = buf;
    int i;

    buf[0] = '\0';
    if (fd == NULL)
	return buf;

#ifdef DYING
    sprintf(be, "fd %p", fd);	be += strlen(be);
    if (fd->rd_timeoutsecs >= 0) {
	sprintf(be, " secs %d", fd->rd_timeoutsecs);
	be += strlen(be);
    }
#endif
    if (fd->bytesRemain != -1) {
	sprintf(be, " clen %d", (int)fd->bytesRemain);
	be += strlen(be);
     }
    if (fd->wr_chunked) {
	strcpy(be, " chunked");
	be += strlen(be);
     }
    *be++ = '\t';
    for (i = fd->nfps; i >= 0; i--) {
	FDSTACK_t * fps = &fd->fps[i];
	if (i != fd->nfps)
	    *be++ = ' ';
	*be++ = '|';
	*be++ = ' ';
	if (fps->io == fdio) {
	    sprintf(be, "FD %d fp %p", fps->fdno, fps->fp);
	} else if (fps->io == ufdio) {
	    sprintf(be, "UFD %d fp %p", fps->fdno, fps->fp);
#if HAVE_ZLIB_H
	} else if (fps->io == gzdio) {
	    sprintf(be, "GZD %p fdno %d", fps->fp, fps->fdno);
#endif
#if HAVE_BZLIB_H
	} else if (fps->io == bzdio) {
	    sprintf(be, "BZD %p fdno %d", fps->fp, fps->fdno);
#endif
	} else if (fps->io == fpio) {
	    sprintf(be, "%s %p(%d) fdno %d",
		(fps->fdno < 0 ? "LIBIO" : "FP"),
		fps->fp, fileno(((FILE *)fps->fp)), fps->fdno);
	} else {
	    sprintf(be, "??? io %p fp %p fdno %d ???",
		fps->io, fps->fp, fps->fdno);
	}
	be += strlen(be);
	*be = '\0';
    }
    return buf;
}

/* =============================================================== */
off_t fdSize(FD_t fd)
{
    struct stat sb;
    off_t rc = -1; 

#ifdef	NOISY
DBGIO(0, (stderr, "==>\tfdSize(%p) rc %ld\n", fd, (long)rc));
#endif
    FDSANE(fd);
    if (fd->contentLength >= 0)
	rc = fd->contentLength;
    else switch (fd->urlType) {
    case URL_IS_PATH:
    case URL_IS_UNKNOWN:
	if (fstat(Fileno(fd), &sb) == 0)
	    rc = sb.st_size;
    case URL_IS_HTTPS:
    case URL_IS_HTTP:
    case URL_IS_HKP:
    case URL_IS_FTP:
    case URL_IS_DASH:
	break;
    }
    return rc;
}

FD_t fdDup(int fdno)
{
    FD_t fd;
    int nfdno;

    if ((nfdno = dup(fdno)) < 0)
	return NULL;
    fd = fdNew("open (fdDup)");
    fdSetFdno(fd, nfdno);
DBGIO(fd, (stderr, "==> fdDup(%d) fd %p %s\n", fdno, (fd ? fd : NULL), fdbg(fd)));
    return fd;
}

static inline int fdSeekNot(void * cookie,
		_libio_pos_t pos,  int whence)
{
    FD_t fd = c2f(cookie);
    FDSANE(fd);		/* XXX keep gcc quiet */
    return -2;
}

#ifdef UNUSED
FILE *fdFdopen(void * cookie, const char *fmode)
{
    FD_t fd = c2f(cookie);
    int fdno;
    FILE * fp;

    if (fmode == NULL) return NULL;
    fdno = fdFileno(fd);
    if (fdno < 0) return NULL;
    fp = fdopen(fdno, fmode);
DBGIO(fd, (stderr, "==> fdFdopen(%p,\"%s\") fdno %d -> fp %p fdno %d\n", cookie, fmode, fdno, fp, fileno(fp)));
    fd = fdFree(fd, "open (fdFdopen)");
    return fp;
}
#endif

/* =============================================================== */
/* FIX: cookie is modified */
static inline FD_t XfdLink(void * cookie, const char * msg,
		const char * file, unsigned line)
{
    FD_t fd;
if (cookie == NULL)
DBGREFS(0, (stderr, "--> fd  %p ++ %d %s at %s:%u\n", cookie, FDNREFS(cookie)+1, msg, file, line));
    fd = c2f(cookie);
    if (fd) {
	fd->nrefs++;
DBGREFS(fd, (stderr, "--> fd  %p ++ %d %s at %s:%u %s\n", fd, fd->nrefs, msg, file, line, fdbg(fd)));
    }
    return fd;
}

static inline
FD_t XfdFree( FD_t fd, const char *msg,
		const char *file, unsigned line)
{
	int i;

if (fd == NULL)
DBGREFS(0, (stderr, "--> fd  %p -- %d %s at %s:%u\n", fd, FDNREFS(fd), msg, file, line));
    FDSANE(fd);
    if (fd) {
DBGREFS(fd, (stderr, "--> fd  %p -- %d %s at %s:%u %s\n", fd, fd->nrefs, msg, file, line, fdbg(fd)));
	if (--fd->nrefs > 0)
	    return fd;
	fd->stats = _free(fd->stats);
	for (i = fd->ndigests - 1; i >= 0; i--) {
	    FDDIGEST_t fddig = fd->digests + i;
	    if (fddig->hashctx == NULL)
		continue;
	    (void) rpmDigestFinal(fddig->hashctx, NULL, NULL, 0);
	    fddig->hashctx = NULL;
	}
	fd->ndigests = 0;
	free(fd);
    }
    return NULL;
}

static inline
FD_t XfdNew(const char * msg, const char * file, unsigned line)
{
    FD_t fd = xcalloc(1, sizeof(*fd));
    if (fd == NULL) /* XXX xmalloc never returns NULL */
	return NULL;
    fd->nrefs = 0;
    fd->flags = 0;
    fd->magic = FDMAGIC;
    fd->urlType = URL_IS_UNKNOWN;

    fd->nfps = 0;
    memset(fd->fps, 0, sizeof(fd->fps));

    fd->fps[0].io = fdio;
    fd->fps[0].fp = NULL;
    fd->fps[0].fdno = -1;

    fd->rd_timeoutsecs = 1;	/* XXX default value used to be -1 */
    fd->contentLength = fd->bytesRemain = -1;
    fd->wr_chunked = 0;
    fd->syserrno = 0;
    fd->errcookie = NULL;
    fd->stats = xcalloc(1, sizeof(*fd->stats));

    fd->ndigests = 0;
    memset(fd->digests, 0, sizeof(fd->digests));

    fd->firstFree = 0;
    fd->fileSize = 0;
    fd->fd_cpioPos = 0;

    return XfdLink(fd, msg, file, line);
}

/**
 */
ssize_t fdRead(void * cookie, char * buf, size_t count)
{
    return fdio->read(cookie, buf, count);
}

/**
 */
static ssize_t __fdRead(void * cookie, char * buf, size_t count)
{
    FD_t fd = c2f(cookie);
    ssize_t rc;

    if (fd->bytesRemain == 0) return 0;	/* XXX simulate EOF */

    fdstat_enter(fd, FDSTAT_READ);
    rc = read(fdFileno(fd), buf, (count > fd->bytesRemain ? fd->bytesRemain : count));
    fdstat_exit(fd, FDSTAT_READ, rc);

    if (fd->ndigests && rc > 0) fdUpdateDigests(fd, (void *)buf, rc);

DBGIO(fd, (stderr, "==>\t__fdRead(%p,%p,%ld) rc %ld %s\n", cookie, buf, (long)count, (long)rc, fdbg(fd)));

    return rc;
}

/**
 */
ssize_t fdWrite(void * cookie, const char * buf, size_t count)
{
    return fdio->write(cookie, buf, count);
}

/**
 */
static ssize_t __fdWrite(void * cookie, const char * buf, size_t count)
{
    FD_t fd = c2f(cookie);
    int fdno = fdFileno(fd);
    ssize_t rc;

    if (fd->bytesRemain == 0) return 0;	/* XXX simulate EOF */

    if (fd->ndigests && count > 0) fdUpdateDigests(fd, (void *)buf, count);

    if (count == 0) return 0;

    fdstat_enter(fd, FDSTAT_WRITE);
    rc = write(fdno, buf, (count > fd->bytesRemain ? fd->bytesRemain : count));
    fdstat_exit(fd, FDSTAT_WRITE, rc);

DBGIO(fd, (stderr, "==>\t__fdWrite(%p,%p,%ld) rc %ld %s\n", cookie, buf, (long)count, (long)rc, fdbg(fd)));

    return rc;
}

static inline int fdSeek(void * cookie, _libio_pos_t pos, int whence)
{
#ifdef USE_COOKIE_SEEK_POINTER
    _IO_off64_t p = *pos;
#else
    off_t p = pos;
#endif
    FD_t fd = c2f(cookie);
    off_t rc;

    assert(fd->bytesRemain == -1);	/* XXX FIXME fadio only for now */
    fdstat_enter(fd, FDSTAT_SEEK);
    rc = lseek(fdFileno(fd), p, whence);
    fdstat_exit(fd, FDSTAT_SEEK, rc);

DBGIO(fd, (stderr, "==>\tfdSeek(%p,%ld,%d) rc %lx %s\n", cookie, (long)p, whence, (unsigned long)rc, fdbg(fd)));

    return rc;
}

/**
 */
int fdClose( void * cookie)
{
   return fdio->close(cookie);
}

/**
 */
static int __fdClose( void * cookie)
{
    FD_t fd;
    int fdno;
    int rc;

    if (cookie == NULL) return -2;
    fd = c2f(cookie);
    fdno = fdFileno(fd);

    fdSetFdno(fd, -1);

    fdstat_enter(fd, FDSTAT_CLOSE);
    rc = ((fdno >= 0) ? close(fdno) : -2);
    fdstat_exit(fd, FDSTAT_CLOSE, rc);

DBGIO(fd, (stderr, "==>\t__fdClose(%p) rc %lx %s\n", (fd ? fd : NULL), (unsigned long)rc, fdbg(fd)));

    fd = fdFree(fd, "open (__fdClose)");
    return rc;
}

/**
 */
FD_t fdOpen(const char *path, int flags, mode_t mode)
{
    return fdio->_open(path, flags, mode);
}

/**
 */
static FD_t __fdOpen(const char *path, int flags, mode_t mode)
{
    FD_t fd;
    int fdno;

    fdno = open(path, flags, mode);
    if (fdno < 0) return NULL;
    if (fcntl(fdno, F_SETFD, FD_CLOEXEC)) {
	(void) close(fdno);
	return NULL;
    }
    fd = fdNew("open (__fdOpen)");
    fdSetFdno(fd, fdno);
    fd->flags = flags;
DBGIO(fd, (stderr, "==>\t__fdOpen(\"%s\",%x,0%o) %s\n", path, (unsigned)flags, (unsigned)mode, fdbg(fd)));
    return fd;
}

static struct FDIO_s fdio_s = {
  __fdRead, __fdWrite, fdSeek, __fdClose, XfdLink, XfdFree, XfdNew, fdFileno,
  __fdOpen, NULL, fdGetFp, NULL,	mkdir, chdir, rmdir, rename, unlink
};
FDIO_t fdio = &fdio_s ;

int fdWritable(FD_t fd, int secs)
{
    int fdno;
    int rc;
#if HAVE_POLL_H
    int msecs = (secs >= 0 ? (1000 * secs) : -1);
    struct pollfd wrfds;
#else
    struct timeval timeout, *tvp = (secs >= 0 ? &timeout : NULL);
    fd_set wrfds;
    FD_ZERO(&wrfds);
#endif
	
    if ((fdno = fdFileno(fd)) < 0)
	return -1;	/* XXX W2DO? */
	
    do {
#if HAVE_POLL_H
	wrfds.fd = fdno;
	wrfds.events = POLLOUT;
	wrfds.revents = 0;
	rc = poll(&wrfds, 1, msecs);
#else
	if (tvp) {
	    tvp->tv_sec = secs;
	    tvp->tv_usec = 0;
	}
	FD_SET(fdno, &wrfds);
	rc = select(fdno + 1, NULL, &wrfds, NULL, tvp);
#endif

	/* HACK: EBADF on PUT chunked termination from ufdClose. */
if (_rpmio_debug && !(rc == 1 && errno == 0))
fprintf(stderr, "*** fdWritable fdno %d rc %d %s\n", fdno, rc, strerror(errno));
	if (rc < 0) {
	    switch (errno) {
	    case EINTR:
		continue;
		break;
	    default:
		return rc;
		break;
	    }
	}
	return rc;
    } while (1);
}

int fdReadable(FD_t fd, int secs)
{
    int fdno;
    int rc;
#if HAVE_POLL_H
    int msecs = (secs >= 0 ? (1000 * secs) : -1);
    struct pollfd rdfds;
#else
    struct timeval timeout, *tvp = (secs >= 0 ? &timeout : NULL);
    fd_set rdfds;
    FD_ZERO(&rdfds);
#endif

    if ((fdno = fdFileno(fd)) < 0)
	return -1;	/* XXX W2DO? */
	
    do {
#if HAVE_POLL_H
	rdfds.fd = fdno;
	rdfds.events = POLLIN;
	rdfds.revents = 0;
	rc = poll(&rdfds, 1, msecs);
#else
	if (tvp) {
	    tvp->tv_sec = secs;
	    tvp->tv_usec = 0;
	}
	FD_SET(fdno, &rdfds);
	rc = select(fdno + 1, &rdfds, NULL, NULL, tvp);
#endif

	if (rc < 0) {
	    switch (errno) {
	    case EINTR:
		continue;
		break;
	    default:
		return rc;
		break;
	    }
	}
	return rc;
    } while (1);
}

int fdFgets(FD_t fd, char * buf, size_t len)
{
    int fdno;
    int secs = fd->rd_timeoutsecs;
    size_t nb = 0;
    int ec = 0;
    char lastchar = '\0';

    if ((fdno = fdFileno(fd)) < 0)
	return 0;	/* XXX W2DO? */
	
    do {
	int rc;

	/* Is there data to read? */
	rc = fdReadable(fd, secs);

	switch (rc) {
	case -1:	/* error */
	    ec = -1;
	    continue;
	    break;
	case  0:	/* timeout */
	    ec = -1;
	    continue;
	    break;
	default:	/* data to read */
	    break;
	}

	errno = 0;
#ifdef	NOISY
	rc = __fdRead(fd, buf + nb, 1);
#else
	rc = read(fdFileno(fd), buf + nb, 1);
#endif
	if (rc < 0) {
	    fd->syserrno = errno;
	    switch (errno) {
	    case EWOULDBLOCK:
		continue;
		break;
	    default:
		break;
	    }
if (_rpmio_debug)
fprintf(stderr, "*** read: fd %p rc %d errno %d %s \"%s\"\n", fd, rc, errno, strerror(errno), buf);
	    ec = -1;
	    break;
	} else if (rc == 0) {
if (_rpmio_debug)
fprintf(stderr, "*** read: fd %p rc %d EOF errno %d %s \"%s\"\n", fd, rc, errno, strerror(errno), buf);
	    break;
	} else {
	    nb += rc;
	    buf[nb] = '\0';
	    lastchar = buf[nb - 1];
	}
    } while (ec == 0 && nb < len && lastchar != '\n');

    return (ec >= 0 ? nb : ec);
}

/* =============================================================== */
/* Support for FTP/HTTP I/O.
 */
const char * ftpStrerror(int errorNumber)
{
    switch (errorNumber) {
    case 0:
	return _("Success");

    /* HACK error impediance match, coalesce and rename. */
    case FTPERR_NE_ERROR:
	return ("NE_ERROR: Generic error.");
    case FTPERR_NE_LOOKUP:
	return ("NE_LOOKUP: Hostname lookup failed.");
    case FTPERR_NE_AUTH:
	return ("NE_AUTH: Server authentication failed.");
    case FTPERR_NE_PROXYAUTH:
	return ("NE_PROXYAUTH: Proxy authentication failed.");
    case FTPERR_NE_CONNECT:
	return ("NE_CONNECT: Could not connect to server.");
    case FTPERR_NE_TIMEOUT:
	return ("NE_TIMEOUT: Connection timed out.");
    case FTPERR_NE_FAILED:
	return ("NE_FAILED: The precondition failed.");
    case FTPERR_NE_RETRY:
	return ("NE_RETRY: Retry request.");
    case FTPERR_NE_REDIRECT:
	return ("NE_REDIRECT: Redirect received.");

    case FTPERR_BAD_SERVER_RESPONSE:
	return _("Bad server response");
    case FTPERR_SERVER_IO_ERROR:
	return _("Server I/O error");
    case FTPERR_SERVER_TIMEOUT:
	return _("Server timeout");
    case FTPERR_BAD_HOST_ADDR:
	return _("Unable to lookup server host address");
    case FTPERR_BAD_HOSTNAME:
	return _("Unable to lookup server host name");
    case FTPERR_FAILED_CONNECT:
	return _("Failed to connect to server");
    case FTPERR_FAILED_DATA_CONNECT:
	return _("Failed to establish data connection to server");
    case FTPERR_FILE_IO_ERROR:
	return _("I/O error to local file");
    case FTPERR_PASSIVE_ERROR:
	return _("Error setting remote server to passive mode");
    case FTPERR_FILE_NOT_FOUND:
	return _("File not found on server");
    case FTPERR_NIC_ABORT_IN_PROGRESS:
	return _("Abort in progress");

    case FTPERR_UNKNOWN:
    default:
	return _("Unknown or unexpected error");
    }
}

static rpmCallbackFunction	urlNotify = NULL;

static void *			urlNotifyData = NULL;

static int			urlNotifyCount = -1;

void urlSetCallback(rpmCallbackFunction notify, void *notifyData, int notifyCount) {
    urlNotify = notify;
    urlNotifyData = notifyData;
    urlNotifyCount = (notifyCount >= 0) ? notifyCount : 4096;
}

int ufdCopy(FD_t sfd, FD_t tfd)
{
    char buf[BUFSIZ];
    int itemsRead;
    int itemsCopied = 0;
    int rc = 0;
    int notifier = -1;

    if (urlNotify) {
	/* FIX: check rc */
	(void)(*urlNotify) (NULL, RPMCALLBACK_INST_OPEN_FILE,
		0, 0, NULL, urlNotifyData);
    }
    
    while (1) {
	rc = Fread(buf, sizeof(buf[0]), sizeof(buf), sfd);
	if (rc < 0)
	    break;
	else if (rc == 0) {
	    rc = itemsCopied;
	    break;
	}
	itemsRead = rc;
	rc = Fwrite(buf, sizeof(buf[0]), itemsRead, tfd);
	if (rc < 0)
	    break;
 	if (rc != itemsRead) {
	    rc = FTPERR_FILE_IO_ERROR;
	    break;
	}

	itemsCopied += itemsRead;
	if (urlNotify && urlNotifyCount > 0) {
	    int n = itemsCopied/urlNotifyCount;
	    if (n != notifier) {
		/* FIX: check rc */
		(void)(*urlNotify) (NULL, RPMCALLBACK_INST_PROGRESS,
			itemsCopied, 0, NULL, urlNotifyData);
		notifier = n;
	    }
	}
    }

    DBGIO(sfd, (stderr, "++ copied %d bytes: %s\n", itemsCopied,
	ftpStrerror(rc)));

    if (urlNotify) {
	/* FIX: check rc */
	(void)(*urlNotify) (NULL, RPMCALLBACK_INST_OPEN_FILE,
		itemsCopied, itemsCopied, NULL, urlNotifyData);
    }
    
    return rc;
}

int ufdGetFile(FD_t sfd, FD_t tfd)
{
    int rc;

    FDSANE(sfd);
    FDSANE(tfd);
    rc = ufdCopy(sfd, tfd);
    (void) Fclose(sfd);
    if (rc > 0)		/* XXX ufdCopy now returns no. bytes copied */
	rc = 0;
    return rc;
}

/* =============================================================== */
static ssize_t ufdRead(void * cookie, char * buf, size_t count)
{
    FD_t fd = c2f(cookie);
    int bytesRead;
    int total;

    /* XXX preserve timedRead() behavior */
    if (fdGetIo(fd) == fdio) {
	struct stat sb;
	int fdno = fdFileno(fd);
	(void) fstat(fdno, &sb);
	if (S_ISREG(sb.st_mode))
	    return __fdRead(fd, buf, count);
    }

    UFDONLY(fd);
    assert(fd->rd_timeoutsecs >= 0);

    for (total = 0; total < count; total += bytesRead) {

	int rc;

	bytesRead = 0;

	/* Is there data to read? */
	if (fd->bytesRemain == 0) return total;	/* XXX simulate EOF */
	rc = fdReadable(fd, fd->rd_timeoutsecs);

	switch (rc) {
	case -1:	/* error */
	case  0:	/* timeout */
	    return total;
	    break;
	default:	/* data to read */
	    break;
	}

	rc = __fdRead(fd, buf + total, count - total);

	if (rc < 0) {
	    switch (errno) {
	    case EWOULDBLOCK:
		continue;
		break;
	    default:
		break;
	    }
if (_rpmio_debug)
fprintf(stderr, "*** read: rc %d errno %d %s \"%s\"\n", rc, errno, strerror(errno), buf);
	    return rc;
	    break;
	} else if (rc == 0) {
	    return total;
	    break;
	}
	bytesRead = rc;
    }

    return count;
}

static ssize_t ufdWrite(void * cookie, const char * buf, size_t count)
{
    FD_t fd = c2f(cookie);
    int bytesWritten;
    int total = 0;

#ifdef	NOTYET
    if (fdGetIo(fd) == fdio) {
	struct stat sb;
	(void) fstat(fdGetFdno(fd), &sb);
	if (S_ISREG(sb.st_mode))
	    return __fdWrite(fd, buf, count);
    }
#endif

    UFDONLY(fd);

    for (total = 0; total < count; total += bytesWritten) {

	int rc;

	bytesWritten = 0;

	/* Is there room to write data? */
	if (fd->bytesRemain == 0) {
fprintf(stderr, "*** ufdWrite fd %p WRITE PAST END OF CONTENT\n", fd);
	    return total;	/* XXX simulate EOF */
	}
	rc = fdWritable(fd, 2);		/* XXX configurable? */

	switch (rc) {
	case -1:	/* error */
	case  0:	/* timeout */
	    return total;
	    break;
	default:	/* data to write */
	    break;
	}

	rc = __fdWrite(fd, buf + total, count - total);

	if (rc < 0) {
	    switch (errno) {
	    case EWOULDBLOCK:
		continue;
		break;
	    default:
		break;
	    }
if (_rpmio_debug)
fprintf(stderr, "*** write: rc %d errno %d %s \"%s\"\n", rc, errno, strerror(errno), buf);
	    return rc;
	    break;
	} else if (rc == 0) {
	    return total;
	    break;
	}
	bytesWritten = rc;
    }

    return count;
}

static inline int ufdSeek(void * cookie, _libio_pos_t pos, int whence)
{
    FD_t fd = c2f(cookie);

    switch (fd->urlType) {
    case URL_IS_UNKNOWN:
    case URL_IS_PATH:
	break;
    case URL_IS_HTTPS:
    case URL_IS_HTTP:
    case URL_IS_HKP:
    case URL_IS_FTP:
    case URL_IS_DASH:
    default:
        return -2;
	break;
    }
    return fdSeek(cookie, pos, whence);
}

int ufdClose( void * cookie)
{
    FD_t fd = c2f(cookie);

    UFDONLY(fd);

    return __fdClose(fd);
}

/*
 * Deal with remote url's by fetching them with a helper application
 * and treat as local file afterwards.
 * TODO:
 * - better error checking + reporting
 * - curl & friends don't know about hkp://, transform to http?
 */
static FD_t urlOpen(const char * url, int flags, mode_t mode)
{
    FD_t fd = NULL;
    char cmd[BUFSIZ];
    char *dest = NULL;
    char *urlhelper = NULL;
    int rc;
    pid_t pid, wait;

    urlhelper = rpmExpand("%{?_urlhelper}", NULL);

    dest = (char *) rpmGenPath(NULL, "%{_tmppath}/", "rpm-transfer.XXXXXX");
    close(mkstemp(dest));
    sprintf(cmd, "%s %s %s\n", urlhelper, dest, url);
    urlhelper = _free(urlhelper);

    if ((pid = fork()) == 0) {
        ARGV_t argv = NULL;
        argvSplit(&argv, cmd, " ");
        execvp(argv[0], (char *const *)argv);
        exit(-1); /* error out if exec fails */
    }
    wait = waitpid(pid, &rc, 0);

    if (!WIFEXITED(rc) || WEXITSTATUS(rc)) {
        rpmError(RPMERR_EXEC, _("URL helper failed: %s (%d)\n"),
                 cmd, WEXITSTATUS(rc));
    } else {
	fd = __fdOpen(dest, flags, mode);
	unlink(dest);
    }
    dest = _free(dest);

    return fd;

}

static FD_t ufdOpen(const char * url, int flags, mode_t mode)
{
    FD_t fd = NULL;
    const char * path;
    int timeout = 1;
    urltype urlType = urlPath(url, &path);

if (_rpmio_debug)
fprintf(stderr, "*** ufdOpen(%s,0x%x,0%o)\n", url, (unsigned)flags, (unsigned)mode);

    switch (urlType) {
    case URL_IS_FTP:
    case URL_IS_HTTPS:
    case URL_IS_HTTP:
    case URL_IS_HKP:
	fd = urlOpen(url, flags, mode);
	/* we're dealing with local file when urlOpen() returns */
	urlType = URL_IS_UNKNOWN;
	break;
    case URL_IS_DASH:
	assert(!(flags & O_RDWR));
	fd = fdDup( ((flags & O_WRONLY) ? STDOUT_FILENO : STDIN_FILENO) );
	timeout = 600; /* XXX W2DO? 10 mins? */
	break;
    case URL_IS_PATH:
    case URL_IS_UNKNOWN:
    default:
	fd = __fdOpen(path, flags, mode);
	break;
    }

    if (fd == NULL) return NULL;

    fdSetIo(fd, ufdio);
    fd->rd_timeoutsecs = timeout;
    fd->contentLength = fd->bytesRemain = -1;
    fd->urlType = urlType;

    if (Fileno(fd) < 0) {
	(void) ufdClose(fd);
	return NULL;
    }
DBGIO(fd, (stderr, "==>\tufdOpen(\"%s\",%x,0%o) %s\n", url, (unsigned)flags, (unsigned)mode, fdbg(fd)));
    return fd;
}

static struct FDIO_s ufdio_s = {
  ufdRead, ufdWrite, ufdSeek, ufdClose, XfdLink, XfdFree, XfdNew, fdFileno,
  ufdOpen, NULL, fdGetFp, NULL,	Mkdir, Chdir, Rmdir, Rename, Unlink
};
FDIO_t ufdio = &ufdio_s ;

/* =============================================================== */
/* Support for GZIP library.
 */
#ifdef	HAVE_ZLIB_H

#include <zlib.h>

static inline void * gzdFileno(FD_t fd)
{
    void * rc = NULL;
    int i;

    FDSANE(fd);
    for (i = fd->nfps; i >= 0; i--) {
	FDSTACK_t * fps = &fd->fps[i];
	if (fps->io != gzdio)
	    continue;
	rc = fps->fp;
	break;
    }
    
    return rc;
}

static
FD_t gzdOpen(const char * path, const char * fmode)
{
    FD_t fd;
    gzFile gzfile;
    if ((gzfile = gzopen(path, fmode)) == NULL)
	return NULL;
    fd = fdNew("open (gzdOpen)");
    fdPop(fd); fdPush(fd, gzdio, gzfile, -1);
    
DBGIO(fd, (stderr, "==>\tgzdOpen(\"%s\", \"%s\") fd %p %s\n", path, fmode, (fd ? fd : NULL), fdbg(fd)));
    return fdLink(fd, "gzdOpen");
}

static FD_t gzdFdopen(void * cookie, const char *fmode)
{
    FD_t fd = c2f(cookie);
    int fdno;
    gzFile gzfile;

    if (fmode == NULL) return NULL;
    fdno = fdFileno(fd);
    fdSetFdno(fd, -1);		/* XXX skip the fdio close */
    if (fdno < 0) return NULL;
    gzfile = gzdopen(fdno, fmode);
    if (gzfile == NULL) return NULL;

    fdPush(fd, gzdio, gzfile, fdno);		/* Push gzdio onto stack */

    return fdLink(fd, "gzdFdopen");
}

static int gzdFlush(FD_t fd)
{
    gzFile gzfile;
    gzfile = gzdFileno(fd);
    if (gzfile == NULL) return -2;
    return gzflush(gzfile, Z_SYNC_FLUSH);	/* XXX W2DO? */
}

/* =============================================================== */
static ssize_t gzdRead(void * cookie, char * buf, size_t count)
{
    FD_t fd = c2f(cookie);
    gzFile gzfile;
    ssize_t rc;

    if (fd == NULL || fd->bytesRemain == 0) return 0;	/* XXX simulate EOF */

    gzfile = gzdFileno(fd);
    if (gzfile == NULL) return -2;	/* XXX can't happen */

    fdstat_enter(fd, FDSTAT_READ);
    rc = gzread(gzfile, buf, count);
DBGIO(fd, (stderr, "==>\tgzdRead(%p,%p,%u) rc %lx %s\n", cookie, buf, (unsigned)count, (unsigned long)rc, fdbg(fd)));
    if (rc < 0) {
	int zerror = 0;
	fd->errcookie = gzerror(gzfile, &zerror);
	if (zerror == Z_ERRNO) {
	    fd->syserrno = errno;
	    fd->errcookie = strerror(fd->syserrno);
	}
    } else if (rc >= 0) {
	fdstat_exit(fd, FDSTAT_READ, rc);
	if (fd->ndigests && rc > 0) fdUpdateDigests(fd, (void *)buf, rc);
    }
    return rc;
}

static ssize_t gzdWrite(void * cookie, const char * buf, size_t count)
{
    FD_t fd = c2f(cookie);
    gzFile gzfile;
    ssize_t rc;

    if (fd == NULL || fd->bytesRemain == 0) return 0;	/* XXX simulate EOF */

    if (fd->ndigests && count > 0) fdUpdateDigests(fd, (void *)buf, count);

    gzfile = gzdFileno(fd);
    if (gzfile == NULL) return -2;	/* XXX can't happen */

    fdstat_enter(fd, FDSTAT_WRITE);
    rc = gzwrite(gzfile, (void *)buf, count);
DBGIO(fd, (stderr, "==>\tgzdWrite(%p,%p,%u) rc %lx %s\n", cookie, buf, (unsigned)count, (unsigned long)rc, fdbg(fd)));
    if (rc < 0) {
	int zerror = 0;
	fd->errcookie = gzerror(gzfile, &zerror);
	if (zerror == Z_ERRNO) {
	    fd->syserrno = errno;
	    fd->errcookie = strerror(fd->syserrno);
	}
    } else if (rc > 0) {
	fdstat_exit(fd, FDSTAT_WRITE, rc);
    }
    return rc;
}

/* XXX zlib-1.0.4 has not */
static inline int gzdSeek(void * cookie, _libio_pos_t pos, int whence)
{
#ifdef USE_COOKIE_SEEK_POINTER
    _IO_off64_t p = *pos;
#else
    off_t p = pos;
#endif
    int rc;
#if HAVE_GZSEEK
    FD_t fd = c2f(cookie);
    gzFile gzfile;

    if (fd == NULL) return -2;
    assert(fd->bytesRemain == -1);	/* XXX FIXME */

    gzfile = gzdFileno(fd);
    if (gzfile == NULL) return -2;	/* XXX can't happen */

    fdstat_enter(fd, FDSTAT_SEEK);
    rc = gzseek(gzfile, p, whence);
DBGIO(fd, (stderr, "==>\tgzdSeek(%p,%ld,%d) rc %lx %s\n", cookie, (long)p, whence, (unsigned long)rc, fdbg(fd)));
    if (rc < 0) {
	int zerror = 0;
	fd->errcookie = gzerror(gzfile, &zerror);
	if (zerror == Z_ERRNO) {
	    fd->syserrno = errno;
	    fd->errcookie = strerror(fd->syserrno);
	}
    } else if (rc >= 0) {
	fdstat_exit(fd, FDSTAT_SEEK, rc);
    }
#else
    rc = -2;
#endif
    return rc;
}

static int gzdClose( void * cookie)
{
    FD_t fd = c2f(cookie);
    gzFile gzfile;
    int rc;

    gzfile = gzdFileno(fd);
    if (gzfile == NULL) return -2;	/* XXX can't happen */

    fdstat_enter(fd, FDSTAT_CLOSE);
    rc = gzclose(gzfile);

    /* XXX TODO: preserve fd if errors */

    if (fd) {
DBGIO(fd, (stderr, "==>\tgzdClose(%p) zerror %d %s\n", cookie, rc, fdbg(fd)));
	if (rc < 0) {
	    fd->errcookie = "gzclose error";
	    if (rc == Z_ERRNO) {
		fd->syserrno = errno;
		fd->errcookie = strerror(fd->syserrno);
	    }
	} else if (rc >= 0) {
	    fdstat_exit(fd, FDSTAT_CLOSE, rc);
	}
    }

DBGIO(fd, (stderr, "==>\tgzdClose(%p) rc %lx %s\n", cookie, (unsigned long)rc, fdbg(fd)));

    if (_rpmio_debug || rpmIsDebug()) fdstat_print(fd, "GZDIO", stderr);
    if (rc == 0)
	fd = fdFree(fd, "open (gzdClose)");
    return rc;
}

static struct FDIO_s gzdio_s = {
  gzdRead, gzdWrite, gzdSeek, gzdClose, XfdLink, XfdFree, XfdNew, fdFileno,
  NULL, gzdOpen, gzdFileno, gzdFlush,	NULL, NULL, NULL, NULL, NULL
};
FDIO_t gzdio = &gzdio_s ;

#endif	/* HAVE_ZLIB_H */

/* =============================================================== */
/* Support for BZIP2 library.
 */
#if HAVE_BZLIB_H

#include <bzlib.h>

#ifdef HAVE_BZ2_1_0
# define bzopen  BZ2_bzopen
# define bzclose BZ2_bzclose
# define bzdopen BZ2_bzdopen
# define bzerror BZ2_bzerror
# define bzflush BZ2_bzflush
# define bzread  BZ2_bzread
# define bzwrite BZ2_bzwrite
#endif /* HAVE_BZ2_1_0 */

static inline void * bzdFileno(FD_t fd)
{
    void * rc = NULL;
    int i;

    FDSANE(fd);
    for (i = fd->nfps; i >= 0; i--) {
	FDSTACK_t * fps = &fd->fps[i];
	if (fps->io != bzdio)
	    continue;
	rc = fps->fp;
	break;
    }
    
    return rc;
}

static FD_t bzdOpen(const char * path, const char * mode)
{
    FD_t fd;
    BZFILE *bzfile;;
    if ((bzfile = bzopen(path, mode)) == NULL)
	return NULL;
    fd = fdNew("open (bzdOpen)");
    fdPop(fd); fdPush(fd, bzdio, bzfile, -1);
    return fdLink(fd, "bzdOpen");
}

static FD_t bzdFdopen(void * cookie, const char * fmode)
{
    FD_t fd = c2f(cookie);
    int fdno;
    BZFILE *bzfile;

    if (fmode == NULL) return NULL;
    fdno = fdFileno(fd);
    fdSetFdno(fd, -1);		/* XXX skip the fdio close */
    if (fdno < 0) return NULL;
    bzfile = bzdopen(fdno, fmode);
    if (bzfile == NULL) return NULL;

    fdPush(fd, bzdio, bzfile, fdno);		/* Push bzdio onto stack */

    return fdLink(fd, "bzdFdopen");
}

static int bzdFlush(FD_t fd)
{
    return bzflush(bzdFileno(fd));
}

/* =============================================================== */
static ssize_t bzdRead(void * cookie, char * buf, size_t count)
{
    FD_t fd = c2f(cookie);
    BZFILE *bzfile;
    ssize_t rc = 0;

    if (fd->bytesRemain == 0) return 0;	/* XXX simulate EOF */
    bzfile = bzdFileno(fd);
    fdstat_enter(fd, FDSTAT_READ);
    if (bzfile)
	rc = bzread(bzfile, buf, count);
    if (rc == -1) {
	int zerror = 0;
	if (bzfile)
	    fd->errcookie = bzerror(bzfile, &zerror);
    } else if (rc >= 0) {
	fdstat_exit(fd, FDSTAT_READ, rc);
	if (fd->ndigests && rc > 0) fdUpdateDigests(fd, (void *)buf, rc);
    }
    return rc;
}

static ssize_t bzdWrite(void * cookie, const char * buf, size_t count)
{
    FD_t fd = c2f(cookie);
    BZFILE *bzfile;
    ssize_t rc;

    if (fd->bytesRemain == 0) return 0;	/* XXX simulate EOF */

    if (fd->ndigests && count > 0) fdUpdateDigests(fd, (void *)buf, count);

    bzfile = bzdFileno(fd);
    fdstat_enter(fd, FDSTAT_WRITE);
    rc = bzwrite(bzfile, (void *)buf, count);
    if (rc == -1) {
	int zerror = 0;
	fd->errcookie = bzerror(bzfile, &zerror);
    } else if (rc > 0) {
	fdstat_exit(fd, FDSTAT_WRITE, rc);
    }
    return rc;
}

static inline int bzdSeek(void * cookie, _libio_pos_t pos,
			int whence)
{
    FD_t fd = c2f(cookie);

    BZDONLY(fd);
    return -2;
}

static int bzdClose( void * cookie)
{
    FD_t fd = c2f(cookie);
    BZFILE *bzfile;
    int rc;

    bzfile = bzdFileno(fd);

    if (bzfile == NULL) return -2;
    fdstat_enter(fd, FDSTAT_CLOSE);
    /* FIX: check rc */
    bzclose(bzfile);
    rc = 0;	/* XXX FIXME */

    /* XXX TODO: preserve fd if errors */

    if (fd) {
	if (rc == -1) {
	    int zerror = 0;
	    fd->errcookie = bzerror(bzfile, &zerror);
	} else if (rc >= 0) {
	    fdstat_exit(fd, FDSTAT_CLOSE, rc);
	}
    }

DBGIO(fd, (stderr, "==>\tbzdClose(%p) rc %lx %s\n", cookie, (unsigned long)rc, fdbg(fd)));

    if (_rpmio_debug || rpmIsDebug()) fdstat_print(fd, "BZDIO", stderr);
    if (rc == 0)
	fd = fdFree(fd, "open (bzdClose)");
    return rc;
}

static struct FDIO_s bzdio_s = {
  bzdRead, bzdWrite, bzdSeek, bzdClose, XfdLink, XfdFree, XfdNew, fdFileno,
  NULL, bzdOpen, bzdFileno, bzdFlush,	NULL, NULL, NULL, NULL, NULL
};
FDIO_t bzdio = &bzdio_s ;

#endif	/* HAVE_BZLIB_H */

/* =============================================================== */
static const char * getFdErrstr (FD_t fd)
{
    const char *errstr = NULL;

#ifdef	HAVE_ZLIB_H
    if (fdGetIo(fd) == gzdio) {
	errstr = fd->errcookie;
    } else
#endif	/* HAVE_ZLIB_H */

#ifdef	HAVE_BZLIB_H
    if (fdGetIo(fd) == bzdio) {
	errstr = fd->errcookie;
    } else
#endif	/* HAVE_BZLIB_H */

    {
	errstr = (fd->syserrno ? strerror(fd->syserrno) : "");
    }

    return errstr;
}

/* =============================================================== */

const char *Fstrerror(FD_t fd)
{
    if (fd == NULL)
	return (errno ? strerror(errno) : "");
    FDSANE(fd);
    return getFdErrstr(fd);
}

#define	FDIOVEC(_fd, _vec)	\
  ((fdGetIo(_fd) && fdGetIo(_fd)->_vec) ? fdGetIo(_fd)->_vec : NULL)

ssize_t Fread(void *buf, size_t size, size_t nmemb, FD_t fd) {
    fdio_read_function_t _read;
    int rc;

    FDSANE(fd);
DBGIO(fd, (stderr, "==> Fread(%p,%u,%u,%p) %s\n", buf, (unsigned)size, (unsigned)nmemb, (fd ? fd : NULL), fdbg(fd)));

    if (fdGetIo(fd) == fpio) {
	rc = fread(buf, size, nmemb, fdGetFILE(fd));
	return rc;
    }

    _read = FDIOVEC(fd, read);

    rc = (_read ? (*_read) (fd, buf, size * nmemb) : -2);
    return rc;
}

ssize_t Fwrite(const void *buf, size_t size, size_t nmemb, FD_t fd)
{
    fdio_write_function_t _write;
    int rc;

    FDSANE(fd);
DBGIO(fd, (stderr, "==> Fwrite(%p,%u,%u,%p) %s\n", buf, (unsigned)size, (unsigned)nmemb, (fd ? fd : NULL), fdbg(fd)));

    if (fdGetIo(fd) == fpio) {
	rc = fwrite(buf, size, nmemb, fdGetFILE(fd));
	return rc;
    }

    _write = FDIOVEC(fd, write);

    rc = (_write ? _write(fd, buf, size * nmemb) : -2);
    return rc;
}

int Fseek(FD_t fd, _libio_off_t offset, int whence) {
    fdio_seek_function_t _seek;
#ifdef USE_COOKIE_SEEK_POINTER
    _IO_off64_t o64 = offset;
    _libio_pos_t pos = &o64;
#else
    _libio_pos_t pos = offset;
#endif

    long int rc;

    FDSANE(fd);
DBGIO(fd, (stderr, "==> Fseek(%p,%ld,%d) %s\n", fd, (long)offset, whence, fdbg(fd)));

    if (fdGetIo(fd) == fpio) {
	FILE *fp;

	fp = fdGetFILE(fd);
	rc = fseek(fp, offset, whence);
	return rc;
    }

    _seek = FDIOVEC(fd, seek);

    rc = (_seek ? _seek(fd, pos, whence) : -2);
    return rc;
}

int Fclose(FD_t fd)
{
    int rc = 0, ec = 0;

    FDSANE(fd);
DBGIO(fd, (stderr, "==> Fclose(%p) %s\n", (fd ? fd : NULL), fdbg(fd)));

    fd = fdLink(fd, "Fclose");
    while (fd->nfps >= 0) {
	FDSTACK_t * fps = &fd->fps[fd->nfps];
	
	if (fps->io == fpio) {
	    FILE *fp;
	    int fpno;

	    fp = fdGetFILE(fd);
	    fpno = fileno(fp);
	    if (fp)
	    	rc = fclose(fp);
	    if (fpno == -1) {
	    	fd = fdFree(fd, "fopencookie (Fclose)");
	    	fdPop(fd);
	    }
	} else {
	    fdio_close_function_t _close = FDIOVEC(fd, close);
	    rc = _close(fd);
	}
	if (fd->nfps == 0)
	    break;
	if (ec == 0 && rc)
	    ec = rc;
	fdPop(fd);
    }
    fd = fdFree(fd, "Fclose");
    return ec;
}

/**
 * Convert stdio fmode to open(2) mode, filtering out zlib/bzlib flags.
 *	returns stdio[0] = NUL on error.
 *
 * - gzopen:	[0-9] is compession level
 * - gzopen:	'f' is filtered (Z_FILTERED)
 * - gzopen:	'h' is Huffman encoding (Z_HUFFMAN_ONLY)
 * - bzopen:	[1-9] is block size (modulo 100K)
 * - bzopen:	's' is smallmode
 * - HACK:	'.' terminates, rest is type of I/O
 */
static inline void cvtfmode (const char *m,
				char *stdio, size_t nstdio,
				char *other, size_t nother,
				const char **end, int * f)
{
    int flags = 0;
    char c;

    switch (*m) {
    case 'a':
	flags |= O_WRONLY | O_CREAT | O_APPEND;
	if (--nstdio > 0) *stdio++ = *m;
	break;
    case 'w':
	flags |= O_WRONLY | O_CREAT | O_TRUNC;
	if (--nstdio > 0) *stdio++ = *m;
	break;
    case 'r':
	flags |= O_RDONLY;
	if (--nstdio > 0) *stdio++ = *m;
	break;
    default:
	*stdio = '\0';
	return;
	break;
    }
    m++;

    while ((c = *m++) != '\0') {
	switch (c) {
	case '.':
	    break;
	case '+':
	    flags &= ~(O_RDONLY|O_WRONLY);
	    flags |= O_RDWR;
	    if (--nstdio > 0) *stdio++ = c;
	    continue;
	    break;
	case 'b':
	    if (--nstdio > 0) *stdio++ = c;
	    continue;
	    break;
	case 'x':
	    flags |= O_EXCL;
	    if (--nstdio > 0) *stdio++ = c;
	    continue;
	    break;
	default:
	    if (--nother > 0) *other++ = c;
	    continue;
	    break;
	}
	break;
    }

    *stdio = *other = '\0';
    if (end != NULL)
	*end = (*m != '\0' ? m : NULL);
    if (f != NULL)
	*f = flags;
}

#if _USE_LIBIO
#if defined(__GLIBC__) && __GLIBC__ == 2 && __GLIBC_MINOR__ == 0
/* XXX retrofit glibc-2.1.x typedef on glibc-2.0.x systems */
typedef _IO_cookie_io_functions_t cookie_io_functions_t;
#endif
#endif

FD_t Fdopen(FD_t ofd, const char *fmode)
{
    char stdio[20], other[20], zstdio[20];
    const char *end = NULL;
    FDIO_t iof = NULL;
    FD_t fd = ofd;

if (_rpmio_debug)
fprintf(stderr, "*** Fdopen(%p,%s) %s\n", fd, fmode, fdbg(fd));
    FDSANE(fd);

    if (fmode == NULL)
	return NULL;

    cvtfmode(fmode, stdio, sizeof(stdio), other, sizeof(other), &end, NULL);
    if (stdio[0] == '\0')
	return NULL;
    zstdio[0] = '\0';
    strncat(zstdio, stdio, sizeof(zstdio) - strlen(zstdio));
    strncat(zstdio, other, sizeof(zstdio) - strlen(zstdio));

    if (end == NULL && other[0] == '\0')
	return fd;

    if (end && *end) {
	if (!strcmp(end, "fdio")) {
	    iof = fdio;
#if HAVE_ZLIB_H
	} else if (!strcmp(end, "gzdio")) {
	    iof = gzdio;
	    fd = gzdFdopen(fd, zstdio);
#endif
#if HAVE_BZLIB_H
	} else if (!strcmp(end, "bzdio")) {
	    iof = bzdio;
	    fd = bzdFdopen(fd, zstdio);
#endif
	} else if (!strcmp(end, "ufdio")) {
	    iof = ufdio;
	} else if (!strcmp(end, "fpio")) {
	    iof = fpio;
	    if (noLibio) {
		int fdno = Fileno(fd);
		FILE * fp = fdopen(fdno, stdio);
if (_rpmio_debug)
fprintf(stderr, "*** Fdopen fpio fp %p\n", (void *)fp);
		if (fp == NULL)
		    return NULL;
		/* XXX gzdio/bzdio use fp for private data */
		if (fdGetFp(fd) == NULL)
		    fdSetFp(fd, fp);
		fdPush(fd, fpio, fp, fdno);	/* Push fpio onto stack */
	    }
	}
    } else if (other[0] != '\0') {
	for (end = other; *end && strchr("0123456789fh", *end); end++)
	    {};
	if (*end == '\0') {
	    iof = gzdio;
	    fd = gzdFdopen(fd, zstdio);
	}
    }
    if (iof == NULL)
	return fd;

    if (!noLibio) {
	FILE * fp = NULL;

#if _USE_LIBIO
	{   cookie_io_functions_t ciof;
	    ciof.read = iof->read;
	    ciof.write = iof->write;
	    ciof.seek = iof->seek;
	    ciof.close = iof->close;
	    fp = fopencookie(fd, stdio, ciof);
DBGIO(fd, (stderr, "==> fopencookie(%p,\"%s\",*%p) returns fp %p\n", fd, stdio, iof, fp));
	}
#endif

	if (fp) {
	    /* XXX gzdio/bzdio use fp for private data */
	    if (fdGetFp(fd) == NULL)
		fdSetFp(fd, fp);
	    fdPush(fd, fpio, fp, fileno(fp));	/* Push fpio onto stack */
	    fd = fdLink(fd, "fopencookie");
	}
    }

DBGIO(fd, (stderr, "==> Fdopen(%p,\"%s\") returns fd %p %s\n", ofd, fmode, (fd ? fd : NULL), fdbg(fd)));
    return fd;
}

FD_t Fopen(const char *path, const char *fmode)
{
    char stdio[20], other[20];
    const char *end = NULL;
    mode_t perms = 0666;
    int flags = 0;
    FD_t fd;

    if (path == NULL || fmode == NULL)
	return NULL;

    stdio[0] = '\0';
    cvtfmode(fmode, stdio, sizeof(stdio), other, sizeof(other), &end, &flags);
    if (stdio[0] == '\0')
	return NULL;

    if (end == NULL || !strcmp(end, "fdio")) {
if (_rpmio_debug)
fprintf(stderr, "*** Fopen fdio path %s fmode %s\n", path, fmode);
	fd = __fdOpen(path, flags, perms);
	if (fdFileno(fd) < 0) {
	    if (fd) (void) __fdClose(fd);
	    return NULL;
	}
    } else {
	/* XXX gzdio and bzdio here too */

	switch (urlIsURL(path)) {
	case URL_IS_HTTPS:
	case URL_IS_HTTP:
	case URL_IS_HKP:
	case URL_IS_PATH:
	case URL_IS_DASH:
	case URL_IS_FTP:
	case URL_IS_UNKNOWN:
if (_rpmio_debug)
fprintf(stderr, "*** Fopen ufdio path %s fmode %s\n", path, fmode);
	    fd = ufdOpen(path, flags, perms);
	    if (fd == NULL || !(fdFileno(fd) >= 0))
		return fd;
	    break;
	default:
if (_rpmio_debug)
fprintf(stderr, "*** Fopen WTFO path %s fmode %s\n", path, fmode);
	    return NULL;
	    break;
	}

    }

    if (fd)
	fd = Fdopen(fd, fmode);
    return fd;
}

int Fflush(FD_t fd)
{
    void * vh;
    if (fd == NULL) return -1;
    if (fdGetIo(fd) == fpio)
	return fflush(fdGetFILE(fd));

    vh = fdGetFp(fd);
#if HAVE_ZLIB_H
    if (vh && fdGetIo(fd) == gzdio)
	return gzdFlush(vh);
#endif
#if HAVE_BZLIB_H
    if (vh && fdGetIo(fd) == bzdio)
	return bzdFlush(vh);
#endif
/* FIXME: If we get here, something went wrong above */
    return 0;
}

int Ferror(FD_t fd)
{
    int i, rc = 0;

    if (fd == NULL) return -1;
    for (i = fd->nfps; rc == 0 && i >= 0; i--) {
	FDSTACK_t * fps = &fd->fps[i];
	int ec;
	
	if (fps->io == fpio) {
	    ec = ferror(fdGetFILE(fd));
#if HAVE_ZLIB_H
	} else if (fps->io == gzdio) {
	    ec = (fd->syserrno || fd->errcookie != NULL) ? -1 : 0;
	    i--;	/* XXX fdio under gzdio always has fdno == -1 */
#endif
#if HAVE_BZLIB_H
	} else if (fps->io == bzdio) {
	    ec = (fd->syserrno  || fd->errcookie != NULL) ? -1 : 0;
	    i--;	/* XXX fdio under bzdio always has fdno == -1 */
#endif
	} else {
	/* XXX need to check ufdio/gzdio/bzdio/fdio errors correctly. */
	    ec = (fdFileno(fd) < 0 ? -1 : 0);
	}

	if (rc == 0 && ec)
	    rc = ec;
    }
DBGIO(fd, (stderr, "==> Ferror(%p) rc %d %s\n", fd, rc, fdbg(fd)));
    return rc;
}

int Fileno(FD_t fd)
{
    int i, rc = -1;

    if (fd == NULL) return -1;
    for (i = fd->nfps ; rc == -1 && i >= 0; i--) {
	rc = fd->fps[i].fdno;
    }
    
DBGIO(fd, (stderr, "==> Fileno(%p) rc %d %s\n", (fd ? fd : NULL), rc, fdbg(fd)));
    return rc;
}

/* XXX this is naive */
int Fcntl(FD_t fd, int op, void *lip)
{
    return fcntl(Fileno(fd), op, lip);
}

/* =============================================================== */
/* Helper routines that may be generally useful.
 */
int rpmioMkpath(const char * path, mode_t mode, uid_t uid, gid_t gid)
{
    char * d, * de;
    int created = 0;
    int rc;

    if (path == NULL)
	return -1;
    d = alloca(strlen(path)+2);
    de = stpcpy(d, path);
    de[1] = '\0';
    for (de = d; *de != '\0'; de++) {
	struct stat st;
	char savec;

	while (*de && *de != '/') de++;
	savec = de[1];
	de[1] = '\0';

	rc = Stat(d, &st);
	if (rc) {
	    switch(errno) {
	    default:
		return errno;
		break;
	    case ENOENT:
		break;
	    }
	    rc = Mkdir(d, mode);
	    if (rc)
		return errno;
	    created = 1;
	    if (!(uid == (uid_t) -1 && gid == (gid_t) -1)) {
		rc = chown(d, uid, gid);
		if (rc)
		    return errno;
	    }
	} else if (!S_ISDIR(st.st_mode)) {
	    return ENOTDIR;
	}
	de[1] = savec;
    }
    rc = 0;
    if (created)
	rpmMessage(RPMMESS_DEBUG, "created directory(s) %s mode 0%o\n",
			path, mode);
    return rc;
}

int rpmioSlurp(const char * fn, byte ** bp, ssize_t * blenp)
{
    static ssize_t blenmax = (32 * BUFSIZ);
    ssize_t blen = 0;
    byte * b = NULL;
    ssize_t size;
    FD_t fd;
    int rc = 0;

    fd = Fopen(fn, "r.ufdio");
    if (fd == NULL || Ferror(fd)) {
	rc = 2;
	goto exit;
    }

    size = fdSize(fd);
    blen = (size >= 0 ? size : blenmax);
    if (blen) {
	int nb;
	b = xmalloc(blen+1);
	b[0] = '\0';
	nb = Fread(b, sizeof(*b), blen, fd);
	if (Ferror(fd) || (size > 0 && nb != blen)) {
	    rc = 1;
	    goto exit;
	}
	if (blen == blenmax && nb < blen) {
	    blen = nb;
	    b = xrealloc(b, blen+1);
	}
	b[blen] = '\0';
    }

exit:
    if (fd) (void) Fclose(fd);
	
    if (rc) {
	if (b) free(b);
	b = NULL;
	blen = 0;
    }

    if (bp) *bp = b;
    else if (b) free(b);

    if (blenp) *blenp = blen;

    return rc;
}

static struct FDIO_s fpio_s = {
  ufdRead, ufdWrite, fdSeek, ufdClose, XfdLink, XfdFree, XfdNew, fdFileno,
  ufdOpen, NULL, fdGetFp, NULL,	Mkdir, Chdir, Rmdir, Rename, Unlink
};
FDIO_t fpio = &fpio_s ;
