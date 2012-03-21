#ifndef CONNECT_SERVICES__REMOTE_APP_MB_HPP
#define CONNECT_SERVICES__REMOTE_APP_MB_HPP

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
 * Authors:  Maxim Didneko, Dmitry Kazimirov
 *
 * File Description:
 *
 */

#include <connect/services/netcache_api.hpp>

#include <connect/connect_export.h>

#include <corelib/ncbimisc.hpp>
#include <corelib/ncbiobj.hpp>
#include <corelib/ncbistre.hpp>

#include <vector>

BEGIN_NCBI_SCOPE

const size_t kMaxBlobInlineSize = 500;

enum EStdOutErrStorageType {
    eLocalFile = 0,
    eBlobStorage
};

class NCBI_XCONNECT_EXPORT CBlobStreamHelper
{
public:
    CBlobStreamHelper(CNetCacheAPI::TInstance storage,
            string& data, size_t& data_size) :
        m_Storage(storage), m_Data(&data), m_DataSize(&data_size)
    {
    }

    ~CBlobStreamHelper();

    CNcbiOstream& GetOStream(const string& fname = "",
        EStdOutErrStorageType type = eBlobStorage,
        size_t max_inline_size = kMaxBlobInlineSize);

    CNcbiIstream& GetIStream(string* fname = NULL,
        EStdOutErrStorageType* type = NULL);
    void Reset();

private:
    CNetCacheAPI m_Storage;
    auto_ptr<CNcbiIstream> m_IStream;
    auto_ptr<IEmbeddedStreamWriter> m_Writer;
    auto_ptr<CNcbiOstream> m_OStream;
    string* m_Data;
    size_t* m_DataSize;
};


/// Remote Application Request (both client side and application executor side)
///
/// It is used by a client application which wants to run a remote application
/// through NetSchedule infrastructure and should be used in conjunction with
/// CGridJobSubmitter class
///
/// It is also used by a grid worker node to get parameters
/// for the remote application.
///
class NCBI_XCONNECT_EXPORT CRemoteAppRequest
{
public:
    explicit CRemoteAppRequest(CNetCacheAPI::TInstance storage,
            size_t max_inline_size = kMaxBlobInlineSize) :
        m_NetCacheAPI(storage),
        m_AppRunTimeout(0),
        m_StdIn(storage, m_InBlobIdOrData, m_StdInDataSize),
        m_StdInDataSize(0),
        m_StorageType(eBlobStorage),
        m_ExlusiveMode(false),
        m_MaxInlineSize(max_inline_size)
    {
    }

    ~CRemoteAppRequest();

    /// Set the command line for the remote application.
    /// The command line must not contain the remote program
    /// name -- only its arguments.
    void SetCmdLine(const string& cmdline) { m_CmdLine = cmdline; }
    /// Get the command line of the remote application.
    const string& GetCmdLine() const { return m_CmdLine; }

    void SetAppRunTimeout(unsigned int sec) { m_AppRunTimeout = sec; }
    unsigned int GetAppRunTimeout() const { return m_AppRunTimeout; }

    /// Transfer a file to an application executor side.
    /// It only makes sense to transfer a file if its name also mentioned in
    /// the command line for the remote application. When the file is transfered
    /// the the executor side it gets stored to a temporary directory and then its
    /// original name in the command line will be replaced with the new temporary name.
    void AddFileForTransfer(const string& fname,
        EStdOutErrStorageType tt = eBlobStorage)
    {
        m_Files[fname] = tt;
    }
    const string& GetWorkingDir() const { return m_TmpDirName; }

    /// Get an output stream to write data to a remote application stdin
    CNcbiOstream& GetStdIn()
    {
        return m_StdIn.GetOStream("", eBlobStorage, m_MaxInlineSize);
    }
    /// Get the stdin stream of the remote application.
    CNcbiIstream& GetStdInForRead()
    {
        return m_StdIn.GetIStream();
    }

    void SetExclusiveMode(bool on_off) { m_ExlusiveMode = on_off; }
    bool IsExclusiveMode() const { return m_ExlusiveMode; }


    void SetStdOutErrFileNames(const string& stdout_fname,
        const string& stderr_fname,
        EStdOutErrStorageType type)
    {
        m_StdOutFileName = stdout_fname;
        m_StdErrFileName = stderr_fname;
        m_StorageType = type;
    }

    const string& GetStdOutFileName() const { return m_StdOutFileName; }
    const string& GetStdErrFileName() const { return m_StdErrFileName; }
    EStdOutErrStorageType GetStdOutErrStorageType() const
        { return m_StorageType; }

    const string& GetInBlobIdOrData() const { return m_InBlobIdOrData; }

    void SetMaxInlineSize(size_t max_inline_size) { m_MaxInlineSize = max_inline_size; }

    /// Serialize a request to a given stream. After call to this method the instance
    /// cleans itself an it can be reused.
    void Send(CNcbiOstream& os);
    void Deserialize(CNcbiIstream& is);

    void Reset();

    static void SetTempDir(const string& path);
    static const string& GetTempDir();

protected:
    typedef map<string, EStdOutErrStorageType> TFiles;

    CNetCacheAPI& GetNetCacheAPI() { return m_NetCacheAPI; }
    const TFiles& GetFileNames() const { return m_Files; }

    void x_CreateWDir();
    void x_RemoveWDir();

private:
    static CAtomicCounter sm_DirCounter;
    static string sm_TmpDirPath;

    CNetCacheAPI m_NetCacheAPI;
    string m_CmdLine;
    unsigned int m_AppRunTimeout;

    string m_TmpDirName;
    TFiles m_Files;

    CBlobStreamHelper m_StdIn;
    size_t m_StdInDataSize;

    string m_InBlobIdOrData;

    string m_StdErrFileName;
    string m_StdOutFileName;
    EStdOutErrStorageType m_StorageType;
    bool m_ExlusiveMode;
    size_t m_MaxInlineSize;

    bool x_CopyLocalFile(const string& old_fname, string& new_fname);
};

/// Remote Application Result (both client side and application executor side)
///
/// It is used by a grid worker node to send results of a
/// finished remote application to the client.
///
/// It is also used by the client application to get the job results
/// and should be used in conjunction with CGridJobStatus
///
class NCBI_XCONNECT_EXPORT CRemoteAppResult
{
public:
    CRemoteAppResult(CNetCacheAPI::TInstance netcache_api,
            size_t max_inline_size = kMaxBlobInlineSize) :
        m_NetCacheAPI(netcache_api),
        m_RetCode(-1),
        m_StdOut(netcache_api, m_OutBlobIdOrData, m_OutBlobSize),
        m_OutBlobSize(0),
        m_StdErr(netcache_api, m_ErrBlobIdOrData, m_ErrBlobSize),
        m_ErrBlobSize(0),
        m_StorageType(eBlobStorage),
        m_MaxInlineSize(max_inline_size)
    {
    }
    ~CRemoteAppResult();

    /// Get a stream to put remote application's stdout to
    CNcbiOstream& GetStdOutForWrite()
    {
        return m_StdOut.GetOStream(m_StdOutFileName,
            m_StorageType, m_MaxInlineSize);
    }
    /// Get a remote application stdout
    CNcbiIstream& GetStdOut()
    {
        return m_StdOut.GetIStream(&m_StdOutFileName, &m_StorageType);
    }

    CNcbiOstream& GetStdErrForWrite()
    {
        return m_StdErr.GetOStream(m_StdErrFileName,
            m_StorageType, m_MaxInlineSize);
    }
    /// Get a remote application stderr
    CNcbiIstream& GetStdErr()
    {
        return m_StdErr.GetIStream(&m_StdErrFileName, &m_StorageType);
    }

    void SetRetCode(int ret_code) { m_RetCode = ret_code; }
    int GetRetCode() const { return m_RetCode; }

    void Serialize(CNcbiOstream& os);
    /// Deserialize a request from a given stream.
    void Receive(CNcbiIstream& is);

    void Reset();

    void SetStdOutErrFileNames(const string& stdout_fname,
                               const string& stderr_fname,
                               EStdOutErrStorageType type)
    {
        m_StdOutFileName = stdout_fname;
        m_StdErrFileName = stderr_fname;
        m_StorageType = type;
    }
    const string& GetStdOutFileName() const { return m_StdOutFileName; }
    const string& GetStdErrFileName() const { return m_StdErrFileName; }
    EStdOutErrStorageType GetStdOutErrStorageType() const
        { return m_StorageType; }
    const string& GetOutBlobIdOrData() const { return m_OutBlobIdOrData; }
    const string& GetErrBlobIdOrData() const { return m_ErrBlobIdOrData; }

    void SetMaxInlineSize(size_t max_inline_size)
        { m_MaxInlineSize = max_inline_size; }

private:
    CNetCacheAPI m_NetCacheAPI;
    int m_RetCode;

    CBlobStreamHelper m_StdOut;
    string m_OutBlobIdOrData;
    size_t m_OutBlobSize;
    string m_StdOutFileName;

    CBlobStreamHelper m_StdErr;
    string m_ErrBlobIdOrData;
    size_t m_ErrBlobSize;
    string m_StdErrFileName;
    EStdOutErrStorageType m_StorageType;
    size_t m_MaxInlineSize;
};


NCBI_XCONNECT_EXPORT
void TokenizeCmdLine(const string& cmdline, vector<string>& args);

NCBI_XCONNECT_EXPORT
string JoinCmdLine(const vector<string>& args);


END_NCBI_SCOPE

#endif // CONNECT_SERVICES__REMOTE_APP_MB_HPP
