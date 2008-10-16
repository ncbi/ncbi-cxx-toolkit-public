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
 *   Formal and dataless ("pure virtual") definition of a simple SSL API.
 *
 */

#include "ncbi_socketp.h"


#ifdef __cplusplus
extern "C" {
#endif


typedef EIO_Status  (*FSSLPull)  (SOCK sock,       void* buf,  size_t size,
                                  size_t* done, int/*bool*/ logdata);
typedef EIO_Status  (*FSSLPush)  (SOCK sock, const void* data, size_t size,
                                  size_t* done, int/*bool*/ logdata);


typedef EIO_Status  (*FSSLInit)  (FSSLPull pull, FSSLPush push);
typedef void*       (*FSSLCreate)(ESOCK_Side side, SOCK sock, int* error);
typedef EIO_Status  (*FSSLOpen)  (void* session, int* error);
typedef EIO_Status  (*FSSLRead)  (void* session,       void* buf,  size_t size,
                                  size_t* done,  int* error);
typedef EIO_Status  (*FSSLWrite) (void* session, const void* data, size_t size,
                                  size_t* done,  int* error);
typedef EIO_Status  (*FSSLClose) (void* session, int how, int* error);
typedef void        (*FSSLDelete)(void* session);
typedef void        (*FSSLExit)  (void);
typedef const char* (*FSSLError) (void* session, int  error);


/* Table of "virtual functions"
 */
struct SOCKSSL_struct {
    FSSLInit   Init;
    FSSLCreate Create;
    FSSLOpen   Open;
    FSSLRead   Read;
    FSSLWrite  Write;
    FSSLClose  Close;
    FSSLDelete Delete;
    FSSLExit   Exit;
    FSSLError  Error;
};


#ifdef __cplusplus
}  /* extern "C" */
#endif


/* @} */

#endif /* CONNECT___NCBI_CONNSSL__H */
