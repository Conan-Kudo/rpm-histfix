/* rpmheader: spit out the header portion of a package */

#include "system.h"

#include "rpmlead.h"
#include "signature.h"
#include "header.h"
#include "intl.h"

#ifndef	FREE
#define FREE(_x) { if (_x) free(_x); (_x) = NULL; }
#endif

static int escape = 1;	/* use octal escape sequence for !isprint(c)? */

static const char *
getTagString(int tval)
{
    const struct headerTagTableEntry *t;
    static char buf[128];

    for (t = rpmTagTable; t->name != NULL; t++) {
	if (t->val == tval)
	    return t->name;
    }
    sprintf(buf, "<RPMTAG_%d>", tval);
    return buf;
}

const struct headerTypeTableEntry {
    char *name;
    int val;
} rpmTypeTable[] = {
    {"RPM_NULL_TYPE",	0},
    {"RPM_CHAR_TYPE",	1},
    {"RPM_INT8_TYPE",	2},
    {"RPM_INT16_TYPE",	3},
    {"RPM_INT32_TYPE",	4},
    {"RPM_INT64_TYPE",	5},
    {"RPM_STRING_TYPE",	6},
    {"RPM_BIN_TYPE",	7},
    {"RPM_STRING_ARRAY_TYPE",	8},
    {"RPM_I18NSTRING_TYPE",	9},
    {NULL,	0}
};

static char *
getTypeString(int tval)
{
    const struct headerTypeTableEntry *t;
    static char buf[128];

    for (t = rpmTypeTable; t->name != NULL; t++) {
	if (t->val == tval)
	    return t->name;
    }
    sprintf(buf, "<RPM_%d_TYPE>", tval);
    return buf;
}

static char **
headerGetLangs(Header h)
{
    char **s, *e, **table;
    int i, type, count;

    if (!headerGetRawEntry(h, HEADER_I18NTABLE, &type, (void **)&s, &count))
	return NULL;

    if ((table = (char **)calloc((count+1), sizeof(char *))) == NULL)
	return NULL;

    for (i = 0, e = *s; i < count > 0; i++, e += strlen(e)+1) {
	table[i] = e;
    }
    table[count] = NULL;

    return table;
}

/* XXX cribbed from gettext/src/message.c */
static const char escapes[] = "\b\f\n\r\t";
static const char escape_names[] = "bfnrt";

static void
copypo(char *t, char *s)
{
    const char *esc;
    int c;

    *t++ = '"';
    while((c = *s++) != '\0') {
	esc = strchr(escapes, c);
	if (esc == NULL && (!escape || isprint(c))) {
		if (c == '\\' || c == '"')
			*t++ = '\\';
		*t++ = c;
	} else if (esc != NULL) {
		*t++ = '\\';
		*t++ = escape_names[esc - escapes];
		if (c == '\n') {
			strcpy(t, "\"\n\"");
			t += strlen(t);
		}
	} else {
		*t++ = '\\';
		sprintf(t, "%3.3o", (c & 0377));
		t += strlen(t);
	}
    }
    *t++ = '"';
    *t = '\0';
}

static int poTags[] = {
    RPMTAG_DESCRIPTION,
    RPMTAG_GROUP,
    RPMTAG_SUMMARY,
#ifdef NOTYET
    RPMTAG_DISTRIBUTION,
    RPMTAG_VENDOR,
    RPMTAG_LICENSE,
    RPMTAG_PACKAGER,
#endif
    0
};

static int
dofile(int fd, const char *file, FILE *fp)
{
    struct rpmlead lead;
    Header h;
    int *tp;
    char **langs;
    char buf[BUFSIZ];
    
    fprintf(fp, "\n#============================================");
    readLead(fd, &lead);
    rpmReadSignature(fd, NULL, lead.signature_type);
    h = headerRead(fd, (lead.major >= 3) ?
		    HEADER_MAGIC_YES : HEADER_MAGIC_NO);

    if ((langs = headerGetLangs(h)) == NULL)
	return 1;

    for (tp = poTags; *tp != 0; tp++) {
	char **s, *e;
	int i, type, count;

	if (!headerGetRawEntry(h, *tp, &type, (void **)&s, &count))
	    continue;

	fprintf(fp, "\n#: %s:%s\n", file, getTagString(*tp));
	e = *s;
	copypo(buf, e);
	fprintf(fp, "msgid %s\n", buf);
	if (count <= 1)
	    fprintf(fp, "nsgstr \"\"\n");
	for (i = 1, e += strlen(e)+1; i < count && e != NULL; i++, e += strlen(e)+1) {
		copypo(buf, e);
		fprintf(fp, "msgstr(%s) %s\n", langs[i], buf);
	}

	if (type == RPM_STRING_ARRAY_TYPE || type == RPM_I18NSTRING_TYPE)
	    FREE(s)
    }
    
    FREE(langs);

    return 0;
}

int debug = 0;
int verbose = 0;

int
main(int argc, char **argv)
{
    int c;
    extern char *optarg;
    extern int optind;
    int errflg = 0;

    while((c = getopt(argc, argv, "dev")) != EOF)
    switch (c) {
    case 'd':
	debug++;
	break;
    case 'e':
	escape = 0;
	break;
    case 'v':
	verbose++;
	break;
    case '?':
    default:
	errflg++;
	break;
    }
    if (errflg) {
	exit(1);
    }

    if (optind == argc) {
	return dofile(0, "<stdin>", stdout);
    }

    for ( ; optind < argc; optind++ ) {
	char *file;
	int fd;

	file = argv[optind];
	if ((fd = open(file, O_RDONLY, 0644)) < 0) {
	    perror(file);
	    exit(2);
	}
	if (dofile(fd, file, stdout)) {
		exit(3);
	}
	close(fd);
    }
}
