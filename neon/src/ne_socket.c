/* 
   socket handling routines
   Copyright (C) 1998-2004, Joe Orton <joe@manyfish.co.uk>, 
   Copyright (C) 1999-2000 Tommi Komulainen <Tommi.Komulainen@iki.fi>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
   
   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with this library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA
*/

/*
  portions were originally under GPL in Mutt, http://www.mutt.org/
  Relicensed under LGPL for neon, http://www.webdav.org/neon/
*/

#include "config.h"

#ifdef __hpux
/* pick up hstrerror */
#define _XOPEN_SOURCE_EXTENDED 1
/* don't use the broken getaddrinfo shipped in HP-UX 11.11 */
#ifdef USE_GETADDRINFO
#undef USE_GETADDRINFO
#endif
#endif

#include <sys/types.h>
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#include <sys/stat.h>
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif

#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_NETINET_TCP_H
#include <netinet/tcp.h>
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif

#ifdef WIN32
#include <winsock2.h>
#include <stddef.h>
#endif

#if defined(NEON_SSL) && defined(HAVE_LIMITS_H)
#include <limits.h> /* for INT_MAX */
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif 
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_SIGNAL_H
#include <signal.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef HAVE_SOCKS_H
#include <socks.h>
#endif

#ifdef NEON_SSL
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/pkcs12.h> /* for PKCS12_PBE_add */
#include <openssl/rand.h>

#include "ne_privssl.h"
#endif

#include "ne_i18n.h"
#include "ne_utils.h"
#include "ne_string.h"

#define NE_INET_ADDR_DEFINED
/* A slightly ugly hack: change the ne_inet_addr definition to be the
 * real address type used.  The API only exposes ne_inet_addr as a
 * pointer to an opaque object, so this should be well-defined
 * behaviour.  It avoids the hassle of a real wrapper ne_inet_addr
 * structure, or losing type-safety by using void *. */
#ifdef USE_GETADDRINFO
typedef struct addrinfo ne_inet_addr;

/* To avoid doing AAAA queries unless absolutely necessary, either use
 * AI_ADDRCONFIG where available, or a run-time check for working IPv6
 * support; the latter is only known to work on Linux. */
#if !defined(USE_GAI_ADDRCONFIG) && defined(__linux__)
#define USE_CHECK_IPV6
#endif

#else
typedef struct in_addr ne_inet_addr;
#endif

#include "ne_socket.h"
#include "ne_alloc.h"

#if defined(__BEOS__) && !defined(BONE_VERSION)
/* pre-BONE */
#define ne_write(a,b,c) send(a,b,c,0)
#define ne_read(a,b,c) recv(a,b,c,0)
#define ne_close(s) closesocket(s)
#define ne_errno errno
#elif defined(WIN32)
#define ne_write(a,b,c) send(a,b,c,0)
#define ne_read(a,b,c) recv(a,b,c,0)
#define ne_close(s) closesocket(s)
#define ne_errno WSAGetLastError()
#else /* really Unix! */
#define ne_write(a,b,c) write(a,b,c)
#define ne_read(a,b,c) read(a,b,c)
#define ne_close(s) close(s)
#define ne_errno errno
#endif

#ifdef WIN32
#define NE_ISRESET(e) ((e) == WSAECONNABORTED || (e) == WSAETIMEDOUT || \
                       (e) == WSAECONNRESET || (e) == WSAENETRESET)
#define NE_ISCLOSED(e) ((e) == WSAESHUTDOWN || (e) == WSAENOTCONN)
#define NE_ISINTR(e) (0)
#else /* Unix */
#define NE_ISRESET(e) ((e) == ECONNRESET)
#define NE_ISCLOSED(e) ((e) == EPIPE)
#define NE_ISINTR(e) ((e) == EINTR)
#endif

/* Socket read timeout */
#define SOCKET_READ_TIMEOUT 120

/* Critical I/O functions on a socket: useful abstraction for easily
 * handling SSL I/O alongside raw socket I/O. */
struct iofns {
    /* Read up to 'len' bytes into 'buf' from socket.  Return <0 on
     * error or EOF, or >0; number of bytes read. */
    ssize_t (*read)(ne_socket *s, char *buf, size_t len);
    /* Write exactly 'len' bytes from 'buf' to socket.  Return zero on
     * success, <0 on error. */
    ssize_t (*write)(ne_socket *s, const char *buf, size_t len);
    /* Wait up to 'n' seconds for socket to become readable.  Returns
     * 0 when readable, otherwise NE_SOCK_TIMEOUT or NE_SOCK_ERROR. */
    int (*readable)(ne_socket *s, int n);
};

struct ne_socket_s {
    int fd;
    char error[200];
    void *progress_ud;
    int rdtimeout; /* read timeout. */
    const struct iofns *ops;
#ifdef NEON_SSL
    ne_ssl_socket ssl;
#endif
    /* The read buffer: ->buffer stores byte which have been read; as
     * these are consumed and passed back to the caller, bufpos
     * advances through ->buffer.  ->bufavail gives the number of
     * bytes which remain to be consumed in ->buffer (from ->bufpos),
     * and is hence always <= RDBUFSIZ. */
#define RDBUFSIZ 4096
    char buffer[RDBUFSIZ];
    char *bufpos;
    size_t bufavail;
};

/* ne_sock_addr represents an Internet address. */
struct ne_sock_addr_s {
#ifdef USE_GETADDRINFO
    struct addrinfo *result, *cursor;
#else
    struct in_addr *addrs;
    size_t cursor, count;
#endif
    int errnum;
};

/* set_error: set socket error string to 'str'. */
#define set_error(s, str) ne_strnzcpy((s)->error, (str), sizeof (s)->error)

/* set_strerror: set socket error to system error string for 'errnum' */
#ifdef WIN32
/* Print system error message to given buffer. */
static void print_error(int errnum, char *buffer, size_t buflen)
{
    if (FormatMessage (FORMAT_MESSAGE_FROM_SYSTEM
                       | FORMAT_MESSAGE_IGNORE_INSERTS,
                       NULL, (DWORD) errnum, 0, 
                       buffer, buflen, NULL) == 0)
        ne_snprintf(buffer, buflen, "Socket error %d", errnum);
}
#define set_strerror(s, e) print_error((e), (s)->error, sizeof (s)->error)
#else /* not WIN32 */
#define set_strerror(s, e) ne_strerror((e), (s)->error, sizeof (s)->error)
#endif

#ifdef NEON_SSL

/* Initialize SSL library. */
static void init_ssl(void)
{
    SSL_load_error_strings();
    SSL_library_init();
    PKCS12_PBE_add();  /* ### not sure why this is needed. */
}

/* Seed the SSL PRNG, if necessary; returns non-zero on failure. */
static int seed_ssl_prng(void)
{
    /* Check whether the PRNG has already been seeded. */
    if (RAND_status() == 1)
	return 0;

#ifdef EGD_PATH
    NE_DEBUG(NE_DBG_SOCKET, "Seeding PRNG from " EGD_PATH "...\n");
    if (RAND_egd(EGD_PATH) != -1)
	return 0;
#elif defined(ENABLE_EGD)
    {
	static const char *paths[] = { "/var/run/egd-pool", "/dev/egd-pool",
				       "/etc/egd-pool", "/etc/entropy" };
	size_t n;
	for (n = 0; n < sizeof(paths) / sizeof(char *); n++) {
	    NE_DEBUG(NE_DBG_SOCKET, "Seeding PRNG from %s...\n", paths[n]);
	    if (RAND_egd(paths[n]) != -1)
		return 0;
	}
    }
#endif /* EGD_PATH */

    NE_DEBUG(NE_DBG_SOCKET, "No entropy source found; could not seed PRNG.\n");
    return -1;
}
#endif /* NEON_SSL */

#ifdef USE_CHECK_IPV6
static int ipv6_disabled = 0;

/* On Linux kernels, IPv6 is typically built as a loadable module, and
 * socket(AF_INET6, ...) will fail if this module is not loaded, so
 * the slow AAAA lookups can be avoided for this common case. */
static void init_ipv6(void)
{
    int fd = socket(AF_INET6, SOCK_STREAM, 0);
    
    if (fd < 0)
        ipv6_disabled = 1;
    else
        close(fd);
}
#else
#define ipv6_disabled (0)
#endif

static int init_result = 0;

int ne_sock_init(void)
{
#ifdef WIN32
    WORD wVersionRequested;
    WSADATA wsaData;
    int err;
#endif

    if (init_result > 0) 
	return 0;
    else if (init_result < 0)
	return -1;

#ifdef WIN32    
    wVersionRequested = MAKEWORD(2, 2);
    
    err = WSAStartup(wVersionRequested, &wsaData);
    if (err != 0) {
	init_result = -1;
	return -1;
    }

#endif

#ifdef NEON_SOCKS
    SOCKSinit("neon");
#endif

#if defined(HAVE_SIGNAL) && defined(SIGPIPE)
    (void) signal(SIGPIPE, SIG_IGN);
#endif

#ifdef USE_CHECK_IPV6
    init_ipv6();
#endif

#ifdef NEON_SSL
    init_ssl();
#endif

    init_result = 1;
    return 0;
}

void ne_sock_exit(void)
{
#ifdef WIN32
    WSACleanup();
#endif
    init_result = 0;
}

int ne_sock_block(ne_socket *sock, int n)
{
    if (sock->bufavail)
	return 0;
    return sock->ops->readable(sock, n);
}

/* Cast address object AD to type 'sockaddr_TY' */ 
#define SACAST(ty, ad) ((struct sockaddr_##ty *)(ad))

#define SOCK_ERR(x) do { ssize_t _sock_err = (x); \
if (_sock_err < 0) return _sock_err; } while(0)

ssize_t ne_sock_read(ne_socket *sock, char *buffer, size_t buflen)
{
    ssize_t bytes;

#if 0
    NE_DEBUG(NE_DBG_SOCKET, "buf: at %d, %d avail [%s]\n", 
	     sock->bufpos - sock->buffer, sock->bufavail, sock->bufpos);
#endif

    if (sock->bufavail > 0) {
	/* Deliver buffered data. */
	if (buflen > sock->bufavail)
	    buflen = sock->bufavail;
	memcpy(buffer, sock->bufpos, buflen);
	sock->bufpos += buflen;
	sock->bufavail -= buflen;
	return buflen;
    } else if (buflen >= sizeof sock->buffer) {
	/* No need for read buffer. */
	return sock->ops->read(sock, buffer, buflen);
    } else {
	/* Fill read buffer. */
	bytes = sock->ops->read(sock, sock->buffer, sizeof sock->buffer);
	if (bytes <= 0)
	    return bytes;

	if (buflen > (size_t)bytes)
	    buflen = bytes;
	memcpy(buffer, sock->buffer, buflen);
	sock->bufpos = sock->buffer + buflen;
	sock->bufavail = bytes - buflen;
	return buflen; 
    }
}

ssize_t ne_sock_peek(ne_socket *sock, char *buffer, size_t buflen)
{
    ssize_t bytes;
    
    if (sock->bufavail) {
	/* just return buffered data. */
	bytes = sock->bufavail;
    } else {
	/* fill the buffer. */
	bytes = sock->ops->read(sock, sock->buffer, sizeof sock->buffer);
	if (bytes <= 0)
	    return bytes;

	sock->bufpos = sock->buffer;
	sock->bufavail = bytes;
    }

    if (buflen > (size_t)bytes)
	buflen = bytes;

    memcpy(buffer, sock->bufpos, buflen);

    return buflen;
}

/* Await data on raw fd in socket. */
static int readable_raw(ne_socket *sock, int secs)
{
    int fdno = sock->fd, ret;
    fd_set rdfds;
    struct timeval timeout, *tvp = (secs >= 0 ? &timeout : NULL);

    /* Init the fd set */
    FD_ZERO(&rdfds);
    do {
	FD_SET(fdno, &rdfds);
	if (tvp) {
	    tvp->tv_sec = secs;
	    tvp->tv_usec = 0;
	}
	ret = select(fdno + 1, &rdfds, NULL, NULL, tvp);
    } while (ret < 0 && NE_ISINTR(ne_errno));
    if (ret < 0) {
	set_strerror(sock, ne_errno);
	return NE_SOCK_ERROR;
    }
    return (ret == 0) ? NE_SOCK_TIMEOUT : 0;
}

static ssize_t read_raw(ne_socket *sock, char *buffer, size_t len)
{
    ssize_t ret;
    
    ret = readable_raw(sock, sock->rdtimeout);
    if (ret) return ret;

    do {
	ret = ne_read(sock->fd, buffer, len);
    } while (ret == -1 && NE_ISINTR(ne_errno));

    if (ret == 0) {
	set_error(sock, _("Connection closed"));
	ret = NE_SOCK_CLOSED;
    } else if (ret < 0) {
	int errnum = ne_errno;
	ret = NE_ISRESET(errnum) ? NE_SOCK_RESET : NE_SOCK_ERROR;
	set_strerror(sock, errnum);
    }

    return ret;
}

#define MAP_ERR(e) (NE_ISCLOSED(e) ? NE_SOCK_CLOSED : \
                    (NE_ISRESET(e) ? NE_SOCK_RESET : NE_SOCK_ERROR))

static ssize_t write_raw(ne_socket *sock, const char *data, size_t length) 
{
    ssize_t wrote;
    
    do {
	wrote = ne_write(sock->fd, data, length);
        if (wrote > 0) {
            data += wrote;
            length -= wrote;
        }
    } while ((wrote > 0 || NE_ISINTR(ne_errno)) && length > 0);

    if (wrote < 0) {
	int errnum = ne_errno;
	set_strerror(sock, errnum);
	return MAP_ERR(errnum);
    }

    return 0;
}

static const struct iofns iofns_raw = { read_raw, write_raw, readable_raw };

#ifdef NEON_SSL
/* OpenSSL I/O function implementations. */
static int readable_ossl(ne_socket *sock, int secs)
{
    /* If there is buffered SSL data, then don't block on the socket.
     * FIXME: make sure that SSL_read *really* won't block if
     * SSL_pending returns non-zero.  Possibly need to do
     * SSL_read(ssl, buf, SSL_pending(ssl)) */

    if (SSL_pending(sock->ssl.ssl))
	return 0;

    return readable_raw(sock, secs);
}

/* SSL error handling, according to SSL_get_error(3). */
static int error_ossl(ne_socket *sock, int sret)
{
    int err = SSL_get_error(sock->ssl.ssl, sret), ret = NE_SOCK_ERROR;
    
    switch (err) {
    case SSL_ERROR_ZERO_RETURN:
	ret = NE_SOCK_CLOSED;
	set_error(sock, _("Connection closed"));
	break;
    case SSL_ERROR_SYSCALL:
	err = ERR_get_error();
	if (err == 0) {
	    if (sret == 0) {
		/* EOF without close_notify, possible truncation */
		set_error(sock, _("Secure connection truncated"));
		ret = NE_SOCK_TRUNC;
	    } else {
		/* Other socket error. */
		err = ne_errno;
		set_strerror(sock, err);
		ret = MAP_ERR(err);
	    }
	} else {
	    ne_snprintf(sock->error, sizeof sock->error, 
			_("SSL error: %s"), ERR_reason_error_string(err));
	}
	break;
    default:
	ne_snprintf(sock->error, sizeof sock->error, _("SSL error: %s"), 
		    ERR_reason_error_string(ERR_get_error()));
	break;
    }
    return ret;
}

/* Work around OpenSSL's use of 'int' rather than 'size_t', to prevent
 * accidentally passing a negative number, etc. */
#define CAST2INT(n) (((n) > INT_MAX) ? INT_MAX : (n))

static ssize_t read_ossl(ne_socket *sock, char *buffer, size_t len)
{
    int ret;

    ret = readable_ossl(sock, sock->rdtimeout);
    if (ret) return ret;
    
    ret = SSL_read(sock->ssl.ssl, buffer, CAST2INT(len));
    if (ret <= 0)
	ret = error_ossl(sock, ret);

    return ret;
}

static ssize_t write_ossl(ne_socket *sock, const char *data, size_t len)
{
    int ret, ilen = CAST2INT(len);
    ret = SSL_write(sock->ssl.ssl, data, ilen);
    /* ssl.h says SSL_MODE_ENABLE_PARTIAL_WRITE must be enabled to
     * have SSL_write return < length...  so, SSL_write should never
     * return < length. */
    if (ret != ilen)
	return error_ossl(sock, ret);
    return 0;
}

static const struct iofns iofns_ossl = {
    read_ossl,
    write_ossl,
    readable_ossl
};

#endif /* NEON_SSL */

int ne_sock_fullwrite(ne_socket *sock, const char *data, size_t len)
{
    return sock->ops->write(sock, data, len);
}

ssize_t ne_sock_readline(ne_socket *sock, char *buf, size_t buflen)
{
    char *lf;
    size_t len;
    
    if ((lf = memchr(sock->bufpos, '\n', sock->bufavail)) == NULL
	&& sock->bufavail < RDBUFSIZ) {
	/* The buffered data does not contain a complete line: move it
	 * to the beginning of the buffer. */
	if (sock->bufavail)
	    memmove(sock->buffer, sock->bufpos, sock->bufavail);
	sock->bufpos = sock->buffer;
	
	/* Loop filling the buffer whilst no newline is found in the data
	 * buffered so far, and there is still buffer space available */ 
	do {
	    /* Read more data onto end of buffer. */
	    ssize_t ret = sock->ops->read(sock, sock->buffer + sock->bufavail,
					  RDBUFSIZ - sock->bufavail);
	    if (ret < 0) return ret;
	    sock->bufavail += ret;
	} while ((lf = memchr(sock->buffer, '\n', sock->bufavail)) == NULL
		 && sock->bufavail < RDBUFSIZ);
    }

    if (lf)
	len = lf - sock->bufpos + 1;
    else
	len = buflen; /* fall into "line too long" error... */

    if ((len + 1) > buflen) {
	set_error(sock, _("Line too long"));
	return NE_SOCK_ERROR;
    }

    memcpy(buf, sock->bufpos, len);
    buf[len] = '\0';
    /* consume the line from buffer: */
    sock->bufavail -= len;
    sock->bufpos += len;
    return len;
}

ssize_t ne_sock_fullread(ne_socket *sock, char *buffer, size_t buflen) 
{
    ssize_t len;

    while (buflen > 0) {
	len = ne_sock_read(sock, buffer, buflen);
	if (len < 0) return len;
	buflen -= len;
	buffer += len;
    }

    return 0;
}

#ifndef INADDR_NONE
#define INADDR_NONE ((unsigned long) -1)
#endif

#if !defined(USE_GETADDRINFO) && !defined(HAVE_DECL_H_ERRNO) && !defined(WIN32)
/* Ancient versions of netdb.h don't export h_errno. */
extern int h_errno;
#endif

/* This implemementation does not attempt to support IPv6 using
 * gethostbyname2 et al.  */
ne_sock_addr *ne_addr_resolve(const char *hostname, int flags)
{
    ne_sock_addr *addr = ne_calloc(sizeof *addr);
#ifdef USE_GETADDRINFO
    struct addrinfo hints = {0};
    char *pnt;
    hints.ai_socktype = SOCK_STREAM;
    if (hostname[0] == '[' && ((pnt = strchr(hostname, ']')) != NULL)) {
	char *hn = ne_strdup(hostname + 1);
	hn[pnt - hostname - 1] = '\0';
#ifdef AI_NUMERICHOST /* added in the RFC2553 API */
	hints.ai_flags = AI_NUMERICHOST;
#endif
        hints.ai_family = AF_INET6;
	addr->errnum = getaddrinfo(hn, NULL, &hints, &addr->result);
	ne_free(hn);
    } else {
#ifdef USE_GAI_ADDRCONFIG /* added in the RFC3493 API */
        hints.ai_flags = AI_ADDRCONFIG;
        hints.ai_family = AF_UNSPEC;
        addr->errnum = getaddrinfo(hostname, NULL, &hints, &addr->result);
#else
        hints.ai_family = ipv6_disabled ? AF_INET : AF_UNSPEC;
	addr->errnum = getaddrinfo(hostname, NULL, &hints, &addr->result);
#endif
    }
#else /* Use gethostbyname() */
    unsigned long laddr;
    struct hostent *hp;
    
    laddr = inet_addr(hostname);
    if (laddr == INADDR_NONE) {
	hp = gethostbyname(hostname);
	if (hp == NULL) {
#ifdef WIN32
	    addr->errnum = WSAGetLastError();
#else
            addr->errnum = h_errno;
#endif
	} else if (hp->h_length != sizeof(struct in_addr)) {
	    /* fail gracefully if somebody set RES_USE_INET6 */
	    addr->errnum = NO_RECOVERY;
	} else {
	    size_t n;
	    /* count addresses */
	    for (n = 0; hp->h_addr_list[n] != NULL; n++)
		/* noop */;

	    addr->count = n;
	    addr->addrs = ne_malloc(n * sizeof *addr->addrs);

	    for (n = 0; n < addr->count; n++)
		memcpy(&addr->addrs[n], hp->h_addr_list[n], hp->h_length);
	}
    } else {
	addr->addrs = ne_malloc(sizeof *addr->addrs);
	addr->count = 1;
	memcpy(addr->addrs, &laddr, sizeof *addr->addrs);
    }
#endif
    return addr;
}

int ne_addr_result(const ne_sock_addr *addr)
{
    return addr->errnum;
}

const ne_inet_addr *ne_addr_first(ne_sock_addr *addr)
{
#ifdef USE_GETADDRINFO
    addr->cursor = addr->result->ai_next;
    return addr->result;
#else
    addr->cursor = 0;
    return &addr->addrs[0];
#endif
}

const ne_inet_addr *ne_addr_next(ne_sock_addr *addr)
{
#ifdef USE_GETADDRINFO
    struct addrinfo *ret = addr->cursor;
    if (addr->cursor) addr->cursor = addr->cursor->ai_next;
#else
    struct in_addr *ret;
    if (++addr->cursor < addr->count)
	ret = &addr->addrs[addr->cursor];
    else
	ret = NULL;
#endif
    return ret;
}

char *ne_addr_error(const ne_sock_addr *addr, char *buf, size_t bufsiz)
{
#ifdef WIN32
    print_error(addr->errnum, buf, bufsiz);
#else
    const char *err;
#ifdef USE_GETADDRINFO
    /* override horrible generic "Name or service not known" error. */
    if (addr->errnum == EAI_NONAME)
	err = _("Host not found");
    else
	err = gai_strerror(addr->errnum);
#elif defined(HAVE_HSTRERROR)
    err = hstrerror(addr->errnum);
#else
    err = _("Host not found");
#endif
    ne_strnzcpy(buf, err, bufsiz);
#endif /* WIN32 */
    return buf;
}

char *ne_iaddr_print(const ne_inet_addr *ia, char *buf, size_t bufsiz)
{
#ifdef USE_GETADDRINFO /* implies inet_ntop */
    const char *ret;
#ifdef AF_INET6
    if (ia->ai_family == AF_INET6) {
	struct sockaddr_in6 *in6 = SACAST(in6, ia->ai_addr);
	ret = inet_ntop(AF_INET6, &in6->sin6_addr, buf, bufsiz);
    } else
#endif
    if (ia->ai_family == AF_INET) {
	struct sockaddr_in *in = SACAST(in, ia->ai_addr);
	ret = inet_ntop(AF_INET, &in->sin_addr, buf, bufsiz);
    } else
	ret = NULL;
    if (ret == NULL)
	ne_strnzcpy(buf, "[IP address]", bufsiz);
#else
    ne_strnzcpy(buf, inet_ntoa(*ia), bufsiz);
#endif
    return buf;
}

void ne_addr_destroy(ne_sock_addr *addr)
{
#ifdef USE_GETADDRINFO
    if (addr->result)
	freeaddrinfo(addr->result);
#else
    if (addr->addrs)
	ne_free(addr->addrs);
#endif
    ne_free(addr);
}

/* Connect socket 'fd' to address 'addr' on given 'port': */
static int raw_connect(int fd, const ne_inet_addr *addr, unsigned int port)
{
#ifdef USE_GETADDRINFO
#ifdef AF_INET6
    /* fill in the _family field for AIX 4.3, which forgets to do so. */
    if (addr->ai_family == AF_INET6) {
	struct sockaddr_in6 in6;
	memcpy(&in6, addr->ai_addr, sizeof in6);
	in6.sin6_port = port;
        in6.sin6_family = AF_INET6;
	return connect(fd, (struct sockaddr *)&in6, sizeof in6);
    } else
#endif
    if (addr->ai_family == AF_INET) {
	struct sockaddr_in in;
	memcpy(&in, addr->ai_addr, sizeof in);
	in.sin_port = port;
        in.sin_family = AF_INET;
	return connect(fd, (struct sockaddr *)&in, sizeof in);
    } else {
	errno = EINVAL;
	return -1;
    }
#else
    struct sockaddr_in sa = {0};
    sa.sin_family = AF_INET;
    sa.sin_port = port;
    sa.sin_addr = *addr;
    return connect(fd, (struct sockaddr *)&sa, sizeof sa);
#endif
}

ne_socket *ne_sock_create(void)
{
    ne_socket *sock = ne_calloc(sizeof *sock);
    sock->rdtimeout = SOCKET_READ_TIMEOUT;
    sock->bufpos = sock->buffer;
    sock->ops = &iofns_raw;
    sock->fd = -1;
    return sock;
}

int ne_sock_connect(ne_socket *sock,
                    const ne_inet_addr *addr, unsigned int port)
{
    int fd;

#ifdef USE_GETADDRINFO
    /* use SOCK_STREAM rather than ai_socktype: some getaddrinfo
     * implementations do not set ai_socktype, e.g. RHL6.2. */
    fd = socket(addr->ai_family, SOCK_STREAM, addr->ai_protocol);
#else
    fd = socket(AF_INET, SOCK_STREAM, 0);
#endif
    if (fd < 0) {
        set_strerror(sock, ne_errno);
	return -1;
    }
    
#if defined(TCP_NODELAY) && defined(HAVE_SETSOCKOPT) && defined(IPPROTO_TCP)
    { /* Disable the Nagle algorithm; better to add write buffering
       * instead of doing this. */
        int flag = 1;
        setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof flag);
    }
#endif

    if (raw_connect(fd, addr, ntohs(port))) {
        set_strerror(sock, ne_errno);
	ne_close(fd);
	return -1;
    }

    sock->fd = fd;
    return 0;
}

ne_inet_addr *ne_iaddr_make(ne_iaddr_type type, const unsigned char *raw)
{
    ne_inet_addr *ia;
#if !defined(AF_INET6) || !defined(USE_GETADDRINFO)
    /* fail if IPv6 address is given if IPv6 is not supported. */
    if (type == ne_iaddr_ipv6)
	return NULL;
#endif
    ia = ne_calloc(sizeof *ia);
#ifdef USE_GETADDRINFO
    /* ai_protocol and ai_socktype aren't used by raw_connect so
     * ignore them here. (for now) */
    if (type == ne_iaddr_ipv4) {
	struct sockaddr_in *in4 = ne_calloc(sizeof *in4);
	ia->ai_family = AF_INET;
	ia->ai_addr = (struct sockaddr *)in4;
	ia->ai_addrlen = sizeof *in4;
	in4->sin_family = AF_INET;
	memcpy(&in4->sin_addr.s_addr, raw, sizeof in4->sin_addr.s_addr);
    }
#ifdef AF_INET6
    else {
	struct sockaddr_in6 *in6 = ne_calloc(sizeof *in6);
	ia->ai_family = AF_INET6;
	ia->ai_addr = (struct sockaddr *)in6;
	ia->ai_addrlen = sizeof *in6;
	in6->sin6_family = AF_INET6;
	memcpy(&in6->sin6_addr, raw, sizeof in6->sin6_addr.s6_addr);
    }
#endif
#else /* !USE_GETADDRINFO */
    memcpy(&ia->s_addr, raw, sizeof ia->s_addr);
#endif    
    return ia;
}

int ne_iaddr_cmp(const ne_inet_addr *i1, const ne_inet_addr *i2)
{
#ifdef USE_GETADDRINFO
    if (i1->ai_family != i2->ai_family)
	return i2->ai_family - i1->ai_family;
    if (i1->ai_family == AF_INET) {
	struct sockaddr_in *in1 = SACAST(in, i1->ai_addr), 
	    *in2 = SACAST(in, i2->ai_addr);
	return memcmp(&in1->sin_addr.s_addr, &in2->sin_addr.s_addr, 
		      sizeof in1->sin_addr.s_addr);
    } else if (i1->ai_family == AF_INET6) {
	struct sockaddr_in6 *in1 = SACAST(in6, i1->ai_addr), 
	    *in2 = SACAST(in6, i2->ai_addr);
	return memcmp(in1->sin6_addr.s6_addr, in2->sin6_addr.s6_addr,
		      sizeof in1->sin6_addr.s6_addr);
    } else
	return -1;
#else
    return memcmp(&i1->s_addr, &i2->s_addr, sizeof i1->s_addr);
#endif
}

void ne_iaddr_free(ne_inet_addr *addr)
{
#ifdef USE_GETADDRINFO
    ne_free(addr->ai_addr);
#endif
    ne_free(addr);
}

int ne_sock_accept(ne_socket *sock, int listener) 
{
    int fd = accept(listener, NULL, NULL);

    if (fd < 0)
        return -1;

    sock->fd = fd;
    return 0;
}

int ne_sock_fd(const ne_socket *sock)
{
    return sock->fd;
}

void ne_sock_read_timeout(ne_socket *sock, int timeout)
{
    sock->rdtimeout = timeout;
}

#ifdef NEON_SSL

void ne_sock_switch_ssl(ne_socket *sock, void *ssl)
{
    sock->ssl.ssl = ssl;
    sock->ops = &iofns_ossl;
}

int ne_sock_connect_ssl(ne_socket *sock, ne_ssl_context *ctx)
{
    SSL *ssl;
    int ret;

    if (seed_ssl_prng()) {
	set_error(sock, _("SSL disabled due to lack of entropy"));
	return NE_SOCK_ERROR;
    }

    sock->ssl.ssl = ssl = SSL_new(ctx->ctx);
    if (!ssl) {
	set_error(sock, _("Could not create SSL structure"));
	return NE_SOCK_ERROR;
    }
    
    SSL_set_app_data(ssl, ctx);
    SSL_set_mode(ssl, SSL_MODE_AUTO_RETRY);
    SSL_set_fd(ssl, sock->fd);
    sock->ops = &iofns_ossl;
    
    if (ctx->sess)
	SSL_set_session(ssl, ctx->sess);

    ret = SSL_connect(ssl);
    if (ret != 1) {
	error_ossl(sock, ret);
	SSL_free(ssl);
	sock->ssl.ssl = NULL;
	return NE_SOCK_ERROR;
    }

    return 0;
}

ne_ssl_socket *ne_sock_sslsock(ne_socket *sock)
{
    return &sock->ssl;
}

#endif

const char *ne_sock_error(const ne_socket *sock)
{
    return sock->error;
}

/* Closes given ne_socket */
int ne_sock_close(ne_socket *sock)
{
    int ret;
#ifdef NEON_SSL
    if (sock->ssl.ssl) {
	SSL_shutdown(sock->ssl.ssl);
	SSL_free(sock->ssl.ssl);
    }
#endif
    if (sock->fd < 0)
        ret = 0;
    else
        ret = ne_close(sock->fd);
    ne_free(sock);
    return ret;
}

/* Returns HOST byte order port of given name */
int ne_service_lookup(const char *name)
{
    struct servent *ent;
    ent = getservbyname(name, "tcp");
    if (ent)
	return ntohs(ent->s_port);
    return 0;
}
