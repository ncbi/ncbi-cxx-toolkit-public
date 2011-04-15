#ifndef CONNECT___NCBI_FTP_CONNECTOR__H
#define CONNECT___NCBI_FTP_CONNECTOR__H

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
 *   FTP CONNECTOR
 *
 *   See <connect/ncbi_connector.h> for the detailed specification of
 *   the connector's methods and structures.
 *
 */

#include <connect/ncbi_buffer.h>
#include <connect/ncbi_connector.h>

#ifndef NCBI_DEPRECATED
#  define NCBI_FTP_CONNECTOR_DEPRECATED
#else
#  define NCBI_FTP_CONNECTOR_DEPRECATED NCBI_DEPRECATED
#endif


/** @addtogroup Connectors
 *
 * @{
 */


#ifdef __cplusplus
extern "C" {
#endif


typedef enum {
    fFTP_LogControl   = 0x01,
    fFTP_LogData      = 0x02,
    fFTP_LogAll       = fFTP_LogControl | fFTP_LogData,
    fFTP_NotifySize   = 0x04,  /* use CB to communicate file size to user   */
    fFTP_UseFeatures  = 0x08,
    fFTP_UsePassive   = 0x10,  /* use only passive mode for data connection */
    fFTP_UseActive    = 0x20,  /* use only active  mode for data connection */
    fFTP_UncorkUpload = 0x40   /* do not use TCP_CORK for uploads           */
} EFTP_Flags;
typedef unsigned int TFTP_Flags;


/* Even though many FTP server implementations provide SIZE command these days,
 * some FTPDs still lack this feature and can post the file size only when the
 * actual download starts.  For them, and for connections that  do not want to
 * get the size inserted into the data stream (which is the default behavior
 * upon a successful SIZE command), the following callback is provided as an
 * alternative solution.
 * The callback gets activated when downloads start, and also upon successful
 * SIZE commands (without causing the file size to appear in the connection
 * data as it usually would otherwise) but only if fFTP_NotifySize has been
 * set in the "flag" parameter of FTP_CreateConnectorEx().
 * Each time the size gets passed in as a '\0'-terminated character string.
 * The callback remains effective for the entire lifetime of the connector.
 *
 * NOTE:  With restarted data retrievals (REST) the size reported by the server
 * in response to transfer initiation can be either the true size of the data
 * to be received or the entire size of the original file (without the restart
 * offset taken into account), and the latter should be considered as a bug.
 */
typedef EIO_Status (*FFTP_Callback)(void* data,
                                    const char* cmd, const char* arg);
typedef struct {
    FFTP_Callback     func;   /* to call upon certain FTP commands           */
    void*             data;   /* to supply as a first callback parameter     */
} SFTP_Callback;


/* Create new CONNECTOR structure to handle ftp transfers,
 * both download and upload.
 * Return NULL on error.
 */
extern NCBI_XCONNECT_EXPORT CONNECTOR FTP_CreateConnectorEx
(const char*          host,   /* hostname, required                          */
 unsigned short       port,   /* port #, 21 [standard] if 0 passed here      */
 const char*          user,   /* username, "ftp" [==anonymous] by default    */
 const char*          pass,   /* password, "none" by default                 */
 const char*          path,   /* initial directory to "chdir" to on server   */
 TFTP_Flags           flag,   /* mostly for logging socket data [optional]   */
 const SFTP_Callback* cmcb    /* command callback [optional]                 */
);


/* Same as FTP_CreateConnectorEx(,,,,,NULL) for backward compatibility */
extern NCBI_XCONNECT_EXPORT CONNECTOR FTP_CreateConnector
(const char*          host,   /* hostname, required                          */
 unsigned short       port,   /* port #, 21 [standard] if 0 passed here      */
 const char*          user,   /* username, "ftp" [==anonymous] by default    */
 const char*          pass,   /* password, "none" by default                 */
 const char*          path,   /* initial directory to "chdir" to on server   */
 TFTP_Flags           flag    /* mostly for logging socket data [optional]   */
);


/* Same as above:  do not use for the obsolete naming */
NCBI_FTP_CONNECTOR_DEPRECATED
extern NCBI_XCONNECT_EXPORT CONNECTOR FTP_CreateDownloadConnector
(const char* host, unsigned short port, const char* user,
 const char* pass, const char*    path, TFTP_Flags  flag);


#ifdef __cplusplus
}  /* extern "C" */
#endif


/* @} */

#endif /* CONNECT___NCBI_FTP_CONNECTOR__H */
