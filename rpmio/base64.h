/* base64 encoder/decoder based on public domain implementation
 * by Chris Venter */

#include <sys/types.h>

/* returns malloced base64 encoded string
 * lines are split with \n characters to be nearest lower multiple of linelen
 * if linelen/4 == 0 lines are not split
 * if linelen < 0 default line length (64) is used
 * the returned string is empty when len == 0
 * returns NULL on failures
 */
char *b64encode(const void *data, size_t len, int linelen);

/* decodes from zero terminated base64 encoded string to a newly malloced buffer
 * ignores whitespace characters in the input string
 * return values:
 *  0 - OK
 *  1 - input is NULL
 *  2 - invalid length
 *  3 - invalid characters on input
 *  4 - malloc failed
 */
int b64decode(const char *in, void **out, size_t *outlen);

/* counts CRC24 and base64 encodes it in a malloced string
 * returns NULL on failures
 */
char *b64crc(const unsigned char *data, size_t len);
