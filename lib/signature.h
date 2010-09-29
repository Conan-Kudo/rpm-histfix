#ifndef H_SIGNATURE
#define	H_SIGNATURE

/** \ingroup signature
 * \file lib/signature.h
 * Generate and verify signatures.
 */

#include <rpm/header.h>

/** \ingroup signature
 * Signature types stored in rpm lead.
 */
typedef	enum sigType_e {
    RPMSIGTYPE_HEADERSIG= 5	/*!< Header style signature */
} sigType;

#ifdef __cplusplus
extern "C" {
#endif

/* Dumb wrapper around headerPut() for signature header */
static inline int sighdrPut(Header h, rpmSigTag tag, rpmTagType type,
                     rpm_data_t p, rpm_count_t c)
{
    struct rpmtd_s sigtd;
    rpmtdReset(&sigtd);
    sigtd.tag = tag;
    sigtd.type = type;
    sigtd.data = p;
    sigtd.count = c;
    return headerPut(h, &sigtd, HEADERPUT_DEFAULT);
}

/** \ingroup signature
 * Return new, empty (signature) header instance.
 * @return		signature header
 */
Header rpmNewSignature(void);

/** \ingroup signature
 * Read (and verify header+payload size) signature header.
 * If an old-style signature is found, we emulate a new style one.
 * @param fd		file handle
 * @retval sighp	address of (signature) header (or NULL)
 * @param sig_type	type of signature header to read (from lead)
 * @retval msg		failure msg
 * @return		rpmRC return code
 */
rpmRC rpmReadSignature(FD_t fd, Header *sighp, sigType sig_type, char ** msg);

/** \ingroup signature
 * Write signature header.
 * @param fd		file handle
 * @param h		(signature) header
 * @return		0 on success, 1 on error
 */
int rpmWriteSignature(FD_t fd, Header h);

/** \ingroup signature
 * Generate digest(s) from a header+payload file, save in signature header.
 * @param sigh		signature header
 * @param file		header+payload file name
 * @param sigTag	type of digest(s) to add
 * @return		0 on success, -1 on failure
 */
int rpmGenDigest(Header sigh, const char * file, rpmSigTag sigTag);

/** \ingroup signature
 * Generate signature(s) from a header+payload file, save in signature header.
 * @param sigh		signature header
 * @param file		header+payload file name
 * @param sigTag	type of signature(s) to add
 * @param passPhrase	private key pass phrase
 * @return		0 on success, -1 on failure
 */
int rpmGenSignature(Header sigh, const char * file,
		    rpmSigTag sigTag, const char * passPhrase);

/** \ingroup signature
 * Verify a signature from a package.
 *
 * @param keyring	keyring handle
 * @param sigtd		signature tag data container
 * @param dig		signature/pubkey parameters
 * @retval result	detailed text result of signature verification
 * 			(malloc'd)
 * @return		result of signature verification
 */
rpmRC rpmVerifySignature(rpmKeyring keyring, rpmtd sigtd, pgpDig dig, DIGEST_CTX ctx, char ** result);

/** \ingroup signature
 * Destroy signature header from package.
 * @param h		signature header
 * @return		NULL always
 */
Header rpmFreeSignature(Header h);

#ifdef __cplusplus
}
#endif

#endif	/* H_SIGNATURE */
