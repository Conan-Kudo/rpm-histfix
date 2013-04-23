#ifndef H_CPIO
#define H_CPIO

/** \ingroup payload
 * \file lib/cpio.h
 *  Structures used to handle cpio payloads within rpm packages.
 *
 *  @warning Rpm's cpio implementation may be different than standard cpio.
 *  The implementation is pretty close, but it has some behaviors which are
 *  more to RPM's liking. I tried to document the differing behavior in cpio.c,
 *  but I may have missed some (ewt).
 *
 */

/** \ingroup payload
 * @note CPIO_CHECK_ERRNO bit is set only if errno is valid.
 */
#define CPIOERR_CHECK_ERRNO	0x00008000

/** \ingroup payload
 */
enum cpioErrorReturns {
	CPIOERR_BAD_MAGIC	= 2,
	CPIOERR_BAD_HEADER	= 3,
	CPIOERR_OPEN_FAILED	= 4	| CPIOERR_CHECK_ERRNO,
	CPIOERR_CHMOD_FAILED	= 5	| CPIOERR_CHECK_ERRNO,
	CPIOERR_CHOWN_FAILED	= 6	| CPIOERR_CHECK_ERRNO,
	CPIOERR_WRITE_FAILED	= 7	| CPIOERR_CHECK_ERRNO,
	CPIOERR_UTIME_FAILED	= 8	| CPIOERR_CHECK_ERRNO,
	CPIOERR_UNLINK_FAILED	= 9	| CPIOERR_CHECK_ERRNO,
	CPIOERR_RENAME_FAILED	= 10	| CPIOERR_CHECK_ERRNO,
	CPIOERR_SYMLINK_FAILED	= 11	| CPIOERR_CHECK_ERRNO,
	CPIOERR_STAT_FAILED	= 12	| CPIOERR_CHECK_ERRNO,
	CPIOERR_LSTAT_FAILED	= 13	| CPIOERR_CHECK_ERRNO,
	CPIOERR_MKDIR_FAILED	= 14	| CPIOERR_CHECK_ERRNO,
	CPIOERR_RMDIR_FAILED	= 15	| CPIOERR_CHECK_ERRNO,
	CPIOERR_MKNOD_FAILED	= 16	| CPIOERR_CHECK_ERRNO,
	CPIOERR_MKFIFO_FAILED	= 17	| CPIOERR_CHECK_ERRNO,
	CPIOERR_LINK_FAILED	= 18	| CPIOERR_CHECK_ERRNO,
	CPIOERR_READLINK_FAILED	= 19	| CPIOERR_CHECK_ERRNO,
	CPIOERR_READ_FAILED	= 20	| CPIOERR_CHECK_ERRNO,
	CPIOERR_COPY_FAILED	= 21	| CPIOERR_CHECK_ERRNO,
	CPIOERR_LSETFCON_FAILED	= 22	| CPIOERR_CHECK_ERRNO,
	CPIOERR_HDR_SIZE	= 23,
	CPIOERR_HDR_TRAILER	= 24,
	CPIOERR_UNKNOWN_FILETYPE= 25,
	CPIOERR_MISSING_HARDLINK= 26,
	CPIOERR_DIGEST_MISMATCH	= 27,
	CPIOERR_INTERNAL	= 28,
	CPIOERR_UNMAPPED_FILE	= 29,
	CPIOERR_ENOENT		= 30,
	CPIOERR_ENOTEMPTY	= 31,
	CPIOERR_SETCAP_FAILED	= 32	| CPIOERR_CHECK_ERRNO,
	CPIOERR_FILE_SIZE	= 33,
};


typedef struct rpmcpio_s * rpmcpio_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Create CPIO file object
 * @param fd		file
 * @param mode		XXX
 * @return CPIO object
 **/
rpmcpio_t rpmcpioOpen(FD_t fd, char mode);

int rpmcpioClose(rpmcpio_t cpio);

off_t rpmcpioTell(rpmcpio_t cpio);

rpmcpio_t rpmcpioFree(rpmcpio_t cpio);

/**
 * Write cpio header.
 * @retval fsm		file path and stat info
 * @param st
 * @return		0 on success
 */
RPM_GNUC_INTERNAL
int rpmcpioHeaderWrite(rpmcpio_t cpio, char * path, struct stat * st);
RPM_GNUC_INTERNAL
int rpmcpioStrippedHeaderWrite(rpmcpio_t cpio, int fx, off_t fsize);

ssize_t rpmcpioWrite(rpmcpio_t cpio, void * buf, size_t size);

/**
 * Read cpio header. Iff fx is returned as -1 a cpio header was read
 * and the file name is found in path. Otherwise a stripped header was read
 * and the fx is the number of the file in the header/rpmfi. In this case
 * rpmcpioSetExpectedFileSize() needs to be called with the file size of the
 * payload content - with may be zero for hard links, directory or other
 * special files.
 * @retval fsm		file path and stat info
 * @retval path		path of the file
 * @retval fx		number in the header of the file read
 * @return		0 on success
 */
RPM_GNUC_INTERNAL
int rpmcpioHeaderRead(rpmcpio_t cpio, char ** path, int * fx);

/**
 * Tell the cpio object the expected file size in the payload.
 * The size must be zero for all but the last of hard linked files,
 * directories and special files.
 * This is needed after reading a stripped cpio header! See above.
 */
RPM_GNUC_INTERNAL
void rpmcpioSetExpectedFileSize(rpmcpio_t cpio, off_t fsize);

ssize_t rpmcpioRead(rpmcpio_t cpio, void * buf, size_t size);

/** \ingroup payload
 * Return formatted error message on payload handling failure.
 * @param rc		error code
 * @return		formatted error string (malloced)
 */
/* XXX should be RPM_GNUC_INTERNAL too but build/pack.c uses */
char * rpmcpioStrerror(int rc);

#ifdef __cplusplus
}
#endif

#endif	/* H_CPIO */
