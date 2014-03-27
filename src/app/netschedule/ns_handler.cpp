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
 * Authors:  Anatoliy Kuznetsov, Victor Joukov
 *
 * File Description: netschedule commands handler
 *
 */

#include <ncbi_pch.hpp>

#include <corelib/ncbi_system.hpp>

#include "ns_handler.hpp"
#include "ns_server.hpp"
#include "ns_server_misc.hpp"
#include "ns_rollback.hpp"
#include "queue_database.hpp"
#include "ns_application.hpp"

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>


USING_NCBI_SCOPE;



// NetSchedule command parser
//

CNetScheduleHandler::SCommandMap CNetScheduleHandler::sm_BatchHeaderMap[] = {
    { "BTCH", { &CNetScheduleHandler::x_ProcessBatchStart,
                eNS_Queue | eNS_Submitter | eNS_Program },
        { { "size", eNSPT_Int, eNSPA_Required } } },
    { "ENDS", { &CNetScheduleHandler::x_ProcessBatchSequenceEnd,
                eNS_Queue | eNS_Submitter | eNS_Program } },
    { NULL }
};

CNetScheduleHandler::SCommandMap CNetScheduleHandler::sm_BatchEndMap[] = {
    { "ENDB" },
    { NULL }
};

SNSProtoArgument s_BatchArgs[] = {
    { "input", eNSPT_Str, eNSPA_Required      },
    { "aff",   eNSPT_Str, eNSPA_Optional, ""  },
    { "msk",   eNSPT_Int, eNSPA_Optional, "0" },
    { NULL }
};

CNetScheduleHandler::SCommandMap CNetScheduleHandler::sm_CommandMap[] = {

    { "SHUTDOWN",      { &CNetScheduleHandler::x_ProcessShutdown,
                         eNS_Admin },
        { { "drain",             eNSPT_Int, eNSPA_Optional, "0" },
          { "ip",                eNSPT_Str, eNSPA_Optional, ""  },
          { "sid",               eNSPT_Str, eNSPA_Optional, ""  } } },
    { "GETCONF",       { &CNetScheduleHandler::x_ProcessGetConf,
                         eNS_Admin },
        { { "effective",         eNSPT_Int, eNSPA_Optional, "0" },
          { "ip",                eNSPT_Str, eNSPA_Optional, ""  },
          { "sid",               eNSPT_Str, eNSPA_Optional, ""  } } },
    { "REFUSESUBMITS", { &CNetScheduleHandler::x_ProcessRefuseSubmits,
                         eNS_Admin },
        { { "mode",              eNSPT_Int, eNSPA_Required      },
          { "ip",                eNSPT_Str, eNSPA_Optional, ""  },
          { "sid",               eNSPT_Str, eNSPA_Optional, ""  } } },
    { "QPAUSE",        { &CNetScheduleHandler::x_ProcessPause,
                         eNS_Queue },
        { { "pullback",          eNSPT_Int, eNSPA_Optional, "0" },
          { "ip",                eNSPT_Str, eNSPA_Optional, ""  },
          { "sid",               eNSPT_Str, eNSPA_Optional, ""  } } },
    { "QRESUME",       { &CNetScheduleHandler::x_ProcessResume,
                         eNS_Queue },
        { { "ip",                eNSPT_Str, eNSPA_Optional, ""  },
          { "sid",               eNSPT_Str, eNSPA_Optional, ""  } } },
    { "RECO",          { &CNetScheduleHandler::x_ProcessReloadConfig,
                         eNS_Admin },
        { { "ip",                eNSPT_Str, eNSPA_Optional, ""  },
          { "sid",               eNSPT_Str, eNSPA_Optional, ""  } } },
    { "VERSION",       { &CNetScheduleHandler::x_ProcessVersion,
                         eNS_NoChecks },
        { { "ip",                eNSPT_Str, eNSPA_Optional, ""  },
          { "sid",               eNSPT_Str, eNSPA_Optional, ""  } } },
    { "HEALTH",        { &CNetScheduleHandler::x_ProcessHealth,
                         eNS_NoChecks },
        { { "ip",                eNSPT_Str, eNSPA_Optional, ""  },
          { "sid",               eNSPT_Str, eNSPA_Optional, ""  } } },
    { "ACKALERT",      { &CNetScheduleHandler::x_ProcessAckAlert,
                         eNS_NoChecks },
        { { "alert",             eNSPT_Id,  eNSPA_Required      },
          { "user",              eNSPT_Str, eNSPA_Required      },
          { "ip",                eNSPT_Str, eNSPA_Optional, ""  },
          { "sid",               eNSPT_Str, eNSPA_Optional, ""  } } },
    { "QUIT",          { &CNetScheduleHandler::x_ProcessQuitSession,
                         eNS_NoChecks },
        { { "ip",                eNSPT_Str, eNSPA_Optional, ""  },
          { "sid",               eNSPT_Str, eNSPA_Optional, ""  } } },
    { "ACNT",          { &CNetScheduleHandler::x_ProcessActiveCount,
                         eNS_NoChecks },
        { { "ip",                eNSPT_Str, eNSPA_Optional, ""  },
          { "sid",               eNSPT_Str, eNSPA_Optional, ""  } } },
    { "QLST",          { &CNetScheduleHandler::x_ProcessQList,
                         eNS_NoChecks },
        { { "ip",                eNSPT_Str, eNSPA_Optional, ""  },
          { "sid",               eNSPT_Str, eNSPA_Optional, ""  } } },
    { "QINF",          { &CNetScheduleHandler::x_ProcessQueueInfo,
                         eNS_NoChecks },
        { { "qname",             eNSPT_Id,  eNSPA_Required      },
          { "ip",                eNSPT_Str, eNSPA_Optional, ""  },
          { "sid",               eNSPT_Str, eNSPA_Optional, ""  } } },
    { "QINF2",         { &CNetScheduleHandler::x_ProcessQueueInfo,
                         eNS_NoChecks },
        { { "qname",             eNSPT_Id,  eNSPA_Optional, ""  },
          { "service",           eNSPT_Id,  eNSPA_Optional, ""  },
          { "ip",                eNSPT_Str, eNSPA_Optional, ""  },
          { "sid",               eNSPT_Str, eNSPA_Optional, ""  } } },
    { "SETQUEUE",      { &CNetScheduleHandler::x_ProcessSetQueue,
                         eNS_NoChecks },
        { { "qname",             eNSPT_Id,  eNSPA_Optional      },
          { "ip",                eNSPT_Str, eNSPA_Optional, ""  },
          { "sid",               eNSPT_Str, eNSPA_Optional, ""  } } },
    { "DROPQ",         { &CNetScheduleHandler::x_ProcessDropQueue,
                         eNS_Queue | eNS_Submitter | eNS_Program },
        { { "ip",                eNSPT_Str, eNSPA_Optional, ""  },
          { "sid",               eNSPT_Str, eNSPA_Optional, ""  } } },
    /* QCRE checks 'program' and 'submit hosts' from the class */
    { "QCRE",          { &CNetScheduleHandler::x_ProcessCreateDynamicQueue,
                         eNS_NoChecks },
        { { "qname",             eNSPT_Id,  eNSPA_Required      },
          { "qclass",            eNSPT_Id,  eNSPA_Required      },
          { "description",       eNSPT_Str, eNSPA_Optional      },
          { "ip",                eNSPT_Str, eNSPA_Optional, ""  },
          { "sid",               eNSPT_Str, eNSPA_Optional, ""  } } },
    /* QDEL checks 'program' and 'submit hosts' from the queue it deletes */
    { "QDEL",          { &CNetScheduleHandler::x_ProcessDeleteDynamicQueue,
                         eNS_NoChecks },
        { { "qname",             eNSPT_Id, eNSPA_Required       },
          { "ip",                eNSPT_Str, eNSPA_Optional, ""  },
          { "sid",               eNSPT_Str, eNSPA_Optional, ""  } } },
    { "STATUS",        { &CNetScheduleHandler::x_ProcessStatus,
                         eNS_Queue },
        { { "job_key",           eNSPT_Id,  eNSPA_Required      },
          { "ip",                eNSPT_Str, eNSPA_Optional, ""  },
          { "sid",               eNSPT_Str, eNSPA_Optional, ""  } } },
    { "STATUS2",       { &CNetScheduleHandler::x_ProcessStatus,
                         eNS_Queue },
        { { "job_key",           eNSPT_Id,  eNSPA_Required      },
          { "ip",                eNSPT_Str, eNSPA_Optional, ""  },
          { "sid",               eNSPT_Str, eNSPA_Optional, ""  } } },
    /* The STAT commands makes sense with and without a queue */
    { "STAT",          { &CNetScheduleHandler::x_ProcessStatistics,
                         eNS_NoChecks },
        { { "option",            eNSPT_Id,  eNSPA_Optional      },
          { "comment",           eNSPT_Id,  eNSPA_Optional      },
          { "aff",               eNSPT_Str, eNSPA_Optional      },
          { "group",             eNSPT_Str, eNSPA_Optional      },
          { "ip",                eNSPT_Str, eNSPA_Optional, ""  },
          { "sid",               eNSPT_Str, eNSPA_Optional, ""  } } },
    { "MPUT",          { &CNetScheduleHandler::x_ProcessPutMessage,
                         eNS_Queue | eNS_Worker | eNS_Program },
        { { "job_key",           eNSPT_Id,  eNSPA_Required      },
          { "progress_msg",      eNSPT_Str, eNSPA_Required      },
          { "ip",                eNSPT_Str, eNSPA_Optional, ""  },
          { "sid",               eNSPT_Str, eNSPA_Optional, ""  } } },
    { "MGET",          { &CNetScheduleHandler::x_ProcessGetMessage,
                         eNS_Queue | eNS_Submitter | eNS_Program },
        { { "job_key",           eNSPT_Id,  eNSPA_Required      },
          { "ip",                eNSPT_Str, eNSPA_Optional, ""  },
          { "sid",               eNSPT_Str, eNSPA_Optional, ""  } } },
    { "DUMP",          { &CNetScheduleHandler::x_ProcessDump,
                         eNS_Queue },
        { { "job_key",           eNSPT_Id,  eNSPA_Optional      },
          { "status",            eNSPT_Id,  eNSPA_Optional      },
          { "start_after",       eNSPT_Id,  eNSPA_Optional      },
          { "count",             eNSPT_Int, eNSPA_Optional, "0" },
          { "group",             eNSPT_Str, eNSPA_Optional      },
          { "ip",                eNSPT_Str, eNSPA_Optional, ""  },
          { "sid",               eNSPT_Str, eNSPA_Optional, ""  } } },
    { "GETP",          { &CNetScheduleHandler::x_ProcessGetParam,
                         eNS_Queue },
        { { "ip",                eNSPT_Str, eNSPA_Optional, ""  },
          { "sid",               eNSPT_Str, eNSPA_Optional, ""  } } },
    { "GETP2",         { &CNetScheduleHandler::x_ProcessGetParam,
                         eNS_Queue },
        { { "ip",                eNSPT_Str, eNSPA_Optional, ""  },
          { "sid",               eNSPT_Str, eNSPA_Optional, ""  } } },
    { "GETC",          { &CNetScheduleHandler::x_ProcessGetConfiguration,
                         eNS_Queue },
        { { "ip",                eNSPT_Str, eNSPA_Optional, ""  },
          { "sid",               eNSPT_Str, eNSPA_Optional, ""  } } },
    { "CLRN",          { &CNetScheduleHandler::x_ProcessClearWorkerNode,
                         eNS_Queue | eNS_Worker | eNS_Program },
        { { "ip",                eNSPT_Str, eNSPA_Optional, ""  },
          { "sid",               eNSPT_Str, eNSPA_Optional, ""  } } },
    { "CANCELQ",       { &CNetScheduleHandler::x_ProcessCancelQueue,
                         eNS_Queue | eNS_Submitter | eNS_Program },
        { { "ip",                eNSPT_Str, eNSPA_Optional, ""  },
          { "sid",               eNSPT_Str, eNSPA_Optional, ""  } } },
    { "SST",           { &CNetScheduleHandler::x_ProcessFastStatusS,
                         eNS_Queue | eNS_Submitter | eNS_Program },
        { { "job_key",           eNSPT_Id,  eNSPA_Required      },
          { "ip",                eNSPT_Str, eNSPA_Optional, ""  },
          { "sid",               eNSPT_Str, eNSPA_Optional, ""  } } },
    { "SST2",          { &CNetScheduleHandler::x_ProcessFastStatusS,
                         eNS_Queue | eNS_Submitter | eNS_Program },
        { { "job_key",           eNSPT_Id,  eNSPA_Required      },
          { "ip",                eNSPT_Str, eNSPA_Optional, ""  },
          { "sid",               eNSPT_Str, eNSPA_Optional, ""  } } },
    { "SUBMIT",        { &CNetScheduleHandler::x_ProcessSubmit,
                         eNS_Queue | eNS_Submitter | eNS_Program },
        { { "input",             eNSPT_Str, eNSPA_Required      },
          { "port",              eNSPT_Int, eNSPA_Optional      },
          { "timeout",           eNSPT_Int, eNSPA_Optional      },
          { "aff",               eNSPT_Str, eNSPA_Optional, ""  },
          { "msk",               eNSPT_Int, eNSPA_Optional, "0" },
          { "ip",                eNSPT_Str, eNSPA_Optional, ""  },
          { "sid",               eNSPT_Str, eNSPA_Optional, ""  },
          { "group",             eNSPT_Str, eNSPA_Optional, ""  } } },
    { "CANCEL",        { &CNetScheduleHandler::x_ProcessCancel,
                         eNS_Queue | eNS_Submitter | eNS_Program },
        { { "job_key",           eNSPT_Id,  eNSPA_Optional      },
          { "group",             eNSPT_Str, eNSPA_Optional      },
          { "ip",                eNSPT_Str, eNSPA_Optional, ""  },
          { "sid",               eNSPT_Str, eNSPA_Optional, ""  } } },
    { "LISTEN",        { &CNetScheduleHandler::x_ProcessListenJob,
                         eNS_Queue | eNS_Submitter | eNS_Program },
        { { "job_key",           eNSPT_Id,  eNSPA_Required      },
          { "port",              eNSPT_Int, eNSPA_Required      },
          { "timeout",           eNSPT_Int, eNSPA_Required      },
          { "ip",                eNSPT_Str, eNSPA_Optional, ""  },
          { "sid",               eNSPT_Str, eNSPA_Optional, ""  } } },
    { "BSUB",          { &CNetScheduleHandler::x_ProcessSubmitBatch,
                         eNS_Queue | eNS_Submitter | eNS_Program },
        { { "port",              eNSPT_Int, eNSPA_Optional      },
          { "timeout",           eNSPT_Int, eNSPA_Optional      },
          { "ip",                eNSPT_Str, eNSPA_Optional, ""  },
          { "sid",               eNSPT_Str, eNSPA_Optional, ""  },
          { "group",             eNSPT_Str, eNSPA_Optional, ""  } } },
    { "READ",          { &CNetScheduleHandler::x_ProcessReading,
                         eNS_Queue | eNS_Submitter | eNS_Program },
        { { "timeout",           eNSPT_Int, eNSPA_Optional, "0" },
          { "group",             eNSPT_Str, eNSPA_Optional, ""  },
          { "ip",                eNSPT_Str, eNSPA_Optional, ""  },
          { "sid",               eNSPT_Str, eNSPA_Optional, ""  } } },
    { "CFRM",          { &CNetScheduleHandler::x_ProcessConfirm,
                         eNS_Queue | eNS_Submitter | eNS_Program },
        { { "job_key",           eNSPT_Id,  eNSPA_Required      },
          { "auth_token",        eNSPT_Id,  eNSPA_Required      },
          { "ip",                eNSPT_Str, eNSPA_Optional, ""  },
          { "sid",               eNSPT_Str, eNSPA_Optional, ""  } } },
    { "FRED",          { &CNetScheduleHandler::x_ProcessReadFailed,
                         eNS_Queue | eNS_Submitter | eNS_Program },
        { { "job_key",           eNSPT_Id,  eNSPA_Required      },
          { "auth_token",        eNSPT_Id,  eNSPA_Required      },
          { "err_msg",           eNSPT_Str, eNSPA_Optional      },
          { "ip",                eNSPT_Str, eNSPA_Optional, ""  },
          { "sid",               eNSPT_Str, eNSPA_Optional, ""  } } },
    { "RDRB",          { &CNetScheduleHandler::x_ProcessReadRollback,
                         eNS_Queue | eNS_Submitter | eNS_Program },
        { { "job_key",           eNSPT_Id,  eNSPA_Required      },
          { "auth_token",        eNSPT_Str, eNSPA_Required      },
          { "ip",                eNSPT_Str, eNSPA_Optional, ""  },
          { "sid",               eNSPT_Str, eNSPA_Optional, ""  } } },
    { "WST",           { &CNetScheduleHandler::x_ProcessFastStatusW,
                         eNS_Queue },
        { { "job_key",           eNSPT_Id,  eNSPA_Required      },
          { "ip",                eNSPT_Str, eNSPA_Optional, ""  },
          { "sid",               eNSPT_Str, eNSPA_Optional, ""  } } },
    { "WST2",          { &CNetScheduleHandler::x_ProcessFastStatusW,
                         eNS_Queue },
        { { "job_key",           eNSPT_Id,  eNSPA_Required      },
          { "ip",                eNSPT_Str, eNSPA_Optional, ""  },
          { "sid",               eNSPT_Str, eNSPA_Optional, ""  } } },
    { "CHAFF",         { &CNetScheduleHandler::x_ProcessChangeAffinity,
                         eNS_Queue | eNS_Worker | eNS_Program },
        { { "add",               eNSPT_Str, eNSPA_Optional, ""  },
          { "del",               eNSPT_Str, eNSPA_Optional, ""  },
          { "ip",                eNSPT_Str, eNSPA_Optional, ""  },
          { "sid",               eNSPT_Str, eNSPA_Optional, ""  } } },
    { "SETAFF",        { &CNetScheduleHandler::x_ProcessSetAffinity,
                         eNS_Queue | eNS_Worker | eNS_Program },
        { { "aff",               eNSPT_Str, eNSPA_Optional, ""  },
          { "ip",                eNSPT_Str, eNSPA_Optional, ""  },
          { "sid",               eNSPT_Str, eNSPA_Optional, ""  } } },
    { "GET",           { &CNetScheduleHandler::x_ProcessGetJob,
                         eNS_Queue | eNS_Worker | eNS_Program },
        { { "port",              eNSPT_Id,  eNSPA_Optional      },
          { "aff",               eNSPT_Str, eNSPA_Optional, ""  },
          { "ip",                eNSPT_Str, eNSPA_Optional, ""  },
          { "sid",               eNSPT_Str, eNSPA_Optional, ""  } } },
    { "GET2",          { &CNetScheduleHandler::x_ProcessGetJob,
                         eNS_Queue | eNS_Worker | eNS_Program },
        { { "wnode_aff",         eNSPT_Int, eNSPA_Required, "0" },
          { "any_aff",           eNSPT_Int, eNSPA_Required, "0" },
          { "exclusive_new_aff", eNSPT_Int, eNSPA_Optional, "0" },
          { "aff",               eNSPT_Str, eNSPA_Optional, ""  },
          { "port",              eNSPT_Int, eNSPA_Optional      },
          { "timeout",           eNSPT_Int, eNSPA_Optional      },
          { "ip",                eNSPT_Str, eNSPA_Optional, ""  },
          { "sid",               eNSPT_Str, eNSPA_Optional, ""  } } },
    { "PUT",           { &CNetScheduleHandler::x_ProcessPut,
                         eNS_Queue | eNS_Worker | eNS_Program },
        { { "job_key",           eNSPT_Id,  eNSPA_Required      },
          { "job_return_code",   eNSPT_Id,  eNSPA_Required      },
          { "output",            eNSPT_Str, eNSPA_Required      },
          { "ip",                eNSPT_Str, eNSPA_Optional, ""  },
          { "sid",               eNSPT_Str, eNSPA_Optional, ""  } } },
    { "PUT2",          { &CNetScheduleHandler::x_ProcessPut,
                         eNS_Queue | eNS_Worker | eNS_Program },
        { { "job_key",           eNSPT_Id,  eNSPA_Required      },
          { "auth_token",        eNSPT_Id,  eNSPA_Required      },
          { "job_return_code",   eNSPT_Id,  eNSPA_Required      },
          { "output",            eNSPT_Str, eNSPA_Required      },
          { "ip",                eNSPT_Str, eNSPA_Optional, ""  },
          { "sid",               eNSPT_Str, eNSPA_Optional, ""  } } },
    { "RETURN",        { &CNetScheduleHandler::x_ProcessReturn,
                         eNS_Queue | eNS_Worker | eNS_Program },
        { { "job_key",           eNSPT_Id,  eNSPA_Required      },
          { "ip",                eNSPT_Str, eNSPA_Optional, ""  },
          { "sid",               eNSPT_Str, eNSPA_Optional, ""  } } },
    { "RETURN2",       { &CNetScheduleHandler::x_ProcessReturn,
                         eNS_Queue | eNS_Worker | eNS_Program },
        { { "job_key",           eNSPT_Id,  eNSPA_Required      },
          { "auth_token",        eNSPT_Id,  eNSPA_Required      },
          { "blacklist",         eNSPT_Int, eNSPA_Optional, "1" },
          { "ip",                eNSPT_Str, eNSPA_Optional, ""  },
          { "sid",               eNSPT_Str, eNSPA_Optional, ""  } } },
    { "WGET",          { &CNetScheduleHandler::x_ProcessGetJob,
                         eNS_Queue | eNS_Worker | eNS_Program },
        { { "port",              eNSPT_Int, eNSPA_Required      },
          { "timeout",           eNSPT_Int, eNSPA_Required      },
          { "aff",               eNSPT_Str, eNSPA_Optional, ""  },
          { "ip",                eNSPT_Str, eNSPA_Optional, ""  },
          { "sid",               eNSPT_Str, eNSPA_Optional, ""  } } },
    { "CWGET",         { &CNetScheduleHandler::x_ProcessCancelWaitGet,
                         eNS_Queue | eNS_Worker | eNS_Program },
        { { "ip",                eNSPT_Str, eNSPA_Optional, ""  },
          { "sid",               eNSPT_Str, eNSPA_Optional, ""  } } },
    { "FPUT",          { &CNetScheduleHandler::x_ProcessPutFailure,
                         eNS_Queue | eNS_Worker | eNS_Program },
        { { "job_key",           eNSPT_Id,  eNSPA_Required      },
          { "err_msg",           eNSPT_Str, eNSPA_Required      },
          { "output",            eNSPT_Str, eNSPA_Required      },
          { "job_return_code",   eNSPT_Int, eNSPA_Required      },
          { "ip",                eNSPT_Str, eNSPA_Optional, ""  },
          { "sid",               eNSPT_Str, eNSPA_Optional, ""  } } },
    { "FPUT2",         { &CNetScheduleHandler::x_ProcessPutFailure,
                         eNS_Queue | eNS_Worker | eNS_Program },
        { { "job_key",           eNSPT_Id,  eNSPA_Required      },
          { "auth_token",        eNSPT_Id,  eNSPA_Required      },
          { "err_msg",           eNSPT_Str, eNSPA_Required      },
          { "output",            eNSPT_Str, eNSPA_Required      },
          { "job_return_code",   eNSPT_Int, eNSPA_Required      },
          { "ip",                eNSPT_Str, eNSPA_Optional, ""  },
          { "sid",               eNSPT_Str, eNSPA_Optional, ""  } } },
    { "JXCG",          { &CNetScheduleHandler::x_ProcessJobExchange,
                         eNS_Queue | eNS_Worker | eNS_Program },
        { { "job_key",           eNSPT_Id,  eNSPA_Optchain      },
          { "job_return_code",   eNSPT_Int, eNSPA_Optchain      },
          { "output",            eNSPT_Str, eNSPA_Optional      },
          { "aff",               eNSPT_Str, eNSPA_Optional, ""  },
          { "ip",                eNSPT_Str, eNSPA_Optional, ""  },
          { "sid",               eNSPT_Str, eNSPA_Optional, ""  } } },
    { "JDEX",          { &CNetScheduleHandler::x_ProcessJobDelayExpiration,
                         eNS_Queue | eNS_Worker | eNS_Program },
        { { "job_key",           eNSPT_Id,  eNSPA_Required      },
          { "timeout",           eNSPT_Int, eNSPA_Required      },
          { "ip",                eNSPT_Str, eNSPA_Optional, ""  },
          { "sid",               eNSPT_Str, eNSPA_Optional, ""  } } },
    { "AFLS",          { &CNetScheduleHandler::x_ProcessGetAffinityList,
                         eNS_Queue },
        { { "ip",                eNSPT_Str, eNSPA_Optional, ""  },
          { "sid",               eNSPT_Str, eNSPA_Optional, ""  } } },

    // Obsolete commands
    { "REGC",          { &CNetScheduleHandler::x_CmdObsolete,
                         eNS_NoChecks } },
    { "URGC",          { &CNetScheduleHandler::x_CmdObsolete,
                         eNS_NoChecks } },
    { "INIT",          { &CNetScheduleHandler::x_CmdObsolete,
                         eNS_NoChecks } },
    { "JRTO",          { &CNetScheduleHandler::x_CmdNotImplemented,
                         eNS_NoChecks } },
    { NULL },
};


static SNSProtoArgument s_AuthArgs[] = {
    { "client", eNSPT_Str, eNSPA_Optional, "Unknown client" },
    { "params", eNSPT_Str, eNSPA_Ellipsis },
    { NULL }
};



static size_t s_BufReadHelper(void* data, const void* ptr, size_t size)
{
    ((string*) data)->append((const char *) ptr, size);
    return size;
}


static void s_ReadBufToString(BUF buf, string& str)
{
    size_t      size = BUF_Size(buf);

    str.erase();
    str.reserve(size);

    BUF_PeekAtCB(buf, 0, s_BufReadHelper, &str, size);
    BUF_Read(buf, NULL, size);
}


CNetScheduleHandler::CNetScheduleHandler(CNetScheduleServer* server)
    : m_MsgBufferSize(kInitialMessageBufferSize),
      m_MsgBuffer(new char[kInitialMessageBufferSize]),
      m_Server(server),
      m_BatchSize(0),
      m_BatchPos(0),
      m_WithinBatchSubmit(false),
      m_SingleCmdParser(sm_CommandMap),
      m_BatchHeaderParser(sm_BatchHeaderMap),
      m_BatchEndParser(sm_BatchEndMap),
      m_RollbackAction(NULL)
{}


CNetScheduleHandler::~CNetScheduleHandler()
{
    x_ClearRollbackAction();
    delete [] m_MsgBuffer;
}


void CNetScheduleHandler::OnOpen(void)
{
    CSocket &       socket = GetSocket();
    STimeout        to = {m_Server->GetInactivityTimeout(), 0};

    socket.DisableOSSendDelay();
    socket.SetTimeout(eIO_ReadWrite, &to);
    x_SetQuickAcknowledge();

    m_ProcessMessage = &CNetScheduleHandler::x_ProcessMsgAuth;

    // Log the fact of opened connection if needed
    if (m_Server->IsLog())
        x_CreateConnContext();
}


void CNetScheduleHandler::OnWrite()
{}


void CNetScheduleHandler::OnClose(IServer_ConnectionHandler::EClosePeer peer)
{
    if (m_WithinBatchSubmit) {
        m_WithinBatchSubmit = false;
        m_Server->DecrementCurrentSubmitsCounter();
    }

    // It's possible that this method will be called before OnOpen - when
    // connection is just created and server is shutting down. In this case
    // OnOpen will never be called.
    //
    // m_ConnContext != NULL also tells that the logging is required
    if (m_ConnContext.IsNull())
        return;

    switch (peer)
    {
    case IServer_ConnectionHandler::eOurClose:
        if (m_CmdContext.NotNull()) {
            m_ConnContext->SetRequestStatus(m_CmdContext->GetRequestStatus());
        } else {
            int status = m_ConnContext->GetRequestStatus();
            if (status != eStatus_HTTPProbe &&
                status != eStatus_BadRequest &&
                status != eStatus_SocketIOError)
                m_ConnContext->SetRequestStatus(eStatus_Inactive);
        }
        break;
    case IServer_ConnectionHandler::eClientClose:
        if (m_CmdContext.NotNull()) {
            m_CmdContext->SetRequestStatus(eStatus_SocketIOError);
            m_ConnContext->SetRequestStatus(eStatus_SocketIOError);
        }
        break;
    }


    // If a command has not finished its logging by some reasons - do it
    // here as the last chance.
    if (m_CmdContext.NotNull()) {
        CDiagContext::SetRequestContext(m_CmdContext);
        GetDiagContext().PrintRequestStop();
        m_CmdContext.Reset();
    }

    CSocket&        socket = GetSocket();
    CDiagContext::SetRequestContext(m_ConnContext);
    m_ConnContext->SetBytesRd(socket.GetCount(eIO_Read));
    m_ConnContext->SetBytesWr(socket.GetCount(eIO_Write));
    GetDiagContext().PrintRequestStop();

    m_ConnContext.Reset();
    CDiagContext::SetRequestContext(NULL);
}


void CNetScheduleHandler::OnTimeout()
{
    if (m_ConnContext.NotNull())
        m_ConnContext->SetRequestStatus(eStatus_Inactive);
}


void CNetScheduleHandler::OnOverflow(EOverflowReason reason)
{
    switch (reason) {
    case eOR_ConnectionPoolFull:
        ERR_POST("eCommunicationError:Connection pool full");
        break;
    case eOR_UnpollableSocket:
        ERR_POST("eCommunicationError:Unpollable connection");
        break;
    case eOR_RequestQueueFull:
        ERR_POST("eCommunicationError:Request queue full");
        break;
    default:
        ERR_POST("eCommunicationError:Unknown overflow error");
        break;
    }
}


void CNetScheduleHandler::OnMessage(BUF buffer)
{
    // Initialize the proper diagnostics context
    if (m_CmdContext.NotNull())
        CDiagContext::SetRequestContext(m_CmdContext);
    else if (m_ConnContext.NotNull())
        CDiagContext::SetRequestContext(m_ConnContext);

    if (m_Server->ShutdownRequested()) {
        ERR_POST("NetSchedule is shutting down. Client input rejected.");
        x_SetCmdRequestStatus(eStatus_ShuttingDown);
        if (x_WriteMessage("ERR:eShuttingDown:NetSchedule server "
                           "is shutting down. Session aborted.") == eIO_Success)
            m_Server->CloseConnection(&GetSocket());
        return;
    }

    bool          error = true;
    string        error_client_message;
    unsigned int  error_code;

    try {
        // Single line user input processor
        (this->*m_ProcessMessage)(buffer);
        error = false;
    }
    catch (const CNetScheduleException &  ex) {
        if (ex.GetErrCode() == CNetScheduleException::eAuthenticationError) {
            ERR_POST(ex);
            if (m_CmdContext.NotNull())
                m_CmdContext->SetRequestStatus(ex.ErrCodeToHTTPStatusCode());
            else if (m_ConnContext.NotNull())
                m_ConnContext->SetRequestStatus(ex.ErrCodeToHTTPStatusCode());

            if (x_WriteMessage("ERR:" + string(ex.GetErrCodeString()) +
                               ":" + ex.GetMsg()) == eIO_Success)
                m_Server->CloseConnection(&GetSocket());
            return;
        }
        ERR_POST(ex);
        error_client_message = "ERR:" + string(ex.GetErrCodeString()) +
                               ":" + ex.GetMsg();
        error_code = ex.ErrCodeToHTTPStatusCode();
    }
    catch (const CNSProtoParserException &  ex) {
        ERR_POST(ex);
        error_client_message = "ERR:" + string(ex.GetErrCodeString()) +
                               ":" + ex.GetMsg();
        error_code = eStatus_BadRequest;
    }
    catch (const CNetServiceException &  ex) {
        ERR_POST(ex);
        error_client_message = "ERR:" + string(ex.GetErrCodeString()) +
                               ":" + ex.GetMsg();
        if (ex.GetErrCode() == CNetServiceException::eCommunicationError)
            error_code = eStatus_SocketIOError;
        else
            error_code = eStatus_ServerError;
    }
    catch (const CBDB_ErrnoException &  ex) {
        ERR_POST(ex);
        if (ex.IsRecovery()) {
            error_client_message = "ERR:eInternalError:" +
                    NStr::PrintableString("Fatal Berkeley DB error "
                                          "(DB_RUNRECOVERY). Emergency "
                                          "shutdown initiated. " +
                                          string(ex.what()));
            m_Server->SetShutdownFlag();
        }
        else
            error_client_message = "ERR:eInternalError:" +
                    NStr::PrintableString("Internal database error - " +
                                          string(ex.what()));
        error_code = eStatus_ServerError;
    }
    catch (const CBDB_Exception &  ex) {
        ERR_POST(ex);
        error_client_message = "ERR:" +
                               NStr::PrintableString("eInternalError:Internal "
                               "database (BDB) error - " + string(ex.what()));
        error_code = eStatus_ServerError;
    }
    catch (const exception &  ex) {
        ERR_POST("STL exception: " << ex.what());
        error_client_message = "ERR:" +
                               NStr::PrintableString("eInternalError:Internal "
                               "error - " + string(ex.what()));
        error_code = eStatus_ServerError;
    }
    catch (...) {
        ERR_POST("ERR:Unknown server exception.");
        error_client_message = "ERR:eInternalError:Unknown server exception.";
        error_code = eStatus_ServerError;
    }

    if (error) {
        x_SetCmdRequestStatus(error_code);
        x_WriteMessage(error_client_message);
        x_PrintCmdRequestStop();
    }
}


void CNetScheduleHandler::x_SetQuickAcknowledge(void)
{
    int     fd = 0;
    int     val = 1;

    GetSocket().GetOSHandle(&fd, sizeof(fd));
    setsockopt(fd, IPPROTO_TCP, TCP_QUICKACK, &val, sizeof(val));
}


EIO_Status CNetScheduleHandler::x_WriteMessage(CTempString msg)
{
    size_t  msg_size = msg.size();
    while (msg_size >= 1 && msg[msg_size-1] == '\n')
        --msg_size;
    size_t  required_size = msg_size + 1;

    if (required_size > m_MsgBufferSize) {
        delete [] m_MsgBuffer;
        while (required_size > m_MsgBufferSize)
            m_MsgBufferSize += kMessageBufferIncrement;
        m_MsgBuffer = new char[m_MsgBufferSize];
    }

    memcpy(m_MsgBuffer, msg.data(), msg_size);
    m_MsgBuffer[required_size-1] = '\n';

    // Write to the socket as a single transaction
    size_t     written;
    EIO_Status result = GetSocket().Write(m_MsgBuffer, required_size, &written);
    if (result == eIO_Success)
        return result;

    // There was an error of writing into a socket, so rollback the action if
    // necessary and close the connection

    string     report =
        "Error writing message to the client. "
        "Peer: " +  GetSocket().GetPeerAddress() + ". "
        "Socket write error status: " + IO_StatusStr(result) + ". "
        "Written bytes: " + NStr::NumericToString(written) + ". "
        "Message begins with: ";
    if (msg_size > 32)
        report += string(m_MsgBuffer, 32) + " (TRUNCATED)";
    else {
        m_MsgBuffer[required_size-1] = '\0';
        report += m_MsgBuffer;
    }
    ERR_POST(report);

    if (m_ConnContext.NotNull()) {
        if (m_ConnContext->GetRequestStatus() == eStatus_OK)
            m_ConnContext->SetRequestStatus(eStatus_SocketIOError);
        if (m_CmdContext.NotNull()) {
            if (m_CmdContext->GetRequestStatus() == eStatus_OK)
                m_CmdContext->SetRequestStatus(eStatus_SocketIOError);
        }
    }

    try {
        if (!m_QueueName.empty()) {
            CRef<CQueue>    ref = GetQueue();
            ref->RegisterSocketWriteError(m_ClientId);
            x_ExecuteRollbackAction(ref.GetPointer());
        }
    } catch (...) {}

    m_Server->CloseConnection(&GetSocket());
    return result;
}


unsigned int CNetScheduleHandler::x_GetPeerAddress(void)
{
    unsigned int        peer_addr;

    GetSocket().GetPeerAddress(&peer_addr, 0, eNH_NetworkByteOrder);

    // always use localhost(127.0*) address for clients coming from
    // the same net address (sometimes it can be 127.* or full address)
    if (peer_addr == m_Server->GetHostNetAddr())
        return CSocketAPI::GetLoopbackAddress();
    return peer_addr;
}


void CNetScheduleHandler::x_ProcessMsgAuth(BUF buffer)
{
    // This should only memorize the received string.
    // The x_ProcessMsgQueue(...)  will parse it.
    // This is done to avoid copying parsed parameters and to have exactly one
    // diagnostics extra with both auth parameters and queue name.
    s_ReadBufToString(buffer, m_RawAuthString);

    // Check if it was systems probe...
    if (strncmp(m_RawAuthString.c_str(), "GET / HTTP/1.", 13) == 0) {
        // That was systems probing ports

        x_SetConnRequestStatus(eStatus_HTTPProbe);
        m_Server->CloseConnection(&GetSocket());
        return;
    }

    m_ProcessMessage = &CNetScheduleHandler::x_ProcessMsgQueue;
    x_SetQuickAcknowledge();
}


void CNetScheduleHandler::x_ProcessMsgQueue(BUF buffer)
{
    s_ReadBufToString(buffer, m_QueueName);

    // Parse saved raw authorization string and produce log output
    TNSProtoParams      params;

    try {
        m_SingleCmdParser.ParseArguments(m_RawAuthString, s_AuthArgs, &params);
    }
    catch (CNSProtoParserException &  ex) {
        string msg = "Error authenticating client: '";
        if (m_RawAuthString.size() > 128)
            msg += string(m_RawAuthString.c_str(), 128) + " (TRUNCATED)";
        else
            msg += m_RawAuthString;
        msg += "': ";

        // This will form request context with the client IP etc.
        if (m_ConnContext.IsNull())
            x_CreateConnContext();

        // ex.what() is here to avoid unnecessery records in the log
        // if it is simple 'ex' -> 2 records are produced
        ERR_POST(msg << ex.what());

        x_SetConnRequestStatus(eStatus_BadRequest);
        m_Server->CloseConnection(&GetSocket());
        return;
    }

    // Memorize what we know about the client
    m_ClientId.Update(this->x_GetPeerAddress(), params);


    // Test if it is an administrative user and memorize it
    if (m_Server->AdminHostValid(m_ClientId.GetAddress()) &&
        m_Server->IsAdminClientName(m_ClientId.GetClientName()))
        m_ClientId.SetPassedChecks(eNS_Admin);

    // Produce the log output if required
    if (m_ConnContext.NotNull()) {
        CDiagContext_Extra diag_extra = GetDiagContext().Extra();
        diag_extra.Print("queue", m_QueueName);
        ITERATE(TNSProtoParams, it, params) {
            diag_extra.Print(it->first, it->second);
        }
    }

    // Empty queue name is a synonim for hardcoded 'noname'.
    // To have exactly one string comparison, make the name empty if 'noname'
    if (m_QueueName == "noname" )
        m_QueueName = "";

    if (!m_QueueName.empty()) {
        CRef<CQueue>    queue;
        try {
            queue = m_Server->OpenQueue(m_QueueName);
            m_QueueRef.Reset(queue);
        }
        catch (const CNetScheduleException &  ex) {
            if (ex.GetErrCode() == CNetScheduleException::eUnknownQueue) {
                // This will form request context with the client IP etc.
                if (m_ConnContext.IsNull())
                    x_CreateConnContext();

                ERR_POST(ex);
                x_SetConnRequestStatus(ex.ErrCodeToHTTPStatusCode());
                if (x_WriteMessage("ERR:" + string(ex.GetErrCodeString()) +
                                   ":" + ex.GetMsg()) == eIO_Success)
                    m_Server->CloseConnection(&GetSocket());
                return;
            }
            throw;
        }

        x_UpdateClientPassedChecks(queue.GetPointer());
    }
    else
        m_QueueRef.Reset(NULL);

    m_ProcessMessage = &CNetScheduleHandler::x_ProcessMsgRequest;
    x_SetQuickAcknowledge();
}


// Workhorse method
void CNetScheduleHandler::x_ProcessMsgRequest(BUF buffer)
{
    if (m_ConnContext.NotNull()) {
        m_CmdContext.Reset(new CRequestContext());
        m_CmdContext->SetRequestStatus(eStatus_OK);
        m_CmdContext->SetRequestID();
        CDiagContext::SetRequestContext(m_CmdContext);
    }

    SParsedCmd      cmd;
    string          msg;
    try {
        s_ReadBufToString(buffer, msg);
        cmd = m_SingleCmdParser.ParseCommand(msg);

        // It throws an exception if the input is not valid
        m_CommandArguments.AssignValues(cmd.params,
                                        cmd.command->cmd,
                                        GetSocket(),
                                        m_Server->GetCompoundIDPool());

        x_PrintCmdRequestStart(cmd);
    }
    catch (const CNSProtoParserException &  ex) {
        // This is the exception from m_SingleCmdParser.ParseCommand(msg)

        // Parsing is done before PrintRequestStart(...) so a diag context is
        // not created here - create it just to provide an error output
        x_OnCmdParserError(true, ex.GetMsg(), "");
        return;
    }
    catch (const CNetScheduleException &  ex) {
        // This is an exception from AssignValues(...)

        // That is, the print request has not been printed yet
        x_PrintCmdRequestStart(cmd);
        throw;
    }

    const SCommandExtra &   extra = cmd.command->extra;

    if (extra.processor == &CNetScheduleHandler::x_ProcessQuitSession) {
        x_ProcessQuitSession(0);
        return;
    }


    // If the command requires queue, hold a hard reference to this
    // queue from a weak one (m_Queue) in queue_ref, and take C pointer
    // into queue_ptr. Otherwise queue_ptr is NULL, which is OK for
    // commands which does not require a queue.
    CRef<CQueue>        queue_ref;
    CQueue *            queue_ptr = NULL;

    bool                restore_client = false;
    TNSCommandChecks    orig_client_passed_checks = 0;
    unsigned int        orig_client_id = 0;

    if (extra.checks & eNS_Queue) {
        if (x_CanBeWithoutQueue(extra.processor) &&
            !m_CommandArguments.queue_from_job_key.empty()) {
            // This command must use queue name from the job key
            queue_ref.Reset(m_Server->OpenQueue(m_CommandArguments.queue_from_job_key));
            queue_ptr = queue_ref.GetPointer();

            if (m_QueueName != m_CommandArguments.queue_from_job_key) {
                orig_client_passed_checks = m_ClientId.GetPassedChecks();
                orig_client_id = m_ClientId.GetID();
                restore_client = true;

                x_UpdateClientPassedChecks(queue_ptr);
            }
        } else {
            // Old fasion way - the queue comes from handshake
            if (m_QueueName.empty())
                NCBI_THROW(CNetScheduleException, eUnknownQueue,
                           "Job queue is required");
            queue_ref.Reset(GetQueue());
            queue_ptr = queue_ref.GetPointer();
        }
    }
    else if (extra.processor == &CNetScheduleHandler::x_ProcessStatistics ||
             extra.processor == &CNetScheduleHandler::x_ProcessRefuseSubmits) {
        if (!m_QueueName.empty()) {
            // The STAT and REFUSESUBMITS commands
            // could be with or without a queue
            queue_ref.Reset(GetQueue());
            queue_ptr = queue_ref.GetPointer();
        }
    }

    m_ClientId.CheckAccess(extra.checks, m_Server);

    if (queue_ptr) {
        bool        client_was_found = false;
        bool        session_was_reset = false;
        string      old_session;
        bool        pref_affs_were_reset = false;

        // The cient has a queue, so memorize the client
        queue_ptr->TouchClientsRegistry(m_ClientId, client_was_found,
                                        session_was_reset, old_session,
                                        pref_affs_were_reset);
        if (client_was_found && session_was_reset) {
            if (m_ConnContext.NotNull()) {
                if (pref_affs_were_reset) {
                    GetDiagContext().Extra()
                        .Print("client_node", m_ClientId.GetNode())
                        .Print("client_session", m_ClientId.GetSession())
                        .Print("client_old_session", old_session)
                        .Print("preferred_affinities_reset", "true");
                } else {
                    GetDiagContext().Extra()
                        .Print("client_node", m_ClientId.GetNode())
                        .Print("client_session", m_ClientId.GetSession())
                        .Print("client_old_session", old_session)
                        .Print("preferred_affinities_reset", "had none");
                }
            }
        }
    }

    // Execute the command
    (this->*extra.processor)(queue_ptr);

    if (restore_client) {
        m_ClientId.SetPassedChecks(orig_client_passed_checks);
        m_ClientId.SetID(orig_client_id);
    }
}


void CNetScheduleHandler::x_UpdateClientPassedChecks(CQueue * q)
{
    // Admin flag is not reset because it comes from a handshake stage and
    // cannot be changed later.
    m_ClientId.ResetPassedCheck();

    // First, deal with a queue
    if (q == NULL)
        return;

    m_ClientId.SetPassedChecks(eNS_Queue);
    if (!m_ClientId.IsAdmin()) {
        // Admin can do everything, so there is no need to check
        // the hosts and programs for non-admins only
        if (q->IsWorkerAllowed(m_ClientId.GetAddress()))
            m_ClientId.SetPassedChecks(eNS_Worker);
        if (q->IsSubmitAllowed(m_ClientId.GetAddress()))
            m_ClientId.SetPassedChecks(eNS_Submitter);
        if (q->IsProgramAllowed(m_ClientId.GetProgramName()))
            m_ClientId.SetPassedChecks(eNS_Program);
    }
}


// Message processors for x_ProcessSubmitBatch
void CNetScheduleHandler::x_ProcessMsgBatchHeader(BUF buffer)
{
    // Expecting BTCH size | ENDS
    try {
        string          msg;
        s_ReadBufToString(buffer, msg);

        SParsedCmd      cmd      = m_BatchHeaderParser.ParseCommand(msg);
        const string &  size_str = cmd.params["size"];

        if (!size_str.empty())
            m_BatchSize = NStr::StringToInt(size_str);
        else
            m_BatchSize = 0;
        (this->*cmd.command->extra.processor)(0);
    }
    catch (const CNSProtoParserException &  ex) {
        m_WithinBatchSubmit = false;
        m_Server->DecrementCurrentSubmitsCounter();
        x_OnCmdParserError(false, ex.GetMsg(), ", BTCH or ENDS expected");
        return;
    }
    catch (const CNetScheduleException &  ex) {
        m_WithinBatchSubmit = false;
        m_Server->DecrementCurrentSubmitsCounter();
        ERR_POST("Server error: " << ex);
        x_SetCmdRequestStatus(ex.ErrCodeToHTTPStatusCode());
        x_WriteMessage("ERR:" + string(ex.GetErrCodeString()) +
                       ":" + ex.GetMsg());
        x_PrintCmdRequestStop();

        m_BatchJobs.clear();
        m_ProcessMessage = &CNetScheduleHandler::x_ProcessMsgRequest;
    }
    catch (const CException &  ex) {
        m_WithinBatchSubmit = false;
        m_Server->DecrementCurrentSubmitsCounter();
        ERR_POST("Error processing command: " << ex);
        x_SetCmdRequestStatus(eStatus_BadRequest);
        x_WriteMessage("ERR:eProtocolSyntaxError:"
                       "Error processing BTCH or ENDS.");
        x_PrintCmdRequestStop();

        m_BatchJobs.clear();
        m_ProcessMessage = &CNetScheduleHandler::x_ProcessMsgRequest;
    }
    catch (...) {
        m_WithinBatchSubmit = false;
        m_Server->DecrementCurrentSubmitsCounter();
        ERR_POST("Unknown error while expecting BTCH or ENDS");
        x_SetCmdRequestStatus(eStatus_BadRequest);
        x_WriteMessage("ERR:eInternalError:Unknown error "
                       "while expecting BTCH or ENDS.");
        x_PrintCmdRequestStop();

        m_BatchJobs.clear();
        m_ProcessMessage = &CNetScheduleHandler::x_ProcessMsgRequest;
    }
}


void CNetScheduleHandler::x_ProcessMsgBatchJob(BUF buffer)
{
    // Expecting:
    // "input" [aff="affinity_token"] [msk=1]
    string          msg;
    s_ReadBufToString(buffer, msg);

    CJob &          job = m_BatchJobs[m_BatchPos].first;
    TNSProtoParams  params;
    try {
        m_BatchEndParser.ParseArguments(msg, s_BatchArgs, &params);
        m_CommandArguments.AssignValues(params, "", GetSocket(),
                                        m_Server->GetCompoundIDPool());
    }
    catch (const CNSProtoParserException &  ex) {
        m_WithinBatchSubmit = false;
        m_Server->DecrementCurrentSubmitsCounter();
        x_OnCmdParserError(false, ex.GetMsg(), "");
        return;
    }
    catch (const CNetScheduleException &  ex) {
        m_WithinBatchSubmit = false;
        m_Server->DecrementCurrentSubmitsCounter();
        ERR_POST(ex.GetMsg());
        x_SetCmdRequestStatus(ex.ErrCodeToHTTPStatusCode());
        x_WriteMessage("ERR:" + string(ex.GetErrCodeString()) +
                       ":" + ex.GetMsg());
        x_PrintCmdRequestStop();

        m_BatchJobs.clear();
        m_ProcessMessage = &CNetScheduleHandler::x_ProcessMsgRequest;
        return;
    }
    catch (const CException &  ex) {
        m_WithinBatchSubmit = false;
        m_Server->DecrementCurrentSubmitsCounter();
        ERR_POST("Error processing command: " << ex);
        x_SetCmdRequestStatus(eStatus_BadRequest);
        x_WriteMessage("ERR:eProtocolSyntaxError:"
                       "Invalid batch submission, syntax error");
        x_PrintCmdRequestStop();

        m_BatchJobs.clear();
        m_ProcessMessage = &CNetScheduleHandler::x_ProcessMsgRequest;
        return;
    }
    catch (...) {
        m_WithinBatchSubmit = false;
        m_Server->DecrementCurrentSubmitsCounter();
        ERR_POST("Arguments parsing unknown exception. Batch submit is rejected.");
        x_SetCmdRequestStatus(eStatus_BadRequest);
        x_WriteMessage("ERR:eProtocolSyntaxError:"
                       "Arguments parsing unknown exception");
        x_PrintCmdRequestStop();

        m_BatchJobs.clear();
        m_ProcessMessage = &CNetScheduleHandler::x_ProcessMsgRequest;
        return;
    }

    job.SetInput(m_CommandArguments.input);

    // Memorize the job affinity if given
    if ( !m_CommandArguments.affinity_token.empty() )
        m_BatchJobs[m_BatchPos].second = m_CommandArguments.affinity_token;

    job.SetMask(m_CommandArguments.job_mask);
    job.SetSubmNotifPort(m_BatchSubmPort);
    job.SetSubmNotifTimeout(m_BatchSubmTimeout);
    job.SetClientIP(m_BatchClientIP);
    job.SetClientSID(m_BatchClientSID);

    if (++m_BatchPos >= m_BatchSize)
        m_ProcessMessage = &CNetScheduleHandler::x_ProcessMsgBatchSubmit;
}


void CNetScheduleHandler::x_ProcessMsgBatchSubmit(BUF buffer)
{
    // Expecting ENDB
    try {
        string      msg;
        s_ReadBufToString(buffer, msg);
        m_BatchEndParser.ParseCommand(msg);
    }
    catch (const CNSProtoParserException &  ex) {
        m_WithinBatchSubmit = false;
        m_Server->DecrementCurrentSubmitsCounter();
        x_OnCmdParserError(false, ex.GetMsg(), ", ENDB expected");
        return;
    }
    catch (const CException &  ex) {
        m_WithinBatchSubmit = false;
        m_Server->DecrementCurrentSubmitsCounter();
        BUF_Read(buffer, 0, BUF_Size(buffer));
        ERR_POST("Error processing command: " << ex);
        x_SetCmdRequestStatus(eStatus_BadRequest);
        x_WriteMessage("ERR:eProtocolSyntaxError:"
                       "Batch submit error - unexpected end of batch");
        x_PrintCmdRequestStop();

        m_BatchJobs.clear();
        m_ProcessMessage = &CNetScheduleHandler::x_ProcessMsgRequest;
        return;
    }
    catch (...) {
        m_WithinBatchSubmit = false;
        m_Server->DecrementCurrentSubmitsCounter();
        ERR_POST("Unknown error while expecting ENDB.");
        x_SetCmdRequestStatus(eStatus_BadRequest);
        x_WriteMessage("ERR:eInternalError:"
                       "Unknown error while expecting ENDB.");
        x_PrintCmdRequestStop();

        m_BatchJobs.clear();
        m_ProcessMessage = &CNetScheduleHandler::x_ProcessMsgRequest;
        return;
    }

    double      comm_elapsed = m_BatchStopWatch.Elapsed();

    // BTCH logging is in a separate context
    CRef<CRequestContext>   current_context(& CDiagContext::GetRequestContext());
    try {
        if (m_ConnContext.NotNull()) {
            CRequestContext *   ctx(new CRequestContext());
            ctx->SetRequestID();
            GetDiagContext().SetRequestContext(ctx);
            GetDiagContext().PrintRequestStart()
                            .Print("_type", "cmd")
                            .Print("_queue", m_QueueName)
                            .Print("bsub", m_CmdContext->GetRequestID())
                            .Print("cmd", "BTCH")
                            .Print("size", m_BatchJobs.size());
            ctx->SetRequestStatus(CNetScheduleHandler::eStatus_OK);
        }

        // we have our batch now
        CStopWatch  sw(CStopWatch::eStart);
        x_ClearRollbackAction();
        unsigned    job_id = GetQueue()->SubmitBatch(m_ClientId,
                                                     m_BatchJobs,
                                                     m_BatchGroup,
                                                     m_RollbackAction);
        double      db_elapsed = sw.Elapsed();

        if (m_ConnContext.NotNull())
            GetDiagContext().Extra()
                .Print("start_job", job_id)
                .Print("commit_time",
                       NStr::DoubleToString(comm_elapsed, 4, NStr::fDoubleFixed))
                .Print("transaction_time",
                       NStr::DoubleToString(db_elapsed, 4, NStr::fDoubleFixed));

        x_WriteMessage("OK:" + NStr::NumericToString(job_id) + " " +
                       m_Server->GetHost().c_str() + " " +
                       NStr::NumericToString(unsigned(m_Server->GetPort())));
        x_ClearRollbackAction();
    }
    catch (const CNetScheduleException &  ex) {
        m_WithinBatchSubmit = false;
        m_Server->DecrementCurrentSubmitsCounter();
        if (m_ConnContext.NotNull()) {
            CDiagContext::GetRequestContext().SetRequestStatus(ex.ErrCodeToHTTPStatusCode());
            GetDiagContext().PrintRequestStop();
            GetDiagContext().SetRequestContext(current_context);
        }
        m_BatchJobs.clear();
        m_ProcessMessage = &CNetScheduleHandler::x_ProcessMsgRequest;
        throw;
    }
    catch (...) {
        m_WithinBatchSubmit = false;
        m_Server->DecrementCurrentSubmitsCounter();
        if (m_ConnContext.NotNull()) {
            CDiagContext::GetRequestContext().SetRequestStatus(eStatus_ServerError);
            GetDiagContext().PrintRequestStop();
            GetDiagContext().SetRequestContext(current_context);
        }
        m_BatchJobs.clear();
        m_ProcessMessage = &CNetScheduleHandler::x_ProcessMsgRequest;
        throw;
    }

    if (m_ConnContext.NotNull()) {
        GetDiagContext().PrintRequestStop();
        GetDiagContext().SetRequestContext(current_context);
    }

    m_ProcessMessage = &CNetScheduleHandler::x_ProcessMsgBatchHeader;
}


//////////////////////////////////////////////////////////////////////////
// Process* methods for processing commands

void CNetScheduleHandler::x_ProcessFastStatusS(CQueue* q)
{
    bool            cmdv2(m_CommandArguments.cmd == "SST2");
    CNSPreciseTime  lifetime;
    TJobStatus      status = q->GetStatusAndLifetime(m_CommandArguments.job_id,
                                                     m_CommandArguments.job_key,
                                                     true, &lifetime);


    if (status == CNetScheduleAPI::eJobNotFound) {
        ERR_POST(Warning << m_CommandArguments.cmd << " for unknown job: "
                         << m_CommandArguments.job_key);
        x_SetCmdRequestStatus(eStatus_NotFound);

        if (cmdv2)
            x_WriteMessage("ERR:eJobNotFound:");
        else
            x_WriteMessage("OK:" + NStr::NumericToString((int) status));
    } else {
        if (cmdv2) {
            string                  pause_status_msg;
            CQueue::TPauseStatus    pause_status = q->GetPauseStatus();

            if (pause_status == CQueue::ePauseWithPullback)
                pause_status_msg = "&pause=pullback";
            else if (pause_status == CQueue::ePauseWithoutPullback)
                pause_status_msg = "&pause=nopullback";

            x_WriteMessage("OK:job_status=" +
                           CNetScheduleAPI::StatusToString(status) +
                           "&job_exptime=" + NStr::NumericToString(lifetime.Sec()) +
                           pause_status_msg);
        }
        else
            x_WriteMessage("OK:" + NStr::NumericToString((int) status));
    }
    x_PrintCmdRequestStop();
}


void CNetScheduleHandler::x_ProcessFastStatusW(CQueue* q)
{
    bool            cmdv2(m_CommandArguments.cmd == "WST2");
    CNSPreciseTime  lifetime;
    TJobStatus      status = q->GetStatusAndLifetime(m_CommandArguments.job_id,
                                                     m_CommandArguments.job_key,
                                                     false, &lifetime);


    if (status == CNetScheduleAPI::eJobNotFound) {
        ERR_POST(Warning << m_CommandArguments.cmd << " for unknown job: "
                         << m_CommandArguments.job_key);
        x_SetCmdRequestStatus(eStatus_NotFound);

        if (cmdv2)
            x_WriteMessage("ERR:eJobNotFound:");
        else
            x_WriteMessage("OK:" + NStr::NumericToString((int) status));
    } else {
        if (cmdv2) {
            string                  pause_status_msg;
            CQueue::TPauseStatus    pause_status = q->GetPauseStatus();

            if (pause_status == CQueue::ePauseWithPullback)
                pause_status_msg = "&pause=pullback";
            else if (pause_status == CQueue::ePauseWithoutPullback)
                pause_status_msg = "&pause=nopullback";

            x_WriteMessage("OK:job_status=" +
                           CNetScheduleAPI::StatusToString(status) +
                           "&job_exptime=" + NStr::NumericToString(lifetime.Sec()) +
                           pause_status_msg);
        }
        else
            x_WriteMessage("OK:" + NStr::NumericToString((int) status));
    }
    x_PrintCmdRequestStop();
}


void CNetScheduleHandler::x_ProcessChangeAffinity(CQueue* q)
{
    // This functionality requires client name and the session
    x_CheckNonAnonymousClient("use CHAFF command");

    if (m_CommandArguments.aff_to_add.empty() &&
        m_CommandArguments.aff_to_del.empty()) {
        ERR_POST(Warning << "CHAFF with neither add list nor del list");
        x_SetCmdRequestStatus(eStatus_BadRequest);
        x_WriteMessage("ERR:eInvalidParameter:");
        x_PrintCmdRequestStop();
        return;
    }

    if (m_ConnContext.NotNull())
        GetDiagContext().Extra()
                .Print("client_node", m_ClientId.GetNode())
                .Print("client_session", m_ClientId.GetSession());


    list<string>    aff_to_add_list;
    list<string>    aff_to_del_list;

    NStr::Split(m_CommandArguments.aff_to_add,
                "\t,", aff_to_add_list, NStr::eNoMergeDelims);
    NStr::Split(m_CommandArguments.aff_to_del,
                "\t,", aff_to_del_list, NStr::eNoMergeDelims);

    string  msg = q->ChangeAffinity(m_ClientId, aff_to_add_list,
                                                aff_to_del_list);
    if (msg.empty())
        x_WriteMessage("OK:");
    else
        x_WriteMessage("OK:WARNING:" + msg + ";");
    x_PrintCmdRequestStop();
}


void CNetScheduleHandler::x_ProcessSetAffinity(CQueue* q)
{
    // This functionality requires client name and the session
    x_CheckNonAnonymousClient("use SETAFF command");

    if (m_ConnContext.NotNull())
        GetDiagContext().Extra()
                .Print("client_node", m_ClientId.GetNode())
                .Print("client_session", m_ClientId.GetSession());

    list<string>    aff_to_set;
    NStr::Split(m_CommandArguments.affinity_token,
                "\t,", aff_to_set, NStr::eNoMergeDelims);
    q->SetAffinity(m_ClientId, aff_to_set);
    x_WriteMessage("OK:");
    x_PrintCmdRequestStop();
}


void CNetScheduleHandler::x_ProcessSubmit(CQueue* q)
{
    if (q->GetRefuseSubmits() || m_Server->GetRefuseSubmits()) {
        x_SetCmdRequestStatus(eStatus_SubmitRefused);
        x_WriteMessage("ERR:eSubmitsDisabled:");
        x_PrintCmdRequestStop();
        return;
    }

    if (m_Server->IncrementCurrentSubmitsCounter() <
        kSubmitCounterInitialValue) {
        // This is a drained shutdown mode
        m_Server->DecrementCurrentSubmitsCounter();
        x_SetCmdRequestStatus(eStatus_SubmitRefused);
        x_WriteMessage("ERR:eSubmitsDisabled:");
        x_PrintCmdRequestStop();
        return;
    }


    CJob        job(m_CommandArguments);
    try {
        x_ClearRollbackAction();
        unsigned int  job_id = q->Submit(m_ClientId, job,
                                         m_CommandArguments.affinity_token,
                                         m_CommandArguments.group,
                                         m_RollbackAction);

        x_WriteMessage("OK:" + q->MakeJobKey(job_id));
        x_ClearRollbackAction();
        m_Server->DecrementCurrentSubmitsCounter();
    } catch (...) {
        m_Server->DecrementCurrentSubmitsCounter();
        throw;
    }

    // There is no need to log the job key, it is logged at lower level
    // together with all the submitted job parameters
    x_PrintCmdRequestStop();
}


void CNetScheduleHandler::x_ProcessSubmitBatch(CQueue* q)
{
    if (q->GetRefuseSubmits() || m_Server->GetRefuseSubmits()) {
        x_SetCmdRequestStatus(eStatus_SubmitRefused);
        x_WriteMessage("ERR:eSubmitsDisabled:");
        x_PrintCmdRequestStop();
        return;
    }

    if (m_Server->IncrementCurrentSubmitsCounter() < kSubmitCounterInitialValue) {
        // This is a drained shutdown mode
        m_Server->DecrementCurrentSubmitsCounter();
        x_SetCmdRequestStatus(eStatus_SubmitRefused);
        x_WriteMessage("ERR:eSubmitsDisabled:");
        x_PrintCmdRequestStop();
        return;
    }

    try {
        // Memorize the fact that batch submit started
        m_WithinBatchSubmit = true;

        m_BatchSubmPort    = m_CommandArguments.port;
        m_BatchSubmTimeout = CNSPreciseTime(m_CommandArguments.timeout, 0);
        m_BatchClientIP    = m_CommandArguments.ip;
        m_BatchClientSID   = m_CommandArguments.sid;
        m_BatchGroup       = m_CommandArguments.group;

        x_WriteMessage("OK:Batch submit ready");
        m_ProcessMessage = &CNetScheduleHandler::x_ProcessMsgBatchHeader;
    }
    catch (...) {
        // WriteMessage can generate an exception
        m_WithinBatchSubmit = false;
        m_Server->DecrementCurrentSubmitsCounter();
        m_ProcessMessage = &CNetScheduleHandler::x_ProcessMsgRequest;
        throw;
    }
}


void CNetScheduleHandler::x_ProcessBatchStart(CQueue*)
{
    m_BatchPos = 0;
    m_BatchStopWatch.Restart();
    m_BatchJobs.resize(m_BatchSize);
    if (m_BatchSize)
        m_ProcessMessage = &CNetScheduleHandler::x_ProcessMsgBatchJob;
    else
        // Unfortunately, because batches can be generated by
        // client program, we better honor zero size ones.
        // Skip right to waiting for 'ENDB'.
        m_ProcessMessage = &CNetScheduleHandler::x_ProcessMsgBatchSubmit;
}


void CNetScheduleHandler::x_ProcessBatchSequenceEnd(CQueue*)
{
    m_WithinBatchSubmit = false;
    m_Server->DecrementCurrentSubmitsCounter();
    m_BatchJobs.clear();
    m_ProcessMessage = &CNetScheduleHandler::x_ProcessMsgRequest;
    x_WriteMessage("OK:");
    x_PrintCmdRequestStop();
}


void CNetScheduleHandler::x_ProcessCancel(CQueue* q)
{
    // Job key or a group must be given
    if (m_CommandArguments.job_id == 0 && m_CommandArguments.group.empty()) {
        LOG_POST(Message << Warning << "CANCEL must have a job key or a group");
        x_SetCmdRequestStatus(eStatus_BadRequest);
        x_WriteMessage("ERR:eInvalidParameter:"
                       "Job key or group must be given");
        x_PrintCmdRequestStop();
        return;
    }

    if (m_CommandArguments.job_id != 0 && !m_CommandArguments.group.empty()) {
        LOG_POST(Message << Warning << "CANCEL must have only one "
                                       "argument - a job key or a group");
        x_SetCmdRequestStatus(eStatus_BadRequest);
        x_WriteMessage("ERR:eInvalidParameter:CANCEL can accept a job key or "
                       "a group but not both");
        x_PrintCmdRequestStop();
        return;
    }

    // Here: one argument is given - a job key or a group
    if (!m_CommandArguments.group.empty()) {
        // CANCEL for a group
        q->CancelGroup(m_ClientId, m_CommandArguments.group);
        x_WriteMessage("OK:");
        x_PrintCmdRequestStop();
        return;
    }

    // Here: CANCEL for a job
    switch (q->Cancel(m_ClientId,
                      m_CommandArguments.job_id,
                      m_CommandArguments.job_key)) {
        case CNetScheduleAPI::eJobNotFound:
            ERR_POST(Warning << "CANCEL for unknown job: "
                             << m_CommandArguments.job_key);
            x_SetCmdRequestStatus(eStatus_NotFound);
            x_WriteMessage("OK:WARNING:Job not found;");
            break;
        case CNetScheduleAPI::eCanceled:
            x_WriteMessage("OK:WARNING:Already canceled;");
            break;
        default:
            x_WriteMessage("OK:");
    }
    x_PrintCmdRequestStop();
}


void CNetScheduleHandler::x_ProcessStatus(CQueue* q)
{
    CJob            job;
    bool            cmdv2 = (m_CommandArguments.cmd == "STATUS2");
    CNSPreciseTime  lifetime;

    if (q->ReadAndTouchJob(m_CommandArguments.job_id,
                           m_CommandArguments.job_key, job, &lifetime) ==
                CNetScheduleAPI::eJobNotFound) {
        // Here: there is no such a job
        ERR_POST(Warning << m_CommandArguments.cmd << " for unknown job: "
                         << m_CommandArguments.job_key);
        x_SetCmdRequestStatus(eStatus_NotFound);
        if (cmdv2)
            x_WriteMessage("ERR:eJobNotFound:");
        else
            x_WriteMessage("OK:" +
                           NStr::NumericToString(
                                        (int)CNetScheduleAPI::eJobNotFound));
        x_PrintCmdRequestStop();
        return;
    }

    // Here: the job was found
    if (cmdv2) {
        string                  pause_status_msg;
        CQueue::TPauseStatus    pause_status = q->GetPauseStatus();

        if (pause_status == CQueue::ePauseWithPullback)
            pause_status_msg = "&pause=pullback";
        else if (pause_status == CQueue::ePauseWithoutPullback)
            pause_status_msg = "&pause=nopullback";

        x_WriteMessage("OK:"
                       "job_status=" +
                       CNetScheduleAPI::StatusToString(job.GetStatus()) +
                       "&job_exptime=" + NStr::NumericToString(lifetime.Sec()) +
                       "&ret_code=" + NStr::NumericToString(job.GetRetCode()) +
                       "&output=" + NStr::URLEncode(job.GetOutput()) +
                       "&err_msg=" + NStr::URLEncode(job.GetErrorMsg()) +
                       "&input=" + NStr::URLEncode(job.GetInput()) +
                       pause_status_msg
                      );
    }
    else
        x_WriteMessage("OK:" + NStr::NumericToString((int) job.GetStatus()) +
                       " " + NStr::NumericToString(job.GetRetCode()) +
                       " \""   + NStr::PrintableString(job.GetOutput()) +
                       "\" \"" + NStr::PrintableString(job.GetErrorMsg()) +
                       "\" \"" + NStr::PrintableString(job.GetInput()) +
                       "\"");
    x_PrintCmdRequestStop();
}


void CNetScheduleHandler::x_ProcessGetJob(CQueue* q)
{
    // GET & WGET are first versions of the command
    bool    cmdv2(m_CommandArguments.cmd == "GET2");

    if (cmdv2) {
        x_CheckNonAnonymousClient("use GET2 command");
        x_CheckPortAndTimeout();
        x_CheckGetParameters();
    }
    else {
        // The affinity options are only for the second version of the command
        m_CommandArguments.wnode_affinity = false;
        m_CommandArguments.exclusive_new_aff = false;

        // The old clients must have any_affinity set to true
        // depending on the explicit affinity - to conform the old behavior
        m_CommandArguments.any_affinity = m_CommandArguments.affinity_token.empty();
    }

    // Check if the queue is paused
    CQueue::TPauseStatus    pause_status = q->GetPauseStatus();
    if (pause_status != CQueue::eNoPause) {
        string      pause_status_str;

        if (pause_status == CQueue::ePauseWithPullback)
            pause_status_str = "pullback";
        else
            pause_status_str = "nopullback";

        if (cmdv2)
            x_WriteMessage("OK:pause=" + pause_status_str);
        else
            x_WriteMessage("OK:");

        if (m_ConnContext.NotNull())
            GetDiagContext().Extra().Print("job_key", "None")
                                    .Print("reason",
                                           "pause: " + pause_status_str);

        x_PrintCmdRequestStop();
        return;
    }


    list<string>    aff_list;
    NStr::Split(m_CommandArguments.affinity_token,
                "\t,", aff_list, NStr::eNoMergeDelims);

    CJob            job;
    string          added_pref_aff;
    x_ClearRollbackAction();
    if (q->GetJobOrWait(m_ClientId,
                        m_CommandArguments.port,
                        m_CommandArguments.timeout,
                        CNSPreciseTime::Current(), &aff_list,
                        m_CommandArguments.wnode_affinity,
                        m_CommandArguments.any_affinity,
                        m_CommandArguments.exclusive_new_aff,
                        cmdv2,
                        &job,
                        m_RollbackAction,
                        added_pref_aff) == false) {
        // Preferred affinities were reset for the client, so no job
        // and bad request
        x_SetCmdRequestStatus(eStatus_BadRequest);
        x_WriteMessage("ERR:ePrefAffExpired:");
    } else {
        if (added_pref_aff.empty() == false) {
            if (m_ConnContext.NotNull()) {
                GetDiagContext().Extra()
                    .Print("client_node", m_ClientId.GetNode())
                    .Print("client_session", m_ClientId.GetSession())
                    .Print("added_preferred_affinity", added_pref_aff);
            }
        }
        x_PrintGetJobResponse(q, job, cmdv2);
        x_ClearRollbackAction();
    }

    x_PrintCmdRequestStop();
}


void CNetScheduleHandler::x_ProcessCancelWaitGet(CQueue* q)
{
    x_CheckNonAnonymousClient("cancel waiting after WGET");

    q->CancelWaitGet(m_ClientId);
    x_WriteMessage("OK:");
    x_PrintCmdRequestStop();
}


void CNetScheduleHandler::x_ProcessPut(CQueue* q)
{
    bool    cmdv2(m_CommandArguments.cmd == "PUT2");

    if (cmdv2) {
        x_CheckNonAnonymousClient("use PUT2 command");
        x_CheckAuthorizationToken();
    }

    TJobStatus  old_status = q->PutResult(m_ClientId, CNSPreciseTime::Current(),
                                          m_CommandArguments.job_id,
                                          m_CommandArguments.job_key,
                                          m_CommandArguments.auth_token,
                                          m_CommandArguments.job_return_code,
                                          m_CommandArguments.output);
    if (old_status == CNetScheduleAPI::ePending ||
        old_status == CNetScheduleAPI::eRunning) {
        x_WriteMessage("OK:");
        x_PrintCmdRequestStop();
        return;
    }
    if (old_status == CNetScheduleAPI::eFailed) {
        // Still accept the job results, but print a warning: CXX-3632
        ERR_POST(Warning << "Accepting results for a job in the FAILED state.");
        x_WriteMessage("OK:");
        x_PrintCmdRequestStop();
        return;
    }
    if (old_status == CNetScheduleAPI::eDone) {
        ERR_POST(Warning << "Cannot accept job "
                         << m_CommandArguments.job_key
                         << " results. The job has already been done.");
        x_WriteMessage("OK:WARNING:Already done;");
        x_PrintCmdRequestStop();
        return;
    }
    if (old_status == CNetScheduleAPI::eJobNotFound) {
        ERR_POST(Warning << "Cannot accept job "
                         << m_CommandArguments.job_key
                         << " results. The job is unknown");
        x_SetCmdRequestStatus(eStatus_NotFound);
        x_WriteMessage("ERR:eJobNotFound:");
        x_PrintCmdRequestStop();
        return;
    }

    // Here: invalid job status, nothing will be done
    ERR_POST(Warning << "Cannot accept job "
                     << m_CommandArguments.job_key
                     << " results; job is in "
                     << CNetScheduleAPI::StatusToString(old_status)
                     << " state");
    x_SetCmdRequestStatus(eStatus_InvalidJobStatus);
    x_WriteMessage("ERR:eInvalidJobStatus:"
                   "Cannot accept job results; job is in " +
                   CNetScheduleAPI::StatusToString(old_status) + " state");
    x_PrintCmdRequestStop();
}


void CNetScheduleHandler::x_ProcessJobExchange(CQueue* q)
{
    // The JXCG command is used only by old clients. All new client should use
    // PUT2 + GET2 sequence.
    // The old clients must have any_affinity set to true
    // depending on the explicit affinity - to conform the old behavior
    m_CommandArguments.any_affinity = m_CommandArguments.affinity_token.empty();


    CNSPreciseTime  curr = CNSPreciseTime::Current();

    // PUT part
    TJobStatus      old_status = q->PutResult(m_ClientId, curr,
                                          m_CommandArguments.job_id,
                                          m_CommandArguments.job_key,
                                          m_CommandArguments.auth_token,
                                          m_CommandArguments.job_return_code,
                                          m_CommandArguments.output);

    if (old_status == CNetScheduleAPI::eJobNotFound) {
        ERR_POST(Warning << "Cannot accept job "
                         << m_CommandArguments.job_key
                         << " results. The job is unknown");
    } else if (old_status == CNetScheduleAPI::eFailed) {
        // Still accept the job results, but print a warning: CXX-3632
        ERR_POST(Warning << "Accepting results for a job in the FAILED state.");
    } else if (old_status != CNetScheduleAPI::ePending &&
               old_status != CNetScheduleAPI::eRunning) {
        ERR_POST(Warning << "Cannot accept job "
                         << m_CommandArguments.job_key
                         << " results. The job has already been done.");
    }


    // Get part
    CQueue::TPauseStatus    pause_status = q->GetPauseStatus();
    if (pause_status != CQueue::eNoPause) {
        x_WriteMessage("OK:");
        if (m_ConnContext.NotNull()) {
            string      pause_status_str;

            if (pause_status == CQueue::ePauseWithPullback)
                pause_status_str = "pullback";
            else
                pause_status_str = "nopullback";

            GetDiagContext().Extra().Print("job_key", "None")
                                    .Print("reason",
                                           "pause: " + pause_status_str);
        }
        x_PrintCmdRequestStop();
        return;
    }

    list<string>        aff_list;
    NStr::Split(m_CommandArguments.affinity_token,
                "\t,", aff_list, NStr::eNoMergeDelims);

    CJob                job;
    string              added_pref_aff;
    x_ClearRollbackAction();
    if (q->GetJobOrWait(m_ClientId,
                        m_CommandArguments.port,
                        m_CommandArguments.timeout,
                        curr, &aff_list,
                        m_CommandArguments.wnode_affinity,
                        m_CommandArguments.any_affinity,
                        false,
                        false,
                        &job,
                        m_RollbackAction,
                        added_pref_aff) == false) {
        // Preferred affinities were reset for the client, so no job
        // and bad request
        x_SetCmdRequestStatus(eStatus_BadRequest);
        x_WriteMessage("ERR:ePrefAffExpired:");
    } else {
        if (added_pref_aff.empty() == false) {
            if (m_ConnContext.NotNull()) {
                GetDiagContext().Extra()
                    .Print("client_node", m_ClientId.GetNode())
                    .Print("client_session", m_ClientId.GetSession())
                    .Print("added_preferred_affinity", added_pref_aff);
            }
        }
        x_PrintGetJobResponse(q, job, false);
        x_ClearRollbackAction();
    }

    x_PrintCmdRequestStop();
}


void CNetScheduleHandler::x_ProcessPutMessage(CQueue* q)
{
    if (q->PutProgressMessage(m_CommandArguments.job_id,
                              m_CommandArguments.progress_msg))
        x_WriteMessage("OK:");
    else {
        ERR_POST(Warning << "MPUT for unknown job "
                         << m_CommandArguments.job_key);
        x_SetCmdRequestStatus(eStatus_NotFound);
        x_WriteMessage("ERR:eJobNotFound:");
    }
    x_PrintCmdRequestStop();
}


void CNetScheduleHandler::x_ProcessGetMessage(CQueue* q)
{
    CJob            job;
    CNSPreciseTime  lifetime;

    if (q->ReadAndTouchJob(m_CommandArguments.job_id,
                           m_CommandArguments.job_key, job, &lifetime) !=
            CNetScheduleAPI::eJobNotFound)
        x_WriteMessage("OK:" + NStr::PrintableString(job.GetProgressMsg()));
    else {
        ERR_POST(Warning << m_CommandArguments.cmd
                         << "MGET for unknown job "
                         << m_CommandArguments.job_key);
        x_SetCmdRequestStatus(eStatus_NotFound);
        x_WriteMessage("ERR:eJobNotFound:");
    }
    x_PrintCmdRequestStop();
}


void CNetScheduleHandler::x_ProcessPutFailure(CQueue* q)
{
    bool    cmdv2(m_CommandArguments.cmd == "FPUT2");

    if (cmdv2) {
        x_CheckNonAnonymousClient("use FPUT2 command");
        x_CheckAuthorizationToken();
    }

    string      warning;
    TJobStatus  old_status = q->FailJob(
                                m_ClientId,
                                m_CommandArguments.job_id,
                                m_CommandArguments.job_key,
                                m_CommandArguments.auth_token,
                                m_CommandArguments.err_msg,
                                m_CommandArguments.output,
                                m_CommandArguments.job_return_code,
                                warning);

    if (old_status == CNetScheduleAPI::eJobNotFound) {
        ERR_POST(Warning << "FPUT for unknown job "
                         << m_CommandArguments.job_key);
        x_SetCmdRequestStatus(eStatus_NotFound);
        x_WriteMessage("ERR:eJobNotFound:");
        x_PrintCmdRequestStop();
        return;
    }

    if (old_status == CNetScheduleAPI::eFailed) {
        ERR_POST(Warning << "FPUT for already failed job "
                         << m_CommandArguments.job_key);
        x_WriteMessage("OK:WARNING:Already failed;");
        x_PrintCmdRequestStop();
        return;
    }

    if (old_status != CNetScheduleAPI::eRunning) {
        ERR_POST(Warning << "Cannot fail job "
                         << m_CommandArguments.job_key
                         << "; job is in "
                         << CNetScheduleAPI::StatusToString(old_status)
                         << " state");
        x_SetCmdRequestStatus(eStatus_InvalidJobStatus);
        x_WriteMessage("ERR:eInvalidJobStatus:Cannot fail job; job is in " +
                       CNetScheduleAPI::StatusToString(old_status) + " state");
        x_PrintCmdRequestStop();
        return;
    }

    // Here: all is fine
    if (warning.empty())
        x_WriteMessage("OK:");
    else
        x_WriteMessage("OK:WARNING:" + warning + ";");
    x_PrintCmdRequestStop();
}


void CNetScheduleHandler::x_ProcessDropQueue(CQueue* q)
{
    q->Truncate();
    x_WriteMessage("OK:");
    x_PrintCmdRequestStop();
}


void CNetScheduleHandler::x_ProcessReturn(CQueue* q)
{
    bool                        cmdv2(m_CommandArguments.cmd == "RETURN2");
    CQueue::TJobReturnOption    return_option = CQueue::eWithBlacklist;

    if (cmdv2) {
        x_CheckNonAnonymousClient("use RETURN2 command");
        x_CheckAuthorizationToken();

        if (m_CommandArguments.blacklist)
            return_option = CQueue::eWithBlacklist;
        else
            return_option = CQueue::eWithoutBlacklist;
    }

    string          warning;
    TJobStatus      old_status = q->ReturnJob(m_ClientId,
                                              m_CommandArguments.job_id,
                                              m_CommandArguments.job_key,
                                              m_CommandArguments.auth_token,
                                              warning, return_option);

    if (old_status == CNetScheduleAPI::eRunning) {
        if (warning.empty())
            x_WriteMessage("OK:");
        else
            x_WriteMessage("OK:WARNING:" + warning + ";");
        x_PrintCmdRequestStop();
        return;
    }

    if (old_status == CNetScheduleAPI::eJobNotFound) {
        ERR_POST(Warning << "RETURN for unknown job "
                         << m_CommandArguments.job_key);
        x_SetCmdRequestStatus(eStatus_NotFound);
        x_WriteMessage("ERR:eJobNotFound:");
        x_PrintCmdRequestStop();
        return;
    }

    ERR_POST(Warning << "Cannot return job "
                     << m_CommandArguments.job_key
                     << "; job is in "
                     << CNetScheduleAPI::StatusToString(old_status)
                     << " state");
    x_SetCmdRequestStatus(eStatus_InvalidJobStatus);
    x_WriteMessage("ERR:eInvalidJobStatus:Cannot return job; job is in " +
                   CNetScheduleAPI::StatusToString(old_status) + " state");

    x_PrintCmdRequestStop();
}


void CNetScheduleHandler::x_ProcessJobDelayExpiration(CQueue* q)
{
    if (m_CommandArguments.timeout <= 0) {
        ERR_POST(Warning << "Invalid timeout "
                         << m_CommandArguments.timeout
                         << " in JDEX for job "
                         << m_CommandArguments.job_key);
        x_SetCmdRequestStatus(eStatus_BadRequest);
        x_WriteMessage("ERR:eInvalidParameter:");
        x_PrintCmdRequestStop();
        return;
    }

    CNSPreciseTime  timeout(m_CommandArguments.timeout, 0);
    TJobStatus      status = q->JobDelayExpiration(m_CommandArguments.job_id,
                                                   timeout);

    if (status == CNetScheduleAPI::eJobNotFound) {
        ERR_POST(Warning << "JDEX for unknown job "
                         << m_CommandArguments.job_key);
        x_SetCmdRequestStatus(eStatus_NotFound);
        x_WriteMessage("ERR:eJobNotFound:");
        x_PrintCmdRequestStop();
        return;
    }
    if (status != CNetScheduleAPI::eRunning) {
        ERR_POST(Warning << "Cannot change expiration for job "
                         << m_CommandArguments.job_key
                         << " in status "
                         << CNetScheduleAPI::StatusToString(status));
        x_SetCmdRequestStatus(eStatus_InvalidJobStatus);
        x_WriteMessage("ERR:eInvalidJobStatus:" +
                       CNetScheduleAPI::StatusToString(status));
        x_PrintCmdRequestStop();
        return;
    }

    // Here: the new timeout has been applied
    x_WriteMessage("OK:");
    x_PrintCmdRequestStop();
}


void CNetScheduleHandler::x_ProcessListenJob(CQueue* q)
{
    size_t          last_event_index = 0;
    CNSPreciseTime  timeout(m_CommandArguments.timeout, 0);
    TJobStatus      status = q->SetJobListener(
                                    m_CommandArguments.job_id,
                                    m_ClientId.GetAddress(),
                                    m_CommandArguments.port,
                                    timeout,
                                    &last_event_index);

    if (status == CNetScheduleAPI::eJobNotFound) {
        ERR_POST(Warning << "LISTEN for unknown job "
                         << m_CommandArguments.job_key);
        x_SetCmdRequestStatus(eStatus_NotFound);
        x_WriteMessage("ERR:eJobNotFound:");
    } else
        x_WriteMessage("OK:job_status=" +
                       CNetScheduleAPI::StatusToString(status) +
                       "&last_event_index=" +
                       NStr::NumericToString(last_event_index));

    x_PrintCmdRequestStop();
}


void CNetScheduleHandler::x_ProcessStatistics(CQueue* q)
{
    CSocket &           socket = GetSocket();
    const string &      what   = m_CommandArguments.option;
    CNSPreciseTime      curr   = CNSPreciseTime::Current();

    if (!what.empty() && what != "QCLASSES" && what != "QUEUES" &&
        what != "JOBS" && what != "ALL" && what != "CLIENTS" &&
        what != "NOTIFICATIONS" && what != "AFFINITIES" &&
        what != "GROUPS" && what != "WNODE" && what != "SERVICES" &&
        what != "ALERTS") {
        NCBI_THROW(CNetScheduleException, eInvalidParameter,
                   "Unsupported '" + what +
                   "' parameter for the STAT command.");
    }

    if (q == NULL && (what == "CLIENTS" || what == "NOTIFICATIONS" ||
                      what == "AFFINITIES" || what == "GROUPS" ||
                      what == "WNODE")) {
        NCBI_THROW(CNetScheduleException, eInvalidParameter,
                   "STAT " + what + " requires a queue");
    }


    if (what == "QCLASSES") {
        string      info = m_Server->GetQueueClassesInfo();

        if (!info.empty())
            x_WriteMessage(info);

        x_WriteMessage("OK:END");
        x_PrintCmdRequestStop();
        return;
    }
    if (what == "QUEUES") {
        string      info = m_Server->GetQueueInfo();

        if (!info.empty())
            x_WriteMessage(info);

        x_WriteMessage("OK:END");
        x_PrintCmdRequestStop();
        return;
    }
    if (what == "SERVICES") {
        string                  output;
        map<string, string>     services;
        m_Server->GetServices(services);
        for (map<string, string>::const_iterator  k = services.begin();
                k != services.end(); ++k) {
            if (!output.empty())
                output += "&";
            output += k->first + "=" + k->second;
        }
        x_WriteMessage("OK:" + output);
        x_PrintCmdRequestStop();
        return;
    }
    if (what == "ALERTS") {
        string  output = m_Server->SerializeAlerts();
        if (!output.empty())
            x_WriteMessage(output);
        x_WriteMessage("OK:END");
        x_PrintCmdRequestStop();
        return;
    }

    if (q == NULL) {
        if (what == "JOBS")
            x_WriteMessage(m_Server->PrintJobsStat());
        else {
            // Transition counters for all the queues
            x_WriteMessage("OK:Started: " +
                           m_Server->GetStartTime().AsString());
            if (m_Server->GetRefuseSubmits())
                x_WriteMessage("OK:SubmitsDisabledEffective: 1");
            else
                x_WriteMessage("OK:SubmitsDisabledEffective: 0");
            if (m_Server->IsDrainShutdown())
                x_WriteMessage("OK:DrainedShutdown: 1");
            else
                x_WriteMessage("OK:DrainedShutdown: 0");
            x_WriteMessage(m_Server->PrintTransitionCounters());
        }
        x_WriteMessage("OK:END");
        x_PrintCmdRequestStop();
        return;
    }


    socket.DisableOSSendDelay(false);
    if (!what.empty() && what != "ALL") {
        x_StatisticsNew(q, what, curr);
        return;
    }

    x_WriteMessage("OK:Started: " + m_Server->GetStartTime().AsString());
    if (m_Server->GetRefuseSubmits() || q->GetRefuseSubmits())
        x_WriteMessage("OK:SubmitsDisabledEffective: 1");
    else
        x_WriteMessage("OK:SubmitsDisabledEffective: 0");
    if (q->GetRefuseSubmits())
        x_WriteMessage("OK:SubmitsDisabledPrivate: 1");
    else
        x_WriteMessage("OK:SubmitsDisabledPrivate: 0");

    for (size_t  k = 0; k < g_ValidJobStatusesSize; ++k) {
        TJobStatus      st = g_ValidJobStatuses[k];
        unsigned        count = q->CountStatus(st);

        x_WriteMessage("OK:" + CNetScheduleAPI::StatusToString(st) +
                       ": " + NStr::NumericToString(count));

        if (what == "ALL") {
            TNSBitVector::statistics bv_stat;
            q->StatusStatistics(st, &bv_stat);
            x_WriteMessage("OK:"
                           "  bit_blk=" +
                           NStr::NumericToString(bv_stat.bit_blocks) +
                           "; gap_blk=" +
                           NStr::NumericToString(bv_stat.gap_blocks) +
                           "; mem_used=" +
                           NStr::NumericToString(bv_stat.memory_used));
        }
    } // for


    if (what == "ALL") {
        x_WriteMessage("OK:[Berkeley DB Mutexes]:");
        {{
            CNcbiOstrstream ostr;

            try {
                m_Server->PrintMutexStat(ostr);
                ostr << ends;
                x_WriteMessage(string("OK:") + ostr.str());
            } catch (...) {
                ostr.freeze(false);
                throw;
            }
            ostr.freeze(false);
        }}

        x_WriteMessage("OK:[Berkeley DB Locks]:");
        {{
            CNcbiOstrstream ostr;

            try {
                m_Server->PrintLockStat(ostr);
                ostr << ends;
                x_WriteMessage(string("OK:") + ostr.str());
            } catch (...) {
                ostr.freeze(false);
                throw;
            }
            ostr.freeze(false);
        }}

        x_WriteMessage("OK:[Berkeley DB Memory Usage]:");
        {{
            CNcbiOstrstream ostr;

            try {
                m_Server->PrintMemStat(ostr);
                ostr << ends;
                x_WriteMessage(string("OK:") + ostr.str());
            } catch (...) {
                ostr.freeze(false);
                throw;
            }
            ostr.freeze(false);
        }}

        x_WriteMessage("OK:[BitVector block pool]:");
        {{
            const TBlockAlloc::TBucketPool::TBucketVector& bv =
                TBlockAlloc::GetPoolVector();
            size_t      pool_vec_size = bv.size();

            x_WriteMessage("OK:Pool vector size: " +
                           NStr::NumericToString(pool_vec_size));

            for (size_t  i = 0; i < pool_vec_size; ++i) {
                const TBlockAlloc::TBucketPool::TResourcePool* rp =
                    TBlockAlloc::GetPool(i);
                if (rp) {
                    size_t pool_size = rp->GetSize();
                    if (pool_size) {
                        x_WriteMessage("OK:Pool [ " + NStr::NumericToString(i) +
                                       "] = " + NStr::NumericToString(pool_size));
                    }
                }
            }
        }}
    }

    x_WriteMessage("OK:[Transitions counters]:");
    x_WriteMessage(q->PrintTransitionCounters());

    x_WriteMessage("OK:END");
    x_PrintCmdRequestStop();
}


void CNetScheduleHandler::x_ProcessReloadConfig(CQueue* q)
{
    CNcbiApplication *      app = CNcbiApplication::Instance();
    bool                    reloaded = app->ReloadConfig(
                                            CMetaRegistry::fReloadIfChanged);

    if (reloaded) {
        const CNcbiRegistry &   reg = app->GetConfig();
        bool                    well_formed = NS_ValidateConfigFile(reg);

        if (!well_formed) {
            m_Server->RegisterAlert(eReconfigure);
            x_SetCmdRequestStatus(eStatus_BadRequest);
            x_WriteMessage("ERR:eInvalidParameter:Configuration file is not "
                           "well formed. See log for the details.");
            x_PrintCmdRequestStop();
            return;
        }

        string                  diff;
        m_Server->Configure(reg, diff);

        // Logging from the [server] section
        SNS_Parameters          params;
        params.Read(reg);

        string      what_changed = m_Server->SetNSParameters(params, true);
        string      services_changed = m_Server->ReadServicesConfig(reg);

        if (what_changed.empty() && diff.empty() && services_changed.empty()) {
            m_Server->AcknowledgeAlert(eConfig, "NSAcknowledge");
            m_Server->AcknowledgeAlert(eReconfigure, "NSAcknowledge");
            if (m_ConnContext.NotNull())
                 GetDiagContext().Extra().Print("accepted_changes", "none");
            x_WriteMessage("OK:WARNING:No changeable parameters were "
                           "identified in the new cofiguration file;");
            x_PrintCmdRequestStop();
            return;
        }

        string      total_changes = what_changed;
        if (!diff.empty()) {
            if (!total_changes.empty())
                total_changes += ", ";
            total_changes += diff;
        }
        if (!services_changed.empty()) {
            if (!total_changes.empty())
                total_changes += ", ";
            total_changes += services_changed;
        }

        if (m_ConnContext.NotNull())
            GetDiagContext().Extra().Print("config_changes", total_changes);

        m_Server->AcknowledgeAlert(eConfig, "NSAcknowledge");
        m_Server->AcknowledgeAlert(eReconfigure, "NSAcknowledge");
        x_WriteMessage("OK:" + total_changes);
    }
    else
        x_WriteMessage("OK:WARNING:Configuration file has not "
                       "been changed, RECO ignored;");

    x_PrintCmdRequestStop();
}


void CNetScheduleHandler::x_ProcessActiveCount(CQueue* q)
{
    string      active_jobs = NStr::NumericToString(m_Server->CountActiveJobs());

    x_WriteMessage("OK:" + active_jobs);
    x_PrintCmdRequestStop();
}


void CNetScheduleHandler::x_ProcessDump(CQueue* q)
{
    if (m_CommandArguments.job_id == 0) {
        // The whole queue dump, may be restricted by one state
        if (m_CommandArguments.job_status == CNetScheduleAPI::eJobNotFound &&
            !m_CommandArguments.job_status_string.empty()) {
            // The state parameter was provided but it was not possible to
            // convert it into a valid job state
            x_SetCmdRequestStatus(eStatus_BadRequest);
            x_WriteMessage("ERR:eInvalidParameter:Status unknown: " +
                           m_CommandArguments.job_status_string);
            x_PrintCmdRequestStop();
            return;
        }

        x_WriteMessage(q->PrintAllJobDbStat(m_CommandArguments.group,
                                            m_CommandArguments.job_status,
                                            m_CommandArguments.start_after_job_id,
                                            m_CommandArguments.count) +
                       "OK:END");
        x_PrintCmdRequestStop();
        return;
    }


    // Certain job dump
    string      job_info = q->PrintJobDbStat(m_CommandArguments.job_id);
    if (job_info.empty()) {
        // Nothing was printed because there is no such a job
        x_SetCmdRequestStatus(eStatus_NotFound);
        x_WriteMessage("ERR:eJobNotFound:");
    } else
        x_WriteMessage(job_info + "OK:END");

    x_PrintCmdRequestStop();
}


void CNetScheduleHandler::x_ProcessShutdown(CQueue*)
{
    if (m_CommandArguments.drain) {
        if (m_Server->ShutdownRequested()) {
            x_SetCmdRequestStatus(eStatus_BadRequest);
            x_WriteMessage("ERR:eShuttingDown:The server is in "
                           "shutting down state");
            x_PrintCmdRequestStop();
            return;
        }
        x_WriteMessage("OK:");
        x_PrintCmdRequestStop();
        m_Server->SetRefuseSubmits(true);
        m_Server->SetDrainShutdown();
        return;
    }

    // Unconditional immediate shutdown.
    x_WriteMessage("OK:");
    x_PrintCmdRequestStop();
    m_Server->SetRefuseSubmits(true);
    m_Server->SetShutdownFlag();
}


void CNetScheduleHandler::x_ProcessGetConf(CQueue*)
{
    if (m_CommandArguments.effective) {
        // The effective config (some parameters could be altered
        // at run-time) has been requested
        x_WriteMessage(x_GetServerSection() +
                       x_GetLogSection() +
                       x_GetDiagSection() +
                       x_GetBdbSection() +
                       m_Server->GetQueueClassesConfig() +
                       m_Server->GetQueueConfig() +
                       m_Server->GetLinkedSectionConfig() +
                       m_Server->GetServiceToQueueSectionConfig());
    } else {
        // The original config file (the one used at the startup)
        // has been requested
        CConn_SocketStream  ss(GetSocket().GetSOCK(), eNoOwnership);

        CNcbiApplication::Instance()->GetConfig().Write(ss);
        ss.flush();
    }
    x_WriteMessage("OK:END");
    x_PrintCmdRequestStop();
}


void CNetScheduleHandler::x_ProcessVersion(CQueue*)
{
    // Further NS versions should exclude ns_node, ns_session and pid from the
    // VERSION command in favor of the HEALTH command
    static string  reply =
                    "OK:server_version=" NETSCHEDULED_VERSION
                    "&storage_version=" NETSCHEDULED_STORAGE_VERSION
                    "&protocol_version=" NETSCHEDULED_PROTOCOL_VERSION
                    "&build_date=" + NStr::URLEncode(NETSCHEDULED_BUILD_DATE) +
                    "&ns_node=" + m_Server->GetNodeID() +
                    "&ns_session=" + m_Server->GetSessionID() +
                    "&pid=" + NStr::NumericToString(CDiagContext::GetPID());
    x_WriteMessage(reply);
    x_PrintCmdRequestStop();
}


void CNetScheduleHandler::x_ProcessHealth(CQueue*)
{
    double      user_time;
    double      system_time;
    bool        process_time_result = GetCurrentProcessTimes(&user_time,
                                                             &system_time);
    Uint8       physical_memory = GetPhysicalMemorySize();
    size_t      mem_used_total;
    size_t      mem_used_resident;
    size_t      mem_used_shared;
    bool        mem_used_result = GetMemoryUsage(&mem_used_total,
                                                 &mem_used_resident,
                                                 &mem_used_shared);
    int         proc_fd_soft_limit;
    int         proc_fd_hard_limit;
    int         proc_fd_used = GetProcessFDCount(&proc_fd_soft_limit,
                                                 &proc_fd_hard_limit);
    int         proc_thread_count = GetProcessThreadCount();

    string      reply =
                    "OK:pid=" + NStr::NumericToString(CDiagContext::GetPID()) +
                    "&ns_node=" + m_Server->GetNodeID() +
                    "&ns_session=" + m_Server->GetSessionID() +
                    "&started=" + NStr::URLEncode(m_Server->GetStartTime().AsString()) +
                    "&cpu_count=" + NStr::NumericToString(GetCpuCount());
    if (process_time_result)
        reply += "&user_time=" + NStr::NumericToString(user_time) +
                 "&system_time=" + NStr::NumericToString(system_time);
    else
        reply += "&user_time=n/a&system_time=n/a";

    if (physical_memory > 0)
        reply += "&physical_memory=" + NStr::NumericToString(physical_memory);
    else
        reply += "&physical_memory=n/a";

    if (mem_used_result)
        reply += "&mem_used_total=" + NStr::NumericToString(mem_used_total) +
                 "&mem_used_resident=" + NStr::NumericToString(mem_used_resident) +
                 "&mem_used_shared=" + NStr::NumericToString(mem_used_shared);
    else
        reply += "&mem_used_total=n/a&mem_used_resident=n/a&mem_used_shared=n/a";

    if (proc_fd_soft_limit >= 0)
        reply += "&proc_fd_soft_limit=" + NStr::NumericToString(proc_fd_soft_limit);
    else
        reply += "&proc_fd_soft_limit=n/a";

    if (proc_fd_hard_limit >= 0)
        reply += "&proc_fd_hard_limit=" + NStr::NumericToString(proc_fd_hard_limit);
    else
        reply += "&proc_fd_hard_limit=n/a";

    if (proc_fd_used >= 0)
        reply += "&proc_fd_used=" + NStr::NumericToString(proc_fd_used);
    else
        reply += "&proc_fd_used=n/a";

    if (proc_thread_count >= 1)
        reply += "&proc_thread_count=" + NStr::NumericToString(proc_thread_count);
    else
        reply += "&proc_thread_count=n/a";

    string  alerts = m_Server->GetAlerts();
    if (!alerts.empty())
        reply += "&" + alerts;

    x_WriteMessage(reply);
    x_PrintCmdRequestStop();
}


void CNetScheduleHandler::x_ProcessAckAlert(CQueue*)
{
    enum EAlertAckResult    result =
                m_Server->AcknowledgeAlert(m_CommandArguments.alert,
                                           m_CommandArguments.user);
    switch (result) {
        case eNotFound:
            x_WriteMessage("OK:WARNING:Alert has not been found;");
            break;
        case eAlreadyAcknowledged:
            x_WriteMessage("OK:WARNING:Alert has already been acknowledged;");
            break;
        case eAcknowledged:
            x_WriteMessage("OK:");
            break;
        default:
            x_WriteMessage("OK:WARNING:unknown acknowledge result;");
    }
    x_PrintCmdRequestStop();
}


void CNetScheduleHandler::x_ProcessQList(CQueue*)
{
    x_WriteMessage("OK:" + m_Server->GetQueueNames(";"));
    x_PrintCmdRequestStop();
}


void CNetScheduleHandler::x_ProcessQuitSession(CQueue*)
{
    m_Server->CloseConnection(&GetSocket());
}


void CNetScheduleHandler::x_ProcessCreateDynamicQueue(CQueue*)
{
    // program and submitter restrictions must be checked for the queue class
    m_Server->CreateDynamicQueue(
                        m_ClientId,
                        m_CommandArguments.qname,
                        m_CommandArguments.qclass,
                        m_CommandArguments.description);
    x_WriteMessage("OK:");
    x_PrintCmdRequestStop();
}


void CNetScheduleHandler::x_ProcessDeleteDynamicQueue(CQueue*)
{
    // program and submitter restrictions must be checked for the queue to
    // be deleted
    m_Server->DeleteDynamicQueue(m_ClientId, m_CommandArguments.qname);
    x_WriteMessage("OK:");
    x_PrintCmdRequestStop();
}


void CNetScheduleHandler::x_ProcessQueueInfo(CQueue*)
{
    bool                cmdv2(m_CommandArguments.cmd == "QINF2");

    if (cmdv2) {
        x_CheckQInf2Parameters();

        string      qname = m_CommandArguments.qname;
        if (!m_CommandArguments.service.empty()) {
            // Service has been provided, need to resolve it to a queue
            qname = m_Server->ResolveService(m_CommandArguments.service);
            if (qname.empty()) {
                x_SetCmdRequestStatus(eStatus_NotFound);
                x_WriteMessage("ERR:eUnknownService:Cannot resolve service " +
                               m_CommandArguments.service + " to a queue");
                x_PrintCmdRequestStop();
                return;
            }
        }

        SQueueParameters    params = m_Server->QueueInfo(qname);
        CRef<CQueue>        queue_ref;
        CQueue *            queue_ptr;
        size_t              jobs_per_state[g_ValidJobStatusesSize];
        string              jobs_part;
        string              linked_sections_part;
        size_t              total = 0;
        map< string,
             map<string, string> >      linked_sections;

        queue_ref.Reset(m_Server->OpenQueue(qname));
        queue_ptr = queue_ref.GetPointer();
        queue_ptr->GetJobsPerState("", "", jobs_per_state);
        queue_ptr->GetLinkedSections(linked_sections);

        for (size_t  index(0); index < g_ValidJobStatusesSize; ++index) {
            jobs_part += "&" +
                CNetScheduleAPI::StatusToString(g_ValidJobStatuses[index]) +
                "=" + NStr::NumericToString(jobs_per_state[index]);
            total += jobs_per_state[index];
        }
        jobs_part += "&Total=" + NStr::NumericToString(total);

        for (map< string, map<string, string> >::const_iterator
                k = linked_sections.begin(); k != linked_sections.end(); ++k) {
            string  prefix((k->first).c_str() + strlen("linked_section_"));
            for (map<string, string>::const_iterator j = k->second.begin();
                    j != k->second.end(); ++j) {
                linked_sections_part += "&" + prefix + "." +
                                        NStr::URLEncode(j->first) + "=" +
                                        NStr::URLEncode(j->second);
            }
        }
        string      qname_part;
        if (!m_CommandArguments.service.empty())
            qname_part = "queue_name=" + qname + "&";

        // Include queue classes and use URL encoding
        x_WriteMessage("OK:" + qname_part +
                       params.GetPrintableParameters(true, true) +
                       jobs_part + linked_sections_part);
    } else {
        SQueueParameters    params = m_Server->QueueInfo(
                                                m_CommandArguments.qname);
        x_WriteMessage("OK:" + NStr::NumericToString(params.kind) + "\t" +
                       params.qclass + "\t\"" +
                       NStr::PrintableString(params.description) + "\"");
    }

    x_PrintCmdRequestStop();
}


void CNetScheduleHandler::x_ProcessSetQueue(CQueue*)
{
    if (m_CommandArguments.qname.empty() ||
        m_CommandArguments.qname == "noname") {
        // Disconnecting from all the queues
        m_QueueRef.Reset(NULL);
        m_QueueName.clear();

        m_ClientId.ResetPassedCheck();

        x_WriteMessage("OK:");
        x_PrintCmdRequestStop();
        return;
    }

    // Here: connecting to another queue

    CRef<CQueue>    queue_ref;
    CQueue *        queue_ptr = NULL;

    // First, deal with the given queue - try to resolve it
    try {
        queue_ref.Reset(m_Server->OpenQueue(m_CommandArguments.qname));
        queue_ptr = queue_ref.GetPointer();
    }
    catch (...) {
        x_SetCmdRequestStatus(eStatus_BadRequest);
        x_WriteMessage("ERR:eUnknownQueue:");
        x_PrintCmdRequestStop();
        return;
    }

    // Second, update the client with its capabilities for the new queue
    x_UpdateClientPassedChecks(queue_ptr);

    // Note:
    // The  m_ClientId.CheckAccess(...) call will take place when a command
    // for the changed queue is executed.


    {
        // The client has appeared for the queue - touch the client registry
        bool        client_was_found = false;
        bool        session_was_reset = false;
        string      old_session;
        bool        pref_affs_were_reset = false;

        queue_ptr->TouchClientsRegistry(m_ClientId, client_was_found,
                                        session_was_reset, old_session,
                                        pref_affs_were_reset);
        if (client_was_found && session_was_reset) {
            if (m_ConnContext.NotNull()) {
                if (pref_affs_were_reset) {
                    GetDiagContext().Extra()
                        .Print("client_node", m_ClientId.GetNode())
                        .Print("client_session", m_ClientId.GetSession())
                        .Print("client_old_session", old_session)
                        .Print("preferred_affinities_reset", "true");
                } else {
                    GetDiagContext().Extra()
                        .Print("client_node", m_ClientId.GetNode())
                        .Print("client_session", m_ClientId.GetSession())
                        .Print("client_old_session", old_session)
                        .Print("preferred_affinities_reset", "had none");
                }
            }
        }
    }

    // Final step - update the current queue reference

    m_QueueRef.Reset(queue_ref);
    m_QueueName = m_CommandArguments.qname;

    x_WriteMessage("OK:");
    x_PrintCmdRequestStop();
}


void CNetScheduleHandler::x_ProcessGetParam(CQueue* q)
{
    unsigned int                max_input_size;
    unsigned int                max_output_size;
    map< string,
         map<string, string> >  linked_sections;

    q->GetMaxIOSizesAndLinkedSections(max_input_size, max_output_size,
                                      linked_sections);

    if (m_CommandArguments.cmd == "GETP2") {
        string  result("OK:max_input_size=" +
                       NStr::NumericToString(max_input_size) + "&" +
                       "max_output_size=" +
                       NStr::NumericToString(max_output_size));

        for (map< string, map<string, string> >::const_iterator
                k = linked_sections.begin(); k != linked_sections.end(); ++k) {
            string  prefix((k->first).c_str() + strlen("linked_section_"));
            for (map<string, string>::const_iterator j = k->second.begin();
                    j != k->second.end(); ++j) {
                result += "&" + prefix + "::" +
                          NStr::URLEncode(j->first) + "=" +
                          NStr::URLEncode(j->second);
            }
        }
        x_WriteMessage(result);
    } else {
        x_WriteMessage("OK:max_input_size=" +
                       NStr::NumericToString(max_input_size) + ";"
                       "max_output_size=" +
                       NStr::NumericToString(max_output_size) + ";" +
                       NETSCHEDULED_FEATURES);
    }
    x_PrintCmdRequestStop();
}


void CNetScheduleHandler::x_ProcessGetConfiguration(CQueue* q)
{
    CQueue::TParameterList      parameters = q->GetParameters();

    ITERATE(CQueue::TParameterList, it, parameters) {
        x_WriteMessage("OK:" + it->first + '=' + it->second);
    }
    x_WriteMessage("OK:END");
    x_PrintCmdRequestStop();
}


void CNetScheduleHandler::x_ProcessReading(CQueue* q)
{
    x_CheckNonAnonymousClient("use READ command");

    CJob            job;

    x_ClearRollbackAction();
    q->GetJobForReading(m_ClientId,
                        CNSPreciseTime(m_CommandArguments.timeout, 0),
                        m_CommandArguments.group,
                        &job, m_RollbackAction);

    unsigned int    job_id = job.GetId();
    string          job_key;

    if (job_id) {
        job_key = q->MakeJobKey(job_id);
        x_WriteMessage("OK:job_key=" + job_key +
                       "&auth_token=" + job.GetAuthToken() +
                       "&status=" +
                       CNetScheduleAPI::StatusToString(job.GetStatus()));
        x_ClearRollbackAction();
    }
    else
        x_WriteMessage("OK:");

    if (m_ConnContext.NotNull()) {
        if (job_id)
            GetDiagContext().Extra().Print("job_key", job_key)
                                    .Print("auth_token", job.GetAuthToken());
        else
            GetDiagContext().Extra().Print("job_key", "None");
    }
    x_PrintCmdRequestStop();
}


void CNetScheduleHandler::x_ProcessConfirm(CQueue* q)
{
    x_CheckNonAnonymousClient("use CFRM command");
    x_CheckAuthorizationToken();

    TJobStatus      old_status = q->ConfirmReadingJob(
                                            m_ClientId,
                                            m_CommandArguments.job_id,
                                            m_CommandArguments.job_key,
                                            m_CommandArguments.auth_token);
    x_FinalizeReadCommand("CFRM", old_status);
}


void CNetScheduleHandler::x_ProcessReadFailed(CQueue* q)
{
    x_CheckNonAnonymousClient("use FRED command");
    x_CheckAuthorizationToken();

    TJobStatus      old_status = q->FailReadingJob(
                                            m_ClientId,
                                            m_CommandArguments.job_id,
                                            m_CommandArguments.job_key,
                                            m_CommandArguments.auth_token,
                                            m_CommandArguments.err_msg);
    x_FinalizeReadCommand("FRED", old_status);
}


void CNetScheduleHandler::x_ProcessReadRollback(CQueue* q)
{
    x_CheckNonAnonymousClient("use RDRB command");
    x_CheckAuthorizationToken();

    TJobStatus      old_status = q->ReturnReadingJob(
                                            m_ClientId,
                                            m_CommandArguments.job_id,
                                            m_CommandArguments.job_key,
                                            m_CommandArguments.auth_token);
    x_FinalizeReadCommand("RDRB", old_status);
}


void CNetScheduleHandler::x_FinalizeReadCommand(const string &  command,
                                                TJobStatus      old_status)
{
    if (old_status == CNetScheduleAPI::eJobNotFound) {
        ERR_POST(Warning << command << " for unknown job "
                         << m_CommandArguments.job_key);
        x_SetCmdRequestStatus(eStatus_NotFound);
        x_WriteMessage("ERR:eJobNotFound:");
        x_PrintCmdRequestStop();
        return;
    }

    if (old_status != CNetScheduleAPI::eReading) {
        string      operation = "unknown";

        if (command == "CFRM")      operation = "confirm";
        else if (command == "FRED") operation = "fail";
        else if (command == "RDRB") operation = "rollback";

        ERR_POST(Warning << "Cannot " << operation << " read job; job is in "
                         << CNetScheduleAPI::StatusToString(old_status)
                         << " state");
        x_SetCmdRequestStatus(eStatus_InvalidJobStatus);
        x_WriteMessage("ERR:eInvalidJobStatus:Cannot " +
                       operation + " job; job is in " +
                       CNetScheduleAPI::StatusToString(old_status) + " state");
    } else
        x_WriteMessage("OK:");

    x_PrintCmdRequestStop();
}


void CNetScheduleHandler::x_ProcessGetAffinityList(CQueue* q)
{
    x_WriteMessage("OK:" + q->GetAffinityList());
    x_PrintCmdRequestStop();
}


void CNetScheduleHandler::x_ProcessClearWorkerNode(CQueue* q)
{
    bool        client_found = false;
    bool        pref_affs_were_reset = false;
    string      old_session;

    q->ClearWorkerNode(m_ClientId, client_found,
                       old_session, pref_affs_were_reset);

    if (client_found) {
        if (m_ConnContext.NotNull()) {
            if (pref_affs_were_reset) {
                GetDiagContext().Extra()
                    .Print("client_node", m_ClientId.GetNode())
                    .Print("client_session", m_ClientId.GetSession())
                    .Print("client_old_session", old_session)
                    .Print("preferred_affinities_reset", "true");
            } else {
                GetDiagContext().Extra()
                    .Print("client_node", m_ClientId.GetNode())
                    .Print("client_session", m_ClientId.GetSession())
                    .Print("client_old_session", old_session)
                    .Print("preferred_affinities_reset", "had none");
            }
        }
    }

    x_WriteMessage("OK:");
    x_PrintCmdRequestStop();
}


void CNetScheduleHandler::x_ProcessCancelQueue(CQueue* q)
{
    q->CancelAllJobs(m_ClientId);
    x_WriteMessage("OK:");
    x_PrintCmdRequestStop();
}


void CNetScheduleHandler::x_ProcessRefuseSubmits(CQueue* q)
{
    if (m_CommandArguments.mode == false &&
            (m_Server->IsDrainShutdown() || m_Server->ShutdownRequested())) {
        x_SetCmdRequestStatus(eStatus_BadRequest);
        x_WriteMessage("ERR:eShuttingDown:"
                       "Server is in drained shutting down state");
        x_PrintCmdRequestStop();
        return;
    }

    if (q == NULL) {
        // This is a whole server scope request
        m_Server->SetRefuseSubmits(m_CommandArguments.mode);
        x_WriteMessage("OK:");
        x_PrintCmdRequestStop();
        return;
    }

    // This is a queue scope request.
    q->SetRefuseSubmits(m_CommandArguments.mode);

    if (m_CommandArguments.mode == false &&
            m_Server->GetRefuseSubmits() == true)
        x_WriteMessage("OK:WARNING:Submits are disabled on the server level;");
    else
        x_WriteMessage("OK:");
    x_PrintCmdRequestStop();
}


void CNetScheduleHandler::x_ProcessPause(CQueue* q)
{
    CQueue::TPauseStatus    current = q->GetPauseStatus();
    CQueue::TPauseStatus    new_value;

    if (m_CommandArguments.pullback)
        new_value = CQueue::ePauseWithPullback;
    else
        new_value = CQueue::ePauseWithoutPullback;

    q->SetPauseStatus(new_value);
    if (current == CQueue::eNoPause)
        x_WriteMessage("OK:");
    else {
        string  reply = "OK:WARNING:The queue has "
                        "already been paused (previous pullback value is ";
        if (current == CQueue::ePauseWithPullback)  reply += "true";
        else                                        reply += "false";
        reply += ", new pullback value is ";
        if (m_CommandArguments.pullback)            reply += "true";
        else                                        reply += "false";
        x_WriteMessage(reply + ");");
    }
    x_PrintCmdRequestStop();
}


void CNetScheduleHandler::x_ProcessResume(CQueue* q)
{
    CQueue::TPauseStatus    current = q->GetPauseStatus();

    q->SetPauseStatus(CQueue::eNoPause);
    if (current == CQueue::eNoPause)
        x_WriteMessage("OK:WARNING:The queue is not paused;");
    else
        x_WriteMessage("OK:");
    x_PrintCmdRequestStop();
}


void CNetScheduleHandler::x_CmdNotImplemented(CQueue *)
{
    x_SetCmdRequestStatus(eStatus_NotImplemented);
    x_WriteMessage("ERR:eObsoleteCommand:");
    x_PrintCmdRequestStop();
}


void CNetScheduleHandler::x_CmdObsolete(CQueue*)
{
    x_SetCmdRequestStatus(eStatus_NotImplemented);
    x_WriteMessage("OK:WARNING:Obsolete;");
    x_PrintCmdRequestStop();
}


void CNetScheduleHandler::x_CheckNonAnonymousClient(const string &  message)
{
    if (!m_ClientId.IsComplete())
        NCBI_THROW(CNetScheduleException, eInvalidClient,
                   "Anonymous client (no client_node and client_session"
                   " at handshake) cannot " + message);
}


void CNetScheduleHandler::x_CheckPortAndTimeout(void)
{
    if ((m_CommandArguments.port != 0 &&
         m_CommandArguments.timeout == 0) ||
        (m_CommandArguments.port == 0 &&
         m_CommandArguments.timeout != 0))
        NCBI_THROW(CNetScheduleException, eInvalidParameter,
                   "Either both or neither of the port and "
                   "timeout parameters must be 0");
}


void CNetScheduleHandler::x_CheckAuthorizationToken(void)
{
    if (m_CommandArguments.auth_token.empty())
        NCBI_THROW(CNetScheduleException, eInvalidAuthToken,
                   "Invalid authorization token. It cannot be empty.");
}


void CNetScheduleHandler::x_CheckGetParameters(void)
{
    // Checks that the given GETx/JXCG parameters make sense
    if (m_CommandArguments.wnode_affinity == false &&
        m_CommandArguments.any_affinity == false &&
        m_CommandArguments.affinity_token.empty()) {
        ERR_POST(Warning << "The job request without explicit affinities, "
                            "without preferred affinities and "
                            "with any_aff flag set to false "
                            "will never match any job.");
        }
    if (m_CommandArguments.exclusive_new_aff == true &&
        m_CommandArguments.any_affinity == true)
        NCBI_THROW(CNetScheduleException, eInvalidParameter,
                   "It is forbidden to have both any_affinity and "
                   "exclusive_new_aff GET2 flags set to 1.");
}


void CNetScheduleHandler::x_CheckQInf2Parameters(void)
{
    // One of the arguments must be provided: qname or service
    if (m_CommandArguments.qname.empty() &&
        m_CommandArguments.service.empty())
        NCBI_THROW(CNetScheduleException, eInvalidParameter,
                   "QINF2 command expects a queue name or a service name. "
                   "Nothing has been provided.");

    if (!m_CommandArguments.qname.empty() &&
        !m_CommandArguments.service.empty())
        NCBI_THROW(CNetScheduleException, eInvalidParameter,
                   "QINF2 command expects only one value: queue name or "
                   "a service name. Both have been provided.");
}


void CNetScheduleHandler::x_PrintCmdRequestStart(const SParsedCmd& cmd)
{
    if (m_CmdContext.NotNull()) {
        CDiagContext::SetRequestContext(m_CmdContext);
        CDiagContext_Extra    ctxt_extra =
                GetDiagContext().PrintRequestStart()
                            .Print("_type", "cmd")
                            .Print("_queue", m_QueueName)
                            .Print("cmd", cmd.command->cmd)
                            .Print("peer", GetSocket().GetPeerAddress(eSAF_IP))
                            .Print("conn", m_ConnContext->GetRequestID());

        // SUMBIT parameters should not be logged. The new job attributes will
        // be logged when a new job is actually submitted.
        if (cmd.command->extra.processor != &CNetScheduleHandler::x_ProcessSubmit) {
            ITERATE(TNSProtoParams, it, cmd.params) {
                if (it->first == "ip")  continue;
                if (it->first == "sid") continue;
                ctxt_extra.Print(it->first, it->second);
            }
        }
        ctxt_extra.Flush();

        // Workaround:
        // When extra of the GetDiagContext().PrintRequestStart() is destroyed
        // or flushed it also resets the status to 0 so I need to set it here
        // to 200 though it was previously set to 200 when the request context
        // is created.
        m_CmdContext->SetRequestStatus(eStatus_OK);
    }
}


void CNetScheduleHandler::x_PrintCmdRequestStart(CTempString  msg)
{
    if (m_CmdContext.NotNull()) {
        CDiagContext::SetRequestContext(m_CmdContext);
        GetDiagContext().PrintRequestStart()
                .Print("_type", "cmd")
                .Print("_queue", m_QueueName)
                .Print("info", msg)
                .Print("peer",  GetSocket().GetPeerAddress(eSAF_IP))
                .Print("conn", m_ConnContext->GetRequestID())
                .Flush();

        // Workaround:
        // When extra of the GetDiagContext().PrintRequestStart() is destroyed
        // or flushed it also resets the status to 0 so I need to set it here
        // to 200 though it was previously set to 200 when the request context
        // is created.
        m_CmdContext->SetRequestStatus(eStatus_OK);
    }
}


void CNetScheduleHandler::x_PrintCmdRequestStop(void)
{
    if (m_CmdContext.NotNull()) {
        CDiagContext::SetRequestContext(m_CmdContext);
        GetDiagContext().PrintRequestStop();
        CDiagContext::SetRequestContext(m_ConnContext);
        m_CmdContext.Reset();
    }
}

// The function forms a responce for various 'get job' commands and prints
// extra to the log if required
void
CNetScheduleHandler::x_PrintGetJobResponse(const CQueue *  q,
                                           const CJob &    job,
                                           bool            cmdv2)
{
    if (!job.GetId()) {
        // No suitable job found
        if (m_ConnContext.NotNull())
            GetDiagContext().Extra().Print("job_key", "None");
        x_WriteMessage("OK:");
        return;
    }

    string      job_key = q->MakeJobKey(job.GetId());
    if (m_ConnContext.NotNull()) {
        // The only piece required for logging is the job key
        GetDiagContext().Extra().Print("job_key", job_key);
    }

    if (cmdv2)
        x_WriteMessage("OK:job_key=" + job_key +
                       "&input=" + NStr::URLEncode(job.GetInput()) +
                       "&affinity=" + NStr::URLEncode(q->GetAffinityTokenByID(job.GetAffinityId())) +
                       "&client_ip=" + NStr::URLEncode(job.GetClientIP()) +
                       "&client_sid=" + NStr::URLEncode(job.GetClientSID()) +
                       "&mask=" + NStr::NumericToString(job.GetMask()) +
                       "&auth_token=" + job.GetAuthToken());
    else
        x_WriteMessage("OK:" + job_key +
                       " \"" + NStr::PrintableString(job.GetInput()) + "\""
                       " \"" + NStr::PrintableString(
                                q->GetAffinityTokenByID(job.GetAffinityId())) + "\""
                       " \"" + job.GetClientIP() + " " + job.GetClientSID() + "\""
                       " " + NStr::NumericToString(job.GetMask()));
}


bool CNetScheduleHandler::x_CanBeWithoutQueue(FProcessor  processor) const
{
    return processor == &CNetScheduleHandler::x_ProcessStatus ||                // STATUS/STATUS2
           processor == &CNetScheduleHandler::x_ProcessDump ||                  // DUMP
           processor == &CNetScheduleHandler::x_ProcessGetMessage ||            // MGET
           processor == &CNetScheduleHandler::x_ProcessFastStatusS ||           // SST/SST2
           processor == &CNetScheduleHandler::x_ProcessListenJob ||             // LISTEN
           processor == &CNetScheduleHandler::x_ProcessCancel ||                // CANCEL
           processor == &CNetScheduleHandler::x_ProcessPutMessage ||            // MPUT
           processor == &CNetScheduleHandler::x_ProcessFastStatusW ||           // WST/WST2
           processor == &CNetScheduleHandler::x_ProcessPut ||                   // PUT/PUT2
           processor == &CNetScheduleHandler::x_ProcessReturn ||                // RETURN/RETURN2
           processor == &CNetScheduleHandler::x_ProcessPutFailure ||            // FPUT/FPUT2
           processor == &CNetScheduleHandler::x_ProcessJobExchange ||           // JXCG
           processor == &CNetScheduleHandler::x_ProcessJobDelayExpiration ||    // JDEX
           processor == &CNetScheduleHandler::x_ProcessConfirm ||               // CFRM
           processor == &CNetScheduleHandler::x_ProcessReadFailed ||            // FRED
           processor == &CNetScheduleHandler::x_ProcessReadRollback;            // RDRB
}


void CNetScheduleHandler::x_ClearRollbackAction(void)
{
    if (m_RollbackAction != NULL) {
        delete m_RollbackAction;
        m_RollbackAction = NULL;
    }
}


void CNetScheduleHandler::x_ExecuteRollbackAction(CQueue * q)
{
    if (m_RollbackAction == NULL)
        return;
    m_RollbackAction->Rollback(q);
    x_ClearRollbackAction();
}


void CNetScheduleHandler::x_CreateConnContext(void)
{
    CSocket &       socket = GetSocket();

    m_ConnContext.Reset(new CRequestContext());
    m_ConnContext->SetRequestID();
    m_ConnContext->SetClientIP(socket.GetPeerAddress(eSAF_IP));

    // Set the connection request as the current one and print request start
    CDiagContext::SetRequestContext(m_ConnContext);
    GetDiagContext().PrintRequestStart()
                    .Print("_type", "conn");
    m_ConnContext->SetRequestStatus(eStatus_OK);
}


static const unsigned int   kMaxParserErrMsgLength = 128;

// Writes into socket, logs the message and closes the connection
void CNetScheduleHandler::x_OnCmdParserError(bool            need_request_start,
                                             const string &  msg,
                                             const string &  suffix)
{
    // Truncating is done to prevent output of an arbitrary long garbage

    if (need_request_start) {
        CDiagContext::GetRequestContext().SetClientIP(GetSocket().GetPeerAddress(eSAF_IP));
        x_PrintCmdRequestStart("Invalid command");
    }

    if (msg.size() < kMaxParserErrMsgLength * 2)
        ERR_POST("Error parsing command: " << msg + suffix);
    else
        ERR_POST("Error parsing command: " <<
                 msg.substr(0, kMaxParserErrMsgLength * 2) <<
                 " (TRUNCATED)" + suffix);
    x_SetCmdRequestStatus(eStatus_BadRequest);

    x_SetConnRequestStatus(eStatus_BadRequest);
    string client_error = "ERR:eProtocolSyntaxError:";
    if (msg.size() < kMaxParserErrMsgLength)
        client_error += msg + suffix;
    else
        client_error += msg.substr(0, kMaxParserErrMsgLength) +
                        " (TRUNCATED)" + suffix;

    if (x_WriteMessage(client_error) == eIO_Success)
        m_Server->CloseConnection(&GetSocket());
}


void
CNetScheduleHandler::x_StatisticsNew(CQueue *                q,
                                     const string &          what,
                                     const CNSPreciseTime &  curr)
{
    bool   verbose = (m_CommandArguments.comment == "VERBOSE");
    string info;

    if (what == "CLIENTS") {
        info = q->PrintClientsList(verbose);
        if (!info.empty())
            x_WriteMessage(info);
    }
    else if (what == "NOTIFICATIONS") {
        info = q->PrintNotificationsList(verbose);
        if (!info.empty())
            x_WriteMessage(info);
    }
    else if (what == "AFFINITIES") {
        info = q->PrintAffinitiesList(verbose);
        if (!info.empty())
            x_WriteMessage(info);
    }
    else if (what == "GROUPS") {
        info = q->PrintGroupsList(verbose);
        if (!info.empty())
            x_WriteMessage(info);
    }
    else if (what == "JOBS") {
        x_WriteMessage(q->PrintJobsStat(m_CommandArguments.group,
                                        m_CommandArguments.affinity_token));
    }
    else if (what == "WNODE") {
        x_WriteMessage("OK:WARNING:Obsolete, use STAT CLIENTS instead;");
    }

    x_WriteMessage("OK:END");
    x_PrintCmdRequestStop();
}


string CNetScheduleHandler::x_GetServerSection(void) const
{
    CNetScheduleDApp *      app = dynamic_cast<CNetScheduleDApp*>(CNcbiApplication::Instance());
    SServer_Parameters      server_params;
    m_Server->GetParameters(&server_params);

    return "[server]\n"
           "reinit=\"" + NStr::BoolToString(app->GetEffectiveReinit()) + "\"\n"
           "max_connections=\"" + NStr::NumericToString(server_params.max_connections) + "\"\n"
           "max_threads=\"" + NStr::NumericToString(server_params.max_threads) + "\"\n"
           "init_threads=\"" + NStr::NumericToString(server_params.init_threads) + "\"\n"
           "port=\"" + NStr::NumericToString(m_Server->GetPort()) + "\"\n"
           "use_hostname=\"" + NStr::BoolToString(m_Server->GetUseHostname()) + "\"\n"
           "network_timeout=\"" + NStr::NumericToString(m_Server->GetInactivityTimeout()) + "\"\n"
           "log=\"" + NStr::BoolToString(m_Server->IsLog()) + "\"\n"
           "log_batch_each_job=\"" + NStr::BoolToString(m_Server->IsLogBatchEachJob()) + "\"\n"
           "log_notification_thread=\"" + NStr::BoolToString(m_Server->IsLogNotificationThread()) + "\"\n"
           "log_cleaning_thread=\"" + NStr::BoolToString(m_Server->IsLogCleaningThread()) + "\"\n"
           "log_execution_watcher_thread=\"" + NStr::BoolToString(m_Server->IsLogExecutionWatcherThread()) + "\"\n"
           "log_statistics_thread=\"" + NStr::BoolToString(m_Server->IsLogStatisticsThread()) + "\"\n"
           "del_batch_size=\"" + NStr::NumericToString(m_Server->GetDeleteBatchSize()) + "\"\n"
           "markdel_batch_size=\"" + NStr::NumericToString(m_Server->GetMarkdelBatchSize()) + "\"\n"
           "scan_batch_size=\"" + NStr::NumericToString(m_Server->GetScanBatchSize()) + "\"\n"
           "purge_timeout=\"" + NStr::NumericToString(m_Server->GetPurgeTimeout()) + "\"\n"
           "stat_interval=\"" + NStr::NumericToString(m_Server->GetStatInterval()) + "\"\n"
           "max_affinities=\"" + NStr::NumericToString(m_Server->GetMaxAffinities()) + "\"\n"
           "admin_host=\"" + m_Server->GetAdminHosts().GetAsFromConfig() + "\"\n"
           "admin_client_name=\"" + m_Server->GetAdminClientNames() + "\"\n"
           "affinity_high_mark_percentage=\"" + NStr::NumericToString(m_Server->GetAffinityHighMarkPercentage()) + "\"\n"
           "affinity_low_mark_percentage=\"" + NStr::NumericToString(m_Server->GetAffinityLowMarkPercentage()) + "\"\n"
           "affinity_high_removal=\"" + NStr::NumericToString(m_Server->GetAffinityHighRemoval()) + "\"\n"
           "affinity_low_removal=\"" + NStr::NumericToString(m_Server->GetAffinityLowRemoval()) + "\"\n"
           "affinity_dirt_percentage=\"" + NStr::NumericToString(m_Server->GetAffinityDirtPercentage()) + "\"\n";
}


string
CNetScheduleHandler::x_GetStoredSectionValues(const string &  section_name,
                                              const map<string, string> & values) const
{
    if (values.empty())
        return "";

    string  ret = "[" + section_name + "]\n";
    for (map<string, string>::const_iterator k = values.begin();
         k != values.end(); ++k)
        ret += k->first + "=\"" + k->second + "\"\n";
    return ret;
}

string CNetScheduleHandler::x_GetLogSection(void) const
{
    CNetScheduleDApp *      app = dynamic_cast<CNetScheduleDApp*>(CNcbiApplication::Instance());
    return x_GetStoredSectionValues("log", app->GetOrigLogSection());
}


string CNetScheduleHandler::x_GetDiagSection(void) const
{
    CNetScheduleDApp *      app = dynamic_cast<CNetScheduleDApp*>(CNcbiApplication::Instance());
    return x_GetStoredSectionValues("diag", app->GetOrigDiagSection());
}


string CNetScheduleHandler::x_GetBdbSection(void) const
{
    CNetScheduleDApp *      app = dynamic_cast<CNetScheduleDApp*>(CNcbiApplication::Instance());
    return x_GetStoredSectionValues("bdb", app->GetOrigBDBSection());
}

