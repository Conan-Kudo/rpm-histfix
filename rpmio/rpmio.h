#ifndef	H_RPMIO
#define	H_RPMIO

/** \ingroup rpmio
 * \file rpmio/rpmio.h
 *
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <rpmsw.h>

#ifdef __cplusplus
extern "C" {
#endif

/** \ingroup rpmio
 * Hide libio API lossage.
 * The libio interface changed after glibc-2.1.3 to pass the seek offset
 * argument as a pointer rather than as an off_t. The snarl below defines
 * typedefs to isolate the lossage.
 */
#if defined(__GLIBC__) && \
	(__GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 2))
#define USE_COOKIE_SEEK_POINTER 1
typedef _IO_off64_t 	_libio_off_t;
typedef _libio_off_t *	_libio_pos_t;
#else
typedef off_t 		_libio_off_t;
typedef off_t 		_libio_pos_t;
#endif

/** \ingroup rpmio
 */
typedef	struct _FD_s * FD_t;

/** \ingroup rpmio
 */
typedef struct FDIO_s * FDIO_t;


/** \ingroup rpmio
 * \name RPMIO Interface.
 */

/** \ingroup rpmio
 * strerror(3) clone.
 */
const char * Fstrerror(FD_t fd);

/** \ingroup rpmio
 * fread(3) clone.
 */
ssize_t Fread(void * buf, size_t size, size_t nmemb, FD_t fd);

/** \ingroup rpmio
 * fwrite(3) clone.
 */
ssize_t Fwrite(const void * buf, size_t size, size_t nmemb, FD_t fd);

/** \ingroup rpmio
 * fseek(3) clone.
 */
int Fseek(FD_t fd, _libio_off_t offset, int whence);

/** \ingroup rpmio
 * fclose(3) clone.
 */
int Fclose( FD_t fd);

/** \ingroup rpmio
 */
FD_t	Fdopen(FD_t ofd, const char * fmode);

/** \ingroup rpmio
 * fopen(3) clone.
 */
FD_t	Fopen(const char * path,
			const char * fmode);


/** \ingroup rpmio
 * fflush(3) clone.
 */
int Fflush(FD_t fd);

/** \ingroup rpmio
 * ferror(3) clone.
 */
int Ferror(FD_t fd);

/** \ingroup rpmio
 * fileno(3) clone.
 */
int Fileno(FD_t fd);

/** \ingroup rpmio
 * fcntl(2) clone.
 */
int Fcntl(FD_t fd, int op, void *lip);

/** \ingroup rpmio
 * \name RPMIO Utilities.
 */

/** \ingroup rpmio
 */
off_t	fdSize(FD_t fd);

/** \ingroup rpmio
 */
FD_t fdDup(int fdno);

/** \ingroup rpmio
 * Get associated FILE stream from fd (if any)
 */
FILE * fdGetFILE(FD_t fd);

/** \ingroup rpmio
 */
extern FD_t fdLink (void * cookie, const char * msg);

/** \ingroup rpmio
 */
extern FD_t fdFree(FD_t fd, const char * msg);

/** \ingroup rpmio
 */
extern FD_t fdNew (const char * msg);

/** \ingroup rpmio
 */
int fdWritable(FD_t fd, int secs);

/** \ingroup rpmio
 */
int fdReadable(FD_t fd, int secs);

/**
 * FTP and HTTP error codes.
 */
typedef enum ftperrCode_e {
    FTPERR_NE_ERROR		= -1,	/*!< Generic error. */
    FTPERR_NE_LOOKUP		= -2,	/*!< Hostname lookup failed. */
    FTPERR_NE_AUTH		= -3,	/*!< Server authentication failed. */
    FTPERR_NE_PROXYAUTH		= -4,	/*!< Proxy authentication failed. */
    FTPERR_NE_CONNECT		= -5,	/*!< Could not connect to server. */
    FTPERR_NE_TIMEOUT		= -6,	/*!< Connection timed out. */
    FTPERR_NE_FAILED		= -7,	/*!< The precondition failed. */
    FTPERR_NE_RETRY		= -8,	/*!< Retry request. */
    FTPERR_NE_REDIRECT		= -9,	/*!< Redirect received. */

    FTPERR_BAD_SERVER_RESPONSE	= -81,	/*!< Bad server response */
    FTPERR_SERVER_IO_ERROR	= -82,	/*!< Server I/O error */
    FTPERR_SERVER_TIMEOUT	= -83,	/*!< Server timeout */
    FTPERR_BAD_HOST_ADDR	= -84,	/*!< Unable to lookup server host address */
    FTPERR_BAD_HOSTNAME		= -85,	/*!< Unable to lookup server host name */
    FTPERR_FAILED_CONNECT	= -86,	/*!< Failed to connect to server */
    FTPERR_FILE_IO_ERROR	= -87,	/*!< Failed to establish data connection to server */
    FTPERR_PASSIVE_ERROR	= -88,	/*!< I/O error to local file */
    FTPERR_FAILED_DATA_CONNECT	= -89,	/*!< Error setting remote server to passive mode */
    FTPERR_FILE_NOT_FOUND	= -90,	/*!< File not found on server */
    FTPERR_NIC_ABORT_IN_PROGRESS= -91,	/*!< Abort in progress */
    FTPERR_UNKNOWN		= -100	/*!< Unknown or unexpected error */
} ftperrCode;

/**
 */
const char * ftpStrerror(int errorNumber);

/**
 */
int ufdCopy(FD_t sfd, FD_t tfd);

/**
 */
int ufdGetFile( FD_t sfd, FD_t tfd);

/**
 * XXX the name is misleading, this is a legacy wrapper that ensures 
 * only S_ISREG() files are read, nothing to do with timed... 
 * TODO: get this out of the API
 */
int timedRead(FD_t fd, void * bufptr, int length);

/** \ingroup rpmio
 * Identify per-desciptor I/O operation statistics.
 */
typedef enum fdOpX_e {
    FDSTAT_READ		= 0,	/*!< Read statistics index. */
    FDSTAT_WRITE	= 1,	/*!< Write statistics index. */
    FDSTAT_SEEK		= 2,	/*!< Seek statistics index. */
    FDSTAT_CLOSE	= 3,	/*!< Close statistics index */
    FDSTAT_DIGEST	= 4,	/*!< Digest statistics index. */
    FDSTAT_MAX		= 5
} fdOpX;

/** \ingroup rpmio
 *
 */
rpmop fdOp(FD_t fd, fdOpX opx);

#ifdef __cplusplus
}
#endif

#endif	/* H_RPMIO */
