/*
** $Id: ltm.h,v 1.2 2004/03/23 05:09:14 jbj Exp $
** Tag methods
** See Copyright Notice in lua.h
*/

#ifndef ltm_h
#define ltm_h


#include "lobject.h"


/*
* WARNING: if you change the order of this enumeration,
* grep "ORDER TM"
*/
typedef enum {
  TM_INDEX,
  TM_NEWINDEX,
  TM_GC,
  TM_MODE,
  TM_EQ,  /* last tag method with `fast' access */
  TM_ADD,
  TM_SUB,
  TM_MUL,
  TM_DIV,
  TM_POW,
  TM_UNM,
  TM_LT,
  TM_LE,
  TM_CONCAT,
  TM_CALL,
  TM_N		/* number of elements in the enum */
} TMS;



#define gfasttm(g,et,e) \
  (((et)->flags & (1u<<(e))) ? NULL : luaT_gettm(et, e, (g)->tmname[e]))

#define fasttm(l,et,e)	gfasttm(G(l), et, e)


/*@observer@*/ /*@null@*/
const TObject *luaT_gettm (Table *events, TMS event, TString *ename)
	/*@modifies events @*/;
/*@observer@*/
const TObject *luaT_gettmbyobj (lua_State *L, const TObject *o, TMS event)
	/*@*/;
void luaT_init (lua_State *L)
	/*@modifies L @*/;

/*@unchecked@*/
extern const char *const luaT_typenames[];

#endif
