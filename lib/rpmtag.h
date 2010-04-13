#ifndef _RPMTAG_H
#define _RPMTAG_H

#include <rpm/rpmtypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Pseudo-tags used by the rpmdb and rpmgi iterator API's.
 */
#define	RPMDBI_PACKAGES		0	/* Installed package headers. */
#define	RPMDBI_LABEL		2	/* Fingerprint search marker. */

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

/** \ingroup rpmtag
 * Tags identify data in package headers.
 * @note tags should not have value 0!
 */
/** @todo: Somehow supply type **/
typedef enum rpmTag_e {

    RPMTAG_HEADERIMAGE		= HEADER_IMAGE,		/*!< Current image. */
    RPMTAG_HEADERSIGNATURES	= HEADER_SIGNATURES,	/*!< Signatures. */
    RPMTAG_HEADERIMMUTABLE	= HEADER_IMMUTABLE,	/*!< Original image. */
    RPMTAG_HEADERREGIONS	= HEADER_REGIONS,	/*!< Regions. */

    RPMTAG_HEADERI18NTABLE	= HEADER_I18NTABLE, 	/* s[] !< I18N string locales. */

/* Retrofit (and uniqify) signature tags for use by rpmTagGetName() and rpmQuery. */
/* the md5 sum was broken *twice* on big endian machines */
/* XXX 2nd underscore prevents tagTable generation */
    RPMTAG_SIG_BASE		= HEADER_SIGBASE,
    RPMTAG_SIGSIZE		= RPMTAG_SIG_BASE+1,	/* i */
    RPMTAG_SIGLEMD5_1		= RPMTAG_SIG_BASE+2,	/* internal - obsolete */
    RPMTAG_SIGPGP		= RPMTAG_SIG_BASE+3,	/* x */
    RPMTAG_SIGLEMD5_2		= RPMTAG_SIG_BASE+4,	/* x internal - obsolete */
    RPMTAG_SIGMD5	        = RPMTAG_SIG_BASE+5,	/* x */
#define	RPMTAG_PKGID	RPMTAG_SIGMD5			/* x */
    RPMTAG_SIGGPG	        = RPMTAG_SIG_BASE+6,	/* x */
    RPMTAG_SIGPGP5	        = RPMTAG_SIG_BASE+7,	/* internal - obsolete */

    RPMTAG_BADSHA1_1		= RPMTAG_SIG_BASE+8,	/* internal - obsolete */
    RPMTAG_BADSHA1_2		= RPMTAG_SIG_BASE+9,	/* internal - obsolete */
    RPMTAG_PUBKEYS		= RPMTAG_SIG_BASE+10,	/* s[] */
    RPMTAG_DSAHEADER		= RPMTAG_SIG_BASE+11,	/* x */
    RPMTAG_RSAHEADER		= RPMTAG_SIG_BASE+12,	/* x */
    RPMTAG_SHA1HEADER		= RPMTAG_SIG_BASE+13,	/* s */
#define	RPMTAG_HDRID	RPMTAG_SHA1HEADER	/* s */
    RPMTAG_LONGSIGSIZE		= RPMTAG_SIG_BASE+14,	/* l */
    RPMTAG_LONGARCHIVESIZE	= RPMTAG_SIG_BASE+15,	/* l */

    RPMTAG_NAME  		= 1000,	/* s */
#define	RPMTAG_N	RPMTAG_NAME	/* s */
    RPMTAG_VERSION		= 1001,	/* s */
#define	RPMTAG_V	RPMTAG_VERSION	/* s */
    RPMTAG_RELEASE		= 1002,	/* s */
#define	RPMTAG_R	RPMTAG_RELEASE	/* s */
    RPMTAG_EPOCH   		= 1003,	/* i */
#define	RPMTAG_E	RPMTAG_EPOCH	/* i */
    RPMTAG_SUMMARY		= 1004,	/* s{} */
    RPMTAG_DESCRIPTION		= 1005,	/* s{} */
    RPMTAG_BUILDTIME		= 1006,	/* i */
    RPMTAG_BUILDHOST		= 1007,	/* s */
    RPMTAG_INSTALLTIME		= 1008,	/* i */
    RPMTAG_SIZE			= 1009,	/* i */
    RPMTAG_DISTRIBUTION		= 1010,	/* s */
    RPMTAG_VENDOR		= 1011,	/* s */
    RPMTAG_GIF			= 1012,	/* x */
    RPMTAG_XPM			= 1013,	/* x */
    RPMTAG_LICENSE		= 1014,	/* s */
    RPMTAG_PACKAGER		= 1015,	/* s */
    RPMTAG_GROUP		= 1016,	/* s{} */
    RPMTAG_CHANGELOG		= 1017, /* s[] internal */
    RPMTAG_SOURCE		= 1018,	/* s[] */
    RPMTAG_PATCH		= 1019,	/* s[] */
    RPMTAG_URL			= 1020,	/* s */
    RPMTAG_OS			= 1021,	/* s legacy used int */
    RPMTAG_ARCH			= 1022,	/* s legacy used int */
    RPMTAG_PREIN		= 1023,	/* s */
    RPMTAG_POSTIN		= 1024,	/* s */
    RPMTAG_PREUN		= 1025,	/* s */
    RPMTAG_POSTUN		= 1026,	/* s */
    RPMTAG_OLDFILENAMES		= 1027, /* s[] obsolete */
    RPMTAG_FILESIZES		= 1028,	/* i[] */
    RPMTAG_FILESTATES		= 1029, /* c[] */
    RPMTAG_FILEMODES		= 1030,	/* h[] */
    RPMTAG_FILEUIDS		= 1031, /* i[] internal */
    RPMTAG_FILEGIDS		= 1032, /* i[] internal */
    RPMTAG_FILERDEVS		= 1033,	/* h[] */
    RPMTAG_FILEMTIMES		= 1034, /* i[] */
    RPMTAG_FILEDIGESTS		= 1035,	/* s[] */
#define RPMTAG_FILEMD5S	RPMTAG_FILEDIGESTS /* s[] */
    RPMTAG_FILELINKTOS		= 1036,	/* s[] */
    RPMTAG_FILEFLAGS		= 1037,	/* i[] */
    RPMTAG_ROOT			= 1038, /* internal - obsolete */
    RPMTAG_FILEUSERNAME		= 1039,	/* s[] */
    RPMTAG_FILEGROUPNAME	= 1040,	/* s[] */
    RPMTAG_EXCLUDE		= 1041, /* internal - obsolete */
    RPMTAG_EXCLUSIVE		= 1042, /* internal - obsolete */
    RPMTAG_ICON			= 1043, /* x */
    RPMTAG_SOURCERPM		= 1044,	/* s */
    RPMTAG_FILEVERIFYFLAGS	= 1045,	/* i[] */
    RPMTAG_ARCHIVESIZE		= 1046,	/* i */
    RPMTAG_PROVIDENAME		= 1047,	/* s[] */
#define	RPMTAG_PROVIDES RPMTAG_PROVIDENAME	/* s[] */
#define	RPMTAG_P	RPMTAG_PROVIDENAME	/* s[] */
    RPMTAG_REQUIREFLAGS		= 1048,	/* i[] */
    RPMTAG_REQUIRENAME		= 1049,	/* s[] */
#define	RPMTAG_REQUIRES RPMTAG_REQUIRENAME	/* s[] */
    RPMTAG_REQUIREVERSION	= 1050,	/* s[] */
    RPMTAG_NOSOURCE		= 1051, /* i */
    RPMTAG_NOPATCH		= 1052, /* i */
    RPMTAG_CONFLICTFLAGS	= 1053, /* i[] */
    RPMTAG_CONFLICTNAME		= 1054,	/* s[] */
#define	RPMTAG_CONFLICTS RPMTAG_CONFLICTNAME	/* s[] */
#define	RPMTAG_C	RPMTAG_CONFLICTNAME	/* s[] */
    RPMTAG_CONFLICTVERSION	= 1055,	/* s[] */
    RPMTAG_DEFAULTPREFIX	= 1056, /* s internal - deprecated */
    RPMTAG_BUILDROOT		= 1057, /* s internal */
    RPMTAG_INSTALLPREFIX	= 1058, /* s internal - deprecated */
    RPMTAG_EXCLUDEARCH		= 1059, /* s[] */
    RPMTAG_EXCLUDEOS		= 1060, /* s[] */
    RPMTAG_EXCLUSIVEARCH	= 1061, /* s[] */
    RPMTAG_EXCLUSIVEOS		= 1062, /* s[] */
    RPMTAG_AUTOREQPROV		= 1063, /* s internal */
    RPMTAG_RPMVERSION		= 1064,	/* s */
    RPMTAG_TRIGGERSCRIPTS	= 1065,	/* s[] */
    RPMTAG_TRIGGERNAME		= 1066,	/* s[] */
    RPMTAG_TRIGGERVERSION	= 1067,	/* s[] */
    RPMTAG_TRIGGERFLAGS		= 1068,	/* i[] */
    RPMTAG_TRIGGERINDEX		= 1069,	/* i[] */
    RPMTAG_VERIFYSCRIPT		= 1079,	/* s */
    RPMTAG_CHANGELOGTIME	= 1080,	/* i[] */
    RPMTAG_CHANGELOGNAME	= 1081,	/* s[] */
    RPMTAG_CHANGELOGTEXT	= 1082,	/* s[] */
    RPMTAG_BROKENMD5		= 1083, /* internal - obsolete */
    RPMTAG_PREREQ		= 1084, /* internal */
    RPMTAG_PREINPROG		= 1085,	/* s */
    RPMTAG_POSTINPROG		= 1086,	/* s */
    RPMTAG_PREUNPROG		= 1087,	/* s */
    RPMTAG_POSTUNPROG		= 1088,	/* s */
    RPMTAG_BUILDARCHS		= 1089, /* s[] */
    RPMTAG_OBSOLETENAME		= 1090,	/* s[] */
#define	RPMTAG_OBSOLETES RPMTAG_OBSOLETENAME	/* s[] */
#define	RPMTAG_O	RPMTAG_OBSOLETENAME	/* s[] */
    RPMTAG_VERIFYSCRIPTPROG	= 1091,	/* s */
    RPMTAG_TRIGGERSCRIPTPROG	= 1092,	/* s[] */
    RPMTAG_DOCDIR		= 1093, /* internal */
    RPMTAG_COOKIE		= 1094,	/* s */
    RPMTAG_FILEDEVICES		= 1095,	/* i[] */
    RPMTAG_FILEINODES		= 1096,	/* i[] */
    RPMTAG_FILELANGS		= 1097,	/* s[] */
    RPMTAG_PREFIXES		= 1098,	/* s[] */
    RPMTAG_INSTPREFIXES		= 1099,	/* s[] */
    RPMTAG_TRIGGERIN		= 1100, /* internal */
    RPMTAG_TRIGGERUN		= 1101, /* internal */
    RPMTAG_TRIGGERPOSTUN	= 1102, /* internal */
    RPMTAG_AUTOREQ		= 1103, /* internal */
    RPMTAG_AUTOPROV		= 1104, /* internal */
    RPMTAG_CAPABILITY		= 1105, /* i legacy - obsolete */
    RPMTAG_SOURCEPACKAGE	= 1106, /* i legacy - obsolete */
    RPMTAG_OLDORIGFILENAMES	= 1107, /* internal - obsolete */
    RPMTAG_BUILDPREREQ		= 1108, /* internal */
    RPMTAG_BUILDREQUIRES	= 1109, /* internal */
    RPMTAG_BUILDCONFLICTS	= 1110, /* internal */
    RPMTAG_BUILDMACROS		= 1111, /* internal - unused */
    RPMTAG_PROVIDEFLAGS		= 1112,	/* i[] */
    RPMTAG_PROVIDEVERSION	= 1113,	/* s[] */
    RPMTAG_OBSOLETEFLAGS	= 1114,	/* i[] */
    RPMTAG_OBSOLETEVERSION	= 1115,	/* s[] */
    RPMTAG_DIRINDEXES		= 1116,	/* i[] */
    RPMTAG_BASENAMES		= 1117,	/* s[] */
    RPMTAG_DIRNAMES		= 1118,	/* s[] */
    RPMTAG_ORIGDIRINDEXES	= 1119, /* i[] relocation */
    RPMTAG_ORIGBASENAMES	= 1120, /* s[] relocation */
    RPMTAG_ORIGDIRNAMES		= 1121, /* s[] relocation */
    RPMTAG_OPTFLAGS		= 1122,	/* s */
    RPMTAG_DISTURL		= 1123,	/* s */
    RPMTAG_PAYLOADFORMAT	= 1124,	/* s */
    RPMTAG_PAYLOADCOMPRESSOR	= 1125,	/* s */
    RPMTAG_PAYLOADFLAGS		= 1126,	/* s */
    RPMTAG_INSTALLCOLOR		= 1127, /* i transaction color when installed */
    RPMTAG_INSTALLTID		= 1128,	/* i */
    RPMTAG_REMOVETID		= 1129,	/* i */
    RPMTAG_SHA1RHN		= 1130, /* internal - obsolete */
    RPMTAG_RHNPLATFORM		= 1131,	/* s deprecated */
    RPMTAG_PLATFORM		= 1132,	/* s */
    RPMTAG_PATCHESNAME		= 1133, /* s[] deprecated placeholder (SuSE) */
    RPMTAG_PATCHESFLAGS		= 1134, /* i[] deprecated placeholder (SuSE) */
    RPMTAG_PATCHESVERSION	= 1135, /* s[] deprecated placeholder (SuSE) */
    RPMTAG_CACHECTIME		= 1136,	/* i internal - obsolete */
    RPMTAG_CACHEPKGPATH		= 1137,	/* s internal - obsolete */
    RPMTAG_CACHEPKGSIZE		= 1138,	/* i internal - obsolete */
    RPMTAG_CACHEPKGMTIME	= 1139,	/* i internal - obsolete */
    RPMTAG_FILECOLORS		= 1140,	/* i[] */
    RPMTAG_FILECLASS		= 1141,	/* i[] */
    RPMTAG_CLASSDICT		= 1142,	/* s[] */
    RPMTAG_FILEDEPENDSX		= 1143,	/* i[] */
    RPMTAG_FILEDEPENDSN		= 1144,	/* i[] */
    RPMTAG_DEPENDSDICT		= 1145,	/* i[] */
    RPMTAG_SOURCEPKGID		= 1146,	/* x */
    RPMTAG_FILECONTEXTS		= 1147,	/* s[] - obsolete */
    RPMTAG_FSCONTEXTS		= 1148,	/* s[] extension */
    RPMTAG_RECONTEXTS		= 1149,	/* s[] extension */
    RPMTAG_POLICIES		= 1150,	/* s[] selinux *.te policy file. */
    RPMTAG_PRETRANS		= 1151,	/* s */
    RPMTAG_POSTTRANS		= 1152,	/* s */
    RPMTAG_PRETRANSPROG		= 1153,	/* s */
    RPMTAG_POSTTRANSPROG	= 1154,	/* s */
    RPMTAG_DISTTAG		= 1155,	/* s */
    RPMTAG_SUGGESTSNAME		= 1156,	/* s[] extension (unimplemented) */
#define	RPMTAG_SUGGESTS RPMTAG_SUGGESTSNAME	/* s[] (unimplemented) */
    RPMTAG_SUGGESTSVERSION	= 1157,	/* s[] extension (unimplemented) */
    RPMTAG_SUGGESTSFLAGS	= 1158,	/* i[] extension (unimplemented) */
    RPMTAG_ENHANCESNAME		= 1159,	/* s[] extension placeholder (unimplemented) */
#define	RPMTAG_ENHANCES RPMTAG_ENHANCESNAME	/* s[] (unimplemented) */
    RPMTAG_ENHANCESVERSION	= 1160,	/* s[] extension placeholder (unimplemented) */
    RPMTAG_ENHANCESFLAGS	= 1161,	/* i[] extension placeholder (unimplemented) */
    RPMTAG_PRIORITY		= 1162, /* i[] extension placeholder (unimplemented) */
    RPMTAG_CVSID		= 1163, /* s (unimplemented) */
#define	RPMTAG_SVNID	RPMTAG_CVSID	/* s (unimplemented) */
    RPMTAG_BLINKPKGID		= 1164, /* s[] (unimplemented) */
    RPMTAG_BLINKHDRID		= 1165, /* s[] (unimplemented) */
    RPMTAG_BLINKNEVRA		= 1166, /* s[] (unimplemented) */
    RPMTAG_FLINKPKGID		= 1167, /* s[] (unimplemented) */
    RPMTAG_FLINKHDRID		= 1168, /* s[] (unimplemented) */
    RPMTAG_FLINKNEVRA		= 1169, /* s[] (unimplemented) */
    RPMTAG_PACKAGEORIGIN	= 1170, /* s (unimplemented) */
    RPMTAG_TRIGGERPREIN		= 1171, /* internal */
    RPMTAG_BUILDSUGGESTS	= 1172, /* internal (unimplemented) */
    RPMTAG_BUILDENHANCES	= 1173, /* internal (unimplemented) */
    RPMTAG_SCRIPTSTATES		= 1174, /* i[] scriptlet exit codes (unimplemented) */
    RPMTAG_SCRIPTMETRICS	= 1175, /* i[] scriptlet execution times (unimplemented) */
    RPMTAG_BUILDCPUCLOCK	= 1176, /* i (unimplemented) */
    RPMTAG_FILEDIGESTALGOS	= 1177, /* i[] (unimplemented) */
    RPMTAG_VARIANTS		= 1178, /* s[] (unimplemented) */
    RPMTAG_XMAJOR		= 1179, /* i (unimplemented) */
    RPMTAG_XMINOR		= 1180, /* i (unimplemented) */
    RPMTAG_REPOTAG		= 1181,	/* s (unimplemented) */
    RPMTAG_KEYWORDS		= 1182,	/* s[] (unimplemented) */
    RPMTAG_BUILDPLATFORMS	= 1183,	/* s[] (unimplemented) */
    RPMTAG_PACKAGECOLOR		= 1184, /* i (unimplemented) */
    RPMTAG_PACKAGEPREFCOLOR	= 1185, /* i (unimplemented) */
    RPMTAG_XATTRSDICT		= 1186, /* s[] (unimplemented) */
    RPMTAG_FILEXATTRSX		= 1187, /* i[] (unimplemented) */
    RPMTAG_DEPATTRSDICT		= 1188, /* s[] (unimplemented) */
    RPMTAG_CONFLICTATTRSX	= 1189, /* i[] (unimplemented) */
    RPMTAG_OBSOLETEATTRSX	= 1190, /* i[] (unimplemented) */
    RPMTAG_PROVIDEATTRSX	= 1191, /* i[] (unimplemented) */
    RPMTAG_REQUIREATTRSX	= 1192, /* i[] (unimplemented) */
    RPMTAG_BUILDPROVIDES	= 1193, /* internal */
    RPMTAG_BUILDOBSOLETES	= 1194, /* internal */
    RPMTAG_DBINSTANCE		= 1195, /* i extension */
    RPMTAG_NVRA			= 1196, /* s extension */
    RPMTAG_FILENAMES		= 5000, /* s[] extension */
    RPMTAG_FILEPROVIDE		= 5001, /* s[] extension */
    RPMTAG_FILEREQUIRE		= 5002, /* s[] extension */
    RPMTAG_FSNAMES		= 5003, /* s[] (unimplemented) */
    RPMTAG_FSSIZES		= 5004, /* l[] (unimplemented) */
    RPMTAG_TRIGGERCONDS		= 5005, /* s[] extension */
    RPMTAG_TRIGGERTYPE		= 5006, /* s[] extension */
    RPMTAG_ORIGFILENAMES	= 5007, /* s[] extension */
    RPMTAG_LONGFILESIZES	= 5008,	/* l[] */
    RPMTAG_LONGSIZE		= 5009, /* l */
    RPMTAG_FILECAPS		= 5010, /* s[] */
    RPMTAG_FILEDIGESTALGO	= 5011, /* i file digest algorithm */
    RPMTAG_BUGURL		= 5012, /* s */
    RPMTAG_EVR			= 5013, /* s extension */
    RPMTAG_NVR			= 5014, /* s extension */
    RPMTAG_NEVR			= 5015, /* s extension */
    RPMTAG_NEVRA		= 5016, /* s extension */
    RPMTAG_HEADERCOLOR		= 5017, /* i extension */
    RPMTAG_VERBOSE		= 5018, /* i extension */
    RPMTAG_EPOCHNUM		= 5019, /* i extension */
    RPMTAG_PREINFLAGS		= 5020, /* i */
    RPMTAG_POSTINFLAGS		= 5021, /* i */
    RPMTAG_PREUNFLAGS		= 5022, /* i */
    RPMTAG_POSTUNFLAGS		= 5023, /* i */
    RPMTAG_PRETRANSFLAGS	= 5024, /* i */
    RPMTAG_POSTTRANSFLAGS	= 5025, /* i */
    RPMTAG_VERIFYSCRIPTFLAGS	= 5026, /* i */
    RPMTAG_TRIGGERSCRIPTFLAGS	= 5027, /* i[] */

    RPMTAG_FIRSTFREE_TAG	/*!< internal */
} rpmTag;

#define	RPMTAG_EXTERNAL_TAG		1000000
#define RPMTAG_NOT_FOUND		-1

/** \ingroup signature
 * Tags found in signature header from package.
 */
typedef enum rpmSigTag_e {
    RPMSIGTAG_SIZE	= 1000,	/*!< internal Header+Payload size (32bit) in bytes. */
    RPMSIGTAG_LEMD5_1	= 1001,	/*!< internal Broken MD5, take 1 @deprecated legacy. */
    RPMSIGTAG_PGP	= 1002,	/*!< internal PGP 2.6.3 signature. */
    RPMSIGTAG_LEMD5_2	= 1003,	/*!< internal Broken MD5, take 2 @deprecated legacy. */
    RPMSIGTAG_MD5	= 1004,	/*!< internal MD5 signature. */
    RPMSIGTAG_GPG	= 1005, /*!< internal GnuPG signature. */
    RPMSIGTAG_PGP5	= 1006,	/*!< internal PGP5 signature @deprecated legacy. */
    RPMSIGTAG_PAYLOADSIZE = 1007,/*!< internal uncompressed payload size (32bit) in bytes. */
    RPMSIGTAG_BADSHA1_1	= RPMTAG_BADSHA1_1,	/*!< internal Broken SHA1, take 1. */
    RPMSIGTAG_BADSHA1_2	= RPMTAG_BADSHA1_2,	/*!< internal Broken SHA1, take 2. */
    RPMSIGTAG_SHA1	= RPMTAG_SHA1HEADER,	/*!< internal sha1 header digest. */
    RPMSIGTAG_DSA	= RPMTAG_DSAHEADER,	/*!< internal DSA header signature. */
    RPMSIGTAG_RSA	= RPMTAG_RSAHEADER,	/*!< internal RSA header signature. */
    RPMSIGTAG_LONGSIZE	= RPMTAG_LONGSIGSIZE,	/*!< internal Header+Payload size (64bit) in bytes. */
    RPMSIGTAG_LONGARCHIVESIZE = RPMTAG_LONGARCHIVESIZE, /*!< internal uncompressed payload size (64bit) in bytes. */
} rpmSigTag;


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
    RPM_INT64_TYPE		=  5,
    RPM_STRING_TYPE		=  6,
    RPM_BIN_TYPE		=  7,
    RPM_STRING_ARRAY_TYPE	=  8,
    RPM_I18NSTRING_TYPE		=  9,
#define	RPM_MAX_TYPE		9
#define RPM_FORCEFREE_TYPE	0xff
#define RPM_MASK_TYPE		0x0000ffff
} rpmTagType;

/** \ingroup rpmtag
 * The classes of data in tags from headers.
 */
typedef enum rpmTagClass_e {
    RPM_NULL_CLASS	= 0,
    RPM_NUMERIC_CLASS	= 1,
    RPM_STRING_CLASS	= 2,
    RPM_BINARY_CLASS	= 3,
} rpmTagClass;

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

/** \ingroup rpmtag
 * Return tag name from value.
 * @param tag		tag value
 * @return		tag name, "(unknown)" on not found
 */
const char * rpmTagGetName(rpmTag tag);

/** \ingroup rpmtag
 * Return tag data type from value.
 * @param tag		tag value
 * @return		tag data type, RPM_NULL_TYPE on not found.
 */
rpmTagType rpmTagGetType(rpmTag tag);

/** \ingroup rpmtag
 * Return tag data class from value.
 * @param tag		tag value
 * @return		tag data class, RPM_NULL_CLASS on not found.
 */
rpmTagClass rpmTagGetClass(rpmTag tag);

/** \ingroup rpmtag
 * Return tag value from name.
 * @param tagstr	name of tag
 * @return		tag value, -1 on not found
 */
rpmTag rpmTagGetValue(const char * tagstr);

/** \ingroup rpmtag
 * Return data class of type
 * @param type		tag type
 * @return		data class, RPM_NULL_CLASS on unknown.
 */
rpmTagClass rpmTagTypeGetClass(rpmTagType type);

/** \ingroup rpmtag
 * Return known rpm tag names, sorted by name.
 * @retval tagnames 	tag container of string array type
 * @param fullname	return short or full name
 * @return		number of tag names, 0 on error
 */
int rpmTagGetNames(rpmtd tagnames, int fullname);

#ifdef __cplusplus
}
#endif

#endif /* _RPMTAG_H */
