#include "system.h"

#include "rpmlib.h"

int main(int argc, char ** argv)
{
    Header h;
    FD_t fdi;

    setprogname(argv[0]);	/* Retrofit glibc __progname */
    if (argc == 1) {
	fdi = fdDup(STDIN_FILENO);
    } else {
	fdi = fdOpen(argv[1], O_RDONLY, 0644);
    }

    if (Fileno(fdi) < 0) {
	fprintf(stderr, _("cannot open %s: %s\n"), argv[1], strerror(errno));
	exit(1);
    }

    h = headerRead(fdi, HEADER_MAGIC_YES);
    if (!h) {
	fprintf(stderr, _("headerRead error: %s\n"), strerror(errno));
	exit(1);
    }
    Fclose(fdi);
  
    headerDump(h, stdout, HEADER_DUMP_INLINE, rpmTagTable);
    headerFree(h);

    return 0;
}
