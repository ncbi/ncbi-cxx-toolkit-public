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
 *   FTP CONNECTOR implements FTP client side API
 *
 *   See <connect/ncbi_connector.h> for the detailed specification of
 *   the connector's methods and structures.
 *
 */

#include <connect/ncbi_connutil.h>


/** @addtogroup Connectors
 *
 * @{
 */


#ifdef __cplusplus
extern "C" {
#endif


enum EFTP_Flag {
    fFTP_LogControl   = 0x1,
    fFTP_LogData      = 0x2,
    fFTP_LogAll       = fFTP_LogControl | fFTP_LogData,
    fFTP_UseFeatures  = 0x4,   /* parse FEAT to check for available FTP feats*/
    fFTP_NotifySize   = 0x8,   /* use C/B to communicate file size to user   */
    fFTP_UsePassive   = 0x10,  /* use only passive mode for data connection  */
    fFTP_UseActive    = 0x20,  /* use only active  mode for data connection  */
    fFTP_UseTypeL8    = 0x40,  /* use "TYPE L8" instead of "TYPE I" for data */
    fFTP_UncleanIAC   = 0x80,  /* do not escape IAC(\'377') in pathnames     */
    fFTP_IgnorePath   = 0x100, /* do not auto-chdir(net_info->path) at login */
    fFTP_UncorkUpload = 0x200, /* do not use TCP_CORK for uploads (poor perf)*/
    fFTP_NoSizeChecks = 0x400, /* do not check sizes of data transfers       */
    fFTP_NoExtensions = 0x800, /* do not use EPSV/EPRT protocol extensions   */
    fFTP_DelayRestart = 0x1000,/* delay RESTart until an actual xfer command */
    fFTP_UseProxy     = 0x2000 /* use proxy settings to establish connections*/
};
typedef unsigned int TFTP_Flags;  /* bitwise OR of EFTP_Flag */


/* FTP protocol implies the use of both control connection (to pass commands
 * and responses) and data connection (to pass file contents), so is a 4-way
 * communication scheme.
 * CONN API supports only a two way scheme, which is why for FTP a special
 * discipline is required to communicate with the server.
 * User code interacts with FTP server by means of writing commands (most of
 * which are direct FTP commands, see below), and either reading responses or
 * file contents.  There is a special stop command to clear up any pending
 * command/action in progress.
 *
 * Table below describes each command and what the user code is expected to do.
 * Note that commands unsupported by the server are automatically unsupported
 * by the connector.
 *
 * Upon open, an FTP server gets connected to with a specified username and
 * password (user accounts [ACCT] are not supported), and the data transfer is
 * set to be STREAM/FILE/BINARY(I or L8) -- these transfer parameters may not
 * be changed.
 *
 * Any disruption in control connection with the server renders FTP connection
 * bad and unusable.  There is no automatic recovery (other than a full restart
 * of the connection) provided by the implementation.
 *
 * USER COMMAND(write) ACTION(server)          OUTPUT(to read on success)
 *
 * REN f1 f2           Rename file f1 to f2    250
 * CWD<SP>d            Change directory to d   250
 * PWD                 Get current directory   Name of current directory
 * MKD<SP>d            Create directory d      Name of created directory
 * RMD<SP>d            Delete directory d      250
 * CDUP                Go one dir level up     200
 * SYST                Get system info         Single-line system info
 * STAT[<SP>f]         Get status[of file f]   Server response as received
 * SIZE<SP>f           Get size of file f      Size of the file (numeric str)
 * MDTM<SP>f           Get time of file f      File time (UTC secs since epoch)
 * DELE<SP>f           Delete file f           250
 * REST<SP>offset      Offset the transfer     350
 * LIST[<SP>d]         List curdir[or dir d]   Full directory listing
 * NLST[<SP>d]         Short list as in LIST   Short dirlist (filenames only)
 * RETR<SP>f           Retrieve file f         File contents
 * MLSD[<SP>d]         List curdir[or dir d]   Mach-readable directory listing
 * MLST[<SP>p]         Facts of curdir[or p]   Mach-readable path facts
 * FEAT                FEAT command            FEAT list as returned by server
 * OPTS<SP>opts        OPTS command            OPTS response as received
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
 * completion of the corresponding commands (per RFC959), but may not
 * necessarily be the actual codes as received from the server (connector is
 * somewhat flexible with accepting various codes noted in several different
 * implementation of FTP servers).
 *
 * <SP> denotes exactly one space character, a blank means any number of space
 * or tab characters.  Single filenames(f), directories(d), and paths(p) span
 * up to the end of the command ('\n'), and do not require any quoting for
 * special characters.  Exception is the REN command, which takes two names,
 * f1 and f2, each being either a single token (no leading '"' and embedded
 * spaces / tabs), or quoted FTP-style (enclosed in double quotes, with any
 * embedded double quote character doubled, e.g. """a""b" encodes the file
 * name "a"b).  Note that the filename a"b (no leading quote) does not require
 * any additional quoting.
 * Some commands (e.g. NLST, MLSD, etc) allow an optional argument, which can
 * either be present or omitted (the optional part is shown in the square
 * brakets, which are not the elements of those commands).
 * UTC seconds can have a fraction portion preceded by a decimal point.
 *
 * Current implementation forbids file names to contain '\0', '\r', or '\n'
 * (even though FTP takes special precautions how to deal with such names).
 *
 * Normally, FTP connection operates in READ mode:  commands are written and
 * responses are read.  In this mode the connection consumes any command, but
 * those invalid, unrecognized, or rejected by the server will cause
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
 * When a RETR/LIST/NLST/MLSD command gets executed, all subsequent reads from
 * the connection will retrieve the contents of the file or directory (until
 * eIO_Closed).  If the connection returns eIO_Closed right away, it means that
 * either the file/directory does not exist, or RETR was attempted on a
 * directory, or finally, the requested file/directory is empty.  The first two
 * cases would cause CONN_Status(eIO_Write) to return a code different from
 * eIO_Success;  and eIO_Success would only result in the case of an empty
 * source.
 *
 * File size will be checked by the connector to see whether the download (or
 * upload, see below) was complete (sometimes, the information returned from
 * the server does not permit doing this check).  Any mismatch will result in
 * an error different from eIO_Closed.  (For buggy / noisy FTP servers, the
 * size checks can be suppressed via the connector flags.)
 *
 * During file download, any command (legitimate or not) written to the
 * connection and triggered for execution will abort the data transfer (result
 * in a warning logged, yet the connection must still be manually drained until
 * eIO_Closed), but if an output is expected from such a command, it cannot be
 * distinguished from the remnants of the file data -- so such a method is not
 * very robust.
 *
 * There is a special NOOP command that can be written to abort the transfer:
 * it produces no output (just inserts eIO_Closed in data), and for it is to be
 * a legitimate command, it usually results in eIO_Success when inquired for
 * write status (the result may be different on a rare occasion if the server
 * has chosen to close control connection, for example).  Still, to be usable
 * again the connection must be drained out until eIO_Closed is received
 * by reading.
 *
 * Note that for commands, which return text codes, it is allowed not to read
 * the codes out, but rely solely on CONN_Status() responses.  Any pending
 * (unread) result of the previous command gets discarded when a new command
 * gets executed (i.e. command accumulation in the internal buffer does not
 * cause the pending result to be discarded; it is the connection flushing, as
 * with '\n', CONN_Flush(), etc that does so).  (Same happens with results of
 * the commands returning non-code information, but reading it out is supposed
 * to be the very purpose of issuing of such commands, and hence, is not
 * mentioned above.)
 *
 * Connection is switched to SEND mode upon either APPE or STOR is executed.
 * If that is successful (CONN_Status(eIO_Write) reports eIO_Success), then
 * any following writes will send the data to the file being uploaded (while
 * the file is being uploaded, CONN_Status(eIO_Write) will report the status of
 * the last write operation to the file).  Should an error occur, eIO_Closed
 * would result, and the connection would not accept any more writes until it
 * is read.  Similarly, when an upload is about to finish, the connection must
 * be read to finalize the transfer.  The result of the read will be a string
 * representing the size of the uploaded file data as a sequence of decimal
 * digits (or an empty read in case of an upload error).  Once all digits are
 * consumed (eIO_Closed seen) the connection returns to READ mode.
 * CONN_Wait(eIO_Read) will also cause the upload to finalize -- still the data
 * size is expected to be extracted (or discarded if another command follows).
 *
 * Unfinalized uploads (such as when connection gets closed before the final
 * read) get reported to the log, and also make CONN_Close() to return an
 * error.  Note that unlike file download (which occurs in READ mode), it is
 * impossible to abort an upload by writing any FTP commands (since writing in
 * SEND mode goes to file), but it is reading that will cause the cancellation.
 * So if a connection is in undetermined state, the recovery would be to do a
 * small quick read (e.g. for just 1 byte with a small timeout), then write the
 * "NOOP" command and cause an execution (e.g. writing "NOOP\n" does that),
 * then drain the connection by reading again until eIO_Closed.
 *
 * Both downloads and uploads (but not file lists!) support restart mode (if
 * the server permits so).  The standard guarantees that the REST command
 * remains in effect only until any subsequent command (which is supposed to be
 * either RETR or STOR), and that servers might lose the restart position,
 * otherwise.  However, many implementations allow to open a data connection in
 * the interim.  Since the FTP connector opens data connection only upon
 * receiving a data transfer command from the user, it thus can clobber the
 * preceding REST for the servers that do not allow the extra activity.  For
 * those, the REST command can be delayed for issuance until right before the
 * data transfer is about to begin (see flags).  In this case, a write of such
 * command does not result in the "350" response on read (still,
 * CONN_Write()/CONN_Flush()/CONN_Status() will all be reported as successful
 * if the command was properly understood by the connector).
 *
 * The connector drops any restart position, which remains for longer than
 * the next user command (so the restart position will not be accidentally
 * taken into account for any further transfer size verifications).  Note
 * that only successful transfers are said to reset the restart position back
 * to 0 at the server end (failed ones might not do so), which is why it is a
 * sole responsibility of the user code to maintain/drop the restart position
 * on the server by issuing the REST commands explicitly, as appropriate.
 * Note that "REST 0" issued by the user code never gets delayed, and relayed
 * immediately to the server (with the result code "350" available for read
 * if succeeded on the server end).
 *
 * The supplement mode of CONN API can make use of FTP connection much easier:
 * instead of checking for CONN_Status(), direct return codes of read/write
 * operations can be used.  Care must be taken to interpret eIO_Closed that may
 * result from read operations (such as when extracting a numeric string of
 * command completion that is immediately followed by the response boundary
 * denoted as eIO_Closed).
 *
 * To make the code robust, it is always advised to process the tranfser byte
 * count first, and then to proceed with the return status analysis.
 */

/* Even though many FTP server implementations provide SIZE command these days,
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
 * code is expected to check, which command it is processing, before proceeding
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


/* Same as above but use fields provided by the connection structure.
 * Note:  info->timeout is only used for tunneling, not for FTP xfers */
extern NCBI_XCONNECT_EXPORT CONNECTOR FTP_CreateConnector
(const SConnNetInfo*  info,   /* all connection params including HTTP proxy  */
 TFTP_Flags           flag,   /* mostly for logging socket data [optional]   */
 const SFTP_Callback* cmcb    /* command callback [optional]                 */
);


#ifdef __cplusplus
}  /* extern "C" */
#endif


/* @} */

#endif /* CONNECT___NCBI_FTP_CONNECTOR__H */
