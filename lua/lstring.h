/*
** $Id: lstring.h,v 1.2 2004/03/23 05:09:14 jbj Exp $
** String table (keep all strings handled by Lua)
** See Copyright Notice in lua.h
*/

#ifndef lstring_h
#define lstring_h


#include "lobject.h"
#include "lstate.h"



#define sizestring(l)	(cast(lu_mem, sizeof(union TString))+ \
                         (cast(lu_mem, l)+1)*sizeof(char))

#define sizeudata(l)	(cast(lu_mem, sizeof(union Udata))+(l))

#define luaS_new(L, s)	(luaS_newlstr(L, s, strlen(s)))
#define luaS_newliteral(L, s)	(luaS_newlstr(L, "" s, \
                                 (sizeof(s)/sizeof(char))-1))

#define luaS_fix(s)	((s)->tsv.marked |= (1<<4))

void luaS_resize (lua_State *L, int newsize)
	/*@modifies L @*/;
/*@null@*/
Udata *luaS_newudata (lua_State *L, size_t s)
	/*@modifies L @*/;
void luaS_freeall (lua_State *L)
	/*@modifies L @*/;
/*@null@*/
TString *luaS_newlstr (lua_State *L, /*@null@*/ const char *str, size_t l)
	/*@modifies L @*/;


#endif
