#ifndef CONNECT_SERVICES__REMOTE_APP_IMPL_HPP
#define CONNECT_SERVICES__REMOTE_APP_IMPL_HPP

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
 * Authors:  Maxim Didneko,
 *
 * File Description:
 *
 */

#include <corelib/ncbistre.hpp>
#include <corelib/blob_storage.hpp>
#include <connect/services/remote_app.hpp>

BEGIN_NCBI_SCOPE

class IRemoteAppRequest_Impl
{

protected:
    explicit IRemoteAppRequest_Impl(IBlobStorage* storage)
        : m_InBlob(storage), m_AppRunTimeout(0)
    {
    }

public:

    virtual ~IRemoteAppRequest_Impl()
    {
        Reset();
    }

    virtual CNcbiOstream& GetStdInForWrite() = 0;
    virtual CNcbiIstream& GetStdInForRead() = 0;

    virtual void Serialize(CNcbiOstream& os) = 0;
    virtual void Deserialize(CNcbiIstream& is) = 0;

    virtual void Reset();

    void SetCmdLine(const string& cmdline) { m_CmdLine = cmdline; }
    const string& GetCmdLine() const { return m_CmdLine; }

    void SetAppRunTimeout(unsigned int sec) { m_AppRunTimeout = sec; }
    unsigned int GetAppRunTimeout() const { return m_AppRunTimeout; }

    void AddFileForTransfer(const string& fname, IRemoteAppRequest::ETrasferType tt)
    {
        m_Files[fname] = tt;
    }
    const string& GetWorkingDir() const { return m_TmpDirName; }


    static void SetTempDir(const string& path);
    static const string& GetTempDir();

protected:
    typedef map<string, IRemoteAppRequest::ETrasferType> TFiles;

    IBlobStorage& GetInBlob() { return *m_InBlob; }
    const TFiles& GetFileNames() const { return m_Files; }

    void x_CreateWDir();
    void x_RemoveWDir();

private:
    static CAtomicCounter sm_DirCounter;
    static string sm_TmpDirPath;

    auto_ptr<IBlobStorage> m_InBlob;
    string m_CmdLine;
    unsigned int m_AppRunTimeout;

    string m_TmpDirName;
    TFiles m_Files;
};


/////////////////////////////////////////////////////////////////////////////////////
//
class IRemoteAppResult_Impl
{
protected:

    IRemoteAppResult_Impl(IBlobStorage* out_cache, IBlobStorage* err_cache)
        : m_OutBlob(out_cache), m_ErrBlob(err_cache), m_RetCode(-1)
    {
    }

public:
    virtual ~IRemoteAppResult_Impl()
    {
        Reset();
    };

    virtual CNcbiIstream& GetStdOutForRead() = 0;
    virtual CNcbiOstream& GetStdOutForWrite() = 0;

    virtual CNcbiIstream& GetStdErrForRead() = 0;
    virtual CNcbiOstream& GetStdErrForWrite() = 0;

    void SetRetCode(int ret_code) { m_RetCode = ret_code; }
    int GetRetCode() const { return m_RetCode; }

    virtual void Serialize(CNcbiOstream& os) = 0;
    virtual void Deserialize(CNcbiIstream& is) = 0;

    virtual void Reset();

protected:
    IBlobStorage& GetOutBlob() { return *m_OutBlob; }
    IBlobStorage& GetErrBlob() { return *m_ErrBlob; }

private:
    auto_ptr<IBlobStorage> m_OutBlob;
    auto_ptr<IBlobStorage> m_ErrBlob;

    int m_RetCode;
};

END_NCBI_SCOPE

#endif // CONNECT_SERVICES__REMOTE_APP_IMPL_HPP
