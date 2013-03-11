#ifndef RPMLOCK_H
#define RPMLOCK_H 

#include <rpm/rpmutil.h>

typedef struct rpmlock_s * rpmlock;

enum {
    RPMLOCK_READ   = 1 << 0,
    RPMLOCK_WRITE  = 1 << 1,
    RPMLOCK_WAIT   = 1 << 2,
};

#ifdef __cplusplus
extern "C" {
#endif

RPM_GNUC_INTERNAL
rpmlock rpmlockAcquire(const char *lock_path, const char *descr);

RPM_GNUC_INTERNAL
rpmlock rpmlockFree(rpmlock lock);

#ifdef __cplusplus
}
#endif

#endif
