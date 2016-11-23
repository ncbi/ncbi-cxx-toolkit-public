#ifndef NETCACHE_NC_UTILS__HPP
#define NETCACHE_NC_UTILS__HPP
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
 * File Description: 
 *   Utility classes that now are used only in NetCache but can be used
 *   anywhere else.
 */


#include <util/simple_buffer.hpp>


#ifdef NCBI_COMPILER_GCC
# define ATTR_PACKED    __attribute__ ((packed))
# define ATTR_ALIGNED_8 __attribute__ ((aligned(8)))
#else
# define ATTR_PACKED
# define ATTR_ALIGNED_8
#endif



BEGIN_NCBI_SCOPE


static const char* const kNCPeerClientName = "nc_peer";


typedef map<string, string> TStringMap;
typedef map<Uint8, string>  TNCPeerList;
typedef vector<Uint8>       TServersList;
//typedef CSimpleBufferT<char> TNCBufferType;
class TNCBufferType : public CSimpleBufferT<char>
{
public:
    TNCBufferType& WriteText(const char* buf) {
        CSimpleBufferT<char>::append(buf, strlen(buf));
        return *this;
    }
    TNCBufferType& WriteText(const string& buf) {
        CSimpleBufferT<char>::append(buf.data(), buf.size());
        return *this;
    }
    template <typename NumType>
    TNCBufferType& WriteNumber(NumType num) {
        return WriteText(NStr::NumericToString(num));
    }
    TNCBufferType& WriteBool(bool b) {
        return WriteText(b ? "true" : "false");
    }
};


/// Type of access to NetCache blob
enum ENCAccessType {
    eNCNone = 0,
    eNCRead,        ///< Read meta information only
    eNCReadData,    ///< Read blob data
    eNCCreate,      ///< Create blob or re-write its contents
    eNCCopyCreate   ///< (Re-)write blob from another NetCache (as opposed to
                    ///< writing from client)
} NCBI_PACKED_ENUM_END();


/// Statuses of commands to be set in diagnostics' request context
/// Additional statuses can be taken from
/// http://www.w3.org/Protocols/rfc2616/rfc2616-sec10.html
enum EHTTPStatus {
// synchronization-related
    /// SYNC_START: synchronization by blobs list will be performed.
    eStatus_SyncBList   = 121,
    /// SYNC_START: synchronization by events will be performed.
    eStatus_SyncEvents  = 122,
    /// Same or newer blob is present on the server.
    eStatus_NewerBlob   = 123,
    /// Synchronization cannot start because server is busy doing cleaning or
    /// some other synchronization on this slot.
    eStatus_SyncBusy    = 124,
    /// Synchronization is rejected because both servers tried to start it
    /// simultaneously.
    eStatus_CrossSync   = 125,

// success
    /// Command is ok and execution is good.
    eStatus_OK          = 200,
    /// New resource has been created
    eStatus_Created     = 201,
    /// The server is delivering only part of the resource
    eStatus_PartialContent = 206,

// errors
    /// Command is incorrect.
    eStatus_BadCmd      = 400,
    /// Bad password for accessing the blob.
    eStatus_BadPassword = 401,
    /// Client was disabled in configuration.
    eStatus_Disabled    = 403,
    /// Blob was not found.
    eStatus_NotFound    = 404,
    /// Operation not allowed with current settings (e.g. password given but
    /// ini-file says that only blobs without password are allowed)
    eStatus_NotAllowed  = 405,
    /// Command requires admin privileges.
    eStatus_NeedAdmin   = 407,
    /// Command timeout is exceeded.
    eStatus_CmdTimeout  = 408,
    /// SETVALID cannot be executed as new version was already written.
    eStatus_RaceCond    = 409,
    /// Blob version did not match
    sStatus_BlobVersion = 410,
    /// Precondition stated in command has failed (size of blob was given but
    /// data has a different size)
    eStatus_CondFailed  = 412,
    /// Blob size exceeds the allowed maximum.
    eStatus_BlobTooBig  = 413,
    /// Connection was closed too early (client didn't send all data or didn't
    /// get confirmation about successful execution).
    eStatus_PrematureClose = 499,
    /// Internal server error.
    eStatus_ServerError = 500,
    eStatus_CmdAborted  = 500,
    /// Command is not implemented.
    eStatus_NoImpl      = 501,
    /// Peer returned something wrong
    eStatus_BadPeer     = 502,
    /// operation canceled because server needs to shutdown.
    eStatus_ShuttingDown = 503,
    eStatus_ServiceUnavailable = 503,
    /// Command cannot be executed because NetCache didn't cache the database
    /// contents yet.
    eStatus_JustStarted  = 503,
    /// Command should be proxied to peers but it's impossible to connect to any
    eStatus_PeerError   = 504,
    /// There's not enough disk space to execute the command.
    eStatus_NoDiskSpace = 507,
    /// Synchronization is aborted because something went wrong.
    eStatus_SyncAborted = 520
};


/// Initializes maps between status codes and error texts sent to client
/// to explain these codes.
void InitClientMessages(void);
EHTTPStatus GetStatusByMessage(const string& msg, EHTTPStatus def);
const string& GetMessageByStatus(EHTTPStatus sts);


/////////////////////////////////////////////////////////////////////////////
// alerts

class CNCAlerts
{
public:
    enum EAlertType {
        eUnknown,
        eStartupConfigChanged,  ///< Configuration file changed
        ePidFileFailed,         ///< Reporting Pid failed
        eStartAfterCrash,       ///< InstanceGuard file was present on startup
        eStorageReinit,         ///< Data storage was reinitialized
        eAccessDenied,          ///< Command was rejected because client lacks administrative privileges
        eSyncFailed,            ///< Synchronization failed
        ePeerIpChanged,         ///< Peer IP address changed
        eDiskSpaceNormal,       ///< Free disk space is back to normal
        eDatabaseTooLarge,      ///< Database is too large (warning)
        eDatabaseOverLimit,     ///< Database size exceeded its limit (error, stop write)
        eDiskSpaceLow,          ///< Free disk space is below threshold
        eDiskSpaceCritical,     ///< Free disk space is below critical threshold
#ifdef _DEBUG
// it is convenient way to keep track of some events
        eDebugOrphanRecordFound,
        eDebugOrphanRecordFound2,
        eDebugWriteBlobInfoFailed,
        eDebugReadBlobInfoFailed0,
        eDebugReadBlobInfoFailed1,
        eDebugReadBlobInfoFailed2,
        eDebugUpdateUpCoords1,
        eDebugUpdateUpCoords2,
        eDebugWriteBlobInfo1,
        eDebugWriteBlobInfo2,
        eDebugWriteBlobInfo3,
        eDebugWriteBlobInfo4,
        eDebugMoveDataToGarbage,
        eDebugReadMapInfo1,
        eDebugReadMapInfo2,
        eDebugReadChunkData1,
        eDebugReadChunkData2,
        eDebugReadChunkData3,
        eDebugReadChunkData4,
        eDebugDeleteNextData1,
        eDebugDeleteNextData2,
        eDebugDeleteVersionData,
        eDebugSaveOneMapImpl1,
        eDebugSaveOneMapImpl2,
        eDebugWriteChunkData1,
        eDebugWriteChunkData2,
        eDebugMoveRecord0,
        eDebugMoveRecord1,
        eDebugMoveRecord2,
        eDebugMoveRecord3,
        eDebugDeleteFile,
        eDebugReleaseCacheData1,
        eDebugReleaseCacheData2,
        eDebugDeleteSNCBlobVerData,
        eDebugDeleteCNCBlobVerManager,
        eDebugExtraWrite,
        eDebugCacheDeleted1,
        eDebugCacheDeleted2,
        eDebugCacheDeleted3,
        eDebugCacheDeleted4,
        eDebugCacheFailedMgrAttach,
        eDebugCacheFailedMgrDetach,
        eDebugCacheWrongMgr,
        eDebugCacheWrong,
        eDebugWrongCacheFound1,
        eDebugWrongCacheFound2,
        eDebugSyncAborted1,
        eDebugSyncAborted2,
        eDebugConnAdjusted1,
        eDebugConnAdjusted2,
#endif
        eLastAlert
    };

    enum EAlertAckResult {
        eNotFound,
        eAcknowledged
    };

    static void Register(EAlertType alert_type, const string&  message);
    static EAlertAckResult Acknowledge(const string&   alert_id,   const string&  user);
    static void Report(CSrvSocketTask& task, bool report_all);

private:
    static EAlertType x_IdToType(const string&  alert_id);
    static string     x_TypeToId(EAlertType  type);
};

END_NCBI_SCOPE

#endif /* NETCACHE_NC_UTILS__HPP */
