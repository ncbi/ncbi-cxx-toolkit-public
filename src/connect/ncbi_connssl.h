#ifndef CONNECT___NCBI_CONNSSL__H
#define CONNECT___NCBI_CONNSSL__H

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
 * @file
 * File Description:
 *   Formal definition of a simple SSL API.
 *
 */

#include "ncbi_socketp.h"


#ifdef __cplusplus
extern "C" {
#endif


typedef enum {
    /* NB: Must be divisible by 100 decimal */
    eNcbiCred_GnuTls  = 0x484FFB94,
    eNcbiCred_MbedTls = 0xC12CC114
} ENcbiCred;


struct SNcbiCred {
    ENcbiCred type;
    void*     data;
};


/* Read up to "size" bytes into buffer "buf", and return the number of bytes
 * actually read via the "done" pointer (must be non-null, on call "*done"==0).
 * The call is allowed to log the transaction data if "logdata" is non-zero.
 * The call is always allowed to log errors (regardless of the last parameter).
 * Return:
 *  eIO_Success if "*done" != 0 or if EOF encountered ("*done" == 0 then);
 *  eIO_Timeout if no data obtained within a preset time allowance;
 *  eIO_Unknown if an error (usually, recoverable) occurred;
 *  eIO_Closed if a non-recoverable error occurred;
 *  other errors per their applicability.
 */
typedef EIO_Status  (*FSSLPull)  (SOCK sock,       void* buf,  size_t size,
                                  size_t* done, int/*bool*/ logdata);

/* Write up to "size" bytes of "data", and return the number of bytes actually
 * written via the "done" pointer (must be non-null, on call "*done"==0).
 * The call is allowed to log the transaction data if "logdata" is non-zero.
 * The call is always allowed to log errors (regardless of the last parameter).
 * Return:
 *  eIO_Success iff "*done" != 0;
 *  eIO_Closed if non-recoverable error;
 *  other error code if no data can be written.
 */
typedef EIO_Status  (*FSSLPush)  (SOCK sock, const void* data, size_t size,
                                  size_t* done, int/*bool*/ logdata);

/* Init SSL layer; called only once and under a lock */
typedef EIO_Status  (*FSSLInit)  (FSSLPull pull, FSSLPush push);

/* Create session data with "ctx" */
typedef void*       (*FSSLCreate)(ESOCK_Side side, SNcbiSSLctx* ctx,
                                  int* error);

/* Begin secure session; "desc" can be NULL for no description to return */
typedef EIO_Status  (*FSSLOpen)  (void* session, int* error, char** desc);

/* See FSSLPull for behavior.  When non-eIO_Success code gets returned,
 * the call must set "*error" to indicate specific problem.  The "*error" may
 * be left unset (and thus, will be ignored) when eIO_Success gets returned. */
typedef EIO_Status  (*FSSLRead)  (void* session,       void* buf,  size_t size,
                                  size_t* done, int* error);

/* See FSSLPush for behavior.  When non-eIO_Success code gets returned,
 * the call must set "*error" to indicate specific problem.  The "*error" may
 * be left unset (and thus, will be ignored) when eIO_Success gets returned. */
typedef EIO_Status  (*FSSLWrite) (void* session, const void* data, size_t size,
                                  size_t* done, int* error);

/* End secure session; "how" is of shutdown(2) and may be ignored */
typedef EIO_Status  (*FSSLClose) (void* session, int how, int* error);

/* Delete session data */
typedef void        (*FSSLDelete)(void* session);

/* Deinit SSL layer; called once and under a lock */
typedef void        (*FSSLExit)  (void);

/* Return an error description (possibly stored in "buf" of size "size") */
typedef const char* (*FSSLError) (void* session, int  error,
                                  char* buf, size_t size);


/* Table of operations
 */
struct SOCKSSL_struct {
    const char* Name;
    FSSLInit    Init;
    FSSLCreate  Create;
    FSSLOpen    Open;
    FSSLRead    Read;
    FSSLWrite   Write;
    FSSLClose   Close;
    FSSLDelete  Delete;
    FSSLExit    Exit;
    FSSLError   Error;
};


/* Internal certificate credentials management routines */

#if defined(HAVE_LIBMBEDTLS)  ||  defined(NCBI_CXX_TOOLKIT)

NCBI_CRED NcbiCreateMbedTlsCertCredentials(const void* cert,
                                           size_t      certsz,
                                           const void* pkey,
                                           size_t      pkeysz);

void NcbiDeleteMbedTlsCertCredentials(NCBI_CRED cred);

#endif /*HAVE_LIBMBEDTLS || NCBI_CXX_TOOLKIT*/


#ifdef HAVE_LIBGNUTLS

NCBI_CRED NcbiCreateGnuTlsCertCredentials(const void* cert,
                                          size_t      certsz,
                                          const void* pkey,
                                          size_t      pkeysz);


void NcbiDeleteGnuTlsCertCredentials(NCBI_CRED cred);

#endif /*HAVE_LIBGNUTLS*/


#ifdef __cplusplus
}  /* extern "C" */
#endif


/* @} */

#endif /* CONNECT___NCBI_CONNSSL__H */
