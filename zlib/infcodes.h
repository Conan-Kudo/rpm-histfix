/*
 * Copyright (C) 1995-1998 Mark Adler
 * For conditions of distribution and use, see copyright notice in zlib.h 
 */

/**
 * \file infcodes.h
 * Header to use infcodes.c.
 */
/* WARNING: this file should *not* be used by applications. It is
   part of the implementation of the compression library and is
   subject to change. Applications should only use zlib.h.
 */

typedef enum {        /* waiting for "i:"=input, "o:"=output, "x:"=nothing */
      START,	/*!< x: set up for LEN */
      LEN,	/*!< i: get length/literal/eob next */
      LENEXT,	/*!< i: getting length extra (have base) */
      DIST,	/*!< i: get distance next */
      DISTEXT,	/*!< i: getting distance extra */
      COPY,	/*!< o: copying bytes in window, waiting for space */
      LIT,	/*!< o: got literal, waiting for output space */
      WASH,	/*!< o: got eob, possibly still output waiting */
      END,	/*!< x: got eob and all data flushed */
      BADCODE	/*!< x: got error */
} inflate_codes_mode;

/* inflate codes private state */
struct inflate_codes_state {

  /* mode */
  inflate_codes_mode mode;      /*!< current inflate_codes mode */

  /* mode dependent information */
  uInt len;
  union {
    struct {
/*@dependent@*/
      inflate_huft *tree;       /*!< pointer into tree */
      uInt need;                /*!< bits needed */
    } code;             /*!< if LEN or DIST, where in tree */
    uInt lit;           /*!< if LIT, literal */
    struct {
      uInt get;                 /*!< bits to get for extra */
      uInt dist;                /*!< distance back to copy from */
    } copy;             /*!< if EXT or COPY, where and how much */
  } sub;                /*!< submode */

  /* mode independent information */
  Byte lbits;           /*!< ltree bits decoded per branch */
  Byte dbits;           /*!< dtree bits decoder per branch */
/*@dependent@*/
  inflate_huft *ltree;          /*!< literal/length/eob tree */
/*@dependent@*/
  inflate_huft *dtree;          /*!< distance tree */

};

typedef struct inflate_codes_state FAR inflate_codes_statef;

/*@null@*/
extern inflate_codes_statef *inflate_codes_new OF((
    uInt bl, uInt bd,
    /*@dependent@*/ inflate_huft *tl, /*@dependent@*/ inflate_huft *td,
    z_streamp z))
	/*@*/;

extern int inflate_codes OF((
    inflate_blocks_statef *s,
    z_streamp z,
    int r))
	/*@modifies s, z @*/;

extern void inflate_codes_free OF((
    /*@only@*/ inflate_codes_statef *s,
    z_streamp z))
	/*@*/;

