#include "header.h"
#include "rpmlib.h"

#include <string.h>

void main(int argc, char ** argv)
{
  Header h;
  FILE *f;
  char *sa[] = { "one", "two", "three" };
  int_32 i32 = 400;
  int_32 i32a[] = { 100, 200, 300 };
  int_16 i16 = 1;
  int_16 i16a[] = { 100, 200, 300 };
  char ca[] = "char array";

  h = newHeader();

  addEntry(h, RPMTAG_NAME, STRING_TYPE, "MarcEwing", 1);
  addEntry(h, RPMTAG_VERSION, STRING_TYPE, "1.1", 1);
  addEntry(h, RPMTAG_VERSION, STRING_TYPE, sa, 3);
  addEntry(h, RPMTAG_SIZE, INT32_TYPE, &i32, 1);
  addEntry(h, RPMTAG_SIZE, INT16_TYPE, &i16, 1);
  addEntry(h, RPMTAG_SIZE, INT16_TYPE, i16a, 3);
  addEntry(h, RPMTAG_VENDOR, CHAR_TYPE, ca, strlen(ca));
  addEntry(h, RPMTAG_SIZE, INT32_TYPE, i32a, 3);

  f = fopen("test.out", "w");
  writeHeader(f, h);
  fclose(f);
  
  dumpHeader(h, stdout, 1);
}

  
