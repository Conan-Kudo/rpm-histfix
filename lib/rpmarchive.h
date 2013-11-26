#ifndef H_ARCHIVE
#define H_ARCHIVE

/** \ingroup payload
 * \file lib/rpmarchive.h
 */

#define RPMERR_CHECK_ERRNO    -32768

/** \ingroup payload
 * Error codes for archive and file handling
 */
enum rpmfilesErrorCodes {
	RPMERR_ITER_END		= -1,
	RPMERR_BAD_MAGIC	= -2,
	RPMERR_BAD_HEADER	= -3,
	RPMERR_HDR_SIZE	= -4,
	RPMERR_UNKNOWN_FILETYPE= -5,
	RPMERR_MISSING_FILE	= -6,
	RPMERR_DIGEST_MISMATCH	= -7,
	RPMERR_INTERNAL	= -8,
	RPMERR_UNMAPPED_FILE	= -9,
	RPMERR_ENOENT		= -10,
	RPMERR_ENOTEMPTY	= -11,
	RPMERR_FILE_SIZE	= -12,

	RPMERR_OPEN_FAILED	= -32768,
	RPMERR_CHMOD_FAILED	= -32769,
	RPMERR_CHOWN_FAILED	= -32770,
	RPMERR_WRITE_FAILED	= -32771,
	RPMERR_UTIME_FAILED	= -32772,
	RPMERR_UNLINK_FAILED	= -32773,
	RPMERR_RENAME_FAILED	= -32774,
	RPMERR_SYMLINK_FAILED	= -32775,
	RPMERR_STAT_FAILED	= -32776,
	RPMERR_LSTAT_FAILED	= -32777,
	RPMERR_MKDIR_FAILED	= -32778,
	RPMERR_RMDIR_FAILED	= -32779,
	RPMERR_MKNOD_FAILED	= -32780,
	RPMERR_MKFIFO_FAILED	= -32781,
	RPMERR_LINK_FAILED	= -32782,
	RPMERR_READLINK_FAILED	= -32783,
	RPMERR_READ_FAILED	= -32784,
	RPMERR_COPY_FAILED	= -32785,
	RPMERR_LSETFCON_FAILED	= -32786,
	RPMERR_SETCAP_FAILED	= -32787,
};

#ifdef __cplusplus
extern "C" {
#endif

/** \ingroup payload
 * Return formatted error message on payload handling failure.
 * @param rc		error code
 * @return		formatted error string (malloced)
 */
char * rpmfileStrerror(int rc);

#ifdef __cplusplus
}
#endif

#endif	/* H_ARCHIVE */
