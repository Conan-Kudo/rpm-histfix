/* Copyright (c) 1998, 1999 Thai Open Source Software Center Ltd
   See the file COPYING for copying permission.
*/

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <fcntl.h>
#ifdef COMPILED_FROM_DSP
#include "winconfig.h"
#else
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#endif
#include "expat.h"
#include "xmlfile.h"
#include "xmltchar.h"
#include "filemap.h"

#ifdef _MSC_VER
#include <io.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifndef O_BINARY
#ifdef _O_BINARY
#define O_BINARY _O_BINARY
#else
#define O_BINARY 0
#endif
#endif

#ifdef _DEBUG
#define READ_SIZE 16
#else
#define READ_SIZE (1024*8)
#endif


typedef struct {
  XML_Parser parser;
  int *retPtr;
} PROCESS_ARGS;

static void
reportError(XML_Parser parser, const XML_Char *filename)
	/*@globals fileSystem @*/
	/*@modifies parser, fileSystem @*/
{
  int code = XML_GetErrorCode(parser);
  const XML_Char *message = XML_ErrorString(code);
  if (message != NULL)
    ftprintf(stdout, T("%s:%d:%d: %s\n"),
             filename,
             XML_GetErrorLineNumber(parser),
             XML_GetErrorColumnNumber(parser),
             message);
  else
    ftprintf(stderr, T("%s: (unknown message %d)\n"), filename, code);
}

static void
processFile(const void *data, size_t size,
            const XML_Char *filename, void *args)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/
{
  XML_Parser parser = ((PROCESS_ARGS *)args)->parser;
  int *retPtr = ((PROCESS_ARGS *)args)->retPtr;
  if (XML_Parse(parser, data, size, 1) == XML_STATUS_ERROR) {
    reportError(parser, filename);
    *retPtr = 0;
  }
  else
    *retPtr = 1;
}

#ifdef WIN32

static int
isAsciiLetter(XML_Char c)
	/*@*/
{
  return (T('a') <= c && c <= T('z')) || (T('A') <= c && c <= T('Z'));
}

#endif /* WIN32 */

static const XML_Char *
resolveSystemId(/*@null@*/ const XML_Char *base,
		/*@returned@*/ const XML_Char *systemId,
                /*@out@*/ XML_Char **toFree)
	/*@modifies *toFree @*/
{
  XML_Char *s;
/*@-mustdefine -nullstate@*/
  *toFree = NULL;
  if (base == NULL
      || *systemId == T('/')
#ifdef WIN32
      || *systemId == T('\\')
      || (isAsciiLetter(systemId[0]) && systemId[1] == T(':'))
#endif
     )
    return systemId;
  *toFree = (XML_Char *)malloc((tcslen(base) + tcslen(systemId) + 2)
                               * sizeof(XML_Char));
  if (*toFree == NULL)
    return systemId;
/*@=mustdefine =nullstate@*/

  tcscpy(*toFree, base);
  s = *toFree;
  if (tcsrchr(s, T('/')) != NULL)
    s = tcsrchr(s, T('/')) + 1;
#ifdef WIN32
  if (tcsrchr(s, T('\\')))
    s = tcsrchr(s, T('\\')) + 1;
#endif
  tcscpy(s, systemId);
/*@-retalias@*/
  return *toFree;
/*@=retalias@*/
}

static int
externalEntityRefFilemap(XML_Parser parser,
                         const XML_Char *context,
                         const XML_Char *base,
                         const XML_Char *systemId,
                         /*@unused@*/ const XML_Char *publicId)
	/*@globals fileSystem, internalState @*/
	/*@modifies fileSystem, internalState @*/
{
  int result;
  XML_Char *s;
  const XML_Char *filename;
  XML_Parser entParser = XML_ExternalEntityParserCreate(parser, context, 0);
  PROCESS_ARGS args;
  args.retPtr = &result;
  args.parser = entParser;
  filename = resolveSystemId(base, systemId, &s);
  (void) XML_SetBase(entParser, filename);
  if (!filemap(filename, processFile, &args))
    result = 0;
  free(s);
/*@-kepttrans@*/
  XML_ParserFree(entParser);
/*@=kepttrans@*/
  return result;
}

static int
processStream(const XML_Char *filename, XML_Parser parser)
	/*@globals fileSystem, internalState @*/
	/*@modifies parser, fileSystem, internalState @*/
{
  /* passing NULL for filename means read intput from stdin */
  int fd = 0;   /* 0 is the fileno for stdin */

  if (filename != NULL) {
    fd = topen(filename, O_BINARY|O_RDONLY);
    if (fd < 0) {
      tperror(filename);
      return 0;
    }
  }
  for (;;) {
    int nread;
    char *buf = XML_GetBuffer(parser, READ_SIZE);
    if (buf == NULL) {
      if (filename != NULL)
        (void) close(fd);
      ftprintf(stderr, T("%s: out of memory\n"),
               filename != NULL ? filename : "xmlwf");
      return 0;
    }
    nread = read(fd, buf, READ_SIZE);
    if (nread < 0) {
      tperror(filename != NULL ? filename : "STDIN");
      if (filename != NULL)
        (void) close(fd);
      return 0;
    }
    if (XML_ParseBuffer(parser, nread, nread == 0) == XML_STATUS_ERROR) {
      reportError(parser, filename != NULL ? filename : "STDIN");
      if (filename != NULL)
        (void) close(fd);
      return 0;
    }
    if (nread == 0) {
      if (filename != NULL)
        (void) close(fd);
      break;;
    }
  }
  return 1;
}

static int
externalEntityRefStream(XML_Parser parser,
                        const XML_Char *context,
                        const XML_Char *base,
                        const XML_Char *systemId,
                        /*@unused@*/ const XML_Char *publicId)
	/*@globals fileSystem, internalState @*/
	/*@modifies fileSystem, internalState @*/
{
  XML_Char *s;
  const XML_Char *filename;
  int ret;
  XML_Parser entParser = XML_ExternalEntityParserCreate(parser, context, 0);
  filename = resolveSystemId(base, systemId, &s);
  (void) XML_SetBase(entParser, filename);
  ret = processStream(filename, entParser);
  free(s);
  XML_ParserFree(entParser);
  return ret;
}

int
XML_ProcessFile(XML_Parser parser,
                const XML_Char *filename,
                unsigned flags)
{
  int result;

  if (!XML_SetBase(parser, filename)) {
    ftprintf(stderr, T("%s: out of memory"), filename);
    exit(EXIT_FAILURE);
  }

  if (flags & XML_EXTERNAL_ENTITIES)
      XML_SetExternalEntityRefHandler(parser,
                                      (flags & XML_MAP_FILE)
                                      ? externalEntityRefFilemap
                                      : externalEntityRefStream);
/*@-branchstate@*/
  if (flags & XML_MAP_FILE) {
    PROCESS_ARGS args;
    args.retPtr = &result;
    args.parser = parser;
    if (!filemap(filename, processFile, &args))
      result = 0;
  }
  else
    result = processStream(filename, parser);
/*@=branchstate@*/
  return result;
}
