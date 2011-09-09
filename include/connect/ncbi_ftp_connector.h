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

#include <connect/ncbi_connutil.h>

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
    fFTP_UseFeatures  = 0x04,  /* parse FEAT to check for available FTP feats*/
    fFTP_NotifySize   = 0x08,  /* use C/B to communicate file size to user   */
    fFTP_UsePassive   = 0x10,  /* use only passive mode for data connection  */
    fFTP_UseActive    = 0x20,  /* use only active  mode for data connection  */
    fFTP_UseTypeL8    = 0x40,  /* use "TYPE L8" instead of "TYPE I" for data */
    fFTP_BypassProxy  = 0x80,  /* do not use HTTP proxy even if provided     */
    fFTP_NoSizeChecks = 0x100, /* do not check sizes of data transfers       */
    fFTP_UncorkUpload = 0x200, /* do not use TCP_CORK for uploads            */
    fFTP_NoExtensions = 0x400  /* do not use EPSV/EPRT protocol extensions   */
} EFTP_Flags;
typedef unsigned int TFTP_Flags;  /* bitwise OR of ECONN_Flags               */


/* FTP protocol implies the use of both control connection (to pass commands
 * and responses) and data connection (to pass file contents), so is a 4-way
 * commmunication scheme.
 * CONN API supports only a two way scheme, which is why for FTP a special
 * discipline is required to communicate with the server.
 * User code interacts with FTP server by means of writing commands
 * (most of which are direct FTP commands, see below), and either reading
 * responses or file contents.  There is a special stop command to clear up
 * any pending command/action in progress.
 * Table below describes each command and what the user code is expected to do.
 *
 * Upon open, FTP server gets connected to with specified username and password
 * (user accounts [ACCT] are not supported), and the data transfer is set to be
 * STREAM/FILE/BINARY(I or L8) -- these transfer parameters may not be changed.
 *
 * Any disruption in control connection with server renders FTP connection bad
 * and unusable.  There is no automatic recovery (other than a full restart of
 * the connection) provided by the implementation.
 *
 * USER COMMAND(write) ACTION(server)          OUTPUT(to read on success)
 *
 * REN f1 f2           Rename file f1 to f2    250
 * CWD<SP>d            Change directory to d   250
 * PWD                 Get current directory   Current directory
 * MKD<SP>d            Create directory d      Directory created
 * RMD<SP>d            Delete directory d      250
 * CDUP                Go one dir level up     200
 * SYST                Get system info         Single-line system info
 * SIZE<SP>f           Get size of file f      Size of the file (numeric str)
 * MDTM<SP>f           Get time of file f      File time (UTC secs since epoch)
 * LIST[<SP>d]         List curdir / dir d     Full directory listing
 * NLST[<SP>d]         Short list as in LIST   Short dirlist (filenames only)
 * DELE<SP>f           Delete file f           250
 * RETR<SP>f           Retrieve file f         File contents
 * REST<SP>offset      Offset the transfer     350
 * NOOP[<SP>anything]  Abort current download  <EOF>
 * STOR<SP>f           Store file f on server
 * APPE<SP>f           Append/create file f
 * 
 * All commands above must be terminated with LF('\n') to be executed
 * (otherwise, successive writes are causing the command to continue to
 * accumulate in the internal command buffer).  Only one command can be
 * executed at a time (i.e. writing "CDUP\nSYST\n" in a single write is
 * illegal).  Note that the codes are text strings each consisting of 3 chars
 * (not ints!) -- the "values" are chosen to be equivalent to FTP response
 * codes that the FTP servers are expected to generate upon successful
 * completion of the corresponding commands (per RFC959), but may not always
 * be the actual codes received from the server (connector is flexible with
 * accepting various codes noted in several different implementation of FTP
 * servers).
 *
 * <SP> denotes exactly one space character, a blank means any number of space
 * or tab characters.  Single filenames(f) and directories(d) span up to the
 * end of the command ('\n'), and do not require any quoting for special
 * characters.  Exception is the REN command, which takes two names, f1 and f2,
 * with each being either a single token (no leading '"' and embedded spaces /
 * tabs), or quoted FTP-style (enclosed in double quotes, with any embedded
 * double quote character doubled, i.e. """a""b" encodes the file name "a"b).
 * Note that the filename a"b (no leading quote) does not require any quoting.
 * LIST and NLST can take optional argument d (the optional part shown in
 * square brakets which are not elements of either command).
 *
 * Current implementation forbids file names to contain '\0', '\n', or '\r'.
 *
 * Normally, FTP connection operates in the READ mode:  commands are written
 * and responses are read.  In this mode the connection consumes any command,
 * but those invalid, unrecognized, or rejected by the server will cause
 * CONN_Status(eIO_Write) to return non-eIO_Success.  Note that since normally
 * CONN_Write() returns eIO_Success when at least one byte of data has been
 * consumed, its return code is basically useless to distinguish the command
 * completion status.  Instead of terminating commands with '\n', CONN_Flush()
 * can be used, and its return code will be the true status of how the command
 * was done.  Alternatively, CONN_Wait(eIO_Read) can cause a similar effect,
 * and finally, a read from FTP connection that operates in READ mode causes a
 * pending command to be executed (even if the connection was created untied,
 * the additional flushing is done internally).
 *
 * When a RETR command gets executed, the following reads from the connection
 * will retrieve the contents of the file (until eIO_Closed).  If the
 * connection returns eIO_Closed right away, it means that either the file does
 * not exist, is not a file but a directory, or is empty.  The first two cases
 * would cause CONN_Status(eIO_Write) to return a code different from
 * eIO_Success; and eIO_Success would only result in the case of an empty file.
 *
 * File size will be checked by the connector to see whether the download (or
 * upload, see below) was complete (sometimes, the information returned from
 * the server does not permit doing this check).  Any mismatch will result in
 * an error different from eIO_Closed.  (For buggy / noisy FTP servers, the
 * size checks can be suppressed in the connector flags.)
 *
 * During file download, any command (legitimate or not) written to connection
 * and triggered for execution will abort the data transfer (cause a warning
 * logged, and connection must still be manually drained until eIO_Closed), but
 * if an output is expected from such a command, it cannot be distinguished
 * from the remnants of the file data -- so such a method is not very robust.
 * There is a special NOOP command that can be written to abort the transfer:
 * it produces no output (just inserts eIO_Closed in data), and for it is to be
 * a legitimate command, it usually results in eIO_Success when inquired for
 * write status (the result may be different on a rare occasion if the server
 * has chosen to close control connection, for example).  Still, to be usable
 * again the connection must be drained out until eIO_Closed is received
 * by reading.
 *
 * Connection is switched to SEND mode upon either APPE or STOR is executed.
 * If they are successful (CONN_Status(eIO_Write) reports eIO_Success), then
 * any following writes will send the data to the file being uploaded (while
 * the file is being uploaded, CONN_Status(eIO_Write) will report the status of
 * last write operation to the file).  Should an error occur eIO_Closed would
 * result and the connection would not accept any more writes until it is read.
 * Similarly, when the upload is about to finish, the connection must be read
 * to finalize the transfer.  The result of the read will be a string
 * representing the size of the uploaded file data (or an empty read in case
 * of an upload error) as a sequence of decimal digits.  Once all digits are
 * extracted (eIO_Closed seen) the connection returns to READ mode.
 * CONN_Wait(eIO_Read) will also cause the upload to finalize.
 *
 * Unfinalized uploads (such as when a connection gets closed before the final
 * read) get reported to the log, and also cause CONN_Close() to return an
 * error.  Note that unlike file download (which occurs in READ mode), it is
 * impossible to abort an upload by writing any FTP commands (since writing in
 * SEND mode goes to file), but it is reading that will do the cancel.  So if
 * a connection is in undetermined state, the recovery would be to do a small
 * quick read (e.g. for just 1 byte with small timeout), then write the "NOOP"
 * command and cause an execution (e.g. writing "NOOP\n" does that), then
 * drain the connection by reading again until eIO_Closed.
 *
 * Note that for commands, which return text codes, it is allowed not to read
 * the codes out, but rely solely on CONN_Status() responses.  Any pending
 * (unread) result of the previous command gets discarded when a new command
 * gets executed (i.e. command accumulation in the internal buffer does not
 * cause the pending result to be discarded, it is connection flushing, as
 * with '\n', CONN_Flush(), etc).  (Same happens with results of the commands
 * returning non-code information, but reading it out is supposed to be the
 * very purpose of issuing such commands, and hence, is not mentioned above.)
 *
 * The supplement mode of CONN API can make use of FTP connection much easier:
 * instead of checking for CONN_Status(), direct return codes of read/write
 * operations can be used.  Care must be taken to interpret eIO_Closed that may
 * result from read operations (such as when extracting the code that is
 * immediately followed by the response boundary denoted as eIO_Closed).
 *
 * To make the code robust, it is always advised to first process the actual
 * byte count reported from CONN I/O and only then to analyze the return code.
 */

/*
 * Even though many FTP server implementations provide SIZE command these days,
 * some FTPDs still lack this feature and can post the file size only when the
 * actual download starts.  For them, and for connections that do not want to
 * get the size inserted into the data stream (which is the default behavior
 * upon a successful SIZE command), the following callback is provided as an
 * alternative solution.
 * The callback gets activated when downloads start, and also upon successful
 * SIZE commands (without causing the file size to appear in the connection
 * data as it usually would otherwise) but the latter is only if
 * fFTP_NotifySize has been set in the "flag" parameter of FTP connector
 * constructors (below).
 * Each time the size gets passed to the callback as a '\0'-terminated
 * character string.
 * The callback remains effective for the entire lifetime of the connector.
 * As the first argument, the callback also gets a copy of the FTP command
 * that triggered it, and for compatibility with future extensions, the user
 * code is expected to check which command it is processing, before proceeding
 * with the "arg" parameter (thus skipping unexpected commands, and returning
 * eIO_Success).  Return code non-eIO_Success causes the command terminate
 * with an error, with the code returned "as-is" from a CONN call.
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


/* Create new CONNECTOR structure to handle FTP transfers,
 * both download and upload.  Return NULL on error.
 */
extern NCBI_XCONNECT_EXPORT CONNECTOR FTP_CreateConnectorSimple
(const char*          host,   /* hostname, required                          */
 unsigned short       port,   /* port #, 21 [standard] if 0 passed here      */
 const char*          user,   /* username, "ftp" [==anonymous] by default    */
 const char*          pass,   /* password, "none" by default                 */
 const char*          path,   /* initial directory to "chdir" to on server   */
 TFTP_Flags           flag,   /* mostly for logging socket data [optional]   */
 const SFTP_Callback* cmcb    /* command callback [optional]                 */
);


/* Same as above but use fields provided by the connection structure */
extern NCBI_XCONNECT_EXPORT CONNECTOR FTP_CreateConnector
(const SConnNetInfo*  info,   /* all connection params including HTTP proxy  */
 TFTP_Flags           flag,   /* mostly for logging socket data [optional]   */
 const SFTP_Callback* cmcb    /* command callback [optional]                 */
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
