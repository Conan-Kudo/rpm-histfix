#ifndef _RPMFI_INTERNAL_H
#define _RPMFI_INTERNAL_H

#include <rpm/header.h>
#include <rpm/rpmfi.h>
#include "lib/fsm.h"		/* for FSM_t */

/**
 */
typedef struct sharedFileInfo_s *		sharedFileInfo;

/**
 */
struct sharedFileInfo_s {
    int pkgFileNum;
    int otherFileNum;
    int otherPkg;
    int isRemoved;
};

/**
 * A package filename set.
 */
struct rpmfi_s {
    int i;			/*!< Current file index. */
    int j;			/*!< Current directory index. */

    const char * Type;		/*!< Tag name. */

    rpmTag tagN;		/*!< Header tag. */
    Header h;			/*!< Header for file info set (or NULL) */

/*?null?*/
    const char ** bnl;		/*!< Base name(s) (from header) */
/*?null?*/
    const char ** dnl;		/*!< Directory name(s) (from header) */

    const char ** flinks;	/*!< File link(s) (from header) */
    const char ** flangs;	/*!< File lang(s) (from header) */

          uint32_t * dil;	/*!< Directory indice(s) (from header) */
/*?null?*/
    const rpm_flag_t * fflags;	/*!< File flag(s) (from header) */
/*?null?*/
    const rpm_off_t * fsizes;	/*!< File size(s) (from header) */
/*?null?*/
    const rpm_time_t * fmtimes;	/*!< File modification time(s) (from header) */
/*?null?*/
          rpm_mode_t * fmodes;	/*!< File mode(s) (from header) */
/*?null?*/
    const rpm_rdev_t * frdevs;	/*!< File rdev(s) (from header) */
/*?null?*/
    const rpm_ino_t * finodes;	/*!< File inodes(s) (from header) */

    const char ** fuser;	/*!< File owner(s) (from header) */
    const char ** fgroup;	/*!< File group(s) (from header) */

    char * fstates;		/*!< File state(s) (from header) */

    const rpm_color_t * fcolors;/*!< File color bits (header) */

    const char ** fcontexts;	/*! FIle security contexts. */

    const char ** fcaps;	/*! File capabilities (header) */

    const char ** cdict;	/*!< File class dictionary (header) */
    rpm_count_t ncdict;		/*!< No. of class entries. */
    const uint32_t * fcdictx;	/*!< File class dictionary index (header) */

    const uint32_t * ddict;	/*!< File depends dictionary (header) */
    rpm_count_t nddict;		/*!< No. of depends entries. */
    const uint32_t * fddictx;	/*!< File depends dictionary start (header) */
    const uint32_t * fddictn;	/*!< File depends dictionary count (header) */

/*?null?*/
    const rpm_flag_t * vflags;	/*!< File verify flag(s) (from header) */

    rpm_count_t dc;		/*!< No. of directories. */
    rpm_count_t fc;		/*!< No. of files. */

/*=============================*/
    rpmte te;

    headerGetFlags scareFlags;	/*!< headerGet flags wrt scareMem */
/*-----------------------------*/
    rpmfileAttrs flags;		/*!< File flags (default). */
    rpmFileAction action;	/*!< File disposition (default). */
    rpmFileAction * actions;	/*!< File disposition(s). */
    struct fingerPrint_s * fps;	/*!< File fingerprint(s). */

    pgpHashAlgo digestalgo;	/*!< File checksum algorithm */
    unsigned char * digests;	/*!< File checksums in binary. */

#define RPMFI_HAVE_PRETRANS	(1 << 0)
#define RPMFI_HAVE_POSTTRANS	(1 << 1)
    int transscripts;		/*!< pre/posttrans script existence */

    char * fn;			/*!< File name buffer. */

    size_t striplen;
    rpm_loff_t archiveSize;
    char ** apath;
    FSM_t fsm;			/*!< File state machine data. */
    int keep_header;		/*!< Keep header? */
    sharedFileInfo replaced;	/*!< (TR_ADDED) */
    rpm_off_t * replacedSizes;	/*!< (TR_ADDED) */
    unsigned int record;	/*!< (TR_REMOVED) */
    int magic;
#define	RPMFIMAGIC	0x09697923
/*=============================*/

int nrefs;		/*!< Reference count. */
};

RPM_GNUC_INTERNAL
rpmfi rpmfiUpdateState(rpmfi fi, rpmts ts, rpmte p);

#endif	/* _RPMFI_INTERNAL_H */

