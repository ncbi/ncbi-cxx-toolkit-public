#ifndef NETCACHE__MESSAGE_HANDLER__HPP
#define NETCACHE__MESSAGE_HANDLER__HPP
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
 * Authors:  Pavel Ivanov
 *
 * File Description: Network cache daemon
 *
 */

#include <connect/services/netservice_protocol_parser.hpp>

#include "nc_utils.hpp"


BEGIN_NCBI_SCOPE


class CNCActiveClientHub;
class CNCBlobAccessor;
struct SNCSpecificParams;
struct SNCBlobVerData;
struct SNCBlobSummary;



enum ENCCmdFlags {
    fNeedsBlobAccess    = 1 <<  0,
    fCanGenerateKey     = 1 <<  1,
    fNeedsStorageCache  = 1 <<  2,
    fDoNotCheckPassword = 1 <<  3,
    fDoNotProxyToPeers  = 1 <<  4,
    fUsesPeerSearch     = 1 <<  5,
    fPeerFindExistsOnly = 1 <<  6,
    fNeedsSpaceAsClient = 1 <<  7,
    fNeedsSpaceAsPeer   = 1 <<  8,
    fSwapLengthBytes    = 1 <<  9,  ///< Byte order should be swapped when
                                    ///< reading length of chunks in blob
                                    ///< transfer protocol
    fConfirmOnFinish    = 1 << 10,
    fReadExactBlobSize  = 1 << 11,  ///< There is exact size of the blob
                                    ///< transferred to NetCache
    fSkipBlobEOF        = 1 << 12,
    fCopyLogEvent       = 1 << 13,
    fNeedsAdminClient   = 1 << 14,
    fRunsInStartedSync  = 1 << 15,
    fProhibitsSyncAbort = 1 << 16,
    fNeedsLowerPriority = 1 << 17,
    fNoBlobVersionCheck = 1 << 18,
    fNoBlobAccessStats  = 1 << 19,
    fSyncCmdSuccessful  = 1 << 20,
    fCursedPUT2Cmd      = 1 << 21,
    fComesFromClient    = 1 << 22,


    eProxyBlobRead      = fNeedsBlobAccess + fUsesPeerSearch,
    eClientBlobRead     = eProxyBlobRead + fComesFromClient,
    eProxyBlobWrite     = fNeedsBlobAccess + fNeedsSpaceAsClient,
    eClientBlobWrite    = eProxyBlobWrite + fComesFromClient,
    eCopyBlobFromPeer   = fNeedsBlobAccess + fNeedsStorageCache + fDoNotProxyToPeers
                          + fDoNotCheckPassword + fNeedsAdminClient + fConfirmOnFinish,
    eRunsInStartedSync  = fRunsInStartedSync + fNeedsAdminClient + fNeedsLowerPriority
                          + fNeedsStorageCache,
    eSyncBlobCmd        = eRunsInStartedSync + fNeedsBlobAccess + fDoNotProxyToPeers
                          + fDoNotCheckPassword
};
typedef Uint4 TNCCmdFlags;

enum ENCProxyCmd {
    eProxyRead = 1,
    eProxyWrite,
    eProxyHasBlob,
    eProxyGetSize,
    eProxyReadLast,
    eProxySetValid,
    eProxyRemove,
    eProxyGetMeta,
    eProxyProlong
} NCBI_PACKED_ENUM_END();


/// Handler of all NetCache incoming requests.
/// Handler written to be reusable object so that if one connection to
/// NetCache is closed then handler from it can go to serve for another newly
/// opened connection. All handling is implemented as finite state machine
/// with central place of state dispatching - x_ManageCmdPipeline - and with
/// transitions between states implemented inside state processors. So that
/// each state processor knows where it can go then, central part only knows
/// which method process which state.
class CNCMessageHandler : public CSrvSocketTask,
                          public CSrvStatesTask<CNCMessageHandler>
{
public:
    /// Create handler for the given NetCache server object
    CNCMessageHandler(void);
    virtual ~CNCMessageHandler(void);

    bool IsBlobWritingFinished(void);

    /// Extra information about each NetCache command
    struct SCommandExtra {
        /// Which method will process the command
        State           processor;
        /// Command name to present to user in statistics
        const char*     cmd_name;
        TNCCmdFlags     cmd_flags;
        /// What access to the blob command should receive
        ENCAccessType   blob_access;
        ENCProxyCmd     proxy_cmd;
    };
    /// Type definitions for NetCache protocol parser
    typedef SNSProtoCmdDef<SCommandExtra>      SCommandDef;
    typedef SNSProtoParsedCmd<SCommandExtra>   SParsedCmd;
    typedef CNetServProtoParser<SCommandExtra> TProtoParser;

    /// Command processors
    State x_DoCmd_Health(void);
    State x_DoCmd_Shutdown(void);
    State x_DoCmd_Version(void);
    State x_DoCmd_GetConfig(void);
    State x_DoCmd_GetStat(void);
    State x_DoCmd_Put(void);
    State x_DoCmd_Get(void);
    State x_DoCmd_GetLast(void);
    State x_DoCmd_SetValid(void);
    State x_DoCmd_GetSize(void);
    State x_DoCmd_Prolong(void);
    State x_DoCmd_HasBlob(void);
    State x_DoCmd_Remove(void);
    State x_DoCmd_IC_Store(void);
    State x_DoCmd_SyncStart(void);
    State x_DoCmd_SyncBlobsList(void);
    State x_DoCmd_CopyPut(void);
    State x_DoCmd_CopyProlong(void);
    State x_DoCmd_SyncGet(void);
    State x_DoCmd_SyncProlongInfo(void);
    State x_DoCmd_SyncCommit(void);
    State x_DoCmd_SyncCancel(void);
    State x_DoCmd_GetMeta(void);
    State x_DoCmd_ProxyMeta(void);
    //State x_DoCmd_GetBlobsList(void);
    /// Universal processor for all commands not implemented now.
    State x_DoCmd_NotImplemented(void);

    State x_FinishCommand(void);

private:
    /// Set additional machine state flag
    void x_SetFlag  (ENCCmdFlags flag);
    /// Remove additional machine state flag
    void x_UnsetFlag(ENCCmdFlags flag);
    /// Check if additional machine state flag is set
    bool x_IsFlagSet(ENCCmdFlags flag);
    unsigned int x_GetBlobTTL(void);

    // State machine implementation

    State x_SocketOpened(void);
    State x_CloseCmdAndConn(void);
    State x_SaveStatsAndClose(void);
    State x_PrintCmdsCntAndClose(void);
    /// Main dispatcher of state machine
    void x_ManageCmdPipeline(void);
    /// Read authentication message from client
    State x_ReadAuthMessage(void);
    /// Read command and start it if it's available
    State x_ReadCommand(void);
    /// Process "waiting" for blob locking. In fact just shift to next state
    /// if lock is acquired and just return if not.
    State x_WaitForBlobAccess(void);
    State x_ReportBlobNotFound(void);
    State x_CloseOnPeerError(void);
    /// Read signature in blob transfer protocol
    State x_ReadBlobSignature(void);
    /// Read length of chunk in blob transfer protocol
    State x_ReadBlobChunkLength(void);
    /// Read chunk data in blob transfer protocol
    State x_ReadBlobChunk(void);
    /// Write data from blob to socket
    State x_WriteBlobData(void);
    State x_WriteSendBuff(void);
    State x_ProxyToNextPeer(void);
    State x_SendCmdAsProxy(void);
    State x_WaitForPeerAnswer(void);
    State x_WriteInitWriteResponse(void);
    State x_ReadMetaNextPeer(void);
    State x_SendGetMetaCmd(void);
    State x_ReadMetaResults(void);
    State x_ExecuteOnLatestSrvId(void);
    State x_PutToNextPeer(void);
    State x_SendPutToPeerCmd(void);
    State x_ReadPutResults(void);

    /// Close connection (or at least make CServer believe that we closed it
    /// by ourselves)
    void x_CloseConnection(void);
    /// Assign command parameters returned by protocol parser to handler
    /// member variables
    void x_AssignCmdParams(void);
    /// Print "request_start" message into diagnostics with all parameters of
    /// the current command.
    void x_PrintRequestStart(CSrvDiagMsg& diag_msg);
    /// Start command returned by protocol parser
    State x_StartCommand(void);
    /// Command execution is finished, do cleanup work
    void x_CleanCmdResources(void);
    /// Start reading blob from socket
    State x_StartReadingBlob(void);
    /// Client finished sending blob, do cleanup
    State x_FinishReadingBlob(void);

    void x_ProlongBlobDeadTime(int add_time);
    void x_ProlongVersionLife(void);
    void x_WriteFullBlobsList(void);
    void x_GetCurSlotServers(void);


    TNCCmdFlags               m_Flags;
    /// NetCache protocol parser
    TProtoParser              m_Parser;
    SParsedCmd                m_ParsedCmd;
    /// Processor for the currently executed NetCache command
    State                     m_CmdProcessor;
    ///
    string                    m_ConnReqId;
    ///
    Uint8                     m_CntCmds;
    /// Holder of the lock for blob
    CNCBlobAccessor*          m_BlobAccess;
    /// Length of the current blob chunk to read from socket
    Uint4                     m_ChunkLen;

    Uint2                     m_LocalPort;
    /// 
    TStringMap                m_ClientParams;
    ///
    const SNCSpecificParams*  m_AppSetup;
    ///
    const SNCSpecificParams*  m_BaseAppSetup;
    ///
    string                    m_PrevCache;
    ///
    const SNCSpecificParams*  m_PrevAppSetup;
    /// Time when command started execution
    CSrvTime                  m_CmdStartTime;

    ///
    string                    m_RawKey;
    /// Blob key in current command
    string                    m_BlobKey;
    /// Blob version in current command
    int                       m_BlobVersion;
    /// "Password" to access the blob
    string                    m_BlobPass;
    /// Version of the blob key to generate
    unsigned int              m_KeyVersion;
    /// Time-to-live value for the blob
    unsigned int              m_BlobTTL;
    /// Offset to start blob reading from
    Uint8                      m_StartPos;
    /// Exact size of the blob sent by client
    Uint8                     m_Size;
    Uint8                     m_BlobSize;
    SNCBlobVerData*           m_CopyBlobInfo;
    Uint8                     m_OrigRecNo;
    Uint8                     m_OrigSrvId;
    Uint8                     m_OrigTime;
    Uint8                     m_SrvId;
    Uint2                     m_Slot;
    Uint8                     m_LocalRecNo;
    Uint8                     m_RemoteRecNo;
    auto_ptr<TNCBufferType>   m_SendBuff;
    size_t                    m_SendPos;
    string                    m_RawBlobPass;
    Uint8                     m_SyncId;
    Uint2                     m_BlobSlot;
    Uint2                     m_TimeBucket;
    Uint1                     m_Quorum;
    bool                      m_SearchOnRead;
    bool                      m_ThisServerIsMain;
    bool                      m_LatestExist;
    bool                      m_ForceLocal;
    bool                      m_StatPrev;
    Uint1                     m_SrvsIndex;
    int                       m_CmdVersion;
    Uint8                     m_LatestSrvId;
    SNCBlobSummary*           m_LatestBlobSum;
    TServersList              m_CheckSrvs;
    CNCActiveClientHub*       m_ActiveHub;
    string                    m_LastPeerError;
    string                    m_StatType;
};


class CNCMsgHandler_Factory : public CSrvSocketFactory
{
public:
    CNCMsgHandler_Factory(void);
    virtual ~CNCMsgHandler_Factory(void);

    virtual CSrvSocketTask* CreateSocketTask(void);
};



//////////////////////////////////////////////////////////////////////////
// Inline functions
//////////////////////////////////////////////////////////////////////////

inline bool
CNCMessageHandler::IsBlobWritingFinished(void)
{
    return m_ChunkLen == 0xFFFFFFFF;
}

END_NCBI_SCOPE

#endif /* NETCACHE__MESSAGE_HANDLER__HPP */
