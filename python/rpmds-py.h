#ifndef H_RPMDS_PY
#define H_RPMDS_PY

#include "rpmds.h"

/** \ingroup python
 * \file python/rpmds-py.h
 */

/**
 */
typedef struct rpmdsObject_s {
    PyObject_HEAD
    rpmds	ds;
} rpmdsObject;

/**
 */
extern PyTypeObject rpmds_Type;

/**
 */
rpmds dsFromDs(rpmdsObject * ds);

/**
 */
rpmdsObject * rpmds_Wrap(rpmds ds);

/**
 */
rpmdsObject * rpmds_Single(PyObject * s, PyObject * args);

/**
 */
rpmdsObject * hdr_dsFromHeader(PyObject * s, PyObject * args);

/**
 */
rpmdsObject * hdr_dsOfHeader(PyObject * s, PyObject * args);

#endif
