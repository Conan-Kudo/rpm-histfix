#ifndef _RPMTD_H
#define _RPMTD_H

#include <rpm/rpmtypes.h>
#include <rpm/argv.h>

/** \ingroup rpmtd
 * Container for rpm tag data (from headers or extensions).
 * @todo		Make this opaque (at least outside rpm itself)
 */
struct rpmtd_s {
    rpmTag tag;		/* rpm tag of this data entry*/
    rpmTagType type;	/* data type */
    rpm_count_t count;	/* number of entries */
    rpm_data_t data;	/* pointer to actual data */
    int freeData;	/* was data malloced */
    int ix;		/* iteration index */
};

/** \ingroup rpmtd
 * Create new tag data container
 * @return		New, initialized tag data container.
 */
rpmtd rpmtdNew(void);

/** \ingroup rpmtd
 * Destroy tag data container.
 * @param td		Tag data container
 * @return		NULL always
 */
rpmtd rpmtdFree(rpmtd td);
 
/** \ingroup rpmtd
 * (Re-)initialize tag data container. Contents will be zeroed out
 * and iteration index reset.
 * @param td		Tag data container
 */
void rpmtdReset(rpmtd td);

/** \ingroup rpmtd
 * Free contained data. This is always safe to call as the container knows 
 * if data was malloc'ed or not. Container is reinitialized.
 * @param td		Tag data container
 */
void rpmtdFreeData(rpmtd td);

/** \ingroup rpmtd
 * Retrieve array size of the container. For non-array types this is always 1.
 * @param td		Tag data container
 * @return		Number of entries in contained data.
 */
rpm_count_t rpmtdCount(rpmtd td);

/** \ingroup rpmtd
 * Retrieve tag of the container.
 * @param td		Tag data container
 * @return		Rpm tag.
 */
rpmTag rpmtdTag(rpmtd td);

/** \ingroup rpmtd
 * Retrieve type of the container.
 * @param td		Tag data container
 * @return		Rpm tag type.
 */
rpmTagType rpmtdType(rpmtd td);

/** \ingroup rpmtd
 * Initialize tag container for iteration
 * @param td		Tag data container
 * @return		0 on success
 */
int rpmtdInit(rpmtd td);

/** \ingroup rpmtd
 * Iterate over tag data container.
 * @param td		Tag data container
 * @return		Tag data container iterator index, -1 on termination
 */
int rpmtdNext(rpmtd td);

/** \ingroup rpmtd
 * Return char data from tag container.
 * For scalar return type, just return pointer to the integer. On array
 * types, return pointer to current iteration index. If the tag container
 * is not for char type, NULL is returned.
 * @param td		Tag data container
 * @return		Pointer to uint16_t, NULL on error
 */
char *rpmtdGetChar(rpmtd td);

/** \ingroup rpmtd
 * Return uint16_t data from tag container.
 * For scalar return type, just return pointer to the integer. On array
 * types, return pointer to current iteration index. If the tag container
 * is not for int16 type, NULL is returned.
 * @param td		Tag data container
 * @return		Pointer to uint16_t, NULL on error
 */
uint16_t * rpmtdGetUint16(rpmtd td);

/** \ingroup rpmtd
 * Return uint32_t data from tag container.
 * For scalar return type, just return pointer to the integer. On array
 * types, return pointer to current iteration index. If the tag container
 * is not for int32 type, NULL is returned.
 * @param td		Tag data container
 * @return		Pointer to uint32_t, NULL on error
 */
uint32_t * rpmtdGetUint32(rpmtd td);

/** \ingroup rpmtd
 * Return string data from tag container.
 * For string types, just return the string. On string array types,
 * return the string from current iteration index. If the tag container
 * is not for a string type, NULL is returned.
 * @param td		Tag data container
 * @return		String constant from container, NULL on error
 */
const char * rpmtdGetString(rpmtd td);

typedef enum rpmtdFormats_e {
    RPMTD_FORMAT_STRING		= 0,	/* plain string (any type) */
    RPMTD_FORMAT_ARMOR		= 1,	/* ascii armor format (bin types) */
    RPMTD_FORMAT_BASE64		= 2,	/* base64 encoding (bin types) */
    RPMTD_FORMAT_PGPSIG		= 3,	/* pgp/gpg signature (bin types) */
    RPMTD_FORMAT_DEPFLAGS	= 4,	/* dependency flags (int32 types) */
    RPMTD_FORMAT_FFLAGS		= 5,	/* file flags (int32 types) */
    RPMTD_FORMAT_PERMS		= 6,	/* permission string (int32 types) */
    RPMTD_FORMAT_TRIGGERTYPE	= 7,	/* trigger types */
    RPMTD_FORMAT_XML		= 8,	/* xml format (any type) */
    RPMTD_FORMAT_OCTAL		= 9,	/* octal format (int32 types) */
    RPMTD_FORMAT_HEX		= 10,	/* hex format (int32 types) */
    RPMTD_FORMAT_DATE		= 11,	/* date format (int32 types) */
    RPMTD_FORMAT_DAY		= 12,	/* day format (int32 types) */
    RPMTD_FORMAT_SHESCAPE	= 13,	/* shell escaped (any type) */
} rpmtdFormats;

/** \ingroup rpmtd
 * Format data from tag container to string presentation of given format.
 * Return malloced string presentation of current data in container,
 * converting from integers etc as necessary. On array types, data from
 * current iteration index is used for formatting.
 * @param td		Tag data container
 * @param fmt		Format to apply
 * @param errmsg	Error message from conversion (or NULL)
 * @return		String representation of current data (malloc'ed), 
 * 			NULL on error
 */
char *rpmtdFormat(rpmtd td, rpmtdFormats fmt, const char *errmsg);

/** \ingroup rpmtd
 * Construct tag container from ARGV_t array.
 * Tag type is checked to be of string array type and array is checked
 * to be non-empty.
 * @param td		Tag data container
 * @param tag		Rpm tag to construct
 * @param argv		ARGV array
 * @return		1 on success, 0 on error (eg wrong type)
 */
int rpmtdFromArgv(rpmtd td, rpmTag tag, ARGV_t argv);

/** \ingroup rpmtd
 * Construct tag container from ARGI_t array.
 * Tag type is checked to be of integer array type and array is checked
 * to be non-empty.
 * @param td		Tag data container
 * @param tag		Rpm tag to construct
 * @param argi		ARGI array
 * @return		1 on success, 0 on error (eg wrong type)
 */
int rpmtdFromArgi(rpmtd td, rpmTag tag, ARGI_t argi);

#endif /* _RPMTD_H */
