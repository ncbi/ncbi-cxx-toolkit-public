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
 *   Government have not placed any restriction on its use or reproduction.
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
 * Authors:  Maxim Didenko
 *
 * File Description:
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbifile.hpp>

#include <connect/services/remote_app_sb.hpp>
#include <connect/services/error_codes.hpp>

#include "remote_app_impl.hpp"


#define NCBI_USE_ERRCODE_X   ConnServ_Remote

BEGIN_NCBI_SCOPE

static inline void s_CopyStream(CNcbiIstream& is, CNcbiOstream& os, size_t bytes)
{
    static const size_t kBufSize = 4096;
    vector<char> buf(kBufSize);
    while(bytes > 0 && is.good()) {
        size_t bytes_to_read = min(bytes, kBufSize);
        is.read(&buf[0], bytes_to_read);
        size_t bytes_to_write = is.gcount();
        os.write(&buf[0], bytes_to_write);        
        bytes -= bytes_to_write;
    }
}
static inline string s_ReadBlob(CNcbiIstream& is, IBlobStorage& storage)
{
    string blob_key;
    size_t bsize = 0;
    is >> bsize;
    char c; is.read(&c,1); // read delemiter between data size and data itself
    CNcbiOstream& os = storage.CreateOStream(blob_key);
    s_CopyStream(is, os, bsize);
    storage.Reset();
    return blob_key;
}

static inline void s_WriteBlob(CNcbiOstream& os, const string& blobkey, 
                               IBlobStorage& storage)
{
    if (!blobkey.empty()) {
        size_t bsize = 0;
        CNcbiIstream& is = storage.GetIStream(blobkey, &bsize);
        os << bsize << " " ;
        if (bsize > 0) os << is.rdbuf();
    } else {
        os << "0 ";
    }
}

//////////////////////////////////////////////////////////////////////////////
//
class CRemoteAppRequestSB_Impl : public IRemoteAppRequest_Impl
{
public:
    explicit CRemoteAppRequestSB_Impl(IBlobStorage* storage)  
        : IRemoteAppRequest_Impl(storage), 
          m_EmptyStream((const char*)"",0)
    {
        m_EmptyStream.setstate(IOS_BASE::eofbit);
    }

    ~CRemoteAppRequestSB_Impl() 
    {
        try {
            Reset();
        } NCBI_CATCH_ALL_X(17, "CRemoteAppRequestSB_Impl::~CRemoteAppRequestSB_Impl()");
    }

    CNcbiOstream& GetStdInForWrite() 
    {         
        return GetInBlob().CreateOStream(m_StdInKey);
    }
    CNcbiIstream& GetStdInForRead() 
    { 
        if (!m_StdInKey.empty())
            return GetInBlob().GetIStream(m_StdInKey);
        return m_EmptyStream;
    }

    void SetAppName(const string& name) { m_AppName = name; }
    const string& GetAppName() const { return m_AppName; }

    const string& GetStdInKey() const { return m_StdInKey; }

    void Serialize(CNcbiOstream& os);
    void Deserialize(CNcbiIstream& is);

    void Reset();
       
private:
    string                 m_StdInKey;
    CNcbiIstrstream m_EmptyStream;
    string m_AppName;
};


void CRemoteAppRequestSB_Impl::Serialize(CNcbiOstream& os)
{
    GetInBlob().Reset();

    WriteStrWithLen(os, m_AppName);
    WriteStrWithLen(os, GetCmdLine());
    os << GetAppRunTimeout() << " ";

    s_WriteBlob(os, m_StdInKey, GetInBlob());
    TFiles checked_files;
    ITERATE(TFiles, it, GetFileNames()) {
        const string& fname = it->first;
        CFile file(fname);
        if (!file.Exists()) {
            LOG_POST_X(11, Warning << "File :\"" << fname << "\" does not exist.");
            continue;
        }
        if (NStr::Find(GetCmdLine(), fname) == NPOS) {
            LOG_POST_X(12, Warning << "File :\"" << fname << "\" is not found in cmdline. Skipping.");
            continue;
        }
        checked_files[fname] = it->second;
    }
    os << checked_files.size() << " ";
    ITERATE(TFiles, it, checked_files) {
        const string& fname = it->first;
        CFile file(fname);
        Int8 len = file.GetLength();
        ifstream ifstr(fname.c_str());
        WriteStrWithLen(os, fname);
        os << len << " ";
        if (len > 0) os << ifstr.rdbuf();        
    }    

    Reset();
}

void CRemoteAppRequestSB_Impl::Deserialize(CNcbiIstream& is)
{
    Reset();
    
    ReadStrWithLen(is,m_AppName);
    string cmdline; ReadStrWithLen(is,cmdline); SetCmdLine(cmdline);
    int timeout; is >> timeout; SetAppRunTimeout(timeout);
    m_StdInKey = s_ReadBlob(is, GetInBlob());

    int fcount = 0;
    vector<string> args;
    if (!is.good()) return;
    is >> fcount;
    if ( fcount > 0 )
        TokenizeCmdLine(GetCmdLine(), args);


    for( int i = 0; i < fcount; ++i) {
        if ( i == 0 )
            x_CreateWDir();

        string fname;
        ReadStrWithLen(is, fname);
        if (!is.good()) break;
        size_t len;
        is >> len;
        CFile file(fname);
        string nfname = GetWorkingDir() + CDirEntry::GetPathSeparator() 
            + file.GetName();
        CNcbiOfstream of(nfname.c_str());
        char c; is.read(&c,1); // read delemiter between data size and data itself
        s_CopyStream(is, of, len);
        for(vector<string>::iterator it = args.begin();
            it != args.end(); ++it) {
            string& arg = *it;
            SIZE_TYPE pos = NStr::Find(arg, fname);
            if (pos == NPOS)
                continue;
            if ( (pos == 0 || !isalnum((unsigned char)arg[pos-1]) )
                 && pos + fname.size() == arg.size())
                arg = NStr::Replace(arg, fname, nfname);
        }
    }
    if ( fcount > 0 ) {
        SetCmdLine(JoinCmdLine(args));
    }
}

void CRemoteAppRequestSB_Impl::Reset()
{
    IRemoteAppRequest_Impl::Reset();
    m_StdInKey = "";
    m_AppName = "";
}

//////////////////////////////////////////////////////////////////////////////
//

CRemoteAppRequestSB::CRemoteAppRequestSB(IBlobStorageFactory& factory)
    : m_Impl(new CRemoteAppRequestSB_Impl(factory.CreateInstance()))
{
}
CRemoteAppRequestSB::~CRemoteAppRequestSB()
{
}

CNcbiOstream& CRemoteAppRequestSB::GetStdIn()
{
    return m_Impl->GetStdInForWrite();
}

void CRemoteAppRequestSB::SetCmdLine(const string& cmd)
{
    m_Impl->SetCmdLine(cmd);
}

void CRemoteAppRequestSB::SetAppRunTimeout(unsigned int sec)
{
    m_Impl->SetAppRunTimeout(sec);
}

void CRemoteAppRequestSB::AddFileForTransfer(const string& fname, ETrasferType )
{
    m_Impl->AddFileForTransfer(fname, eBlobStorage);
}

void CRemoteAppRequestSB::Send(CNcbiOstream& os)
{
    m_Impl->Serialize(os);
}

void CRemoteAppRequestSB::SetAppName(const string& name)
{
    m_Impl->SetAppName(name);
}

//////////////////////////////////////////////////////////////////////////////
//

CRemoteAppRequestSB_Executer::CRemoteAppRequestSB_Executer(IBlobStorageFactory& factory)
    : m_Impl(new CRemoteAppRequestSB_Impl(factory.CreateInstance()))
{
}
CRemoteAppRequestSB_Executer::~CRemoteAppRequestSB_Executer()
{
}

CNcbiIstream&  CRemoteAppRequestSB_Executer::GetStdIn()
{
    return m_Impl->GetStdInForRead();
}

const string& CRemoteAppRequestSB_Executer::GetCmdLine() const
{
    return m_Impl->GetCmdLine();
}

unsigned int CRemoteAppRequestSB_Executer::GetAppRunTimeout() const
{
    return m_Impl->GetAppRunTimeout();
}
void CRemoteAppRequestSB_Executer::Receive(CNcbiIstream& is)
{
    m_Impl->Deserialize(is);
}

void CRemoteAppRequestSB_Executer::Reset()
{
    m_Impl->Reset();
}

void CRemoteAppRequestSB_Executer::Log(const string& prefix)
{
    LOG_POST_X(13, prefix
             << " Args: " << m_Impl->GetCmdLine());
}

//////////////////////////////////////////////////////////////////////////////
//

class CRemoteAppResultSB_Impl : public IRemoteAppResult_Impl
{
public:
    CRemoteAppResultSB_Impl(IBlobStorage* out_cache, IBlobStorage* err_cache)
        : IRemoteAppResult_Impl(out_cache, err_cache),
          m_EmptyStream((const char*)"",0)
    {
        m_EmptyStream.setstate(IOS_BASE::eofbit);
    }

    ~CRemoteAppResultSB_Impl() 
    {
        try {
            Reset();
        } NCBI_CATCH_ALL_X(18, "CRemoteAppResultSB_Impl::~CRemoteAppResultSB_Impl()");
    }

    CNcbiIstream& GetStdOutForRead() 
    { 
        if (!m_StdOutKey.empty()) 
            return GetOutBlob().GetIStream(m_StdOutKey); 
        return m_EmptyStream;
    }
    CNcbiOstream& GetStdOutForWrite() { return GetOutBlob().CreateOStream(m_StdOutKey); }

    CNcbiIstream& GetStdErrForRead() 
    {
        if (!m_StdErrKey.empty())
            return GetErrBlob().GetIStream(m_StdErrKey); 
        return m_EmptyStream;
    }
    CNcbiOstream& GetStdErrForWrite() { return GetErrBlob().CreateOStream(m_StdErrKey); }

    void Serialize(CNcbiOstream& os);
    void Deserialize(CNcbiIstream& is);

    void Reset();

private:

    string m_StdOutKey;
    string m_StdErrKey;
    
    CNcbiIstrstream m_EmptyStream;
};

void CRemoteAppResultSB_Impl::Serialize(CNcbiOstream& os)
{
    GetOutBlob().Reset();
    GetErrBlob().Reset();
    s_WriteBlob(os, m_StdOutKey, GetOutBlob());       
    s_WriteBlob(os, m_StdErrKey, GetErrBlob());       
    os << GetRetCode();
    Reset();    
}
void CRemoteAppResultSB_Impl::Deserialize(CNcbiIstream& is)
{
    Reset();
    m_StdOutKey = s_ReadBlob(is, GetOutBlob());
    m_StdErrKey = s_ReadBlob(is, GetErrBlob());
    int ret; is >> ret; SetRetCode(ret);
}

void CRemoteAppResultSB_Impl::Reset()
{
    IRemoteAppResult_Impl::Reset();
    m_StdOutKey = "";
    m_StdErrKey = "";
}

//////////////////////////////////////////////////////////////////////////////
//

CRemoteAppResultSB::CRemoteAppResultSB(IBlobStorageFactory& factory)
    : m_Impl(new CRemoteAppResultSB_Impl(factory.CreateInstance(), factory.CreateInstance()))
{
}
CRemoteAppResultSB::~CRemoteAppResultSB()
{
}

CNcbiIstream& CRemoteAppResultSB::GetStdOut()
{
    return m_Impl->GetStdOutForRead();
}

CNcbiIstream& CRemoteAppResultSB::GetStdErr()
{
    return m_Impl->GetStdErrForRead();
}

int CRemoteAppResultSB::GetRetCode() const
{
    return m_Impl->GetRetCode();
}

void CRemoteAppResultSB::Receive(CNcbiIstream& is)
{
    m_Impl->Deserialize(is);
}

//////////////////////////////////////////////////////////////////////////////
//

CRemoteAppResultSB_Executer::CRemoteAppResultSB_Executer(IBlobStorageFactory& factory)
    : m_Impl(new CRemoteAppResultSB_Impl(factory.CreateInstance(), factory.CreateInstance()))
{
}
CRemoteAppResultSB_Executer::~CRemoteAppResultSB_Executer()
{
}

CNcbiOstream& CRemoteAppResultSB_Executer::GetStdOut()
{
    return m_Impl->GetStdOutForWrite();
}

CNcbiOstream& CRemoteAppResultSB_Executer::GetStdErr()
{
    return m_Impl->GetStdErrForWrite();
}
   
void CRemoteAppResultSB_Executer::SetRetCode(int ret_code)
{
    m_Impl->SetRetCode(ret_code);
}

void CRemoteAppResultSB_Executer::Send(CNcbiOstream& os)
{
    m_Impl->Serialize(os);
}

void CRemoteAppResultSB_Executer::Reset()
{
    m_Impl->Reset();
}
void CRemoteAppResultSB_Executer::Log(const string& prefix)
{
}


END_NCBI_SCOPE
