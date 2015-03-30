#ifndef NCBI_C_LOG_P__H
#define NCBI_C_LOG_P__H

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
 * Author:  Vladimir Ivanov
 *
 * @file
 * File Description:
 *    The C library to provide C++ Toolkit-like logging semantics 
 *    and output for C/C++ programs and CGIs.
 *    Private part (for internal use only).
 *
 */

#include <misc/clog/ncbi_c_log.h>


#ifdef __cplusplus
extern "C" {
#endif


/******************************************************************************
 *  Configurables that could theoretically change 
 */

#define NCBILOG_HOST_MAX       256
#define NCBILOG_CLIENT_MAX     256
#define NCBILOG_SESSION_MAX    256
#define NCBILOG_HITID_MAX      256
#define NCBILOG_APPNAME_MAX   1024
#define NCBILOG_PORT_MAX     65535


/******************************************************************************
 *  Internal types and definitions
 */

/** Application execution states shown in the std prefix
 */
typedef enum {
    eNcbiLog_NotSet = 0,     /**< Reserved value, never used in messages */
    eNcbiLog_AppBegin,       /**< PB  */
    eNcbiLog_AppRun,         /**< P   */
    eNcbiLog_AppEnd,         /**< PE  */
    eNcbiLog_RequestBegin,   /**< RB  */
    eNcbiLog_Request,        /**< R   */
    eNcbiLog_RequestEnd      /**< RE  */
} ENcbiLog_AppState;


/** Type of file for the output.
 *  Specialization for the file-based diagnostics.
 *  Splits output into four files: .err, .log, .trace, .perf.
 */
typedef enum {
    eDiag_Trace,             /**< .trace */
    eDiag_Err,               /**< .err   */
    eDiag_Log,               /**< .log   */
    eDiag_Perf               /**< .perf  */
} ENcbiLog_DiagFile;


/** Application access log and error postings info structure.
 */
struct STime_tag {
    time_t         sec;      /**< GMT time                    */
    unsigned long  ns;       /**< Nanosecond part of the time */
};
typedef struct STime_tag STime;


/** Application access log and error postings info structure (global).
 */
struct SInfo_tag {

    /* Posting data */

    TNcbiLog_PID      pid;                      /**< Process ID                                          */
    TNcbiLog_Counter  rid;                      /**< Request ID (e.g. iteration number in case of a CGI) */
    ENcbiLog_AppState state;                    /**< Application state                                   */
    TNcbiLog_Int8     guid;                     /**< Globally unique process ID                          */
    TNcbiLog_Counter  psn;                      /**< Serial number of the posting within the process     */
    STime             post_time;                /**< GMT time at which the message was posted, 
                                                     use current time if it is not specified (equal to 0)*/
    int/*bool*/       user_posting_time;        /**< If 1 use post_time as is, and never change it       */
    char              host[NCBILOG_HOST_MAX+1]; /**< Name of the host where the process runs 
                                                     (UNK_HOST if unknown)                               */
    char              appname[3*NCBILOG_APPNAME_MAX+1];
                                                /**< Name of the application (UNK_APP if unknown)        */
    char              client[NCBILOG_CLIENT_MAX+1];       
                                                /**< App-wide client IP address (UNK_CLIENT if unknown)  */
    char              session[3*NCBILOG_SESSION_MAX+1];    
                                                /**< App-wide session ID (UNK_SESSION if unknown)        */
    char*             message;                  /**< Buffer used to collect a message and log it         */

    /* Extras */

    char              phid[3*NCBILOG_HITID_MAX+1];    
                                                /**< App-wide hit ID (empty string if unknown)           */
    unsigned int      phid_sub_id;              /**< App-wide sub-hit ID counter                         */
    int/*bool*/       phid_inherit;             /**< 1 if PHID set explicitly and should be inherited (by requests) */
    const char*       host_role;                /**< Host role (NULL if unknown or not set)              */
    const char*       host_location;            /**< Host location (NULL if unknown or not set)          */

    /* Control parameters */
    
    int/*bool*/       remote_logging;           /**< 1 if logging request is going from remote host */
    ENcbiLog_Severity post_level;               /**< Posting level                                  */
    STime             app_start_time;           /**< Application start time                         */
    char*             app_full_name;            /**< Pointer to a full application name (argv[0])   */
    char*             app_base_name;            /**< Pointer to application base name               */

    /* Log file names and handles */

    ENcbiLog_Destination destination;           /**< Current logging destination            */
    unsigned int      server_port;              /**< $SERVER_PORT on calling/current host   */
    time_t            last_reopen_time;         /**< Last reopen time for log files         */
    FILE*             file_trace;               /**< Saved files for log files              */
    FILE*             file_err;
    FILE*             file_log;
    FILE*             file_perf;
    char*             file_trace_name;          /**< Saved file names for log files         */
    char*             file_err_name;
    char*             file_log_name;
    char*             file_perf_name;
    int/*bool*/       reuse_file_names;         /**< File names where not changed, reuse it */
};
typedef struct SInfo_tag TNcbiLog_Info;


/** Thread-specific context data.
 */
struct SContext_tag {
    TNcbiLog_TID      tid;                      /**< Thread  ID                                          */
    TNcbiLog_Counter  tsn;                      /**< Serial number of the posting within the thread      */
    TNcbiLog_Counter  rid;                      /**< Request ID (e.g. iteration number in case of a CGI) */
                                                /**< Local value, use global value if equal to 0         */
    ENcbiLog_AppState state;                    /**< Application state                                   */
                                                /**< Local value, use global value if eNcbiLog_NotSet    */
    char  client [NCBILOG_CLIENT_MAX+1];        /**< Request specific client IP address                  */
    int/*bool*/  is_client_set;                 /**< 1 if request 'client' has set, even to empty        */
    char  session[3*NCBILOG_SESSION_MAX+1];     /**< Request specific session ID                         */
    int/*bool*/  is_session_set;                /**< 1 if request 'session' has set, even to empty       */
    char  phid   [3*NCBILOG_HITID_MAX+1];       /**< Request specific hit ID                             */
    unsigned int phid_sub_id;                   /**< Request specific sub-hit ID counter                 */

    STime req_start_time;                       /**< Request start time                                  */
};
typedef struct SContext_tag TNcbiLog_Context_Data;



/******************************************************************************
 *  Logging setup functions --- for internal use only
 */

/* Can be used in ST mode only */
extern TNcbiLog_Info*   NcbiLogP_GetInfoPtr(void);
extern TNcbiLog_Context NcbiLogP_GetContextPtr(void);


/* Enable/disable internal checks (enabled by default) */
extern int /*bool*/ NcbiLogP_DisableChecks(int /*bool*/ disable);


/** Variant of NcbiLog_SetDestination. 
 *  Try to force set new destination without additional checks.
 *  The 'port' parameter redefine value of $SERVER_PORT environment variable.
 *  Use 0 if undefined. Can be used only for redirect mode. See for details:
 *  http://www.ncbi.nlm.nih.gov/toolkit/doc/book/ch_core/#ch_core.Where_Diagnostic_Messages_Go
 *  @sa NcbiLog_SetDestination, ENcbiLog_Destination, NcbiLog_Init
 */
extern ENcbiLog_Destination NcbiLogP_SetDestination(ENcbiLog_Destination ds, unsigned int port);


/** Redefine value of $SERVER_PORT environment variable.
 *  Can be used only for redirect mode.
 *  See this for details:
 *  http://www.ncbi.nlm.nih.gov/toolkit/doc/book/ch_core/#ch_core.Where_Diagnostic_Messages_Go
 *  @sa NcbiLog_SetDestination
 */
extern unsigned int NcbiLogP_SetServerPort(unsigned int server_port);


/** Variant of NcbiLog_ReqStart, that use already prepared string with
 *  parameters. Both, key and value in pairs 'key=value' should be 
 *  URL-encoded and separated with '&'.
 *  @sa NcbiLog_ReqStart
 */
extern void NcbiLogP_ReqStartStr(const char* params);


/** Variant of NcbiLog_Perf, that use already prepared string with
 *  parameters. Both, key and value in pairs 'key=value' should be 
 *  URL-encoded and separated with '&'.
 *  @sa NcbiLog_Perf
 */
extern void NcbiLogP_PerfStr(int status, double timespan, const char* params);


/** Variant of NcbiLog_Extra, that use already prepared string with
 *  parameters. Both, key and value in pairs 'key=value' should be 
 *  URL-encoded and separated with '&'.
 *  @sa NcbiLog_Extra
 */
extern void NcbiLogP_ExtraStr(const char* params);


/** Post already prepared line in applog format to the applog.
 *  It do not perform any additional check and don' change context as well,
 *  just write specified line to applog as is.
 */
extern void NcbiLogP_Raw(const char* line);


/** Get value of session ID from the environment.
 *  Return NULL if not found.
 */
extern const char* NcbiLogP_GetSessionID_Env(void);


/** Get value of hit ID from the environment.
 *  Return NULL if not found.
 */
extern const char* NcbiLogP_GetHitID_Env(void);



#ifdef __cplusplus
}  /* extern "C" */
#endif


#endif /* NCBI_C_LOG_P__H */
