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
 *    Send mail (with accordance to RFC821 [protocol] and RFC822 [headers])
 *
 * --------------------------------------------------------------------------
 * $Log$
 * Revision 6.4  2001/07/10 15:07:51  lavr
 * More comments added
 *
 * Revision 6.3  2001/03/01 01:03:46  lavr
 * SendMailInfo_Init got extern
 *
 * Revision 6.2  2001/02/28 18:13:02  lavr
 * Heavily documented
 *
 * Revision 6.1  2001/02/28 00:52:37  lavr
 * Initial revision
 *
 * ==========================================================================
 */

#include <connect/ncbi_core.h>


#ifdef __cplusplus
extern "C" {
#endif


/* Default values for the structure below */
#define MX_HOST         "ncbi"
#define MX_PORT         25
#define MX_TIMEOUT      120


/* Define optional parameters for communication with sendmail
 */
typedef struct {
    unsigned int magic_number;  /* Filled in by SendMailInfo_Init        */
    const char*  cc;            /* Carbon copy recipient(s)              */
    const char*  bcc;           /* Blind carbon copy recipient(s)        */
    char         from[1024];    /* Originator address                    */
    const char*  header;        /* Custom header fields ('\n'-separated) */
    const char*  mx_host;       /* Host to contact sendmail at           */
    short        mx_port;       /* Port to contact sendmail at           */
    STimeout     mx_timeout;    /* Timeout for all network transactions  */
} SSendMailInfo;


/* NOTE about recipient lists:
 * They are not parsed; valid recipient (according to a standard)
 * can be specified in the form "Name" <address>; recipients should
 * be separated by commas. In case of address-only recipients (no "Name"
 * part as above), angle brackets around address may be omitted.
 */


/* Initialize SSendMailInfo structure, setting:
 *   'magic_number' to proper value (verified by CORE_SendMailEx()!);
 *   'cc', 'bcc', 'header' to NULL (means no recipients/additional headers);
 *   'from' is filled out using current user name (if discovered, 'anonymous'
 *          otherwise) and host in the form: username@hostname; may be later
 *          reset by application to "" for sending no-return messages
 *          (aka MAILER-DAEMON messages);
 *   'mx_*' filled out with accordance to corresponding macros defined above;
 *          application may choose different values afterwards.
 * Return value equals passed argument.
 * Note: This call is the only valid way to init SSendMailInfo.
 */
extern SSendMailInfo* SendMailInfo_Init(SSendMailInfo* info);


/* Send a simple message to recipient(s) defined in 'to',
 * and having subject 'subject', which may be empty (both NULL and "" treated
 * equally as empty subjects), and message body 'body' (may be NULL/empty,
 * if not empty, lines are separated by '\n').
 * Return value 0 means success; otherwise descriptive error message
 * gets returned. Communicaiton parameters for connection with sendmail
 * are set using default values as described in SendMailInfo_Init().
 */
extern const char* CORE_SendMail(const char* to,
                                 const char* subject,
                                 const char* body);

/* Send a message as in CORE_SendMail() but with specifying explicitly
 * all additional parameters of the message and communication via
 * argument 'info'. In case of 'info' == NULL, the call is completely
 * equivalent to CORE_SendMail().
 */
extern const char* CORE_SendMailEx(const char* to,
                                   const char* subject,
                                   const char* body,
                                   const SSendMailInfo* info);


#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* NCBI_SENDMAIL__H */
