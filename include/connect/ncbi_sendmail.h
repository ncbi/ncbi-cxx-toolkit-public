#ifndef NCBI_SENDMAIL__H
#define NCBI_SENDMAIL__H

/*  $Id$
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
 *    Send mail
 *
 * --------------------------------------------------------------------------
 * $Log$
 * Revision 6.1  2001/02/28 00:52:37  lavr
 * Initial revision
 *
 * ==========================================================================
 */

#include <connect/ncbi_core.h>


#ifdef __cplusplus
extern "C" {
#endif


#define MX_HOST         "ncbi"
#define MX_PORT         25
#define MX_TIMEOUT      120


typedef struct {
    unsigned int magic_number;
    const char*  cc;
    const char*  bcc;
    char         from[1024];
    const char*  header;
    const char*  mx_host;
    short        mx_port;
    STimeout     mx_timeout;
} SSendMailInfo;

SSendMailInfo* SendMailInfo_Init(SSendMailInfo* info);


/* 0 - Ok, "Error message" */
extern const char* CORE_SendMail(const char* to,
                                 const char* subject,
                                 const char* body);

extern const char* CORE_SendMailEx(const char* to,
                                   const char* subject,
                                   const char* body,
                                   const SSendMailInfo* info);


#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* NCBI_SENDMAIL__H */
