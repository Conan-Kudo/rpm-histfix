#ifndef H_QUERY
#define H_QUERY

enum querysources { QUERY_PATH, QUERY_PACKAGE, QUERY_ALL, QUERY_SPATH,
		    QUERY_SPACKAGE, QUERY_RPM, QUERY_SRPM };

#define QUERY_FOR_INFO 1
#define QUERY_FOR_LIST 2
#define QUERY_FOR_STATE 4
#define QUERY_FOR_DOCS 8
#define QUERY_FOR_CONFIG 16

void doQuery(char * prefix, enum querysources source, int queryFlags, 
	     char * arg);

#endif
