/* Print size information from ELF file.
   Copyright (C) 2000, 2001, 2002 Red Hat, Inc.
   Written by Ulrich Drepper <drepper@redhat.com>, 2000.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2 as
   published by the Free Software Foundation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <argp.h>
#include <error.h>
#include <fcntl.h>
#include <gelf.h>
#include <inttypes.h>
#include <libelf.h>
#include <libintl.h>
#include <locale.h>
#include <mcheck.h>
#include <stdio.h>
#include <stdio_ext.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/param.h>

#include <system.h>


/* Name and version of program.  */
static void print_version (FILE *stream, struct argp_state *state);
void (*argp_program_version_hook) (FILE *, struct argp_state *) = print_version;


/* Values for the parameters which have no short form.  */
#define OPT_FORMAT	0x100
#define OPT_RADIX	0x101

/* Definitions of arguments for argp functions.  */
static const struct argp_option options[] =
{
  { NULL, 0, NULL, 0, N_("Output format:") },
  { "format", OPT_FORMAT, "FORMAT", 0, N_("Use the output format FORMAT.  FORMAT can be `bsd' or `sysv'.  The default is `bsd'") },
  { NULL, 'A', NULL, 0, N_("Same as `--format=sysv'") },
  { NULL, 'B', NULL, 0, N_("Same as `--format=bsd'") },
  { "radix", OPT_RADIX, "RADIX", 0, N_("Use RADIX for printing symbol values") },
  { NULL, 'd', NULL, 0, N_("Same as `--radix=10'") },
  { NULL, 'o', NULL, 0, N_("Same as `--radix=8'") },
  { NULL, 'x', NULL, 0, N_("Same as `--radix=16'") },
  { NULL, 'f', NULL, 0, N_("Similar to `--format=sysv' output but in one line") },
  { NULL, 'F', NULL, 0, N_("Print size and permission flags for loadable segments") },
  { NULL, 0, NULL, 0, NULL }
};

/* Short description of program.  */
static const char doc[] = N_("\
List section sizes of FILEs (a.out by default).");

/* Strings for arguments in help texts.  */
static const char args_doc[] = N_("[FILE...]");

/* Prototype for option handler.  */
static error_t parse_opt (int key, char *arg, struct argp_state *state);

/* Function to print some extra text in the help message.  */
static char *more_help (int key, const char *text, void *input);

/* Data structure to communicate with argp functions.  */
static struct argp argp =
{
  options, parse_opt, args_doc, doc, NULL, more_help
};


/* Print symbols in file named FNAME.  */
static int process_file (const char *fname);

/* Handle content of archive.  */
static int handle_ar (int fd, Elf *elf, const char *prefix, const char *fname);

/* Handle ELF file.  */
static void handle_elf (Elf *elf, const char *fullname, const char *fname);

#define INTERNAL_ERROR(fname) \
  error (EXIT_FAILURE, 0, gettext ("%s: INTERNAL ERROR: %s"),		      \
	 fname, elf_errmsg (-1))


/* User-selectable options.  */

/* The selected output format.  */
static enum
{
  format_bsd = 0,
  format_sysv,
  format_sysv_one_line,
  format_segments
} format;

/* Radix for printed numbers.  */
static enum
{
  radix_decimal = 0,
  radix_hex,
  radix_octal
} radix;


/* Mapping of radix and binary class to length.  */
static const int length_map[2][3] =
{
  [ELFCLASS32 - 1] =
  {
    [radix_hex] = 8,
    [radix_decimal] = 10,
    [radix_octal] = 11
  },
  [ELFCLASS64 - 1] =
  {
    [radix_hex] = 16,
    [radix_decimal] = 20,
    [radix_octal] = 22
  }
};


int
main (int argc, char *argv[])
{
  int remaining;
  int result = 0;

  /* Make memory leak detection possible.  */
  mtrace ();

  /* We use no threads here which can interfere with handling a stream.  */
  __fsetlocking (stdin, FSETLOCKING_BYCALLER);
  __fsetlocking (stdout, FSETLOCKING_BYCALLER);
  __fsetlocking (stderr, FSETLOCKING_BYCALLER);

  /* Set locale.  */
  setlocale (LC_ALL, "");

  /* Make sure the message catalog can be found.  */
  bindtextdomain (PACKAGE, LOCALEDIR);

  /* Initialize the message catalog.  */
  textdomain (PACKAGE);

  /* Parse and process arguments.  */
  argp_parse (&argp, argc, argv, 0, &remaining, NULL);


  /* Tell the library which version we are expecting.  */
  elf_version (EV_CURRENT);

  if (remaining == argc)
    /* The user didn't specify a name so we use a.out.  */
    result = process_file ("a.out");
  else
    /* Process all the remaining files.  */
    do
      result |= process_file (argv[remaining]);
    while (++remaining < argc);

  return result;
}


/* Print the version information.  */
static void
print_version (FILE *stream, struct argp_state *state)
{
  fprintf (stream, "size (Red Hat %s) %s\n", PACKAGE, VERSION);
  fprintf (stream, gettext ("\
Copyright (C) %s Red Hat, Inc.\n\
This is free software; see the source for copying conditions.  There is NO\n\
warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n\
"), "2002");
  fprintf (stream, gettext ("Written by %s.\n"), "Ulrich Drepper");
}


/* Handle program arguments.  */
static error_t
parse_opt (int key, char *arg, struct argp_state *state)
{
  switch (key)
    {
    case 'd':
      radix = radix_decimal;
      break;

    case 'f':
      format = format_sysv_one_line;
      break;

    case 'o':
      radix = radix_octal;
      break;

    case 'x':
      radix = radix_hex;
      break;

    case 'A':
      format = format_sysv;
      break;

    case 'B':
      format = format_bsd;
      break;

    case 'F':
      format = format_segments;
      break;

    case OPT_FORMAT:
      if (strcmp (arg, "bsd") == 0)
	format = format_bsd;
      else if (strcmp (arg, "sysv") == 0)
	format = format_sysv;
      else
	error (EXIT_FAILURE, 0, gettext ("Invalid format: %s"), arg);
      break;

    case OPT_RADIX:
      if (strcmp (arg, "x") == 0 || strcmp (arg, "16") == 0)
	radix = radix_hex;
      else if (strcmp (arg, "d") == 0 || strcmp (arg, "10") == 0)
	radix = radix_decimal;
      else if (strcmp (arg, "o") == 0 || strcmp (arg, "8") == 0)
	radix = radix_octal;
      else
	error (EXIT_FAILURE, 0, gettext ("Invalid radix: %s"), arg);
      break;

    default:
      return ARGP_ERR_UNKNOWN;
    }
  return 0;
}


static char *
more_help (int key, const char *text, void *input)
{
  switch (key)
    {
    case ARGP_KEY_HELP_EXTRA:
      /* We print some extra information.  */
      return xstrdup (gettext ("\
Report bugs to <drepper@redhat.com>.\n"));
    default:
      break;
    }
  return (char *) text;
}


static int
process_file (const char *fname)
{
  /* Open the file and determine the type.  */
  int fd;
  Elf *elf;

  /* Open the file.  */
  fd = open (fname, O_RDONLY);
  if (fd == -1)
    {
      error (0, errno, fname);
      return 1;
    }

  /* Now get the ELF descriptor.  */
  elf = elf_begin (fd, ELF_C_READ_MMAP, NULL);
  if (elf != NULL)
    {
      if (elf_kind (elf) == ELF_K_ELF)
	{
	  handle_elf (elf, NULL, fname);

	  if (elf_end (elf) != 0)
	    INTERNAL_ERROR (fname);

	  if (close (fd) != 0)
	    error (EXIT_FAILURE, errno, gettext ("while close `%s'"), fname);

	  return 0;
	}
      else
	return handle_ar (fd, elf, NULL, fname);

      /* We cannot handle this type.  Close the descriptor anyway.  */
      if (elf_end (elf) != 0)
	INTERNAL_ERROR (fname);
    }

  error (0, 0, gettext ("%s: file format not recognized"), fname);

  return 1;
}


/* Print the BSD-style header.  This is done exactly once.  */
static void
print_header (Elf *elf)
{
  static int done;

  if (! done)
    {
      int ddigits = length_map[gelf_getclass (elf) - 1][radix_decimal];
      int xdigits = length_map[gelf_getclass (elf) - 1][radix_hex];

      printf ("%*s %*s %*s %*s %*s %s\n",
	      ddigits - 2, sgettext ("bsd|text"),
	      ddigits - 2, sgettext ("bsd|data"),
	      ddigits - 2, sgettext ("bsd|bss"),
	      ddigits - 2, sgettext ("bsd|dec"),
	      xdigits - 2, sgettext ("bsd|hex"),
	      sgettext ("bsd|filename"));

      done = 1;
    }
}


static int
handle_ar (int fd, Elf *elf, const char *prefix, const char *fname)
{
  Elf *subelf;
  Elf_Cmd cmd = ELF_C_READ_MMAP;
  size_t prefix_len = prefix == NULL ? 0 : strlen (prefix);
  size_t fname_len = strlen (fname) + 1;
  char new_prefix[prefix_len + 1 + fname_len];
  int result = 0;
  char *cp = new_prefix;

  /* Create the full name of the file.  */
  if (prefix != NULL)
    {
      cp = mempcpy (cp, prefix, prefix_len);
      *cp++ = ':';
    }
  memcpy (cp, fname, fname_len);

  /* Process all the files contained in the archive.  */
  while ((subelf = elf_begin (fd, cmd, elf)) != NULL)
    {
      /* The the header for this element.  */
      Elf_Arhdr *arhdr = elf_getarhdr (subelf);

      if (elf_kind (subelf) == ELF_K_ELF)
	handle_elf (subelf, new_prefix, arhdr->ar_name);
      else if (elf_kind (subelf) == ELF_K_AR)
	result |= handle_ar (fd, subelf, new_prefix, arhdr->ar_name);
      /* else signal error??? */

      /* Get next archive element.  */
      cmd = elf_next (subelf);
      if (elf_end (subelf) != 0)
	INTERNAL_ERROR (fname);
    }

  if (elf_end (elf) != 0)
    INTERNAL_ERROR (fname);

  if (close (fd) != 0)
    error (EXIT_FAILURE, errno, gettext ("while closing `%s'"), fname);

  return result;
}


/* Show sizes in SysV format.  */
static void
show_sysv (Elf *elf, const char *prefix, const char *fname,
	   const char *fullname)
{
  uint32_t shstrndx;
  Elf_Scn *scn = NULL;
  GElf_Shdr shdr_mem;
  int maxlen = 10;
  int digits = length_map[gelf_getclass (elf) - 1][radix];
  const char *fmtstr;
  GElf_Off total = 0;

  /* Get the section header string table index.  */
  if (elf_getshstrndx (elf, &shstrndx) < 0)
    error (EXIT_FAILURE, 0,
	   gettext ("cannot get section header string table index"));

  /* First round over the sections: determine the longest section name.  */
  while ((scn = elf_nextscn (elf, scn)) != NULL)
    {
      GElf_Shdr *shdr = gelf_getshdr (scn, &shdr_mem);

      if (shdr == NULL)
	INTERNAL_ERROR (fullname);

      /* Ignore all sections which are not used at runtime.  */
      if ((shdr->sh_flags & SHF_ALLOC) == 0)
	continue;

      maxlen = MAX (maxlen,
		    strlen (elf_strptr (elf, shstrndx, shdr->sh_name)));
    }

  fputs_unlocked (fname, stdout);
  if (prefix != NULL)
    printf (gettext (" (ex %s)"), prefix);
  printf (":\n%-*s %*s %*s\n",
	  maxlen, sgettext ("sysv|section"),
	  digits - 2, sgettext ("sysv|size"),
	  digits, sgettext ("sysv|addr"));

  if (radix == radix_hex)
    fmtstr = "%-*s %*" PRIx64 " %*" PRIx64 "\n";
  else if (radix == radix_decimal)
    fmtstr = "%-*s %*" PRId64 " %*" PRId64 "\n";
  else
    fmtstr = "%-*s %*" PRIo64 " %*" PRIo64 "\n";

  /* Iterate over all sections.  */
  while ((scn = elf_nextscn (elf, scn)) != NULL)
    {
      GElf_Shdr *shdr = gelf_getshdr (scn, &shdr_mem);

      /* Ignore all sections which are not used at runtime.  */
      if ((shdr->sh_flags & SHF_ALLOC) == 0)
	continue;

      printf (fmtstr,
	      maxlen, elf_strptr (elf, shstrndx, shdr->sh_name),
	      digits - 2, shdr->sh_size,
	      digits, shdr->sh_addr);

      total += shdr->sh_size;
    }

  if (radix == radix_hex)
    printf ("%s %*" PRIx64 "\n\n\n", sgettext ("sysv|Total"), digits, total);
  else if (radix == radix_decimal)
    printf ("%s %*" PRId64 "\n\n\n", sgettext ("sysv|Total"), digits, total);
  else
    printf ("%s %*" PRIo64 "\n\n\n", sgettext ("sysv|Total"), digits, total);
}


/* Show sizes in SysV format in one line.  */
static void
show_sysv_one_line (Elf *elf, const char *prefix, const char *fname,
		    const char *fullname)
{
  uint32_t shstrndx;
  Elf_Scn *scn = NULL;
  GElf_Shdr shdr_mem;
  const char *fmtstr;
  GElf_Off total = 0;
  int first = 1;

  /* Get the section header string table index.  */
  if (elf_getshstrndx (elf, &shstrndx) < 0)
    error (EXIT_FAILURE, 0,
	   gettext ("cannot get section header string table index"));

  if (radix == radix_hex)
    fmtstr = "%" PRIx64 "(%s)";
  else if (radix == radix_decimal)
    fmtstr = "%" PRId64 "(%s)";
  else
    fmtstr = "%" PRIo64 "(%s)";

  /* Iterate over all sections.  */
  while ((scn = elf_nextscn (elf, scn)) != NULL)
    {
      GElf_Shdr *shdr = gelf_getshdr (scn, &shdr_mem);

      /* Ignore all sections which are not used at runtime.  */
      if ((shdr->sh_flags & SHF_ALLOC) == 0)
	continue;

      if (! first)
	fputs_unlocked (" + ", stdout);
      first = 0;

      printf (fmtstr, shdr->sh_size,
	      elf_strptr (elf, shstrndx, shdr->sh_name));

      total += shdr->sh_size;
    }

  if (radix == radix_hex)
    printf (" = %#" PRIx64 "\n", total);
  else if (radix == radix_decimal)
    printf (" = %" PRId64 "\n", total);
  else
    printf (" = %" PRIo64 "\n", total);
}


/* Show sizes in BSD format.  */
static void
show_bsd (Elf *elf, const char *prefix, const char *fname,
	  const char *fullname)
{
  Elf_Scn *scn = NULL;
  GElf_Shdr shdr_mem;
  GElf_Off textsize = 0;
  GElf_Off datasize = 0;
  GElf_Off bsssize = 0;
  int ddigits = length_map[gelf_getclass (elf) - 1][radix_decimal];
  int xdigits = length_map[gelf_getclass (elf) - 1][radix_hex];

  /* Iterate over all sections.  */
  while ((scn = elf_nextscn (elf, scn)) != NULL)
    {
      GElf_Shdr *shdr = gelf_getshdr (scn, &shdr_mem);

      if (shdr == NULL)
	INTERNAL_ERROR (fullname);

      /* Ignore all sections which are not marked as loaded.  */
      if ((shdr->sh_flags & SHF_ALLOC) == 0)
	continue;

      if ((shdr->sh_flags & SHF_WRITE) == 0)
	textsize += shdr->sh_size;
      else if (shdr->sh_type == SHT_NOBITS)
	bsssize += shdr->sh_size;
      else
	datasize += shdr->sh_size;
    }

  printf ("%*" PRId64 " %*" PRId64 " %*" PRId64 " %*" PRId64 " %*"
	  PRIx64 " %s",
	  ddigits - 2, textsize,
	  ddigits - 2, datasize,
	  ddigits - 2, bsssize,
	  ddigits - 2, textsize + datasize + bsssize,
	  xdigits - 2, textsize + datasize + bsssize,
	  fname);
  if (prefix != NULL)
    printf (gettext (" (ex %s)"), prefix);
  fputs_unlocked ("\n", stdout);
}


/* Show size and permission of loadable segments.  */
static void
show_segments (Elf *elf, const char *prefix, const char *fname,
	       const char *fullname)
{
  GElf_Ehdr ehdr_mem;
  GElf_Ehdr *ehdr;
  size_t cnt;
  GElf_Off total = 0;
  int first = 1;

  ehdr = gelf_getehdr (elf, &ehdr_mem);
  if (ehdr == NULL)
    INTERNAL_ERROR (fullname);

  for (cnt = 0; cnt < ehdr->e_phnum; ++cnt)
    {
      GElf_Phdr phdr_mem;
      GElf_Phdr *phdr;

      phdr = gelf_getphdr (elf, cnt, &phdr_mem);
      if (phdr == NULL)
	INTERNAL_ERROR (fullname);

      if (phdr->p_type != PT_LOAD)
	/* Only load segments.  */
	continue;

      if (! first)
	fputs_unlocked (" + ", stdout);
      first = 0;

      printf (radix == radix_hex ? "%" PRIx64 "(%c%c%c)"
	      : (radix == radix_decimal ? "%" PRId64 "(%c%c%c)"
		 : "%" PRIo64 "(%c%c%c)"),
	      phdr->p_memsz,
	      (phdr->p_flags & PF_R) == 0 ? '-' : 'r',
	      (phdr->p_flags & PF_W) == 0 ? '-' : 'w',
	      (phdr->p_flags & PF_X) == 0 ? '-' : 'x');

      total += phdr->p_memsz;
    }

  if (radix == radix_hex)
    printf (" = %#" PRIx64 "\n", total);
  else if (radix == radix_decimal)
    printf (" = %" PRId64 "\n", total);
  else
    printf (" = %" PRIo64 "\n", total);
}


static void
handle_elf (Elf *elf, const char *prefix, const char *fname)
{
  size_t prefix_len = prefix == NULL ? 0 : strlen (prefix);
  size_t fname_len = strlen (fname) + 1;
  char fullname[prefix_len + 1 + fname_len];
  char *cp = fullname;

  /* Create the full name of the file.  */
  if (prefix != NULL)
    {
      cp = mempcpy (cp, prefix, prefix_len);
      *cp++ = ':';
    }
  memcpy (cp, fname, fname_len);

  if (format == format_sysv)
    show_sysv (elf, prefix, fname, fullname);
  else if (format == format_sysv_one_line)
    show_sysv_one_line (elf, prefix, fname, fullname);
  else if (format == format_segments)
    show_segments (elf, prefix, fname, fullname);
  else
    {
      print_header (elf);

      show_bsd (elf, prefix, fname, fullname);
    }
}
