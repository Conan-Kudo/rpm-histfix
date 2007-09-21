#ifndef H_HEADER
#define H_HEADER

/** \ingroup header
 * \file rpmdb/header.h
 *
 * An rpm header carries all information about a package. A header is
 * a collection of data elements called tags. Each tag has a data type,
 * and includes 1 or more values.
 * 
 * \par Historical Issues
 *
 * Here's a brief description of features/incompatibilities that
 * have been added to headers and tags.
 *
 * - version 1
 *	- Support for version 1 headers was removed in rpm-4.0.
 *
 * - version 2
 *	- @todo Document version2 headers.
 *
 * - version 3	(added in rpm-3.0)
 *	- Added RPM_I18NSTRING_TYPE as an associative array reference
 *	  for i18n locale dependent single element tags (i.e Group).
 *	- Added an 8 byte magic string to headers in packages on-disk. The
 *	  magic string was not added to headers in the database.
 *
 * - version 4	(added in rpm-4.0)
 *	- Represent file names as a (dirname/basename/dirindex) triple
 *	  rather than as an absolute path name. Legacy package headers are
 *	  converted when the header is read. Legacy database headers are
 *	  converted when the database is rebuilt.
 *	- Simplify dependencies by eliminating the implict check on
 *	  package name/version/release in favor of an explict check
 *	  on package provides. Legacy package headers are converted
 *	  when the header is read. Legacy database headers are
 *        converted when the database is rebuilt.
 *	- (rpm-4.0.2) The original package header (and all original
 *	  metadata) is preserved in what's called an "immutable header region".
 *	  The original header can be retrieved as an RPM_BIN_TYPE, just
 *	  like any other tag, and the original header reconstituted using
 *	  headerLoad().
 *	- (rpm-4.0.2) The signature tags are added (and renumbered to avoid
 *	  tag value collisions) to the package header during package
 *	  installation.
 *	- (rpm-4.0.3) A SHA1 digest of the original header is appended
 *	  (i.e. detached digest) to the immutable header region to verify
 *	  changes to the original header.
 *	- (rpm-4.0.3) Private methods (e.g. headerLoad(), headerUnload(), etc.)
 *	  to permit header data to be manipulated opaquely through vectors.
 *	- (rpm-4.0.3) Sanity checks on header data to limit #tags to 65K,
 *	  #bytes to 16Mb, and total metadata size to 32Mb added.
 *	- with addition of tracking dependencies, the package version has been
 *	  reverted back to 3.
 * .
 *
 * \par Development Issues
 *
 * Here's a brief description of future features/incompatibilities that
 * will be added to headers.
 *
 * - Private header methods.
 *	- Private methods for the transaction element file info rpmfi may
 *	  be used as proof-of-concept, binary XML may be implemented
 *	  as a header format representation soon thereafter.
 * - DSA signature for header metadata.
 *	- The manner in which rpm packages are signed is going to change.
 *	  The SHA1 digest in the header will be signed, equivalent to a DSA
 *	  digital signature on the original header metadata. As the original
 *	  header will contain "trusted" (i.e. because the header is signed
 *	  with DSA) file MD5 digests, there will be little or no reason
 *	  to sign the payload, but that may happen as well. Note that cpio
 *	  headers in the payload are not used to install package metadata,
 *	  only the name field in the cpio header is used to associate an
 *	  archive file member with the corresponding entry for the file
 *	  in header metadata.
 * .
 */

/* RPM - Copyright (C) 1995-2001 Red Hat Software */

#include <stdio.h>
#include "rpmio.h"

#ifdef __cplusplus
extern "C" {
#endif

#if 0	/* XXX hpux needs -Ae in CFLAGS to grok this */
typedef long long int int_64;
#endif
typedef int int_32;
typedef short int int_16;
typedef char int_8;

#if 0	/* XXX hpux needs -Ae in CFLAGS to grok this */
typedef unsigned long long int uint_64;
#endif
typedef unsigned int uint_32;
typedef unsigned short uint_16;
typedef unsigned char uint_8;

/** \ingroup header
 */
typedef const char *	errmsg_t;

/** \ingroup header
 */
typedef int_32 *	hTAG_t;
typedef int_32 *	hTYP_t;
typedef const void *	hPTR_t;
typedef int_32 *	hCNT_t;

/** \ingroup header
 */
typedef struct headerToken_s * Header;

/** \ingroup header
 */
typedef struct headerIterator_s * HeaderIterator;

/** \ingroup header
 * Associate tag names with numeric values.
 */
typedef struct headerTagTableEntry_s * headerTagTableEntry;
struct headerTagTableEntry_s {
    const char * name;		/*!< Tag name. */
    int val;			/*!< Tag numeric value. */
    int type;			/*!< Tag type. */
};

/**
 */
typedef struct headerTagIndices_s * headerTagIndices;

/** \ingroup header
 */
enum headerSprintfExtensionType {
    HEADER_EXT_LAST = 0,	/*!< End of extension chain. */
    HEADER_EXT_FORMAT,		/*!< headerTagFormatFunction() extension */
    HEADER_EXT_MORE,		/*!< Chain to next table. */
    HEADER_EXT_TAG		/*!< headerTagTagFunction() extension */
};

/** \ingroup header
 * HEADER_EXT_TAG format function prototype.
 * This will only ever be passed RPM_INT32_TYPE or RPM_STRING_TYPE to
 * help keep things simple.
 *
 * @param type		tag type
 * @param data		tag value
 * @param formatPrefix
 * @param padding
 * @param element	RPM_BIN_TYPE: no. bytes of data
 * @return		formatted string
 */
typedef /*only@*/ char * (*headerTagFormatFunction)(int_32 type,
				const void * data, char * formatPrefix,
				int padding, int element);

/** \ingroup header
 * HEADER_EXT_FORMAT format function prototype.
 * This is allowed to fail, which indicates the tag doesn't exist.
 *
 * @param h		header
 * @retval *type	tag type
 * @retval *data	tag value
 * @retval *count	no. of data items
 * @retval *freedata	data-was-malloc'ed indicator
 * @return		0 on success
 */
typedef int (*headerTagTagFunction) (Header h,
		hTYP_t type,
		hPTR_t * data,
		hCNT_t count,
		int * freeData);

/** \ingroup header
 * Define header tag output formats.
 */
typedef struct headerSprintfExtension_s * headerSprintfExtension;
struct headerSprintfExtension_s {
    enum headerSprintfExtensionType type;	/*!< Type of extension. */
    const char * name;				/*!< Name of extension. */
    union {
	void * generic;				/*!< Private extension. */
	headerTagFormatFunction formatFunction; /*!< HEADER_EXT_TAG extension. */
	headerTagTagFunction tagFunction;	/*!< HEADER_EXT_FORMAT extension. */
	struct headerSprintfExtension_s * more;	/*!< Chained table extension. */
    } u;
};

/** \ingroup header
 * Supported default header tag output formats.
 */
extern const struct headerSprintfExtension_s headerDefaultFormats[];

/** \ingroup header
 * Include calculation for 8 bytes of (magic, 0)?
 */
enum hMagic {
    HEADER_MAGIC_NO		= 0,
    HEADER_MAGIC_YES		= 1
};

/** \ingroup header
 * The basic types of data in tags from headers.
 */
typedef enum rpmTagType_e {
#define	RPM_MIN_TYPE		0
    RPM_NULL_TYPE		=  0,
    RPM_CHAR_TYPE		=  1,
    RPM_INT8_TYPE		=  2,
    RPM_INT16_TYPE		=  3,
    RPM_INT32_TYPE		=  4,
/*    RPM_INT64_TYPE	= 5,   ---- These aren't supported (yet) */
    RPM_STRING_TYPE		=  6,
    RPM_BIN_TYPE		=  7,
    RPM_STRING_ARRAY_TYPE	=  8,
    RPM_I18NSTRING_TYPE		=  9,
    RPM_MASK_TYPE               =  0x0000ffff
#define	RPM_MAX_TYPE		9
} rpmTagType;

/** \ingroup header
 * New rpm data types under consideration/development.
 * These data types may (or may not) be added to rpm at some point. In order
 * to avoid incompatibility with legacy versions of rpm, these data (sub-)types
 * are introduced into the header by overloading RPM_BIN_TYPE, with the binary
 * value of the tag a 16 byte image of what should/will be in the header index,
 * followed by per-tag private data.
 */
typedef enum rpmSubTagType_e {
    RPM_REGION_TYPE		= -10,
    RPM_BIN_ARRAY_TYPE		= -11,
  /*!<@todo Implement, kinda like RPM_STRING_ARRAY_TYPE for known (but variable)
	length binary data. */
    RPM_XREF_TYPE		= -12
  /*!<@todo Implement, intent is to to carry a (???,tagNum,valNum) cross
	reference to retrieve data from other tags. */
} rpmSubTagType;

/** \ingroup header
 *  * Identify how to return the header data type.
 *   */
typedef enum rpmTagReturnType_e {
    RPM_ANY_RETURN_TYPE         = 0,
    RPM_SCALAR_RETURN_TYPE      = 0x00010000,
    RPM_ARRAY_RETURN_TYPE       = 0x00020000,
    RPM_MAPPING_RETURN_TYPE     = 0x00040000,
    RPM_MASK_RETURN_TYPE        = 0xffff0000
} rpmTagReturnType;

/**
 * Header private tags.
 * @note General use tags should start at 1000 (RPM's tag space starts there).
 */
#define	HEADER_IMAGE		61
#define	HEADER_SIGNATURES	62
#define	HEADER_IMMUTABLE	63
#define	HEADER_REGIONS		64
#define HEADER_I18NTABLE	100
#define	HEADER_SIGBASE		256
#define	HEADER_TAGBASE		1000

/**
 */
typedef union hRET_s {
    const void * ptr;
    const char ** argv;
    const char * str;
    uint_32 * ui32p;
    uint_16 * ui16p;
    int_32 * i32p;
    int_16 * i16p;
    int_8 * i8p;
} * hRET_t;

/**
 */
typedef struct HE_s {
    int_32 tag;
    hTYP_t typ;
    union {
	hPTR_t * ptr;
	hRET_t * ret;
    } u;
    hCNT_t cnt;
} * HE_t;

/** \ingroup header
 * Create new (empty) header instance.
 * @return		header
 */
typedef
Header (*HDRnew) (void);

/** \ingroup header
 * Dereference a header instance.
 * @param h		header
 * @return		NULL always
 */
typedef
Header (*HDRfree) (Header h);

/** \ingroup header
 * Reference a header instance.
 * @param h		header
 * @return		referenced header instance
 */
typedef
Header (*HDRlink) (Header h);

/** \ingroup header
 * Dereference a header instance.
 * @param h		header
 * @return		NULL always
 */
typedef
Header (*HDRunlink) (Header h);

/** \ingroup header
 * Sort tags in header.
 * @todo Eliminate from API.
 * @param h		header
 */
typedef
void (*HDRsort) (Header h);

/** \ingroup header
 * Restore tags in header to original ordering.
 * @todo Eliminate from API.
 * @param h		header
 */
typedef
void (*HDRunsort) (Header h);

/** \ingroup header
 * Return size of on-disk header representation in bytes.
 * @param h		header
 * @param magicp	include size of 8 bytes for (magic, 0)?
 * @return		size of on-disk header
 */
typedef
unsigned int (*HDRsizeof) (Header h, enum hMagic magicp);

/** \ingroup header
 * Convert header to on-disk representation.
 * @param h		header (with pointers)
 * @return		on-disk header blob (i.e. with offsets)
 */
typedef
void * (*HDRunload) (Header h);

/** \ingroup header
 * Convert header to on-disk representation, and then reload.
 * This is used to insure that all header data is in one chunk.
 * @param h		header (with pointers)
 * @param tag		region tag
 * @return		on-disk header (with offsets)
 */
typedef
Header (*HDRreload) (Header h, int tag);

/** \ingroup header
 * Duplicate a header.
 * @param h		header
 * @return		new header instance
 */
typedef
Header (*HDRcopy) (Header h);

/** \ingroup header
 * Convert header to in-memory representation.
 * @param uh		on-disk header blob (i.e. with offsets)
 * @return		header
 */
typedef
Header (*HDRload) (void * uh);

/** \ingroup header
 * Make a copy and convert header to in-memory representation.
 * @param uh		on-disk header blob (i.e. with offsets)
 * @return		header
 */
typedef
Header (*HDRcopyload) (const void * uh);

/** \ingroup header
 * Read (and load) header from file handle.
 * @param fd		file handle
 * @param magicp	read (and verify) 8 bytes of (magic, 0)?
 * @return		header (or NULL on error)
 */
typedef
Header (*HDRread) (FD_t fd, enum hMagic magicp);

/** \ingroup header
 * Write (with unload) header to file handle.
 * @param fd		file handle
 * @param h		header
 * @param magicp	prefix write with 8 bytes of (magic, 0)?
 * @return		0 on success, 1 on error
 */
typedef
int (*HDRwrite) (FD_t fd, Header h, enum hMagic magicp);

/** \ingroup header
 * Check if tag is in header.
 * @param h		header
 * @param tag		tag
 * @return		1 on success, 0 on failure
 */
typedef
int (*HDRisentry) (Header h, int_32 tag);  

/** \ingroup header
 * Free data allocated when retrieved from header.
 * @param h		header
 * @param data		address of data (or NULL)
 * @param type		type of data (or -1 to force free)
 * @return		NULL always
 */
typedef
void * (*HDRfreetag) (Header h,
		const void * data, rpmTagType type);

/** \ingroup header
 * Retrieve tag value.
 * Will never return RPM_I18NSTRING_TYPE! RPM_STRING_TYPE elements with
 * RPM_I18NSTRING_TYPE equivalent entries are translated (if HEADER_I18NTABLE
 * entry is present).
 *
 * @param h		header
 * @param tag		tag
 * @retval type		address of tag value data type (or NULL)
 * @retval p		address of pointer to tag value(s) (or NULL)
 * @retval c		address of number of values (or NULL)
 * @return		1 on success, 0 on failure
 */
typedef
int (*HDRget) (Header h, int_32 tag,
			hTYP_t type,
			void ** p,
			hCNT_t c);

/** \ingroup header
 * Retrieve tag value using header internal array.
 * Get an entry using as little extra RAM as possible to return the tag value.
 * This is only an issue for RPM_STRING_ARRAY_TYPE.
 *
 * @param h		header
 * @param tag		tag
 * @retval type		address of tag value data type (or NULL)
 * @retval p		address of pointer to tag value(s) (or NULL)
 * @retval c		address of number of values (or NULL)
 * @return		1 on success, 0 on failure
 */
typedef
int (*HDRgetmin) (Header h, int_32 tag,
			hTYP_t type,
			hPTR_t * p,
			hCNT_t c);

/** \ingroup header
 * Add tag to header.
 * Duplicate tags are okay, but only defined for iteration (with the
 * exceptions noted below). While you are allowed to add i18n string
 * arrays through this function, you probably don't mean to. See
 * headerAddI18NString() instead.
 *
 * @param h		header
 * @param tag		tag
 * @param type		tag value data type
 * @param p		pointer to tag value(s)
 * @param c		number of values
 * @return		1 on success, 0 on failure
 */
typedef
int (*HDRadd) (Header h, int_32 tag, int_32 type, const void * p, int_32 c);

/** \ingroup header
 * Append element to tag array in header.
 * Appends item p to entry w/ tag and type as passed. Won't work on
 * RPM_STRING_TYPE. Any pointers into header memory returned from
 * headerGetEntryMinMemory() for this entry are invalid after this
 * call has been made!
 *
 * @param h		header
 * @param tag		tag
 * @param type		tag value data type
 * @param p		pointer to tag value(s)
 * @param c		number of values
 * @return		1 on success, 0 on failure
 */
typedef
int (*HDRappend) (Header h, int_32 tag, int_32 type, const void * p, int_32 c);

/** \ingroup header
 * Add or append element to tag array in header.
 * @todo Arg "p" should have const.
 * @param h		header
 * @param tag		tag
 * @param type		tag value data type
 * @param p		pointer to tag value(s)
 * @param c		number of values
 * @return		1 on success, 0 on failure
 */
typedef
int (*HDRaddorappend) (Header h, int_32 tag, int_32 type, const void * p, int_32 c);

/** \ingroup header
 * Add locale specific tag to header.
 * A NULL lang is interpreted as the C locale. Here are the rules:
 * \verbatim
 *	- If the tag isn't in the header, it's added with the passed string
 *	   as new value.
 *	- If the tag occurs multiple times in entry, which tag is affected
 *	   by the operation is undefined.
 *	- If the tag is in the header w/ this language, the entry is
 *	   *replaced* (like headerModifyEntry()).
 * \endverbatim
 * This function is intended to just "do the right thing". If you need
 * more fine grained control use headerAddEntry() and headerModifyEntry().
 *
 * @param h		header
 * @param tag		tag
 * @param string	tag value
 * @param lang		locale
 * @return		1 on success, 0 on failure
 */
typedef
int (*HDRaddi18n) (Header h, int_32 tag, const char * string,
                const char * lang);

/** \ingroup header
 * Modify tag in header.
 * If there are multiple entries with this tag, the first one gets replaced.
 * @param h		header
 * @param tag		tag
 * @param type		tag value data type
 * @param p		pointer to tag value(s)
 * @param c		number of values
 * @return		1 on success, 0 on failure
 */
typedef
int (*HDRmodify) (Header h, int_32 tag, int_32 type, const void * p, int_32 c);

/** \ingroup header
 * Delete tag in header.
 * Removes all entries of type tag from the header, returns 1 if none were
 * found.
 *
 * @param h		header
 * @param tag		tag
 * @return		0 on success, 1 on failure (INCONSISTENT)
 */
typedef
int (*HDRremove) (Header h, int_32 tag);

/** \ingroup header
 * Return formatted output string from header tags.
 * The returned string must be free()d.
 *
 * @param h		header
 * @param fmt		format to use
 * @param tags		array of tag name/value pairs
 * @param extensions	chained table of formatting extensions.
 * @retval errmsg	error message (if any)
 * @return		formatted output string (malloc'ed)
 */
typedef
char * (*HDRsprintf) (Header h, const char * fmt,
		     const struct headerTagTableEntry_s * tags,
		     const struct headerSprintfExtension_s * extensions,
		     errmsg_t * errmsg);

/** \ingroup header
 * Duplicate tag values from one header into another.
 * @param headerFrom	source header
 * @param headerTo	destination header
 * @param tagstocopy	array of tags that are copied
 */
typedef
void (*HDRcopytags) (Header headerFrom, Header headerTo, hTAG_t tagstocopy);

/** \ingroup header
 * Destroy header tag iterator.
 * @param hi		header tag iterator
 * @return		NULL always
 */
typedef
HeaderIterator (*HDRfreeiter) (HeaderIterator hi);

/** \ingroup header
 * Create header tag iterator.
 * @param h		header
 * @return		header tag iterator
 */
typedef
HeaderIterator (*HDRinititer) (Header h);

/** \ingroup header
 * Return next tag from header.
 * @param hi		header tag iterator
 * @retval tag		address of tag
 * @retval type		address of tag value data type
 * @retval p		address of pointer to tag value(s)
 * @retval c		address of number of values
 * @return		1 on success, 0 on failure
 */
typedef
int (*HDRnextiter) (HeaderIterator hi,
		hTAG_t tag,
		hTYP_t type,
		hPTR_t * p,
		hCNT_t c);

/** \ingroup header
 * Header method vectors.
 */
typedef struct HV_s * HV_t;
struct HV_s {
    HDRlink	hdrlink;
    HDRunlink	hdrunlink;
    HDRfree	hdrfree;
    HDRnew	hdrnew;
    HDRsort	hdrsort;
    HDRunsort	hdrunsort;
    HDRsizeof	hdrsizeof;
    HDRunload	hdrunload;
    HDRreload	hdrreload;
    HDRcopy	hdrcopy;
    HDRload	hdrload;
    HDRcopyload	hdrcopyload;
    HDRread	hdrread;
    HDRwrite	hdrwrite;
    HDRisentry	hdrisentry;
    HDRfreetag	hdrfreetag;
    HDRget	hdrget;
    HDRgetmin	hdrgetmin;
    HDRadd	hdradd;
    HDRappend	hdrappend;
    HDRaddorappend hdraddorappend;
    HDRaddi18n	hdraddi18n;
    HDRmodify	hdrmodify;
    HDRremove	hdrremove;
    HDRsprintf	hdrsprintf;
    HDRcopytags	hdrcopytags;
    HDRfreeiter	hdrfreeiter;
    HDRinititer	hdrinititer;
    HDRnextiter	hdrnextiter;
    void *	hdrvecs;
    void *	hdrdata;
    int		hdrversion;
};

/** \ingroup header
 * Free data allocated when retrieved from header.
 * @deprecated Use headerFreeTag() instead.
 * @todo Remove from API.
 *
 * @param data		address of data (or NULL)
 * @param type		type of data (or -1 to force free)
 * @return		NULL always
 */
static inline
void * headerFreeData( const void * data, rpmTagType type)
{
    if (data) {
	if (type == -1 ||
	    type == RPM_STRING_ARRAY_TYPE ||
	    type == RPM_I18NSTRING_TYPE ||
	    type == RPM_BIN_TYPE)
		free((void *)data);
    }
    return NULL;
}

#if !defined(__HEADER_PROTOTYPES__)
#include "hdrinline.h"
#endif

#ifdef __cplusplus
}
#endif

#endif	/* H_HEADER */
