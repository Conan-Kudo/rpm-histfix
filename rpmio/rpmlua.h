#ifndef RPMLUA_H
#define RPMLUA_H

typedef enum rpmluavType_e {
    RPMLUAV_NIL		= 0,
    RPMLUAV_STRING	= 1,
    RPMLUAV_NUMBER	= 2
} rpmluavType;

#if defined(_RPMLUA_INTERNAL)

#include <stdarg.h>
#include <lua.h>

struct rpmlua_s {
    lua_State *L;
    int pushsize;
    int storeprint;
    int printbufsize;
    int printbufused;
    char *printbuf;
};

struct rpmluav_s {
    rpmluavType keyType;
    rpmluavType valueType;
    union {
	const char *str;
	const void *ptr;
	double num;
    } key;
    union {
	const char *str;
	const void *ptr;
	double num;
    } value;
    int listmode;
};

#endif /* _RPMLUA_INTERNAL */

typedef /*@abstract@*/ struct rpmlua_s * rpmlua;
typedef /*@abstract@*/ struct rpmluav_s * rpmluav;

/*@-exportlocal@*/
/*@only@*/
rpmlua rpmluaNew(void)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/;
/*@=exportlocal@*/
void *rpmluaFree(/*@only@*/ rpmlua lua)
	/*@modifies lua @*/;

int rpmluaCheckScript(/*@null@*/ rpmlua lua, const char *script,
		      /*@null@*/ const char *name)
	/*@globals fileSystem @*/
	/*@modifies lua, fileSystem @*/;
int rpmluaRunScript(/*@null@*/ rpmlua lua, const char *script,
		    /*@null@*/ const char *name)
	/*@globals fileSystem @*/
	/*@modifies lua, fileSystem @*/;
/*@-exportlocal@*/
int rpmluaRunScriptFile(/*@null@*/ rpmlua lua, const char *filename)
	/*@globals fileSystem @*/
	/*@modifies lua, fileSystem @*/;
/*@=exportlocal@*/
void rpmluaInteractive(/*@null@*/ rpmlua lua)
	/*@globals fileSystem @*/
	/*@modifies lua, fileSystem @*/;

void *rpmluaGetData(/*@null@*/ rpmlua lua, const char *key)
	/*@globals fileSystem @*/
	/*@modifies lua, fileSystem @*/;
/*@-exportlocal@*/
void rpmluaSetData(/*@null@*/ rpmlua lua, const char *key, const void *data)
	/*@globals fileSystem @*/
	/*@modifies lua, fileSystem @*/;
/*@=exportlocal@*/

/*@exposed@*/
const char *rpmluaGetPrintBuffer(/*@null@*/ rpmlua lua)
	/*@globals fileSystem @*/
	/*@modifies lua, fileSystem @*/;
void rpmluaSetPrintBuffer(/*@null@*/ rpmlua lua, int flag)
	/*@globals fileSystem @*/
	/*@modifies lua, fileSystem @*/;

void rpmluaGetVar(/*@null@*/ rpmlua lua, rpmluav var)
	/*@globals fileSystem @*/
	/*@modifies lua, var, fileSystem @*/;
void rpmluaSetVar(/*@null@*/ rpmlua lua, rpmluav var)
	/*@globals fileSystem @*/
	/*@modifies lua, var, fileSystem @*/;
void rpmluaDelVar(/*@null@*/ rpmlua lua, const char *key, ...)
	/*@globals fileSystem @*/
	/*@modifies lua, fileSystem @*/;
int rpmluaVarExists(/*@null@*/ rpmlua lua, const char *key, ...)
	/*@globals fileSystem @*/
	/*@modifies lua, fileSystem @*/;
void rpmluaPushTable(/*@null@*/ rpmlua lua, const char *key, ...)
	/*@globals fileSystem @*/
	/*@modifies lua, fileSystem @*/;
void rpmluaPop(/*@null@*/ rpmlua lua)
	/*@globals fileSystem @*/
	/*@modifies lua, fileSystem @*/;

/*@only@*/
rpmluav rpmluavNew(void)
	/*@*/;
void * rpmluavFree(/*@only@*/ rpmluav var)
	/*@modifes var @*/;
void rpmluavSetListMode(rpmluav var, int flag)
	/*@modifies var @*/;
/*@-exportlocal@*/
void rpmluavSetKey(rpmluav var, rpmluavType type, const void *value)
	/*@modifies var @*/;
/*@=exportlocal@*/
/*@-exportlocal@*/
void rpmluavSetValue(rpmluav var, rpmluavType type, const void *value)
	/*@modifies var @*/;
/*@=exportlocal@*/
/*@-exportlocal@*/
void rpmluavGetKey(rpmluav var, /*@out@*/ rpmluavType *type, /*@out@*/ void **value)
	/*@modifies *type, *value @*/;
/*@=exportlocal@*/
/*@-exportlocal@*/
void rpmluavGetValue(rpmluav var, /*@out@*/ rpmluavType *type, /*@out@*/ void **value)
	/*@modifies *type, *value @*/;
/*@=exportlocal@*/

/* Optional helpers for numbers. */
void rpmluavSetKeyNum(rpmluav var, double value)
	/*@modifies var @*/;
void rpmluavSetValueNum(rpmluav var, double value)
	/*@modifies var @*/;
double rpmluavGetKeyNum(rpmluav var)
	/*@*/;
double rpmluavGetValueNum(rpmluav var)
	/*@*/;
int rpmluavKeyIsNum(rpmluav var)
	/*@*/;
int rpmluavValueIsNum(rpmluav var)
	/*@*/;

#endif /* RPMLUA_H */
