#ifndef CONNECT___NCBI_SENDMAIL__H
#define CONNECT___NCBI_SENDMAIL__H

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
 * @file
 *   Send mail (in accordance with RFC821 [protocol] and RFC822 [headers])
 *
 */

#include <connect/ncbi_types.h>


/** @addtogroup Sendmail
 *
 * @{
 */


#ifdef __cplusplus
extern "C" {
#endif


/** Options apply to various fields of SSendMailInfo structure, below
 * @sa SSendMailInfo
 */
enum ESendMailOption {
    fSendMail_NoMxHeader       = (1 << 0), /**< Don't add standard mail header,
                                                just use what user provided */
    fSendMail_StripNonFQDNHost = (1 << 8)  /**< Strip host part in "from" field
                                                if it does not look like an
                                                FQDN (i.e. does not have at
                                                least two domain name labels
                                                separated by a dot); leave only
                                                the username part */
};
typedef unsigned int TSendMailOptions;     /**< Bitwise OR of ESendMailOption*/


/** Define optional parameters for communication with sendmail.
 */
typedef struct {
    const char*      cc;            /**< Carbon copy recipient(s)            */
    const char*      bcc;           /**< Blind carbon copy recipient(s)      */
    char             from[1024];    /**< Originator address                  */
    const char*      header;        /**< Custom msg header ('\n'-separated)  */
    size_t           body_size;     /**< Message body size (if specified)    */
    const char*      mx_host;       /**< Host to contact an MTA at           */
    short            mx_port;       /**< Port to contact an MTA at           */
    STimeout         mx_timeout;    /**< Timeout for all network transactions*/
    TSendMailOptions mx_options;    /**< From the above                      */
    unsigned int     magic_cookie;  /**< Filled in by SendMailInfo_Init      */
} SSendMailInfo;


/** Initialize SSendMailInfo structure, setting:
 *   'cc', 'bcc', 'header' to NULL (means no recipients/additional headers);
 *   'from' filled out using either the provided (non-empty) user name
 *          or the name of the current user if discovered, 'anonymous'
 *          otherwise, and host in the form: username@hostname; may be later
 *          reset by the application to "" for sending no-return messages
 *          (aka MAILER-DAEMON messages);
 *   'mx_*' filled out in accordance with some hard-coded defaults, which are
 *          very NCBI-specific; an outside application is likely to choose and
 *          use different values (at least for mx_host).
 *          The mx_... fields can be configured via the registry file at
 *          [CONN]MX_HOST, [CONN]MX_PORT, and [CONN]MX_TIMEOUT, as well as
 *          through their process environment equivalents (which have higher
 *          precedence, and override the values found in the registry):
 *          CONN_MX_HOST, CONN_MX_PORT, and CONN_MX_TIMEOUT, respectively;
 *   'magic_cookie' to a proper value (verified by CORE_SendMailEx()!).
 * @note This call is the only valid way to properly init SSendMailInfo.
 * @param info
 *  A pointer to the structure to initialize
 * @return
 *  Return value equals the argument passed in.
 * @sa
 *  CORE_SendMailEx
 */
extern NCBI_XCONNECT_EXPORT SSendMailInfo* SendMailInfo_InitEx
(SSendMailInfo*       info,
 const char*          user
 );

#define SendMailInfo_Init(info)  SendMailInfo_InitEx(info, 0)


/** Send a simple message to recipient(s) defined in 'to', and having:
 * 'subject', which may be empty (both NULL and "" treated equally), and
 * message 'body' (may be NULL/empty, if not empty, lines are separated by
 * '\n', must be '\0'-terminated).  Communicaiton parameters for connection
 * with sendmail are set using default values as described in
 * SendMailInfo_InitEx().
 * @note  Use of this call in out-of-house applications is discouraged as
 *        it is likely to fail since MTA communication parameters set
 *        to their defaults (which are NCBI-specific) may not be suitable.
 * @param to
 *  Recipient list
 * @param subject
 *  Subject of the message
 * @param body
 *  The message body
 * @return
 *  0 on success;  otherwise, a descriptive error message.
 * @sa
 *  SendMailInfo_InitEx, CORE_SendMailEx
 */
extern NCBI_XCONNECT_EXPORT const char* CORE_SendMail
(const char*          to,
 const char*          subject,
 const char*          body
 );

/** Send a message as in CORE_SendMail() but by explicitly specifying all
 * additional parameters of the message and the communication via argument
 * 'info'. In case of 'info' == NULL, the call is completely equivalent to
 * CORE_SendMail().
 * @note Body may not neccessarily be '\0'-terminated if 'info->body_size'
 * specifies non-zero message body size (see SSendMailInfo::body_size above).
 *
 * @note
 * Recipient lists are not parsed;  valid recipient (according to the standard)
 * can be specified in the form '"Name" \<address\>';  recipients should be
 * separated by commas.  In case of address-only recipients (with no "Name"
 * part above), angle brackets around the address may be omitted.
 *
 * @note
 * If not specified (0), and by default, the message body size is calculated
 * as strlen() of passed body argument, which thus must be '\0'-terminated.
 * Otherwise, exactly "body_size" bytes are read from the location pointed to
 * by "body" parameter, and are sent in the message.
 *
 * @note
 * If fSendMail_NoMxHeader is set in 'info->mx_options', the body can have
 * an additional header part  (otherwise, a standard header gets generated as
 * needed).  In this case, even if no additional headers are supplied, the body
 * must provide proper header / message text delimiter (an empty line), which
 * will not be automatically inserted in the no-header (aka "as-is") mode.
 *
 * @param to
 *  Recipient list
 * @param subject
 *  Subject of the message
 * @param body
 *  The message body
 * @param info
 *  Communication parameters
 * @return
 *  0 on success;  otherwise, a descriptive error message.
 * @sa
 *  SendMailInfo_InitEx
 */
extern NCBI_XCONNECT_EXPORT const char* CORE_SendMailEx
(const char*          to,
 const char*          subject,
 const char*          body,
 const SSendMailInfo* info
 );


#ifdef __cplusplus
}  /* extern "C" */
#endif


/* @} */

#endif /* CONNECT___NCBI_SENDMAIL__H */
