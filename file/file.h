/*
 * file.h - definitions for file(1) program
 * @(#)Id: file.h,v 1.43 2002/07/03 18:57:52 christos Exp 
 *
 * Copyright (c) Ian F. Darwin, 1987.
 * Written by Ian F. Darwin.
 *
 * This software is not subject to any license of the American Telephone
 * and Telegraph Company or of the Regents of the University of California.
 *
 * Permission is granted to anyone to use this software for any purpose on
 * any computer system, and to alter it and redistribute it freely, subject
 * to the following restrictions:
 *
 * 1. The author is not responsible for the consequences of use of this
 *    software, no matter how awful, even if they arise from flaws in it.
 *
 * 2. The origin of this software must not be misrepresented, either by
 *    explicit claim or by omission.  Since few users ever read sources,
 *    credits must appear in the documentation.
 *
 * 3. Altered versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.  Since few users
 *    ever read sources, credits must appear in the documentation.
 *
 * 4. This notice may not be removed or altered.
 */

#ifndef __file_h__
#define __file_h__

#ifndef HOWMANY
# define HOWMANY 65536		/* how much of the file to look at */
#endif
#define MAXMAGIS 4096		/* max entries in /etc/magic */
#define MAXDESC	50		/* max leng of text description */
#define MAXstring 32		/* max leng of "string" types */

#define MAGICNO		0xF11E041C
#define VERSIONNO	1

#define CHECK	1
#define COMPILE	2

struct magic {
	uint16_t cont_level;	/* level of ">" */
	uint8_t nospflag;	/* supress space character */
	uint8_t flag;
#define INDIR	1		/* if '>(...)' appears,  */
#define	UNSIGNED 2		/* comparison is unsigned */
#define OFFADD	4		/* if '>&' appears,  */
	uint8_t reln;		/* relation (0=eq, '>'=gt, etc) */
	uint8_t vallen;		/* length of string value, if any */
	uint8_t type;		/* int, short, long or string. */
	uint8_t in_type;	/* type of indirrection */
#define 			BYTE	1
#define				SHORT	2
#define				LONG	4
#define				STRING	5
#define				DATE	6
#define				BESHORT	7
#define				BELONG	8
#define				BEDATE	9
#define				LESHORT	10
#define				LELONG	11
#define				LEDATE	12
#define				PSTRING	13
#define				LDATE	14
#define				BELDATE	15
#define				LELDATE	16
#define				REGEX	17
	uint8_t in_op;		/* operator for indirection */
	uint8_t mask_op;	/* operator for mask */
#define				OPAND	1
#define				OPOR	2
#define				OPXOR	3
#define				OPADD	4
#define				OPMINUS	5
#define				OPMULTIPLY	6
#define				OPDIVIDE	7
#define				OPMODULO	8
#define				OPINVERSE	0x80
	int32_t offset;		/* offset to magic number */
	int32_t in_offset;	/* offset from indirection */
	union VALUETYPE {
		uint8_t b;
		uint16_t h;
		uint32_t l;
		char s[MAXstring];
		char *buf;
		uint8_t hs[2];	/* 2 bytes of a fixed-endian "short" */
		uint8_t hl[4];	/* 4 bytes of a fixed-endian "long" */
	} value;		/* either number or string */
	uint32_t mask;	/* mask before comparison with value */
	char desc[MAXDESC];	/* description */
} __attribute__((__packed__));

#define BIT(A)   (1 << (A))
#define STRING_IGNORE_LOWERCASE		BIT(0)
#define STRING_COMPACT_BLANK		BIT(1)
#define STRING_COMPACT_OPTIONAL_BLANK	BIT(2)
#define CHAR_IGNORE_LOWERCASE		'c'
#define CHAR_COMPACT_BLANK		'B'
#define CHAR_COMPACT_OPTIONAL_BLANK	'b'


/* list of magic entries */
struct mlist {
	struct magic *magic;		/* array of magic entries */
	uint32_t nmagic;		/* number of entries in array */
/*@null@*/
	struct mlist *next;
/*@null@*/
	struct mlist *prev;
};

/*@unchecked@*/ /*@observer@*/ /*@null@*/
extern char *progname;		/* the program name 			*/
/*@unchecked@*/ /*@dependent@*/ /*@observer@*/
extern const char *magicfile;	/* name of the magic file		*/
/*@unchecked@*/
extern int lineno;		/* current line number in magic file	*/

/*@unchecked@*/
extern struct mlist mlist;	/* list of arrays of magic entries	*/

/*@unchecked@*/
extern int debug;		/* enable debugging?			*/
/*@unchecked@*/
extern int zflag;		/* process compressed files?		*/
/*@unchecked@*/
extern int lflag;		/* follow symbolic links?		*/
/*@unchecked@*/
extern int sflag;		/* read/analyze block special files?	*/
/*@unchecked@*/
extern int iflag;		/* Output types as mime-types		*/

#ifdef NEED_GETOPT
/*@unchecked@*/
extern int optind;		/* From getopt(3)			*/
/*@unchecked@*/
extern char *optarg;
#endif

/*@mayexit@*/
extern int   apprentice(const char *fn, int action)
	/*@globals lineno, mlist, fileSystem, internalState @*/
	/*@modifies lineno, mlist, fileSystem, internalState @*/;
extern int   ascmagic(unsigned char *buf, int nbytes)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/;
/*@exits@*/
extern void  error(const char *f, ...)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/;
extern void  ckfputs(const char *str, FILE *fil)
	/*@globals fileSystem @*/
	/*@modifies fil, fileSystem @*/;
struct stat;
extern int   fsmagic(const char *fn, /*@out@*/ struct stat *sb)
	/*@globals fileSystem @*/
	/*@modifies *sb, fileSystem @*/;
/*@observer@*/
extern char *fmttime(long v, int local)
	/*@*/;
extern int   is_tar(unsigned char *buf, int nbytes)
	/*@*/;
extern void  magwarn(const char *f, ...)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/;
extern void  mdump(struct magic *m)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/;
extern void  process(const char *inname, int wid)
	/*@globals fileSystem, internalState @*/
	/*@modifies fileSystem, internalState @*/;
extern void  showstr(FILE *fp, const char *s, int len)
	/*@globals fileSystem @*/
	/*@modifies fp, fileSystem @*/;
extern int   softmagic(unsigned char *buf, int nbytes)
	/*@globals fileSystem @*/
	/*@modifies buf, fileSystem @*/;
extern int   tryit(const char *fn, unsigned char *buf, int nb, int zfl)
	/*@globals fileSystem, internalState @*/
	/*@modifies buf, fileSystem, internalState @*/;
extern int   zmagic(const char *fname, unsigned char *buf, int nbytes)
	/*@globals fileSystem, internalState @*/
	/*@modifies buf, fileSystem, internalState @*/;
extern void  ckfprintf(FILE *f, const char *fmt, ...)
	/*@globals fileSystem @*/
	/*@modifies f, fileSystem @*/;
extern uint32_t signextend(struct magic *m, uint32_t v)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/;
extern void tryelf(int fd, unsigned char *buf, int nbytes)
	/*@globals fileSystem, internalState @*/
	/*@modifies fileSystem, internalState @*/;
extern int pipe2file(int fd, void *startbuf, size_t nbytes)
	/*@globals errno, fileSystem, internalState @*/
	/*@modifies errno, fileSystem, internalState @*/;

#endif /* __file_h__ */
