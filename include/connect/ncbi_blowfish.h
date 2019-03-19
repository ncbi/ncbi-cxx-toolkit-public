#ifndef CONNECT___NCBI_BLOWFISH__H
#define CONNECT___NCBI_BLOWFISH__H

/* $Id$
 * ===========================================================================
 *
 *                            PUBLIC DOMAIN NOTICE
 *               National Center for Biotechnology Information
 *
 *  This software/database is a "United States Government Work" under the
 *  terms of the United States Copyright Act.  It was written as part of
 *  the author's official duties as a United States Government employee and
 *  thus cannot be copyrighted.  This software/database is freely available
 *  to the public for use. The National Library of Medicine and the U.S.
 *  Government have not placed any restriction on its use or reproduction.
 *
 *  Although all reasonable efforts have been taken to ensure the accuracy
 *  and reliability of the software and data, the NLM and the U.S.
 *  Government do not and cannot warrant the performance or results that
 *  may be obtained by using this software or data. The NLM and the U.S.
 *  Government disclaim all warranties, express or implied, including
 *  warranties of performance, merchantability or fitness for any particular
 *  purpose.
 *
 *  Please cite the author in any work or product based on this material.
 *
 * ===========================================================================
 *
 * Author:  Anton Lavrentiev
 *
 * File Description:
 * @file ncbi_blowfish.h
 *   Implementation of the Blowfish encryption algorithm
 *
 */

#include <connect/connect_export.h>
#include <corelib/ncbitype.h>
#include <stddef.h>


/** @addtogroup UtilityFunc
 *
 * @{
 */


#ifdef __cplusplus
extern "C" {
#endif


/* fwdecl */
struct SNcbiBlowfish;
/** Opaque encryption / decryption context type */
typedef const struct SNcbiBlowfish* NCBI_BLOWFISH;


/** Init the cipher context with a key of the specified length.
 *  Note that Blowfish limits the key to be 448 bits (56 bytes) long, so the
 *  remainder of a longer key (if so provided) is ignored.  A shorter key gets
 *  cyclically repeated as necessary to fill up the 56 bytes.  To specify a
 *  shorter key explicitly pad with zero bits up to 448.
 *  Return 0 on memory allocation error.
 */
extern NCBI_XCONNECT_EXPORT
NCBI_BLOWFISH NcbiBlowfishInit
(const void* key,
 size_t      keylen);


/** Encrypt a 64-bit block of data pointed to by "text" with an encrypted
 *  scrambled cipher data stored back at the same location.
 */
extern NCBI_XCONNECT_EXPORT
void NcbiBlowfishEncrypt
(NCBI_BLOWFISH ctx,           /**< Context from NcbiBlowfishInit()      */
 Uint8*        text           /**< Clear text replaced with cipher data */
 );


/** Decrypt a 64-bit of cipher data pointed to by "data" back into the clear
 *  text stored at the same location.
 */
extern NCBI_XCONNECT_EXPORT
void NcbiBlowfishDecrypt
(NCBI_BLOWFISH ctx,           /**< Context from NcbiBlowfishInit()      */   
 Uint8*        data           /**< Cipher data replaced with clear text */
 );


/** Destroy the context created by NcbiBlowfishInit(). */
extern NCBI_XCONNECT_EXPORT
void NcbiBlowfishFini
(NCBI_BLOWFISH ctx
 );


#ifdef __cplusplus
}  /* extern "C" */
#endif


/* @} */

#endif /* CONNECT___NCBI_BLOWFISH__H */
