/** \ingroup python
 * \file python/rpmfts-py.c
 */

#include "system.h"

#include "Python.h"
#include "structmember.h"
#ifdef __LCLINT__
#undef  PyObject_HEAD
#define PyObject_HEAD   int _PyObjectHead;
#endif

#include <fts.h>

#include "rpmfts-py.h"

#include "rpmdebug-py.c"

#include <rpmlib.h>	/* XXX _free */

#include "debug.h"

/*@unchecked@*/
static int _rpmfts_debug = 1;

#define	infoBit(_ix)	(1 << (((unsigned)(_ix)) & 0x1f))

static const char * ftsInfoStrings[] = {
    "UNKNOWN",
    "D",
    "DC",
    "DEFAULT",
    "DNR",
    "DOT",
    "DP",
    "ERR",
    "F",
    "INIT",
    "NS",
    "NSOK",
    "SL",
    "SLNONE",
    "W",
};

/*@observer@*/
static const char * ftsInfoStr(int fts_info)
	/*@*/
{
    if (!(fts_info >= 1 && fts_info <= 14))
	fts_info = 0; 
    return ftsInfoStrings[ fts_info ];
}

static void
rpmfts_initialize(rpmftsObject * s, const char * root, int options, int ignore)
	/*@modifies s @*/
{
    int ac = 1;
    char * t;
    size_t nb;

    if (root == NULL)	root = "/";
    if (options == -1)	options = (FTS_COMFOLLOW | FTS_LOGICAL | FTS_NOSTAT);
    if (ignore == -1)	ignore = infoBit(FTS_DP);

    nb = (ac + 1) * sizeof(*s->roots);
    nb += strlen(root) + 1;
    s->roots = malloc(nb);
    t = (char *) &s->roots[ac + 1];
    s->roots[0] = t;
    s->roots[ac] = NULL;
    (void) stpcpy(t, root);
    
    s->options = options;
    s->ignore = ignore;
    s->compare = NULL;

    s->ftsp = NULL;
    s->fts = NULL;
    s->active = 0;

if (_rpmfts_debug < 0)
fprintf(stderr, "*** initialize: %p[0] %p %s %x %x\n", s->roots, s->roots[0], s->roots[0], s->options, s->ignore);

}

static int
rpmfts_inactive(rpmftsObject * s)
	/*@modifies s @*/
{
    int rc = 0;

    if (s->ftsp) {
	Py_BEGIN_ALLOW_THREADS
	rc = Fts_close(s->ftsp);
	Py_END_ALLOW_THREADS
	s->ftsp = NULL;
	s->fts = NULL;
	s->active = 0;
    }
    return rc;
}

static PyObject *
rpmfts_walk(rpmftsObject * s)
	/*@modifies s @*/
{
    PyObject * result = NULL;
    int xx;

    if (s->ftsp == NULL)
	return NULL;

    do {
	Py_BEGIN_ALLOW_THREADS
	s->fts = Fts_read(s->ftsp);
	Py_END_ALLOW_THREADS
    } while (s->fts && (infoBit(s->fts->fts_info) & s->ignore));

    if (s->fts != NULL) {
	Py_INCREF(s);
	result = (PyObject *)s;
    } else {
	if (s->active == 2)
	    xx = rpmfts_inactive(s);
	s->active = 0;
    }
    return result;
}

/* ---------- */

/** \ingroup python
 * \name Class: Rpmfts
 * \class Rpmfts
 * \brief A python rpm.fts object represents an rpm fts(3) handle.
 */

static PyObject *
rpmfts_Debug(/*@unused@*/ rpmftsObject * s, PyObject * args)
	/*@globals _Py_NoneStruct @*/
	/*@modifies _Py_NoneStruct @*/
{
    if (!PyArg_ParseTuple(args, "i:Debug", &_rpmfts_debug))
	return NULL;

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *
rpmfts_iter(rpmftsObject * s)
	/*@*/
{
    Py_INCREF(s);
    return (PyObject *)s;
}

static PyObject *
rpmfts_iternext(rpmftsObject * s)
	/*@modifies s @*/
{
    /* Reset loop indices on 1st entry. */
    if (!s->active) {
	Py_BEGIN_ALLOW_THREADS
	s->ftsp = Fts_open((char *const *)s->roots, s->options, (int (*)())s->compare);
	Py_END_ALLOW_THREADS
	s->fts = NULL;
	s->active = 2;
    }
    return rpmfts_walk(s);
}

static PyObject *
rpmfts_Open(rpmftsObject * s, PyObject * args)
	/*@modifies s @*/
{
    char * root = NULL;
    int options = -1;
    int ignore = -1;

if (_rpmfts_debug)
fprintf(stderr, "*** rpmfts_Open(%p) %d %d ftsp %p fts %p\n", s, s->ob_refcnt, s->active, s->ftsp, s->fts);

    if (!PyArg_ParseTuple(args, "|sii:Open", &root, &options, &ignore))
	return NULL;
    rpmfts_initialize(s, root, options, ignore);

    Py_BEGIN_ALLOW_THREADS
    s->ftsp = Fts_open((char *const *)s->roots, s->options, (int (*)())s->compare);
    Py_END_ALLOW_THREADS
    s->active = 1;

    return (PyObject *)s;
}

static PyObject *
rpmfts_Read(rpmftsObject * s, PyObject * args)
	/*@globals _Py_NoneStruct @*/
	/*@modifies s, _Py_NoneStruct @*/
{
    PyObject * result;

if (_rpmfts_debug)
fprintf(stderr, "*** rpmfts_Read(%p) %d %d ftsp %p fts %p\n", s, s->ob_refcnt, s->active, s->ftsp, s->fts);

    if (!PyArg_ParseTuple(args, ":Read")) return NULL;

    result = rpmfts_walk(s);

    if (result == NULL) {
	Py_INCREF(Py_None);
        return Py_None;
    }

    return result;
}

static PyObject *
rpmfts_Children(rpmftsObject * s, PyObject * args)
	/*@globals _Py_NoneStruct @*/
	/*@modifies s, _Py_NoneStruct @*/
{
    int instr;

if (_rpmfts_debug)
fprintf(stderr, "*** rpmfts_Children(%p) %d %d ftsp %p fts %p\n", s, s->ob_refcnt, s->active, s->ftsp, s->fts);

    if (!PyArg_ParseTuple(args, "i:Children", &instr)) return NULL;

    if (!(s && s->ftsp))
	return NULL;

    Py_BEGIN_ALLOW_THREADS
    s->fts = Fts_children(s->ftsp, instr);
    Py_END_ALLOW_THREADS

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *
rpmfts_Close(rpmftsObject * s, PyObject * args)
	/*@modifies s @*/
{
if (_rpmfts_debug)
fprintf(stderr, "*** rpmfts_Close(%p) %d %d ftsp %p fts %p\n", s, s->ob_refcnt, s->active, s->ftsp, s->fts);

    if (!PyArg_ParseTuple(args, ":Close")) return NULL;

    return Py_BuildValue("i", rpmfts_inactive(s));
}

static PyObject *
rpmfts_Set(rpmftsObject * s, PyObject * args)
	/*@modifies s @*/
{
    int instr = 0;
    int rc = 0;

if (_rpmfts_debug)
fprintf(stderr, "*** rpmfts_Set(%p) %d %d ftsp %p fts %p\n", s, s->ob_refcnt, s->active, s->ftsp, s->fts);

    if (!PyArg_ParseTuple(args, "i:Set", &instr)) return NULL;

    if (s->ftsp && s->fts)
	rc = Fts_set(s->ftsp, s->fts, instr);

    return Py_BuildValue("i", rc);
}

/** \ingroup python
 */
/*@-fullinitblock@*/
/*@unchecked@*/ /*@observer@*/
static struct PyMethodDef rpmfts_methods[] = {
    {"Debug",	(PyCFunction)rpmfts_Debug,	METH_VARARGS,
	NULL},
    {"open",	(PyCFunction)rpmfts_Open,	METH_VARARGS,
	NULL},
    {"read",	(PyCFunction)rpmfts_Read,	METH_VARARGS,
	NULL},
    {"children",(PyCFunction)rpmfts_Children,	METH_VARARGS,
	NULL},
    {"close",	(PyCFunction)rpmfts_Close,	METH_VARARGS,
	NULL},
    {"set",	(PyCFunction)rpmfts_Set,	METH_VARARGS,
	NULL},
    {NULL,		NULL}		/* sentinel */
};
/*@=fullinitblock@*/

/* ---------- */

static PyMemberDef rpmfts_members[] = {
    {"__dict__",T_OBJECT,offsetof(rpmftsObject, md_dict),	READONLY,
	NULL},
    {"callbacks",T_OBJECT,offsetof(rpmftsObject, callbacks),	0,
"Callback dictionary for FTS_{D|DC|DEFAULT|DNR|DOT|DP|ERR|F|INIT|NS|NSOK|SL|SLNONE|W}"}, 
    {"options",	T_INT,	offsetof(rpmftsObject, options),	0,
"Option bit(s): FTS_{COMFOLLOW|LOGICAL|NOCHDIR|NOSTAT|PHYSICAL|SEEDOT|XDEV}"},
    {"ignore",	T_INT,	offsetof(rpmftsObject, ignore),		0,
"Ignore bit(s): (1 << info) with info one of FTS_{D|DC|DEFAULT|DNR|DOT|DP|ERR|F|INIT|NS|NSOK|SL|SLNONE|W}"}, 
    {NULL, 0, 0, 0}
};              

static PyObject * rpmfts_getattro(rpmftsObject * s, PyObject * n)
	/*@*/
{

if (_rpmfts_debug)
fprintf(stderr, "*** rpmfts_getattro(%p[%s],%p[%s]) %d %d ftsp %p fts %p\n", s, lbl(s), n, lbl(n), s->ob_refcnt, s->active, s->ftsp, s->fts);

    return PyObject_GenericGetAttr((PyObject *)s, n);
}

static int rpmfts_setattro(rpmftsObject * s, PyObject * n, PyObject * v)
	/*@*/
{

if (_rpmfts_debug)
fprintf(stderr, "*** rpmfts_setattro(%p[%s],%p[%s],%p[%s]) %d %d ftsp %p fts %p\n", s, lbl(s), n, lbl(n), v, lbl(v), s->ob_refcnt, s->active, s->ftsp, s->fts);

    return PyObject_GenericSetAttr((PyObject *)s, n, v);
}

/* ---------- */

static void rpmfts_free(/*@only@*/ PyObject * s)
	/*@modifies s @*/
{
    _PyObject_GC_Del(s);
}

static PyObject * rpmfts_alloc(PyTypeObject * type, int nitems)
	/*@*/
{
    return PyType_GenericAlloc(type, nitems);
}

static void rpmfts_dealloc(/*@only@*/ rpmftsObject * s)
	/*@modifies s @*/
{
    int xx;

    xx = rpmfts_inactive(s);
    s->roots = _free(s->roots);

    PyObject_GC_UnTrack((PyObject *)s);
    if (s->md_dict != NULL) {
	_PyModule_Clear((PyObject *)s);
	Py_DECREF(s->md_dict);
    }
    if (s->callbacks != NULL) {
	_PyModule_Clear((PyObject *)s);
	Py_DECREF(s->callbacks);
    }
    _PyObject_GC_Del((PyObject *)s);
}

static int rpmfts_init(rpmftsObject * s, PyObject *args, PyObject *kwds)
	/*@*/
{
    char * root = NULL;
    int options = -1;
    int ignore = -1;

    if (!PyArg_ParseTuple(args, "|sii:rpmfts_init", &root, &options, &ignore))
	return -1;
    rpmfts_initialize(s, root, options, ignore);

    return 0;
}

static PyObject * rpmfts_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
	/*@*/
{
    rpmftsObject *s;
    PyObject *o;
    PyObject *n = NULL;

    if ((s = PyObject_GC_New(rpmftsObject, type)) == NULL)
	return NULL;

    s->md_dict = PyDict_New();
    if (s->md_dict == NULL)
	goto fail;
    s->callbacks = PyDict_New();
    if (s->md_dict == NULL)
	goto fail;
    if (type->tp_name) {
	char * name;
	if ((name = strrchr(type->tp_name, '.')) != NULL)
	    name++;
	else
	    name = type->tp_name;
	n = PyString_FromString(name);
    }
    if (n != NULL && PyDict_SetItemString(s->md_dict, "__name__", n) != 0)
	goto fail;
    if (PyDict_SetItemString(s->md_dict, "__doc__", Py_None) != 0)
	goto fail;

#define	CONSTANT(_v) \
    PyDict_SetItemString(s->md_dict, #_v, o=PyInt_FromLong(_v)); Py_DECREF(o)

    CONSTANT(FTS_ROOTPARENTLEVEL);
    CONSTANT(FTS_ROOTLEVEL);

    CONSTANT(FTS_COMFOLLOW);
    CONSTANT(FTS_LOGICAL);
    CONSTANT(FTS_NOCHDIR);
    CONSTANT(FTS_NOSTAT);
    CONSTANT(FTS_PHYSICAL);
    CONSTANT(FTS_SEEDOT);
    CONSTANT(FTS_XDEV);
    CONSTANT(FTS_WHITEOUT);
    CONSTANT(FTS_OPTIONMASK);

    CONSTANT(FTS_NAMEONLY);
    CONSTANT(FTS_STOP);

    CONSTANT(FTS_D);
    CONSTANT(FTS_DC);
    CONSTANT(FTS_DEFAULT);
    CONSTANT(FTS_DNR);
    CONSTANT(FTS_DOT);
    CONSTANT(FTS_DP);
    CONSTANT(FTS_ERR);
    CONSTANT(FTS_F);
    CONSTANT(FTS_NS);
    CONSTANT(FTS_NSOK);
    CONSTANT(FTS_SL);
    CONSTANT(FTS_SLNONE);
    CONSTANT(FTS_W);

    CONSTANT(FTS_DONTCHDIR);
    CONSTANT(FTS_SYMFOLLOW);
    
    CONSTANT(FTS_AGAIN);
    CONSTANT(FTS_FOLLOW);
    CONSTANT(FTS_NOINSTR);
    CONSTANT(FTS_SKIP);

    /* Perform additional initialization. */
    if (rpmfts_init(s, args, kwds) < 0)
	goto fail;

    Py_XDECREF(n);
    PyObject_GC_Track((PyObject *)s);
    return (PyObject *)s;

 fail:
    Py_XDECREF(n);
    Py_DECREF(s);
    return NULL;
}

static int rpmfts_traverse(rpmftsObject * s, visitproc visit, void * arg)
	/*@*/
{
    if (s->md_dict != NULL)
	return visit(s->md_dict, arg);
    if (s->callbacks != NULL)
	return visit(s->callbacks, arg);
    return 0;
}

static int rpmfts_print(rpmftsObject * s, FILE * fp, /*@unused@*/ int flags)
	/*@globals fileSystem @*/
	/*@modifies fp, fileSystem @*/
{
    static int indent = 2;

    if (!(s && s->ftsp && s->fts))
	return -1;
    fprintf(fp, "FTS_%-7s %*s%s", ftsInfoStr(s->fts->fts_info),
	indent * (s->fts->fts_level < 0 ? 0 : s->fts->fts_level), "",
	s->fts->fts_name);
    return 0;

}

/**
 */
/*@unchecked@*/ /*@observer@*/
static char rpmfts_doc[] =
"";

/** \ingroup python
 */
/*@-fullinitblock@*/
PyTypeObject rpmfts_Type = {
	PyObject_HEAD_INIT(&PyType_Type)
	0,				/* ob_size */
	"rpm.fts",			/* tp_name */
	sizeof(rpmftsObject),		/* tp_size */
	0,				/* tp_itemsize */
	/* methods */
	(destructor) rpmfts_dealloc, 	/* tp_dealloc */
	(printfunc) rpmfts_print,	/* tp_print */
	(getattrfunc)0, 		/* tp_getattr */
	(setattrfunc)0,			/* tp_setattr */
	(cmpfunc)0,			/* tp_compare */
	(reprfunc)0,			/* tp_repr */
	0,				/* tp_as_number */
	0,				/* tp_as_sequence */
	0,				/* tp_as_mapping */
	(hashfunc)0,			/* tp_hash */
	(ternaryfunc)0,			/* tp_call */
	(reprfunc)0,			/* tp_str */
	(getattrofunc) rpmfts_getattro,	/* tp_getattro */
	(setattrofunc) rpmfts_setattro,	/* tp_setattro */
	0,				/* tp_as_buffer */
	Py_TPFLAGS_DEFAULT |		/* tp_flags */
		Py_TPFLAGS_HAVE_GC | Py_TPFLAGS_BASETYPE,
	rpmfts_doc,			/* tp_doc */
	(traverseproc) rpmfts_traverse,	/* tp_traverse */
	0,				/* tp_clear */
	0,				/* tp_richcompare */
	0,				/* tp_weaklistoffset */
	(getiterfunc) rpmfts_iter,	/* tp_iter */
	(iternextfunc) rpmfts_iternext,	/* tp_iternext */
	rpmfts_methods,			/* tp_methods */
	rpmfts_members,			/* tp_members */
	0,				/* tp_getset */
	0,				/* tp_base */
	0,				/* tp_dict */
	0,				/* tp_descr_get */
	0,				/* tp_descr_set */
	offsetof(rpmftsObject, md_dict),/* tp_dictoffset */
	(initproc) rpmfts_init,		/* tp_init */
	rpmfts_alloc,			/* tp_alloc */
	rpmfts_new,			/* tp_new */
	rpmfts_free,			/* tp_free */
	0,				/* tp_is_gc */
};
/*@=fullinitblock@*/
