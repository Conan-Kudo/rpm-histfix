#ifndef _RPMSTRPOOL_H
#define _RPMSTRPOOL_H

typedef uint32_t rpmsid;
typedef struct rpmstrPool_s * rpmstrPool;

#ifdef __cplusplus
extern "C" {
#endif

/* XXX TODO: properly document... */

/* create a new string pool */
rpmstrPool rpmstrPoolCreate(void);

/* destroy a string pool (refcounted) */
rpmstrPool rpmstrPoolFree(rpmstrPool sidpool);

/* reference a string pool */
rpmstrPool rpmstrPoolLink(rpmstrPool sidpool);

/* freeze pool to free memory */
void rpmstrPoolFreeze(rpmstrPool sidpool);

/* unfreeze pool (ie recreate hash table) */
void rpmstrPoolUnfreeze(rpmstrPool sidpool);

/* get the id of a string, optionally storing if not already present */
rpmsid rpmstrPoolId(rpmstrPool sidpool, const char *s, int create);

/* get the id of a string + length, optionally storing if not already present */
rpmsid rpmstrPoolIdn(rpmstrPool sidpool, const char *s, size_t slen, int create);

/* get a string by its id */
const char * rpmstrPoolStr(rpmstrPool sidpool, rpmsid sid);

/* get a strings length by its id (in constant time) */
size_t rpmstrPoolStrlen(rpmstrPool pool, rpmsid sid);

#ifdef __cplusplus
}
#endif

#endif /* _RPMSIDPOOL_H */
