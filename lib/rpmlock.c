
#include "system.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#include <rpmlib.h>

#include "rpmts.h"

#include "rpmlock.h"

/* Internal interface */

#define RPMLOCK_FILE "/var/lock/rpm/transaction"

/*@observer@*/ /*@unchecked@*/
static const char * _rpmlock_file = RPMLOCK_FILE;

enum {
	RPMLOCK_READ   = 1 << 0,
	RPMLOCK_WRITE  = 1 << 1,
	RPMLOCK_WAIT   = 1 << 2,
};

typedef struct {
	int fd;
	int openmode;
} * rpmlock;

/*@null@*/
static rpmlock rpmlock_new(const char *rootdir)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/
{
	rpmlock lock = (rpmlock) malloc(sizeof(*lock));
	if (lock) {
		mode_t oldmask = umask(022);
		lock->fd = open(RPMLOCK_FILE, O_RDWR|O_CREAT, 0644);
		(void) umask(oldmask);

/*@-branchstate@*/
		if (lock->fd == -1) {
			lock->fd = open(RPMLOCK_FILE, O_RDONLY);
			if (lock->fd == -1) {
				free(lock);
				lock = NULL;
			} else {
				lock->openmode = RPMLOCK_READ;
			}
		} else {
			lock->openmode = RPMLOCK_WRITE | RPMLOCK_READ;
		}
/*@=branchstate@*/
	}
/*@-compdef@*/
	return lock;
/*@=compdef@*/
}

static void rpmlock_free(/*@only@*/ /*@null@*/ rpmlock lock)
	/*@globals fileSystem, internalState @*/
	/*@modifies lock, fileSystem, internalState @*/
{
	if (lock) {
		(void) close(lock->fd);
		free(lock);
	}
}

static int rpmlock_acquire(/*@null@*/ rpmlock lock, int mode)
	/*@*/
{
	int res = 0;
	if (lock && (mode & lock->openmode)) {
		struct flock info;
		int cmd;
		if (mode & RPMLOCK_WAIT)
			cmd = F_SETLKW;
		else
			cmd = F_SETLK;
		if (mode & RPMLOCK_READ)
			info.l_type = F_RDLCK;
		else
			info.l_type = F_WRLCK;
		info.l_whence = SEEK_SET;
		info.l_start = 0;
		info.l_len = 0;
		info.l_pid = 0;
		if (fcntl(lock->fd, cmd, &info) != -1)
			res = 1;
	}
	return res;
}

static void rpmlock_release(/*@null@*/ rpmlock lock)
	/*@globals internalState @*/
	/*@modifies internalState @*/
{
	if (lock) {
		struct flock info;
		info.l_type = F_UNLCK;
		info.l_whence = SEEK_SET;
		info.l_start = 0;
		info.l_len = 0;
		info.l_pid = 0;
		(void) fcntl(lock->fd, F_SETLK, &info);
	}
}


/* External interface */

void *rpmtsAcquireLock(rpmts ts)
{
	const char *rootDir = rpmtsRootDir(ts);
	rpmlock lock;

	if (!rootDir)
		rootDir = "/";
	lock = rpmlock_new(rootDir);
/*@-branchstate@*/
	if (!lock) {
		rpmMessage(RPMMESS_ERROR, _("can't create transaction lock\n"));
	} else if (!rpmlock_acquire(lock, RPMLOCK_WRITE)) {
		if (lock->openmode & RPMLOCK_WRITE)
			rpmMessage(RPMMESS_WARNING,
				   _("waiting for transaction lock\n"));
		if (!rpmlock_acquire(lock, RPMLOCK_WRITE|RPMLOCK_WAIT)) {
			rpmMessage(RPMMESS_ERROR,
				   _("can't create transaction lock\n"));
			rpmlock_free(lock);
			lock = NULL;
		}
	}
/*@=branchstate@*/
	return lock;
}

void rpmtsFreeLock(void *lock)
{
	rpmlock_release((rpmlock)lock); /* Not really needed here. */
	rpmlock_free((rpmlock)lock);
}


