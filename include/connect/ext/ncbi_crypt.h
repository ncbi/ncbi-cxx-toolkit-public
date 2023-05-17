#ifndef CONNECT_EXT___NCBI_CRYPT__H
#define CONNECT_EXT___NCBI_CRYPT__H

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
 *   NCBI crypting module.
 *
 * MT-safety:
 * The API is MT-safe (except for CRYPT_Version(), which is not supposed to be
 * routinely used, anyways) provided that the CRYPT_Key handles returned by
 * CRYPT_Init() are not shared between the threads concurrently for encoding.
 * Decoding does not modify the CRYPT_Key handle so can be done in parallel
 * with the same object.  Convenience routines NcbiCrypt() and NcbiDecrypt()
 * build private temporary copies of CRYPT_Key, and thus, are safer to use.
 *
 */

#ifdef __cplusplus
extern "C" {
#endif


/* Forward declaration of internal opaque key handle: MT-safety */
typedef struct SCRYPT_KeyTag* CRYPT_Key;


/* Special key value to denote a failure from CRYPT_Init() */
#define CRYPT_BAD_KEY  ((CRYPT_Key)(-1L))


/* Build a key handle based on textual "key" (no more than 64 chars taken in)*/
/* Return 0 if the key is empty (either 0 or ""), or CRYPT_BAD_KEY if failed.*/
extern CRYPT_Key CRYPT_Init(const char* key);


/* Free a key handle (may be also 0 or CRYPT_BAD_KEY, both result in NOOP) */
extern void      CRYPT_Free(CRYPT_Key key);


/*
 * All [de]crypt procedures below return dynamically allocated
 * '\0'-terminated character array (to be later free()'d by the caller).
 * NOTE:  key == 0 causes no (de)crypting, just a copy of input returned;
 *        key == CRYPT_BAD_KEY results in an error logged, 0 returned;
 *        return value 0 (w/o logging) if memory allocation failed.
 */

/* Encode a plain text string "plaintext" using a key handle "key".
 * NB: "key" may get modified internally.
 * Return 0 if encryption failed, or a dynamically allocated result, which has
 * to be free()'d when no longer needed.
 */
extern char* CRYPT_EncodeString(CRYPT_Key key, const char* plaintext);


/* Decode an encrypted string "ciphertext" using a key handle "key".
 * Return 0 if decryption failed, or a dynamically allocated result, which has
 * to be free()'d when no longer needed.
 */
extern char* CRYPT_DecodeString(const CRYPT_Key key, const char* ciphertext);


/* COMPATIBILITY */
/* Return the result of encryption of "plaintext" with "key"; or 0 if failed.
 * The returned value must be free()'d when no longer needed.
 */
extern char* NcbiCrypt
(const char* plaintext, const char* key);


/* COMPATIBILITY */
/* Return the result of decryption of "ciphertext" with "key"; or 0 if failed.
 * The returned value must be free()'d when no longer needed.
 */
extern char* NcbiDecrypt
(const char* ciphertext, const char* key);


/* Set crypt version (to the value of the passed argument "version" >= 0), or
 * do nothing if the argument value is out of the known version range.
 * The call is *not* recommended for use in the user's code, but solely
 * for test purposes.  The notion of default version is supported internally,
 * and "version" < 0 causes the default version to become effective.
 * Return the actual version that has been acting prior to the call, or
 * -1 if the version may not be changed (e.g. stub variant of the library).
 */
extern int CRYPT_Version(int version);


#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* CONNECT_EXT___NCBI_CRYPT__H */
