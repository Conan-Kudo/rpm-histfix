/** \ingroup py_c
 * \file python/header-py.c
 */

#include "system.h"

#include "Python.h"
#ifdef __LCLINT__
#undef  PyObject_HEAD
#define PyObject_HEAD   int _PyObjectHead;
#endif

#include "rpmio_internal.h"
#include "rpmcli.h"	/* XXX for rpmCheckSig */

#include "legacy.h"
#include "misc.h"
#include "header_internal.h"

#include "rpmts.h"	/* XXX rpmtsCreate/rpmtsFree */

#include "header-py.h"
#include "rpmds-py.h"
#include "rpmfi-py.h"

#include "debug.h"

/** \ingroup python
 * \class Rpm
 * \brief START HERE / RPM base module for the Python API
 *
 * The rpm base module provides the main starting point for
 * accessing RPM from Python. For most usage, call
 * the TransactionSet method to get a transaction set (rpmts).
 *
 * For example:
 * \code
 *	import rpm
 *
 *	ts = rpm.TransactionSet()
 * \endcode
 *
 * The transaction set will open the RPM database as needed, so
 * in most cases, you do not need to explicitly open the
 * database. The transaction set is the workhorse of RPM.
 *
 * You can open another RPM database, such as one that holds
 * all packages for a given Linux distribution, to provide
 * packages used to solve dependencies. To do this, use
 * the following code:
 *
 * \code
 * rpm.addMacro('_dbpath', '/path/to/alternate/database')
 * solvets = rpm.TransactionSet()
 * solvets.openDB()
 * rpm.delMacro('_dbpath')
 *
 * # Open default database
 * ts = rpm.TransactionSet()
 * \endcode
 *
 * This code gives you access to two RPM databases through
 * two transaction sets (rpmts): ts is a transaction set
 * associated with the default RPM database and solvets
 * is a transaction set tied to an alternate database, which
 * is very useful for resolving dependencies.
 *
 * The rpm methods used here are:
 *
 * - addMacro(macro, value)
 * @param macro   Name of macro to add
 * @param value   Value for the macro
 *
 * - delMacro(macro)
 * @param macro   Name of macro to delete
 *
 */

/** \ingroup python
 * \class Rpmhdr
 * \brief A python header object represents an RPM package header.
 *
 * All RPM packages have headers that provide metadata for the package.
 * Header objects can be returned by database queries or loaded from a
 * binary package on disk.
 *
 * The ts.hdrFromFdno() function returns the package header from a
 * package on disk, verifying package signatures and digests of the
 * package while reading.
 *
 * Note: The older method rpm.headerFromPackage() which has been replaced
 * by ts.hdrFromFdno() used to return a (hdr, isSource) tuple.
 *
 * If you need to distinguish source/binary headers, do:
 * \code
 * 	import os, rpm
 *
 *	ts = rpm.TranssactionSet()
 * 	fdno = os.open("/tmp/foo-1.0-1.i386.rpm", os.O_RDONLY)
 * 	hdr = ts.hdrFromFdno(fdno)
 *	os.close(fdno)
 *	if hdr[rpm.RPMTAG_SOURCEPACKAGE]:
 *	   print "header is from a source package"
 *	else:
 *	   print "header is from a binary package"
 * \endcode
 *
 * The Python interface to the header data is quite elegant.  It
 * presents the data in a dictionary form.  We'll take the header we
 * just loaded and access the data within it:
 * \code
 * 	print hdr[rpm.RPMTAG_NAME]
 * 	print hdr[rpm.RPMTAG_VERSION]
 * 	print hdr[rpm.RPMTAG_RELEASE]
 * \endcode
 * in the case of our "foo-1.0-1.i386.rpm" package, this code would
 * output:
\verbatim
  	foo
  	1.0
  	1
\endverbatim
 *
 * You make also access the header data by string name:
 * \code
 * 	print hdr['name']
 * 	print hdr['version']
 * 	print hdr['release']
 * \endcode
 *
 * This method of access is a teensy bit slower because the name must be
 * translated into the tag number dynamically. You also must make sure
 * the strings in header lookups don't get translated, or the lookups
 * will fail.
 */

/** \ingroup python
 * \name Class: rpm.hdr
 */
/*@{*/

/** \ingroup py_c
 */
struct hdrObject_s {
    PyObject_HEAD
    Header h;
    char ** md5list;
    char ** fileList;
    char ** linkList;
    int_32 * fileSizes;
    int_32 * mtimes;
    int_32 * uids, * gids;	/* XXX these tags are not used anymore */
    unsigned short * rdevs;
    unsigned short * modes;
} ;

/*@unused@*/ static inline Header headerAllocated(Header h)
	/*@modifies h @*/
{
    h->flags |= HEADERFLAG_ALLOCATED;
    return 0;
}

/** \ingroup py_c
 */
static PyObject * hdrKeyList(hdrObject * s, PyObject * args)
	/*@*/
{
    PyObject * list, *o;
    HeaderIterator hi;
    int tag, type;

    if (!PyArg_ParseTuple(args, "")) return NULL;

    list = PyList_New(0);

    hi = headerInitIterator(s->h);
    while (headerNextIterator(hi, &tag, &type, NULL, NULL)) {
        if (tag == HEADER_I18NTABLE) continue;

	switch (type) {
	case RPM_BIN_TYPE:
	case RPM_INT32_TYPE:
	case RPM_CHAR_TYPE:
	case RPM_INT8_TYPE:
	case RPM_INT16_TYPE:
	case RPM_STRING_ARRAY_TYPE:
	case RPM_STRING_TYPE:
	    PyList_Append(list, o=PyInt_FromLong(tag));
	    Py_DECREF(o);
	}
    }
    headerFreeIterator(hi);

    return list;
}

/** \ingroup py_c
 */
static PyObject * hdrUnload(hdrObject * s, PyObject * args, PyObject *keywords)
	/*@*/
{
    char * buf;
    PyObject * rc;
    int len, legacy = 0;
    Header h;
    static char *kwlist[] = { "legacyHeader", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, keywords, "|i", kwlist, &legacy))
	return NULL;

    h = headerLink(s->h);
    /* XXX this legacy switch is a hack, needs to be removed. */
    if (legacy) {
	h = headerCopy(s->h);	/* XXX strip region tags, etc */
	headerFree(s->h);
    }
    len = headerSizeof(h, 0);
    buf = headerUnload(h);
    h = headerFree(h);

    if (buf == NULL || len == 0) {
	PyErr_SetString(pyrpmError, "can't unload bad header\n");
	return NULL;
    }

    rc = PyString_FromStringAndSize(buf, len);
    buf = _free(buf);

    return rc;
}

/** \ingroup py_c
 */
static PyObject * hdrExpandFilelist(hdrObject * s, PyObject * args)
	/*@*/
{
    expandFilelist (s->h);

    Py_INCREF(Py_None);
    return Py_None;
}

/** \ingroup py_c
 */
static PyObject * hdrCompressFilelist(hdrObject * s, PyObject * args)
	/*@*/
{
    compressFilelist (s->h);

    Py_INCREF(Py_None);
    return Py_None;
}

/* make a header with _all_ the tags we need */
/** \ingroup py_c
 */
static void mungeFilelist(Header h)
	/*@*/
{
    const char ** fileNames = NULL;
    int count = 0;

    if (!headerIsEntry (h, RPMTAG_BASENAMES)
	|| !headerIsEntry (h, RPMTAG_DIRNAMES)
	|| !headerIsEntry (h, RPMTAG_DIRINDEXES))
	compressFilelist(h);

    rpmfiBuildFNames(h, RPMTAG_BASENAMES, &fileNames, &count);

    if (fileNames == NULL || count <= 0)
	return;

    /* XXX Legacy tag needs to go away. */
    headerAddEntry(h, RPMTAG_OLDFILENAMES, RPM_STRING_ARRAY_TYPE,
			fileNames, count);

    fileNames = _free(fileNames);
}

/**
 */
static PyObject * rhnUnload(hdrObject * s, PyObject * args)
	/*@*/
{
    int len;
    char * uh;
    PyObject * rc;
    Header h;

    if (!PyArg_ParseTuple(args, ""))
        return NULL;

    h = headerLink(s->h);

    /* Retrofit a RHNPlatform: tag. */
    if (!headerIsEntry(h, RPMTAG_RHNPLATFORM)) {
	const char * arch;
	int_32 at;
	if (headerGetEntry(h, RPMTAG_ARCH, &at, (void **)&arch, NULL))
	    headerAddEntry(h, RPMTAG_RHNPLATFORM, at, arch, 1);
    }

    /* Legacy headers are forced into immutable region. */
    if (!headerIsEntry(h, RPMTAG_HEADERIMMUTABLE)) {
	Header nh = headerReload(h, RPMTAG_HEADERIMMUTABLE);
	/* XXX Another unload/load cycle to "seal" the immutable region. */
	uh = headerUnload(nh);
	headerFree(nh);
	h = headerLoad(uh);
	headerAllocated(h);
    }

    /* All headers have SHA1 digest, compute and add if necessary. */
    if (!headerIsEntry(h, RPMTAG_SHA1HEADER)) {
	int_32 uht, uhc;
	const char * digest;
        size_t digestlen;
        DIGEST_CTX ctx;

	headerGetEntry(h, RPMTAG_HEADERIMMUTABLE, &uht, (void **)&uh, &uhc);

	ctx = rpmDigestInit(PGPHASHALGO_SHA1, RPMDIGEST_NONE);
        rpmDigestUpdate(ctx, uh, uhc);
        rpmDigestFinal(ctx, (void **)&digest, &digestlen, 1);

	headerAddEntry(h, RPMTAG_SHA1RHN, RPM_STRING_TYPE, digest, 1);

	uh = headerFreeData(uh, uht);
	digest = _free(digest);
    }

    len = headerSizeof(h, 0);
    uh = headerUnload(h);
    headerFree(h);

    rc = PyString_FromStringAndSize(uh, len);
    uh = _free(uh);

    return rc;
}

/** \ingroup py_c
 */
static PyObject * hdrFullFilelist(hdrObject * s, PyObject * args)
	/*@*/
{
    if (!PyArg_ParseTuple(args, ""))
	return NULL;

    mungeFilelist (s->h);

    Py_INCREF(Py_None);
    return Py_None;
}

/** \ingroup py_c
 */
static PyObject * hdrSprintf(hdrObject * s, PyObject * args)
	/*@*/
{
    char * fmt;
    char * r;
    errmsg_t err;
    PyObject * result;

    if (!PyArg_ParseTuple(args, "s", &fmt))
	return NULL;

    r = headerSprintf(s->h, fmt, rpmTagTable, rpmHeaderFormats, &err);
    if (!r) {
	PyErr_SetString(pyrpmError, err);
	return NULL;
    }

    result = Py_BuildValue("s", r);
    r = _free(r);

    return result;
}

/**
 */
static int hdr_compare(hdrObject * a, hdrObject * b)
	/*@*/
{
    return rpmVersionCompare(a->h, b->h);
}

static long hdr_hash(PyObject * h)
{
    return (long) h;
}

/** \ingroup py_c
 */
/*@unchecked@*/ /*@observer@*/
static struct PyMethodDef hdr_methods[] = {
    {"keys",		(PyCFunction) hdrKeyList,	METH_VARARGS,
	NULL },
    {"unload",		(PyCFunction) hdrUnload,	METH_VARARGS|METH_KEYWORDS,
	NULL },
    {"expandFilelist",	(PyCFunction) hdrExpandFilelist,METH_VARARGS,
	NULL },
    {"compressFilelist",(PyCFunction) hdrCompressFilelist,METH_VARARGS,
	NULL },
    {"fullFilelist",	(PyCFunction) hdrFullFilelist,	METH_VARARGS,
	NULL },
    {"rhnUnload",	(PyCFunction) rhnUnload,	METH_VARARGS,
	NULL },
    {"sprintf",		(PyCFunction) hdrSprintf,	METH_VARARGS,
	NULL },

    {"dsOfHeader",	(PyCFunction)hdr_dsOfHeader,	METH_VARARGS,
	NULL},
    {"dsFromHeader",	(PyCFunction)hdr_dsFromHeader,	METH_VARARGS,
	NULL},
    {"fiFromHeader",	(PyCFunction)hdr_fiFromHeader,	METH_VARARGS,
	NULL},

    {NULL,		NULL}		/* sentinel */
};

static PyObject * hdr_getattro(PyObject * o, PyObject * n)
	/*@*/
{
    return PyObject_GenericGetAttr(o, n);
}

static int hdr_setattro(PyObject * o, PyObject * n, PyObject * v)
	/*@*/
{
    return PyObject_GenericSetAttr(o, n, v);
}


/** \ingroup py_c
 */
static void hdr_dealloc(hdrObject * s)
	/*@*/
{
    if (s->h) headerFree(s->h);
    s->md5list = _free(s->md5list);
    s->fileList = _free(s->fileList);
    s->linkList = _free(s->linkList);
    PyObject_Del(s);
}

/** \ingroup py_c
 */
long tagNumFromPyObject (PyObject *item)
{
    char * str;
    int i;

    if (PyInt_Check(item)) {
	return PyInt_AsLong(item);
    } else if (PyString_Check(item)) {
	str = PyString_AsString(item);
	for (i = 0; i < rpmTagTableSize; i++)
	    if (!xstrcasecmp(rpmTagTable[i].name + 7, str)) break;
	if (i < rpmTagTableSize) return rpmTagTable[i].val;
    }
    return -1;
}

/** \ingroup py_c
 */
static PyObject * hdr_subscript(hdrObject * s, PyObject * item)
	/*@*/
{
    int type, count, i, tag = -1;
    void * data;
    PyObject * o, * metao;
    char ** stringArray;
    int forceArray = 0;
    int freeData = 0;
    char * str;
    struct headerSprintfExtension_s * ext = NULL;
    const struct headerSprintfExtension_s * extensions = rpmHeaderFormats;

    if (PyCObject_Check (item))
        ext = PyCObject_AsVoidPtr(item);
    else
	tag = tagNumFromPyObject (item);
    if (tag == -1 && PyString_Check(item)) {
	/* if we still don't have the tag, go looking for the header
	   extensions */
	str = PyString_AsString(item);
	while (extensions->name) {
	    if (extensions->type == HEADER_EXT_TAG
		&& !xstrcasecmp(extensions->name + 7, str)) {
		(const struct headerSprintfExtension *) ext = extensions;
	    }
	    extensions++;
	}
    }

    if (ext) {
        ext->u.tagFunction(s->h, &type, (const void **) &data, &count, &freeData);
    } else {
        if (tag == -1) {
            PyErr_SetString(PyExc_KeyError, "unknown header tag");
            return NULL;
        }

        if (!rpmHeaderGetEntry(s->h, tag, &type, &data, &count)) {
            Py_INCREF(Py_None);
            return Py_None;
        }
    }

    switch (tag) {
    case RPMTAG_OLDFILENAMES:
    case RPMTAG_FILESIZES:
    case RPMTAG_FILESTATES:
    case RPMTAG_FILEMODES:
    case RPMTAG_FILEUIDS:
    case RPMTAG_FILEGIDS:
    case RPMTAG_FILERDEVS:
    case RPMTAG_FILEMTIMES:
    case RPMTAG_FILEMD5S:
    case RPMTAG_FILELINKTOS:
    case RPMTAG_FILEFLAGS:
    case RPMTAG_ROOT:
    case RPMTAG_FILEUSERNAME:
    case RPMTAG_FILEGROUPNAME:
	forceArray = 1;
	break;
    case RPMTAG_SUMMARY:
    case RPMTAG_GROUP:
    case RPMTAG_DESCRIPTION:
	freeData = 1;
	break;
    default:
        break;
    }

    switch (type) {
    case RPM_BIN_TYPE:
	o = PyString_FromStringAndSize(data, count);
	break;

    case RPM_INT32_TYPE:
	if (count != 1 || forceArray) {
	    metao = PyList_New(0);
	    for (i = 0; i < count; i++) {
		o = PyInt_FromLong(((int *) data)[i]);
		PyList_Append(metao, o);
		Py_DECREF(o);
	    }
	    o = metao;
	} else {
	    o = PyInt_FromLong(*((int *) data));
	}
	break;

    case RPM_CHAR_TYPE:
    case RPM_INT8_TYPE:
	if (count != 1 || forceArray) {
	    metao = PyList_New(0);
	    for (i = 0; i < count; i++) {
		o = PyInt_FromLong(((char *) data)[i]);
		PyList_Append(metao, o);
		Py_DECREF(o);
	    }
	    o = metao;
	} else {
	    o = PyInt_FromLong(*((char *) data));
	}
	break;

    case RPM_INT16_TYPE:
	if (count != 1 || forceArray) {
	    metao = PyList_New(0);
	    for (i = 0; i < count; i++) {
		o = PyInt_FromLong(((short *) data)[i]);
		PyList_Append(metao, o);
		Py_DECREF(o);
	    }
	    o = metao;
	} else {
	    o = PyInt_FromLong(*((short *) data));
	}
	break;

    case RPM_STRING_ARRAY_TYPE:
	stringArray = data;

	metao = PyList_New(0);
	for (i = 0; i < count; i++) {
	    o = PyString_FromString(stringArray[i]);
	    PyList_Append(metao, o);
	    Py_DECREF(o);
	}
	free (stringArray);
	o = metao;
	break;

    case RPM_STRING_TYPE:
	if (count != 1 || forceArray) {
	    stringArray = data;

	    metao = PyList_New(0);
	    for (i=0; i < count; i++) {
		o = PyString_FromString(stringArray[i]);
		PyList_Append(metao, o);
		Py_DECREF(o);
	    }
	    o = metao;
	} else {
	    o = PyString_FromString(data);
	    if (freeData)
		free (data);
	}
	break;

    default:
	PyErr_SetString(PyExc_TypeError, "unsupported type in header");
	return NULL;
    }

    return o;
}

/** \ingroup py_c
 */
/*@unchecked@*/ /*@observer@*/
static PyMappingMethods hdr_as_mapping = {
	(inquiry) 0,			/* mp_length */
	(binaryfunc) hdr_subscript,	/* mp_subscript */
	(objobjargproc)0,		/* mp_ass_subscript */
};

/**
 */
static char hdr_doc[] =
"";

/** \ingroup py_c
 */
/*@unchecked@*/ /*@observer@*/
PyTypeObject hdr_Type = {
	PyObject_HEAD_INIT(&PyType_Type)
	0,				/* ob_size */
	"rpm.hdr",			/* tp_name */
	sizeof(hdrObject),		/* tp_size */
	0,				/* tp_itemsize */
	(destructor) hdr_dealloc, 	/* tp_dealloc */
	0,				/* tp_print */
	(getattrfunc) 0, 		/* tp_getattr */
	0,				/* tp_setattr */
	(cmpfunc) hdr_compare,		/* tp_compare */
	0,				/* tp_repr */
	0,				/* tp_as_number */
	0,	 			/* tp_as_sequence */
	&hdr_as_mapping,		/* tp_as_mapping */
	hdr_hash,			/* tp_hash */
	0,				/* tp_call */
	0,				/* tp_str */
	(getattrofunc) hdr_getattro,	/* tp_getattro */
	(setattrofunc) hdr_setattro,	/* tp_setattro */
	0,				/* tp_as_buffer */
	Py_TPFLAGS_DEFAULT,		/* tp_flags */
	hdr_doc,			/* tp_doc */
#if Py_TPFLAGS_HAVE_ITER
	0,				/* tp_traverse */
	0,				/* tp_clear */
	0,				/* tp_richcompare */
	0,				/* tp_weaklistoffset */
	0,				/* tp_iter */
	0,				/* tp_iternext */
	hdr_methods,			/* tp_methods */
	0,				/* tp_members */
	0,				/* tp_getset */
	0,				/* tp_base */
	0,				/* tp_dict */
	0,				/* tp_descr_get */
	0,				/* tp_descr_set */
	0,				/* tp_dictoffset */
	0,				/* tp_init */
	0,				/* tp_alloc */
	0,				/* tp_new */
	0,				/* tp_free */
	0,				/* tp_is_gc */
#endif
};

hdrObject * hdr_Wrap(Header h)
{
    hdrObject * hdr = PyObject_New(hdrObject, &hdr_Type);
    hdr->h = headerLink(h);
    hdr->fileList = hdr->linkList = hdr->md5list = NULL;
    hdr->uids = hdr->gids = hdr->mtimes = hdr->fileSizes = NULL;
    hdr->modes = hdr->rdevs = NULL;
    return hdr;
}

Header hdrGetHeader(hdrObject * s)
{
    return s->h;
}

/**
 */
PyObject * hdrLoad(PyObject * self, PyObject * args)
{
    hdrObject * hdr;
    char * copy = NULL;
    char * obj;
    Header h;
    int len;

    if (!PyArg_ParseTuple(args, "s#", &obj, &len)) return NULL;

    /* malloc is needed to avoid surprises from data swab in headerLoad(). */
    copy = malloc(len);
    if (copy == NULL) {
	PyErr_SetString(pyrpmError, "out of memory");
	return NULL;
    }
    memcpy (copy, obj, len);

    h = headerLoad(copy);
    if (!h) {
	PyErr_SetString(pyrpmError, "bad header");
	return NULL;
    }
    headerAllocated(h);
    compressFilelist (h);
    providePackageNVR (h);

    hdr = hdr_Wrap(h);
    h = headerFree(h);	/* XXX ref held by hdr */

    return (PyObject *) hdr;
}

/**
 */
PyObject * rhnLoad(PyObject * self, PyObject * args)
{
    char * obj, * copy=NULL;
    Header h;
    int len;

    if (!PyArg_ParseTuple(args, "s#", &obj, &len)) return NULL;

    /* malloc is needed to avoid surprises from data swab in headerLoad(). */
    copy = malloc(len);
    if (copy == NULL) {
	PyErr_SetString(pyrpmError, "out of memory");
	return NULL;
    }
    memcpy (copy, obj, len);

    h = headerLoad(copy);
    if (!h) {
	PyErr_SetString(pyrpmError, "bad header");
	return NULL;
    }
    headerAllocated(h);

    /* XXX avoid the false OK's from rpmverifyDigest() with missing tags. */
    if (!headerIsEntry(h, RPMTAG_HEADERIMMUTABLE)) {
	PyErr_SetString(pyrpmError, "bad header, not immutable");
	headerFree(h);
	return NULL;
    }

    /* XXX avoid the false OK's from rpmverifyDigest() with missing tags. */
    if (!headerIsEntry(h, RPMTAG_SHA1HEADER)
    &&  !headerIsEntry(h, RPMTAG_SHA1RHN)) {
	PyErr_SetString(pyrpmError, "bad header, no digest");
	headerFree(h);
	return NULL;
    }

    /* Retrofit a RHNPlatform: tag. */
    if (!headerIsEntry(h, RPMTAG_RHNPLATFORM)) {
	const char * arch;
	int_32 at;
	if (headerGetEntry(h, RPMTAG_ARCH, &at, (void **)&arch, NULL))
	    headerAddEntry(h, RPMTAG_RHNPLATFORM, at, arch, 1);
    }

    return (PyObject *) hdr_Wrap(h);
}

/**
 */
PyObject * rpmReadHeaders (FD_t fd)
{
    PyObject * list;
    Header h;
    hdrObject * hdr;

    if (!fd) {
	PyErr_SetFromErrno(pyrpmError);
	return NULL;
    }

    list = PyList_New(0);
    Py_BEGIN_ALLOW_THREADS
    h = headerRead(fd, HEADER_MAGIC_YES);
    Py_END_ALLOW_THREADS

    while (h) {
	compressFilelist(h);
	providePackageNVR(h);
	hdr = hdr_Wrap(h);
	if (PyList_Append(list, (PyObject *) hdr)) {
	    Py_DECREF(list);
	    Py_DECREF(hdr);
	    return NULL;
	}
	Py_DECREF(hdr);

	h = headerFree(h);	/* XXX ref held by hdr */

	Py_BEGIN_ALLOW_THREADS
	h = headerRead(fd, HEADER_MAGIC_YES);
	Py_END_ALLOW_THREADS
    }

    return list;
}

/**
 */
PyObject * rpmHeaderFromFD(PyObject * self, PyObject * args)
{
    FD_t fd;
    int fileno;
    PyObject * list;

    if (!PyArg_ParseTuple(args, "i", &fileno)) return NULL;
    fd = fdDup(fileno);

    list = rpmReadHeaders (fd);
    Fclose(fd);

    return list;
}

/**
 */
PyObject * rpmHeaderFromFile(PyObject * self, PyObject * args)
{
    char * filespec;
    FD_t fd;
    PyObject * list;

    if (!PyArg_ParseTuple(args, "s", &filespec)) return NULL;
    fd = Fopen(filespec, "r.fdio");

    if (!fd) {
	PyErr_SetFromErrno(pyrpmError);
	return NULL;
    }

    list = rpmReadHeaders (fd);
    Fclose(fd);

    return list;
}

/**
 * This assumes the order of list matches the order of the new headers, and
 * throws an exception if that isn't true.
 */
int rpmMergeHeaders(PyObject * list, FD_t fd, int matchTag)
{
    Header h;
    HeaderIterator hi;
    int_32 * newMatch;
    int_32 * oldMatch;
    hdrObject * hdr;
    int count = 0;
    int type, c, tag;
    void * p;

    Py_BEGIN_ALLOW_THREADS
    h = headerRead(fd, HEADER_MAGIC_YES);
    Py_END_ALLOW_THREADS

    while (h) {
	if (!headerGetEntry(h, matchTag, NULL, (void **) &newMatch, NULL)) {
	    PyErr_SetString(pyrpmError, "match tag missing in new header");
	    return 1;
	}

	hdr = (hdrObject *) PyList_GetItem(list, count++);
	if (!hdr) return 1;

	if (!headerGetEntry(hdr->h, matchTag, NULL, (void **) &oldMatch, NULL)) {
	    PyErr_SetString(pyrpmError, "match tag missing in new header");
	    return 1;
	}

	if (*newMatch != *oldMatch) {
	    PyErr_SetString(pyrpmError, "match tag mismatch");
	    return 1;
	}

	hdr->md5list = _free(hdr->md5list);
	hdr->fileList = _free(hdr->fileList);
	hdr->linkList = _free(hdr->linkList);

	for (hi = headerInitIterator(h);
	    headerNextIterator(hi, &tag, &type, (void *) &p, &c);
	    p = headerFreeData(p, type))
	{
	    /* could be dupes */
	    headerRemoveEntry(hdr->h, tag);
	    headerAddEntry(hdr->h, tag, type, p, c);
	}

	headerFreeIterator(hi);
	h = headerFree(h);

	Py_BEGIN_ALLOW_THREADS
	h = headerRead(fd, HEADER_MAGIC_YES);
	Py_END_ALLOW_THREADS
    }

    return 0;
}

PyObject * rpmMergeHeadersFromFD(PyObject * self, PyObject * args)
{
    FD_t fd;
    int fileno;
    PyObject * list;
    int rc;
    int matchTag;

    if (!PyArg_ParseTuple(args, "Oii", &list, &fileno, &matchTag))
	return NULL;

    if (!PyList_Check(list)) {
	PyErr_SetString(PyExc_TypeError, "first parameter must be a list");
	return NULL;
    }

    fd = fdDup(fileno);

    rc = rpmMergeHeaders (list, fd, matchTag);
    Fclose(fd);

    if (rc) {
	return NULL;
    }

    Py_INCREF(Py_None);
    return Py_None;
}

/**
 */
PyObject * versionCompare (PyObject * self, PyObject * args)
{
    hdrObject * h1, * h2;

    if (!PyArg_ParseTuple(args, "O!O!", &hdr_Type, &h1, &hdr_Type, &h2))
	return NULL;

    return Py_BuildValue("i", hdr_compare(h1, h2));
}

/**
 */
PyObject * labelCompare (PyObject * self, PyObject * args)
{
    char *v1, *r1, *e1, *v2, *r2, *e2;
    int rc;

    if (!PyArg_ParseTuple(args, "(zzz)(zzz)",
			  &e1, &v1, &r1,
			  &e2, &v2, &r2)) return NULL;

    if (e1 && !e2)
	return Py_BuildValue("i", 1);
    else if (!e1 && e2)
	return Py_BuildValue("i", -1);
    else if (e1 && e2) {
	int ep1, ep2;
	ep1 = atoi (e1);
	ep2 = atoi (e2);
	if (ep1 < ep2)
	    return Py_BuildValue("i", -1);
	else if (ep1 > ep2)
	    return Py_BuildValue("i", 1);
    }

    rc = rpmvercmp(v1, v2);
    if (rc)
	return Py_BuildValue("i", rc);

    return Py_BuildValue("i", rpmvercmp(r1, r2));
}

/*@}*/
