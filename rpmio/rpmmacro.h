#ifndef _H_MACRO_
#define	_H_MACRO_

/** \ingroup rpmio
 * \file rpmio/rpmmacro.h
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct rpmMacroEntry_s * rpmMacroEntry;

typedef struct rpmMacroContext_s * rpmMacroContext;

extern rpmMacroContext rpmGlobalMacroContext;

extern rpmMacroContext rpmCLIMacroContext;

/** \ingroup rpmrc
 * List of macro files to read when configuring rpm.
 * This is a colon separated list of files. URI's are permitted as well,
 * identified by the token '://', so file paths must not begin with '//'.
 */
extern const char * macrofiles;

/**
 * Markers for sources of macros added throughout rpm.
 */
#define	RMIL_DEFAULT	-15
#define	RMIL_MACROFILES	-13
#define	RMIL_RPMRC	-11

#define	RMIL_CMDLINE	-7
#define	RMIL_TARBALL	-5
#define	RMIL_SPEC	-3
#define	RMIL_OLDSPEC	-1
#define	RMIL_GLOBAL	0

/**
 * Print macros to file stream.
 * @param mc		macro context (NULL uses global context).
 * @param fp		file stream (NULL uses stderr).
 */
void	rpmDumpMacroTable	(rpmMacroContext mc,
					FILE * fp);

/**
 * Return URL path(s) from a (URL prefixed) pattern glob.
 * @param patterns	glob pattern
 * @retval *argcPtr	no. of paths
 * @retval *argvPtr	array of paths (malloc'd contiguous blob)
 * @return		0 on success
 */
int rpmGlob(const char * patterns, int * argcPtr,
		const char *** argvPtr);

/**
 * Expand macro into buffer.
 * @deprecated Use rpmExpand().
 * @todo Eliminate from API.
 * @param spec		cookie (unused)
 * @param mc		macro context (NULL uses global context).
 * @retval sbuf		input macro to expand, output expansion
 * @param slen		size of buffer
 * @return		0 on success
 */
int	expandMacros	(void * spec, rpmMacroContext mc,
				char * sbuf,
				size_t slen);

/**
 * Add macro to context.
 * @deprecated Use rpmDefineMacro().
 * @param mc		macro context (NULL uses global context).
 * @param n		macro name
 * @param o		macro paramaters
 * @param b		macro body
 * @param level		macro recursion level (0 is entry API)
 */
void	addMacro	(rpmMacroContext mc, const char * n,
				const char * o,
				const char * b, int level);

/**
 * Delete macro from context.
 * @param mc		macro context (NULL uses global context).
 * @param n		macro name
 */
void	delMacro	(rpmMacroContext mc, const char * n);

/**
 * Define macro in context.
 * @param mc		macro context (NULL uses global context).
 * @param macro		macro name, options, body
 * @param level		macro recursion level (0 is entry API)
 * @return		@todo Document.
 */
int	rpmDefineMacro	(rpmMacroContext mc, const char * macro,
				int level);

/**
 * Load macros from specific context into global context.
 * @param mc		macro context (NULL does nothing).
 * @param level		macro recursion level (0 is entry API)
 */
void	rpmLoadMacros	(rpmMacroContext mc, int level);

/**
 * Load macro context from a macro file.
 * @param mc		(unused)
 * @param fn		macro file name
 */
int	rpmLoadMacroFile(rpmMacroContext mc, const char * fn);

/**
 * Initialize macro context from set of macrofile(s).
 * @param mc		macro context
 * @param macrofiles	colon separated list of macro files (NULL does nothing)
 */
void	rpmInitMacros	(rpmMacroContext mc, const char * macrofiles);

/**
 * Destroy macro context.
 * @param mc		macro context (NULL uses global context).
 */
void	rpmFreeMacros	(rpmMacroContext mc);

/**
 * Return (malloc'ed) concatenated macro expansion(s).
 * @param arg		macro(s) to expand (NULL terminates list)
 * @return		macro expansion (malloc'ed)
 */
char * rpmExpand	(const char * arg, ...);

/**
 * Return macro expansion as a numeric value.
 * Boolean values ('Y' or 'y' returns 1, 'N' or 'n' returns 0)
 * are permitted as well. An undefined macro returns 0.
 * @param arg		macro to expand
 * @return		numeric value
 */
int	rpmExpandNumeric (const char * arg);

#ifdef __cplusplus
}
#endif

#endif	/* _H_ MACRO_ */
