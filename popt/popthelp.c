#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "popt.h"
#include "poptint.h"

static void displayArgs(poptContext con, struct poptOption * key, 
			const char * arg, void * data) {
    if (key->shortName== '?')
	poptPrintHelp(con, stderr, 0);
    else
	poptPrintUsage(con, stderr, 0);
    exit(0);
}

struct poptOption poptHelpOptions[] = {
    { NULL, '\0', POPT_ARG_CALLBACK, &displayArgs, '\0', NULL },
    { "help", '?', 0, NULL, '?', "Show this help message" },
    { "usage", '\0', 0, NULL, 'u', "Display brief usage message" },
    { NULL, '\0', 0, NULL, 0 }
} ;

static void singleOptionHelp(FILE * f, int maxLeftCol, 
			     const struct poptOption * opt) {
    int indentLength = maxLeftCol + 5;
    int lineLength = 79 - indentLength;
    const char * help = opt->descrip;
    int helpLength;
    const char * ch;
    char format[10];
    char * left = alloca(maxLeftCol + 1);

    *left = '\0';
    if (opt->longName && opt->shortName)
	sprintf(left, "-%c, --%s", opt->shortName, opt->longName);
    else if (opt->shortName) 
	sprintf(left, "-%c", opt->shortName);
    else if (opt->longName)
	sprintf(left, "--%s", opt->longName);
    if (!*left) return ;
    if (opt->argDescrip) {
	strcat(left, "=");
	strcat(left, opt->argDescrip);
    }

    if (help)
	fprintf(f,"  %-*s   ", maxLeftCol, left);
    else {
	fprintf(f,"  %s\n", left); 
	return;
    }

    helpLength = strlen(help);
    while (helpLength > lineLength) {
	ch = help + lineLength - 1;
	while (ch > help && !isspace(*ch)) ch--;
	if (ch == help) break;		/* give up */
	while (ch > (help + 1) && isspace(*ch)) ch--;
	ch++;

	sprintf(format, "%%.%ds\n%%%ds", (int) (ch - help), indentLength);
	fprintf(f, format, help, " ");
	help = ch;
	while (isspace(*help) && *help) help++;
	helpLength = strlen(help);
    }

    if (helpLength) fprintf(f, "%s\n", help);
}

static int maxArgWidth(const struct poptOption * opt) {
    int max = 0;
    int this;
    
    while (opt->longName || opt->shortName || opt->arg) {
	if (opt->argInfo == POPT_ARG_INCLUDE_TABLE) {
	    this = maxArgWidth(opt->arg);
	} else {
	    this = opt->shortName ? 2 : 0;
	    if (opt->longName) {
		if (this) this += 2;
		this += strlen(opt->longName) + 2;
	    }

	    if (opt->argDescrip)
		this += strlen(opt->argDescrip) + 1;
	}

	if (this > max) max = this;

	opt++;
    }
    
    return max;
}

static void singleTableHelp(FILE * f, const struct poptOption * table, 
			    int left) {
    const struct poptOption * opt;

    opt = table;
    while (opt->longName || opt->shortName || opt->arg) {
	if (opt->longName || opt->shortName)
	    singleOptionHelp(f, left, opt);
	opt++;
    }

    opt = table;
    while (opt->longName || opt->shortName || opt->arg) {
	if (opt->argInfo == POPT_ARG_INCLUDE_TABLE) {
	    if (opt->descrip)
		fprintf(f, "\n%s\n", opt->descrip);
	    singleTableHelp(f, opt->arg, left);
	}
	opt++;
    }
}

static int showHelpIntro(poptContext con, FILE * f) {
    int len = 6;
    char * fn;

    fprintf(f, "Usage:");
    if (!(con->flags & POPT_CONTEXT_KEEP_FIRST)) {
	fn = con->optionStack->argv[0];
	if (strchr(fn, '/')) fn = strchr(fn, '/') + 1;
	fprintf(f, " %s", fn);
	len += strlen(fn) + 1;
    }

    return len;
}

void poptPrintHelp(poptContext con, FILE * f, int flags) {
    int leftColWidth;

    showHelpIntro(con, f);
    if (con->otherHelp)
	fprintf(f, " %s\n", con->otherHelp);
    else
	fprintf(f, " [OPTION...]\n");

    leftColWidth = maxArgWidth(con->options);
    singleTableHelp(f, con->options, leftColWidth);
}

static int singleOptionUsage(FILE * f, int cursor, 
			      const struct poptOption * opt) {
    int len = 3;
    char shortStr[2];
    const char * item = shortStr;

    if (opt->shortName) {
	if (!opt->argInfo) return cursor;	/* we did these already */
	len++;
	*shortStr = opt->shortName;
	shortStr[1] = '\0';
    } else if (opt->longName) {
	len += 1 + strlen(opt->longName);
	item = opt->longName;
    }

    if (len == 3) return cursor;

    if (opt->argDescrip) 
	len += strlen(opt->argDescrip) + 1;

    if ((cursor + len) > 79) {
	fprintf(f, "\n       ");
	cursor = 7;
    } 

    fprintf(f, " [-%s%s%s%s]", opt->shortName ? "" : "-", item,
	    opt->argDescrip ? "=" : "", 
	    opt->argDescrip ? opt->argDescrip : "");

    return cursor + len + 1;
}

int singleTableUsage(FILE * f, int cursor, const struct poptOption * table) {
    const struct poptOption * opt;
    
    opt = table;
    while (opt->longName || opt->shortName || opt->arg) {
	if (opt->longName || opt->shortName)
	    cursor = singleOptionUsage(f, cursor, opt);
	else if (opt->argInfo == POPT_ARG_INCLUDE_TABLE) 
	    cursor = singleTableUsage(f, cursor, opt->arg);
	opt++;
    }

    return cursor;
}

static int showShortOptions(const struct poptOption * opt, FILE * f, 
			    char * str) {
    char s[300];		/* this is larger then the ascii set, so
				   it should do just fine */

    if (!str) {
	str = s;
	memset(str, 0, sizeof(str));
    }

    while (opt->longName || opt->shortName || opt->arg) {
	if (opt->shortName && !opt->argInfo)
	    str[strlen(str)] = opt->shortName;
	else if (opt->argInfo == POPT_ARG_INCLUDE_TABLE)
	    showShortOptions(opt->arg, f, str);

	opt++;
    } 

    if (s != str || !*s)
	return 0;

    fprintf(f, " [-%s]", s);
    return strlen(s) + 4;
}

void poptPrintUsage(poptContext con, FILE * f, int flags) {
    int cursor;

    cursor = showHelpIntro(con, f);
    cursor += showShortOptions(con->options, f, NULL);
    singleTableUsage(f, cursor, con->options);

    if (con->otherHelp) {
	cursor += strlen(con->otherHelp) + 1;
	if (cursor > 79) fprintf(f, "\n       ");
	fprintf(f, " %s", con->otherHelp);
    }

    fprintf(f, "\n");
}

void poptSetOtherOptionHelp(poptContext con, const char * text) {
    if (con->otherHelp) free(con->otherHelp);
    con->otherHelp = strdup(text);
}
