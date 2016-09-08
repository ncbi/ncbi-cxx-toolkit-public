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

#include <connect/ncbi_util.h>


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
    fSendMail_LogOn            = eOn,      /**< see: fSOCK_LogOn             */
    fSendMail_LogOff           = eOff,     /**<      fSOCK_LogDefault        */
    fSendMail_NoMxHeader       = (1 << 4), /**< Don't add standard mail header,
                                                just use what user provided  */
    fSendMail_Old822Headers    = (1 << 6), /**< Form "Date:" and "From:" hdrs
                                                (usually they are defaulted) */
    fSendMail_StripNonFQDNHost = (1 << 8), /**< Strip host part off the "from"
                                                field if it does not look like
                                                an FQDN (i.e. does not have at
                                                least two domain name labels
                                                separated by a dot); leave only
                                                the username part */
    fSendMail_ExtendedErrInfo  = (1 << 10) /**< Return extended error info that
                                                must be free()'d by caller   */
};
typedef unsigned short TSendMailOptions;   /**< Bitwise OR of ESendMailOption*/


/** Define optional parameters for communication with sendmail.
 */
typedef struct {
    const char*      cc;            /**< Carbon copy recipient(s)            */
    const char*      bcc;           /**< Blind carbon copy recipient(s)      */
    char             from[1024];    /**< Originator address                  */
    const char*      header;        /**< Custom msg header ('\n'-separated)  */
    size_t           body_size;     /**< Message body size (if specified)    */
    STimeout         mx_timeout;    /**< Timeout for all network transactions*/
    const char*      mx_host;       /**< Host to contact an MTA at           */
    short            mx_port;       /**< Port to contact an MTA at           */
    TSendMailOptions mx_options;    /**< See ESendMailOption                 */
    unsigned int     magic_cookie;  /**< RO, filled in by SendMailInfo_Init  */
} SSendMailInfo;


/** Initialize SSendMailInfo structure, setting:
 *   'cc', 'bcc', 'header' to NULL (means no recipients/additional headers);
 *   'from' filled out with a return address using either the provided
 *          (non-empty) user name or the name of the current user if
 *          discoverable ("anonymous" otherwise), and the host, in the form:
 *          "username@hostname";  that may later be reset by the application
 *          to "" for sending no-return messages (aka MAILER-DAEMON messages);
 *   'mx_*' filled out in accordance with some hard-coded defaults, which are
 *          rather NCBI-specific;  an outside application is likely to choose
 *          and use different values (at least for 'mx_host').
 *          The mx_... fields can be configured via the registry file at
 *          [CONN]MX_HOST, [CONN]MX_PORT, and [CONN]MX_TIMEOUT, as well as
 *          through their process environment equivalents (which have higher
 *          precedence, and override the values found in the registry):
 *          CONN_MX_HOST, CONN_MX_PORT, and CONN_MX_TIMEOUT, respectively;
 *   'magic_cookie' to a proper value (verified by CORE_SendMailEx()!).
 * @note This call is the only valid way to properly init SSendMailInfo.
 * @param info
 *  A pointer to the structure to initialize
 * @param from
 *  Return address pattern to use in 'info->from' as the following:
 *  * "user@host" or "user" is copied verbatim (as-is);
 *  * "user@" is appended with the local host name;
 *  * "@host" is prepended with the user name according to the 'user' argument;
 *  * "@"     is replaced with an empty string (for no-return messages);
 *  * "" or NULL cause to generate both the user and the host parts.
 * @param user
 *  Which username to use when auto-generating (ignored otherwise)
 * @return
 *  Return value equals the argument 'info' passed in.
 * @note It is allowed to pre-fill 'info->from' (of the 'info' being inited)
 *       with one of the above patterns, and pass it as the 'from' parameter.
 * @note Unlike the username part of the return address, the hostname part is
 *       never truncated but dropped altogether if it does not fit.
 * @note If the username is unobtainable, then it is be replaced with the word
 *       "anonymous".
 * @sa
 *  CORE_SendMailEx, CORE_GetUsernameEx
 */
extern NCBI_XCONNECT_EXPORT SSendMailInfo* SendMailInfo_InitEx
(SSendMailInfo* info,
 const char*    from,
 ECORE_Username user
 );

#define SendMailInfo_Init(i)  SendMailInfo_InitEx(i, 0, eCORE_UsernameLogin)


/** Send a simple message to recipient(s) defined in 'to', and having:
 * 'subject', which may be empty (both NULL and "" treated equally), and
 * message 'body' (may be NULL/empty, if not empty, lines are separated by
 * '\n', must be '\0'-terminated).  Communicaiton parameters for connection
 * with sendmail are set using default values as described in
 * SendMailInfo_InitEx().
 * @note  Use of this call in out-of-house applications is discouraged as it is
 *        likely to fail because the MTA communication parameters (set to their
 *        defaults, which are NCBI-specific) may not be suitable.
 * @param to
 *  Recipient list
 * @param subject
 *  Subject of the message
 * @param body
 *  The message body
 * @return
 *  0 on success;  otherwise, a descriptive error message.  The message is
 *  kept in a static storage and is not to be deallocated by the caller.
 * @sa
 *  SendMailInfo_InitEx, CORE_SendMailEx
 */
extern NCBI_XCONNECT_EXPORT const char* CORE_SendMail
(const char* to,
 const char* subject,
 const char* body
 );

/** Send a message as in CORE_SendMail() but by explicitly specifying all
 * additional parameters of the message and communication via the argument
 * 'info'. In case of 'info' == NULL, the call is completely equivalent to
 * CORE_SendMail().
 * @note Body may not necessarily be '\0'-terminated if 'info->body_size'
 * specifies non-zero message body size (see SSendMailInfo::body_size above).
 *
 * @note
 * Recipient lists are not validated;  a valid recipient (according to the
 * standard) can be specified in the form '"Name" \<address\>' (excluding all
 * comment fields, which are discouraged from appearance in the addresses);
 * recipients must be separated by commas.  In case of address-only recipients
 * (with no "Name" part above), the angle brackets around the address should
 * be omitted.  Group syntax for recipients 'groupname ":" [mailbox-list] ";"'
 * is not currently supported.
 *
 * @note
 * If not specified (0), and by default, the message body size is calculated as
 * strlen() of the passed 'body' argument, which thus must be '\0'-terminated.
 * Otherwise, exactly 'info->body_size' bytes are read from the location
 * pointed to by the 'body' parameter, and are then sent in the message.
 *
 * @note
 * If fSendMail_NoMxHeader is set in 'info->mx_options', the body can have
 * an additional header part  (otherwise, a standard header gets generated as
 * needed).  In this case, even if no additional headers are supplied, the body
 * must provide the proper header / message text delimiter (an empty line),
 * which will not be automatically inserted (aka "as-is" message mode).
 *
 * @param to
 *  Recipient list
 * @param subject
 *  Subject of the message
 * @param body
 *  The message body
 * @param info
 *  Communicational and additional protocol parameters
 * @return
 *  0 on success;  otherwise, a descriptive error message.  The message is
 *  not to be deallocated by the caller unless fSendMail_ExtendedErrInfo is
 *  specified in SSendMailinfo::mx_options, in which case the message contains
 *  an explanation string followed by ": " and possibly an RFC821-compliant
 *  error code (as received from the server) followed by additional error text
 *  (which in turn may begin with RFC3462-compliant status as
 *  "class.subject.detail") -- this extended message string must be free()'d by
 *  the caller when no longer necessary.  If there was an allocation error, the
 *  string is returned empty ("") and must not be deallocated.  The latter
 *  makes the caller still aware there was a problem.
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
